// server.cpp — Stage 4: a small multithreaded TCP server for the KV store.

#include "kvstore.h"
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <stdexcept>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  typedef SOCKET socket_t;
  #define CLOSESOCK closesocket
  #define BAD_SOCKET INVALID_SOCKET
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int socket_t;
  #define CLOSESOCK close
  #define BAD_SOCKET (-1)
#endif

static const int PORT = 6380;
static const size_t MAX_REQUEST_SIZE = 1024 * 1024;

// TCP send() may write only part of a response, so keep sending until done.
static bool sendAll(socket_t client, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(client, data.data() + sent,
                     static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

static bool parseKey(std::istringstream& iss, std::string& key) {
    return static_cast<bool>(iss >> key);
}

static bool hasExtraArgument(std::istringstream& iss) {
    std::string extra;
    return static_cast<bool>(iss >> extra);
}

void handleClient(socket_t client, KVStore* store) {
    std::string buffer;
    char chunk[1024];

    while (true) {
        int n = recv(client, chunk, sizeof(chunk), 0);
        if (n <= 0) break;

        buffer.append(chunk, n);
        if (buffer.size() > MAX_REQUEST_SIZE) {
            sendAll(client, "ERR request too large\n");
            break;
        }

        size_t nl;
        while ((nl = buffer.find('\n')) != std::string::npos) {
            std::string line = buffer.substr(0, nl);
            buffer.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();

            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;
            if (cmd.empty()) continue;

            std::string reply;
            try {
                if (cmd == "SET" || cmd == "set") {
                    std::string key, value;
                    if (!parseKey(iss, key)) {
                        reply = "ERR usage: SET <key> <value>\n";
                    } else {
                        std::getline(iss, value);
                        if (!value.empty() && value[0] == ' ') value.erase(0, 1);
                        store->set(key, value);
                        reply = "OK\n";
                    }
                } else if (cmd == "GET" || cmd == "get") {
                    std::string key, out;
                    if (!parseKey(iss, key) || hasExtraArgument(iss)) {
                        reply = "ERR usage: GET <key>\n";
                    } else {
                        reply = store->get(key, out) ? out + "\n" : "(nil)\n";
                    }
                } else if (cmd == "DEL" || cmd == "del") {
                    std::string key;
                    if (!parseKey(iss, key) || hasExtraArgument(iss)) {
                        reply = "ERR usage: DEL <key>\n";
                    } else {
                        store->del(key);
                        reply = "OK\n";
                    }
                } else if (cmd == "COMPACT" || cmd == "compact") {
                    if (hasExtraArgument(iss)) {
                        reply = "ERR usage: COMPACT\n";
                    } else {
                        store->compact();
                        reply = "OK\n";
                    }
                } else if (cmd == "QUIT" || cmd == "quit") {
                    CLOSESOCK(client);
                    return;
                } else {
                    reply = "ERR unknown command\n";
                }
            } catch (const std::exception& e) {
                reply = std::string("ERR ") + e.what() + "\n";
            }

            if (!sendAll(client, reply)) {
                CLOSESOCK(client);
                return;
            }
        }
    }

    CLOSESOCK(client);
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    try {
        KVStore store("data.db");

        socket_t server = socket(AF_INET, SOCK_STREAM, 0);
        if (server == BAD_SOCKET) {
            std::cerr << "socket() failed\n";
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }

        int yes = 1;
        setsockopt(server, SOL_SOCKET, SO_REUSEADDR,
                   (const char*)&yes, sizeof(yes));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // local development server
        addr.sin_port = htons(PORT);

        if (bind(server, (sockaddr*)&addr, sizeof(addr)) != 0) {
            std::cerr << "bind() failed on port " << PORT << "\n";
            CLOSESOCK(server);
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }

        if (listen(server, 16) != 0) {
            std::cerr << "listen() failed\n";
            CLOSESOCK(server);
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }

        std::cout << "kvstore server listening on 127.0.0.1:" << PORT << std::endl;

        while (true) {
            socket_t client = accept(server, nullptr, nullptr);
            if (client == BAD_SOCKET) continue;
            std::thread(handleClient, client, &store).detach();
        }
    } catch (const std::exception& e) {
        std::cerr << "server error: " << e.what() << "\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
}
