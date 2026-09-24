#include "common/config.hpp"
#include "common/SocketUtils.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>


// ------------------------------------------------------------
// Client arguments
// ------------------------------------------------------------
struct ClientArgs {

    std::string command;

    std::string target;

    std::string config_path =
        "config.json";
};


// ------------------------------------------------------------
// Parse client arguments
// ------------------------------------------------------------
ClientArgs parseClientArgs(
    int argc,
    char* argv[])
{

    if (argc < 3) {

        throw std::runtime_error(
            "error: invalid client arguments "
            "(usage: client <put|get> <target> "
            "[--config <path>])"
        );
    }


    ClientArgs args;


    args.command =
        argv[1];


    args.target =
        argv[2];


    for (
        int i = 3;
        i < argc;
        ++i)
    {

        std::string arg =
            argv[i];


        if (
            arg == "--config"
            && i + 1 < argc)
        {

            args.config_path =
                argv[++i];
        }
    }


    return args;
}


// ------------------------------------------------------------
// Connect to server
// ------------------------------------------------------------
int connectToServer(
    const std::string& ip,
    int port)
{

    int sock =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );


    if (sock < 0) {
        return -1;
    }


    sockaddr_in addr{};


    addr.sin_family =
        AF_INET;


    addr.sin_addr.s_addr =
        inet_addr(
            ip.c_str()
        );


    addr.sin_port =
        htons(port);


    if (connect(
            sock,
            reinterpret_cast<struct sockaddr*>(
                &addr
            ),
            sizeof(addr)) < 0)
    {

        close(sock);

        return -1;
    }


    return sock;
}


