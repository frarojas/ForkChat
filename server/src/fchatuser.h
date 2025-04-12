#include <string>

struct FchatUser {
    std::string userName;  
    std::string userIP;      
    int userPort;          
    bool userIsConnect;   

    // Constructor para inicializar un usuario
    FchatUser(const std::string& _name, const std::string& _ip, int _port, bool _connect)
        : userName(_name), userIP(_ip), userPort(_port), userIsConnect(_connect) {}
};