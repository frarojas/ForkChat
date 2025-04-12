#include <string>

static const char* const CONFIG_OPTIONS = "config.json";

struct Handler {
    int port;  
    std::string userFileName;

    Handler() : port(0), userFileName("") {}
};

Handler loadConfig();