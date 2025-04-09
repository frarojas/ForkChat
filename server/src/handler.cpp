#include <fstream>
#include <vector>
#include "../../include/json.hpp"
#include "fchatuser.h"
#include "handler.h"

using json = nlohmann::json;

Handler loadConfig() {
    std::ifstream in(CONFIG_OPTIONS);
    if (!in.is_open()) {
        throw std::runtime_error("Error: No se pudo abrir el archivo de configuración.");
    }

    json configFile;
    in >> configFile;

    Handler configs;
    configs.port = configFile["port"];
    configs.userFileName = configFile["userFileName"];
    return configs;
}

void addUser(const FchatUser& _fcUser, const std::string& _fileName) {
    json usersFile;
    
    usersFile.push_back({
        {"userName", _fcUser.userName},
        {"userIP", _fcUser.userIP},
        {"userPort", _fcUser.userPort},
        {"userIsConnect", _fcUser.userIsConnect}
        });
    std::ofstream out(_fileName);
    out << usersFile.dump(4); // Guarda con formato legible
}

std::vector<FchatUser> loadUsers(const std::string& _fileName) {
    std::ifstream in(_fileName);
    json usersFile;
    in >> usersFile;

    std::vector<FchatUser> userList;
    for (const auto& item : usersFile) {
        userList.emplace_back(
            item["userName"],
            item["userIP"],
            item["userPort"],
            item["userIsConnect"]
        );
    }
    return userList;
}