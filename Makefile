CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread
INCLUDES = -I src -I include

# -------------------------
# Executables
# -------------------------
SERVER = server
CLIENT = client

# -------------------------
# Server source files
# -------------------------
SERVER_SOURCES = \
	src/server.cpp \
	src/common/CLI.cpp \
	src/common/config.cpp \
	src/common/SocketUtils.cpp \
	src/protocol/Protocol.cpp

SERVER_OBJECTS = $(SERVER_SOURCES:.cpp=.o)

# -------------------------
# Client source files
# -------------------------
CLIENT_SOURCES = \
	src/client.cpp \
	src/common/config.cpp \
	src/common/SocketUtils.cpp

CLIENT_OBJECTS = $(CLIENT_SOURCES:.cpp=.o)

# -------------------------
# Default target
# -------------------------
all: $(SERVER) $(CLIENT)

# -------------------------
# Build server
# -------------------------
$(SERVER): $(SERVER_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# -------------------------
# Build client
# -------------------------
$(CLIENT): $(CLIENT_OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# -------------------------
# Compile .cpp -> .o
# -------------------------
src/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# -------------------------
# Clean
# -------------------------
clean:
	rm -f $(SERVER_OBJECTS) $(CLIENT_OBJECTS)
	rm -f $(SERVER) $(CLIENT)

.PHONY: all clean
