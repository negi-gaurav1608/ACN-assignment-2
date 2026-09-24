#include "common/CLI.hpp"
#include "common/config.hpp"
#include "common/SocketUtils.hpp"
#include "protocol/Protocol.hpp"
#include "server/Request.hpp"
#include "server/RequestQueue.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>


/*
 * ================================================================
 * Helper: Convert command-line scheduler to SchedulingPolicy
 * ================================================================
 *
 * Currently implemented:
 *
 *     FCFS
 *     SJF
 *
 * RR and DRR will be implemented in the next step because they
 * require request preemption and re-admission.
 */
SchedulingPolicy getSchedulingPolicy(
    const std::string& scheduler
) {
    if (scheduler == "fcfs") {
        return SchedulingPolicy::FCFS;
    }

    if (scheduler == "sjf") {
        return SchedulingPolicy::SJF;
    }

    throw std::runtime_error(
        "error: scheduler '" +
        scheduler +
        "' is not implemented in this step; "
        "use fcfs or sjf"
    );
}


/*
 * ================================================================
 * Monotonic timestamp
 * ================================================================
 *
 * We will later use this for:
 *
 *     arrival_ns
 *     start_ns
 *     finish_ns
 *
 * using CLOCK_MONOTONIC-equivalent steady_clock.
 */
uint64_t nowNs() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<
            std::chrono::nanoseconds
        >(
            std::chrono::steady_clock::now()
                .time_since_epoch()
        ).count()
    );
}


/*
 * ================================================================
 * Process one request
 * ================================================================
 *
 * At this step a request is processed completely once a worker
 * selects it.
 *
 * RR/DRR will later change this so that a request can be paused
 * and returned to the scheduler queue.
 */
void processRequest(
    const RequestPtr& request,
    const ServerOptions& args
) {
    if (!request) {
        return;
    }


    /*
     * ------------------------------------------------------------
     * Worker log
     * ------------------------------------------------------------
     */
    std::cout
        << "[Worker "
        << std::this_thread::get_id()
        << "] Request "
        << request->id
        << " ";


    if (request->type == RequestType::GET) {
        std::cout
            << "GET "
            << request->filename;
    }
    else if (request->type == RequestType::PUT) {
        std::cout
            << "PUT "
            << request->filename;
    }
    else {
        std::cout << "UNKNOWN";
    }


    std::cout
        << " ("
        << request->bytes
        << " bytes)"
        << std::endl;


    /*
     * ============================================================
     * PUT
     * ============================================================
     */
    if (request->type == RequestType::PUT) {

        /*
         * Tell the client that the server is ready
         * to receive the PUT body.
         */
        const std::string response = "OK 0\n";

        if (!sendAll(
                request->client_fd,
                response.data(),
                response.size()
            )) {

            close(request->client_fd);
            return;
        }


        /*
         * Destination path.
         *
         * Filename validation is already performed by the
         * protocol parser.
         */
        const std::string path =
            args.file_dir + "/" + request->filename;


        std::ofstream output(
            path,
            std::ios::binary
        );


        if (!output.is_open()) {

            const std::string error =
                "ERR Cannot open destination file\n";

            sendAll(
                request->client_fd,
                error.data(),
                error.size()
            );

            close(request->client_fd);
            return;
        }


        /*
         * Receive exactly request->bytes bytes.
         */
        constexpr std::size_t BUFFER_SIZE = 64 * 1024;

        std::vector<char> buffer(BUFFER_SIZE);

        std::size_t remaining = request->bytes;


        while (remaining > 0) {

            const std::size_t chunk =
                std::min(
                    BUFFER_SIZE,
                    remaining
                );


            if (!recvAll(
                    request->client_fd,
                    buffer.data(),
                    chunk
                )) {

                output.close();
                close(request->client_fd);
                return;
            }


            output.write(
                buffer.data(),
                static_cast<std::streamsize>(chunk)
            );


            if (!output) {

                output.close();
                close(request->client_fd);
                return;
            }


            remaining -= chunk;
        }


        output.close();


        /*
         * Final PUT response.
         */
        const std::string final_response = "OK 0\n";

        sendAll(
            request->client_fd,
            final_response.data(),
            final_response.size()
        );


        close(request->client_fd);

        return;
    }


    /*
     * ============================================================
     * GET
     * ============================================================
     */
    if (request->type == RequestType::GET) {

        const std::string path =
            args.file_dir + "/" + request->filename;


        std::ifstream input(
            path,
            std::ios::binary
        );


        if (!input.is_open()) {

            const std::string error =
                "ERR File not found\n";

            sendAll(
                request->client_fd,
                error.data(),
                error.size()
            );

            close(request->client_fd);

            return;
        }


        /*
         * Send:
         *
         *     OK <file_size>\n
         */
        const std::string header =
            "OK " +
            std::to_string(request->bytes) +
            "\n";


        if (!sendAll(
                request->client_fd,
                header.data(),
                header.size()
            )) {

            input.close();
            close(request->client_fd);

            return;
        }


        /*
         * Send file body.
         */
        constexpr std::size_t BUFFER_SIZE = 64 * 1024;

        std::vector<char> buffer(BUFFER_SIZE);

        std::size_t remaining = request->bytes;


        while (remaining > 0) {

            const std::size_t chunk =
                std::min(
                    BUFFER_SIZE,
                    remaining
                );


            input.read(
                buffer.data(),
                static_cast<std::streamsize>(chunk)
            );


            const std::streamsize received =
                input.gcount();


            if (received <= 0) {

                input.close();
                close(request->client_fd);

                return;
            }


            if (!sendAll(
                    request->client_fd,
                    buffer.data(),
                    static_cast<std::size_t>(received)
                )) {

                input.close();
                close(request->client_fd);

                return;
            }


            remaining -=
                static_cast<std::size_t>(received);
        }


        input.close();

        close(request->client_fd);

        return;
    }


    /*
     * ============================================================
     * Unknown request type
     * ============================================================
     */
    const std::string error =
        "ERR Unknown request type\n";


    sendAll(
        request->client_fd,
        error.data(),
        error.size()
    );


    close(request->client_fd);
}


