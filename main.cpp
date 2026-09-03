#include "kvstore.h"
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>

// Tiny local REPL for trying the storage engine directly.

static bool hasExtraArgument(std::istringstream& iss) {
    std::string extra;
    return static_cast<bool>(iss >> extra);
}

int main() {
    try {
        KVStore store("data.db");

        std::cout << "kvstore - commands: set <k> <v> | get <k> | del <k> | compact | exit\n";

        std::string line;
        while (true) {
            std::cout << "> ";
            if (!std::getline(std::cin, line)) break;

            std::istringstream iss(line);
            std::string cmd;
            iss >> cmd;

            try {
                if (cmd == "set") {
                    std::string key, value;
                    if (!(iss >> key)) {
                        std::cout << "usage: set <key> <value>\n";
                        continue;
                    }
                    std::getline(iss, value);
                    if (!value.empty() && value[0] == ' ') value.erase(0, 1);
                    store.set(key, value);
                    std::cout << "OK\n";
                } else if (cmd == "get") {
                    std::string key, out;
                    if (!(iss >> key) || hasExtraArgument(iss)) {
                        std::cout << "usage: get <key>\n";
                        continue;
                    }
                    std::cout << (store.get(key, out) ? out : "(nil)") << "\n";
                } else if (cmd == "del") {
                    std::string key;
                    if (!(iss >> key) || hasExtraArgument(iss)) {
                        std::cout << "usage: del <key>\n";
                        continue;
                    }
                    store.del(key);
                    std::cout << "OK\n";
                } else if (cmd == "compact") {
                    if (hasExtraArgument(iss)) {
                        std::cout << "usage: compact\n";
                        continue;
                    }
                    uint64_t before = store.fileSize();
                    store.compact();
                    uint64_t after = store.fileSize();
                    std::cout << "compacted: " << before << " bytes -> " << after << " bytes\n";
                } else if (cmd == "exit" || cmd == "quit") {
                    break;
                } else if (!cmd.empty()) {
                    std::cout << "unknown command: " << cmd << "\n";
                }
            } catch (const std::exception& e) {
                std::cout << "error: " << e.what() << "\n";
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "startup error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