// ------------------------------------------------------------
// Main
// ------------------------------------------------------------
int main(
    int argc,
    char* argv[])
{

    try {

        // --------------------------------------------------------
        // Parse arguments
        // --------------------------------------------------------
        ClientArgs args =
            parseClientArgs(
                argc,
                argv
            );


        // --------------------------------------------------------
        // Load configuration
        // --------------------------------------------------------
        ServerConfig cfg =
            loadServerConfig(
                args.config_path
            );


        // --------------------------------------------------------
        // Connect to server
        //
        // Both PUT and GET use this socket.
        // --------------------------------------------------------
        int sock =
            connectToServer(
                cfg.ip,
                cfg.port
            );


        if (sock < 0) {

            std::cerr
                << "error: failed to connect to server\n";

            return 1;
        }


        // ========================================================
        // PUT
        // ========================================================
        if (
            args.command ==
            "put")
        {

            // ----------------------------------------------------
            // Open local file
            // ----------------------------------------------------
            std::ifstream ifs(
                args.target,
                std::ios::binary |
                std::ios::ate
            );


            if (!ifs.is_open()) {

                std::cerr
                    << "error: cannot open local file '"
                    << args.target
                    << "'\n";


                close(sock);

                return 1;
            }


            // ----------------------------------------------------
            // Determine file size
            // ----------------------------------------------------
            std::streamoff file_size =
                ifs.tellg();


            if (file_size < 0) {

                std::cerr
                    << "error: unable to determine file size\n";


                close(sock);

                return 1;
            }


            ifs.seekg(
                0,
                std::ios::beg
            );


            std::size_t size =
                static_cast<std::size_t>(
                    file_size
                );


            // ----------------------------------------------------
            // Extract filename
            // ----------------------------------------------------
            std::filesystem::path p(
                args.target
            );


            std::string basename =
                p.filename().string();


            // ----------------------------------------------------
            // Send PUT header
            // ----------------------------------------------------
            std::string req =
                "PUT "
                + basename
                + " "
                + std::to_string(size)
                + "\n";


            if (!sendAll(
                    sock,
                    req.data(),
                    req.size()))
            {

                std::cerr
                    << "error: failed to send PUT request\n";


                close(sock);

                return 1;
            }


            // ----------------------------------------------------
            // Wait for server's first OK
            // ----------------------------------------------------
            std::string response;


            if (!recvLine(
                    sock,
                    response))
            {

                std::cerr
                    << "error: no response from server\n";


                close(sock);

                return 1;
            }


            if (response != "OK 0") {

                std::cerr
                    << "error: server rejected PUT: "
                    << response
                    << "\n";


                close(sock);

                return 1;
            }


            // ----------------------------------------------------
            // Send file contents
            // ----------------------------------------------------
            const std::size_t
                BUFFER_SIZE =
                    64 * 1024;


            std::vector<char> buffer(
                BUFFER_SIZE
            );


            std::size_t remaining =
                size;


            while (
                remaining > 0)
            {

                std::size_t chunk =
                    std::min(
                        BUFFER_SIZE,
                        remaining
                    );


                ifs.read(
                    buffer.data(),
                    static_cast<std::streamsize>(
                        chunk
                    )
                );


                if (
                    ifs.gcount() !=
                    static_cast<std::streamsize>(
                        chunk
                    ))
                {

                    std::cerr
                        << "error: failed to read local file\n";


                    close(sock);

                    return 1;
                }


                if (!sendAll(
                        sock,
                        buffer.data(),
                        chunk))
                {

                    std::cerr
                        << "error: failed to send file data\n";


                    close(sock);

                    return 1;
                }


                remaining -=
                    chunk;
            }


            // ----------------------------------------------------
            // Wait for final OK
            // ----------------------------------------------------
            if (!recvLine(
                    sock,
                    response))
            {

                std::cerr
                    << "error: no final response from server\n";


                close(sock);

                return 1;
            }


            close(sock);


            if (response != "OK 0") {

                std::cerr
                    << "error: PUT failed: "
                    << response
                    << "\n";


                return 1;
            }


            std::cout
                << "[PUT Success] Uploaded "
                << basename
                << " ("
                << size
                << " bytes)\n";
        }


        // ========================================================
        // GET
        // ========================================================
        else if (
            args.command ==
            "get")
        {

            // ----------------------------------------------------
            // Send GET request
            // ----------------------------------------------------
            std::string request =
                "GET "
                + args.target
                + "\n";


            if (!sendAll(
                    sock,
                    request.data(),
                    request.size()))
            {

                std::cerr
                    << "Failed to send GET request\n";


                close(sock);

                return 1;
            }


            // ----------------------------------------------------
            // Receive response header
            // ----------------------------------------------------
            std::string response;


            if (!recvLine(
                    sock,
                    response))
            {

                std::cerr
                    << "Failed to receive server response\n";


                close(sock);

                return 1;
            }


            // ----------------------------------------------------
            // Server returned an error
            // ----------------------------------------------------
            if (
                response.rfind(
                    "ERR ",
                    0
                ) == 0)
            {

                std::cerr
                    << "[GET Error] "
                    << response
                    << std::endl;


                close(sock);

                return 1;
            }


            // ----------------------------------------------------
            // Validate OK response
            // ----------------------------------------------------
            if (
                response.rfind(
                    "OK ",
                    0
                ) != 0)
            {

                std::cerr
                    << "[GET Error] Invalid server response: "
                    << response
                    << std::endl;


                close(sock);

                return 1;
            }


            // ----------------------------------------------------
            // Extract file size
            //
            // Expected:
            //
            // OK <size>
            // ----------------------------------------------------
            std::string size_string =
                response.substr(3);


            std::size_t file_size =
                0;


            try {

                std::size_t pos =
                    0;


                unsigned long long parsed =
                    std::stoull(
                        size_string,
                        &pos
                    );


                // Make sure the entire string
                // was numeric.
                if (
                    pos !=
                    size_string.size())
                {

                    throw std::invalid_argument(
                        "invalid size"
                    );
                }


                file_size =
                    static_cast<std::size_t>(
                        parsed
                    );
            }


            catch (...) {

                std::cerr
                    << "[GET Error] Invalid file size\n";


                close(sock);

                return 1;
            }


            // ----------------------------------------------------
            // Open local destination file
            // ----------------------------------------------------
            std::ofstream output(
                args.target,
                std::ios::binary |
                std::ios::trunc
            );


            if (!output.is_open()) {

                std::cerr
                    << "[GET Error] Cannot create local file\n";


                close(sock);

                return 1;
            }


            // ----------------------------------------------------
            // Receive exactly file_size bytes
            // ----------------------------------------------------
            const std::size_t
                BUFFER_SIZE =
                    64 * 1024;


            std::vector<char> buffer(
                BUFFER_SIZE
            );


            std::size_t total_received =
                0;


            while (
                total_received <
                file_size)
            {

                std::size_t remaining =
                    file_size
                    - total_received;


                std::size_t to_receive =
                    std::min(
                        BUFFER_SIZE,
                        remaining
                    );


                if (!recvAll(
                        sock,
                        buffer.data(),
                        to_receive))
                {

                    std::cerr
                        << "[GET Error] Connection closed "
                           "before receiving complete file\n";


                    output.close();

                    close(sock);

                    return 1;
                }


                output.write(
                    buffer.data(),
                    static_cast<std::streamsize>(
                        to_receive
                    )
                );


                if (!output) {

                    std::cerr
                        << "[GET Error] Failed writing local file\n";


                    output.close();

                    close(sock);

                    return 1;
                }


                total_received +=
                    to_receive;
            }


            // ----------------------------------------------------
            // Close output and socket
            // ----------------------------------------------------
            output.close();

            close(sock);


            // ----------------------------------------------------
            // GET success
            // ----------------------------------------------------
            std::cout
                << "[GET Success] Downloaded "
                << args.target
                << " ("
                << file_size
                << " bytes)"
                << std::endl;
        }


        // ========================================================
        // Unknown command
        // ========================================================
        else {

            std::cerr
                << "error: unknown command '"
                << args.command
                << "' "
                << "(use 'put' or 'get')\n";


            close(sock);

            return 1;
        }
    }


    catch (
        const std::exception& e)
    {

        std::cerr
            << e.what()
            << std::endl;

        return 1;
    }


    return 0;
}