/*
 * ================================================================
 * Main
 * ================================================================
 */
int main(
    int argc,
    char* argv[]
) {
    try {

        /*
         * --------------------------------------------------------
         * Parse command-line arguments
         * --------------------------------------------------------
         */
        ServerOptions args =
            parseServerArgs(
                argc,
                argv
            );


        /*
         * --------------------------------------------------------
         * Validate command-line options
         * --------------------------------------------------------
         */
        validateServerOptions(args);


        /*
         * --------------------------------------------------------
         * Load config.json
         * --------------------------------------------------------
         */
        ServerConfig config =
            loadServerConfig(
                args.config_path
            );


        /*
         * --------------------------------------------------------
         * Print configuration
         * --------------------------------------------------------
         */
        std::cout
            << "[Configuration Loaded]"
            << std::endl;

        std::cout
            << "IP: "
            << config.ip
            << std::endl;

        std::cout
            << "Port: "
            << config.port
            << std::endl;

        std::cout
            << "Server threads: "
            << config.server_threads
            << std::endl;

        std::cout
            << "Client threads: "
            << config.client_threads
            << std::endl;

        std::cout
            << "Storage: "
            << args.file_dir
            << std::endl;

        std::cout
            << "Scheduler: "
            << args.sched
            << std::endl;


        /*
         * --------------------------------------------------------
         * Convert scheduler name to policy.
         * --------------------------------------------------------
         */
        SchedulingPolicy policy =
            getSchedulingPolicy(
                args.sched
            );


        /*
         * --------------------------------------------------------
         * Create TCP socket
         * --------------------------------------------------------
         */
        int server_fd =
            socket(
                AF_INET,
                SOCK_STREAM,
                0
            );


        if (server_fd < 0) {

            throw std::runtime_error(
                "error: socket creation failed"
            );
        }


        /*
         * --------------------------------------------------------
         * SO_REUSEADDR
         * --------------------------------------------------------
         */
        int reuse = 1;

        if (
            setsockopt(
                server_fd,
                SOL_SOCKET,
                SO_REUSEADDR,
                &reuse,
                sizeof(reuse)
            ) < 0
        ) {

            close(server_fd);

            throw std::runtime_error(
                "error: setsockopt(SO_REUSEADDR) failed"
            );
        }


        /*
         * --------------------------------------------------------
         * Server address
         * --------------------------------------------------------
         */
        sockaddr_in server_addr{};

        server_addr.sin_family =
            AF_INET;

        server_addr.sin_port =
            htons(
                static_cast<uint16_t>(
                    config.port
                )
            );


        if (
            inet_pton(
                AF_INET,
                config.ip.c_str(),
                &server_addr.sin_addr
            ) <= 0
        ) {

            close(server_fd);

            throw std::runtime_error(
                "error: invalid server IP address"
            );
        }


        /*
         * --------------------------------------------------------
         * Bind
         * --------------------------------------------------------
         */
        if (
            bind(
                server_fd,
                reinterpret_cast<sockaddr*>(
                    &server_addr
                ),
                sizeof(server_addr)
            ) < 0
        ) {

            close(server_fd);

            throw std::runtime_error(
                "error: bind failed"
            );
        }


        /*
         * --------------------------------------------------------
         * Listen
         * --------------------------------------------------------
         */
        if (
            listen(
                server_fd,
                128
            ) < 0
        ) {

            close(server_fd);

            throw std::runtime_error(
                "error: listen failed"
            );
        }


        std::cout
            << "[TCP Server Listening] "
            << config.ip
            << ":"
            << config.port
            << std::endl;


        /*
         * --------------------------------------------------------
         * Shared scheduler queue
         * --------------------------------------------------------
         */
        RequestQueue queue(policy);


        /*
         * --------------------------------------------------------
         * Request ID generator
         * --------------------------------------------------------
         */
        std::atomic<uint64_t>
            next_request_id{1};


        /*
         * --------------------------------------------------------
         * Worker pool
         * --------------------------------------------------------
         *
         * Number of workers comes from:
         *
         *     server_threads
         */
        std::vector<std::thread> workers;

        workers.reserve(
            static_cast<std::size_t>(
                config.server_threads
            )
        );


        for (
            int i = 0;
            i < config.server_threads;
            ++i
        ) {

            workers.emplace_back(
                [&queue, &args]() {

                    while (true) {

                        /*
                         * Select the next request according
                         * to FCFS or SJF.
                         */
                        RequestPtr request =
                            queue.pop();


                        /*
                         * nullptr means that shutdown has
                         * started and no requests remain.
                         */
                        if (!request) {
                            break;
                        }


                        /*
                         * Serve request.
                         */
                        processRequest(
                            request,
                            args
                        );


                        /*
                         * Worker completion log.
                         */
                        std::cout
                            << "[Worker "
                            << std::this_thread::get_id()
                            << "] Completed request "
                            << request->id
                            << std::endl;
                    }
                }
            );
        }


        /*
         * --------------------------------------------------------
         * Main admission loop
         * --------------------------------------------------------
         */
        while (true) {

            sockaddr_in client_addr{};

            socklen_t client_len =
                sizeof(client_addr);


            int client_fd =
                accept(
                    server_fd,
                    reinterpret_cast<sockaddr*>(
                        &client_addr
                    ),
                    &client_len
                );


            if (client_fd < 0) {

                if (errno == EINTR) {
                    continue;
                }


                std::cerr
                    << "error: accept failed"
                    << std::endl;

                break;
            }


            /*
             * ----------------------------------------------------
             * Read request header
             * ----------------------------------------------------
             */
            std::string header;


            if (
                !recvLine(
                    client_fd,
                    header
                )
            ) {

                const std::string error =
                    "ERR Invalid request\n";


                sendAll(
                    client_fd,
                    error.data(),
                    error.size()
                );


                close(client_fd);

                continue;
            }


            /*
             * ----------------------------------------------------
             * Parse protocol request
             * ----------------------------------------------------
             */
            ParsedRequest parsed;


            try {

                parsed =
                    parseProtocolRequest(
                        header
                    );
            }
            catch (
                const std::exception& e
            ) {

                const std::string error =
                    std::string("ERR ") +
                    e.what() +
                    "\n";


                sendAll(
                    client_fd,
                    error.data(),
                    error.size()
                );


                close(client_fd);

                continue;
            }


            /*
             * ----------------------------------------------------
             * HEALTH
             * ----------------------------------------------------
             *
             * HEALTH bypasses the scheduler.
             *
             * It must not become a scheduled request.
             */
            if (
                parsed.type ==
                RequestType::HEALTH
            ) {

                const std::string response =
                    "OK " +
                    std::to_string(
                        queue.size()
                    ) +
                    "\n";


                sendAll(
                    client_fd,
                    response.data(),
                    response.size()
                );


                close(client_fd);

                continue;
            }


            /*
             * ----------------------------------------------------
             * Create request
             * ----------------------------------------------------
             */
            auto request =
                std::make_shared<Request>();


            request->id =
                next_request_id.fetch_add(1);


            request->type =
                parsed.type;


            request->filename =
                parsed.filename;


            request->client_fd =
                client_fd;


            request->arrival_ns =
                nowNs();


            /*
             * ----------------------------------------------------
             * GET
             * ----------------------------------------------------
             *
             * Determine file size before admitting the request.
             */
            if (
                request->type ==
                RequestType::GET
            ) {

                const std::string path =
                    args.file_dir +
                    "/" +
                    request->filename;


                std::ifstream input(
                    path,
                    std::ios::binary |
                    std::ios::ate
                );


                if (!input.is_open()) {

                    const std::string error =
                        "ERR File not found\n";


                    sendAll(
                        client_fd,
                        error.data(),
                        error.size()
                    );


                    close(client_fd);

                    continue;
                }


                const std::streamoff file_size =
                    input.tellg();


                input.close();


                if (file_size < 0) {

                    const std::string error =
                        "ERR Cannot determine file size\n";


                    sendAll(
                        client_fd,
                        error.data(),
                        error.size()
                    );


                    close(client_fd);

                    continue;
                }


                request->bytes =
                    static_cast<std::size_t>(
                        file_size
                    );
            }


            /*
             * ----------------------------------------------------
             * PUT
             * ----------------------------------------------------
             *
             * PUT size comes directly from the protocol header.
             */
            else if (
                request->type ==
                RequestType::PUT
            ) {

                request->bytes =
                    parsed.bytes;
            }


            /*
             * ----------------------------------------------------
             * Admission log
             * ----------------------------------------------------
             */
            std::cout
                << "[Admitted] Request "
                << request->id
                << " ";


            if (
                request->type ==
                RequestType::GET
            ) {

                std::cout
                    << "GET "
                    << request->filename;
            }
            else if (
                request->type ==
                RequestType::PUT
            ) {

                std::cout
                    << "PUT "
                    << request->filename;
            }


            std::cout
                << " ("
                << request->bytes
                << " bytes)"
                << std::endl;


            /*
             * ----------------------------------------------------
             * Admit request into shared queue.
             * ----------------------------------------------------
             */
            queue.push(request);
        }


        /*
         * --------------------------------------------------------
         * Stop accepting new connections.
         * --------------------------------------------------------
         */
        close(server_fd);


        /*
         * --------------------------------------------------------
         * Shut down scheduler.
         * --------------------------------------------------------
         *
         * Existing queued requests are still returned by pop().
         * Once the queue becomes empty, workers receive nullptr
         * and exit.
         */
        queue.shutdown();


        /*
         * --------------------------------------------------------
         * Join all worker threads.
         * --------------------------------------------------------
         */
        for (auto& worker : workers) {

            if (worker.joinable()) {
                worker.join();
            }
        }


        std::cout
            << "[Server Shutdown]"
            << std::endl;


        return 0;
    }


    /*
     * ------------------------------------------------------------
     * Error handling
     * ------------------------------------------------------------
     */
    catch (
        const std::exception& e
    ) {

        std::cerr
            << e.what()
            << std::endl;

        return 1;
    }
}
