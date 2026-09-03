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

static bool sendAll(socket_t socket, const std::string& data) {
    size_t sent = 0;
    while (sent < data.size()) {
        int n = send(socket, data.data() + sent,
                     static_cast<int>(data.size() - sent), 0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
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
            std::string reply;

            try {
                if (cmd == "SET" || cmd == "set") {
                    std::string key, value;
                    if (!(iss >> key)) reply = "ERR missing key\n";
                    else {
                        std::getline(iss, value);
                        if (!value.empty() && value[0] == ' ') value.erase(0, 1);
                        store->set(key, value);
                        reply = "OK\n";
                    }
                } else if (cmd == "GET" || cmd == "get") {
                    std::string key, out;
                    if (!(iss >> key)) reply = "ERR missing key\n";
                    else reply = store->get(key, out) ? out + "\n" : "(nil)\n";
                } else if (cmd == "DEL" || cmd == "del") {
                    std::string key;
                    if (!(iss >> key)) reply = "ERR missing key\n";
                    else { store->del(key); reply = "OK\n"; }
                } else if (cmd == "COMPACT" || cmd == "compact") {
                    store->compact();
                    reply = "OK\n";
                } else if (cmd == "QUIT" || cmd == "quit") {
                    CLOSESOCK(client);
                    return;
                } else if (!cmd.empty()) {
                    reply = "ERR unknown command\n";
                }
            } catch (const std::exception& e) {
                reply = std::string("ERR ") + e.what() + "\n";
            }

            if (!reply.empty() && !sendAll(client, reply)) {
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
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 1;
#endif

    KVStore store("data.db");
    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == BAD_SOCKET) return 1;

    int yes = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(PORT);
    if (bind(server, (sockaddr*)&addr, sizeof(addr)) != 0) {
        CLOSESOCK(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    if (listen(server, 16) != 0) {
        CLOSESOCK(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "kvstore server listening on localhost:" << PORT << std::endl;
    while (true) {
        socket_t client = accept(server, nullptr, nullptr);
        if (client == BAD_SOCKET) continue;
        std::thread(handleClient, client, &store).detach();
    }
}
