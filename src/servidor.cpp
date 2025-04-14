#include <iostream>
#include <cstring>
#include <sstream>
#include <map>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <cstdlib>
#include <fstream>

int SERVER_TCP_PORT = 5000;
int DISCOVERY_UDP_PORT = 5001;
#define MAX_BUFFER 1024

const char* DISCOVERY_REQUEST = "DISCOVER_SERVER";
const char* DISCOVERY_RESPONSE = "SERVER_RESPONSE";

struct Usuario {
    std::string nombre;
    std::string ip;
    int socket_fd;
};

std::map<std::string, Usuario> usuarios;
std::map<std::string, std::vector<std::string>> mensajesPendientes;

void leerConfiguracion() {
    std::ifstream archivo("../config/config.txt");
    if (!archivo.is_open()) {
        std::cerr << "No se pudo abrir config.txt. Usando puertos por defecto." << std::endl;
        return;
    }
    std::string linea;
    while (getline(archivo, linea)) {
        std::istringstream iss(linea);
        std::string clave;
        int valor;
        if (iss >> clave >> valor) {
            if (clave == "TCP_PORT") SERVER_TCP_PORT = valor;
            else if (clave == "UDP_PORT") DISCOVERY_UDP_PORT = valor;
        }
    }
    archivo.close();
}

void registrarUsuario(int client_fd, sockaddr_in client_addr, const std::string &nombre) {
    Usuario usr;
    usr.nombre = nombre;
    usr.ip = inet_ntoa(client_addr.sin_addr);
    usr.socket_fd = client_fd;
    usuarios[nombre] = usr;

    std::cout << "Usuario registrado: " << nombre << " desde " << usr.ip << std::endl;

    if (mensajesPendientes.find(nombre) != mensajesPendientes.end()) {
        for (const std::string &mensaje : mensajesPendientes[nombre]) {
            send(client_fd, mensaje.c_str(), mensaje.length(), 0);
        }
        mensajesPendientes.erase(nombre);
        std::cout << "Mensajes pendientes enviados a " << nombre << std::endl;
    }
}

void reenviarMensaje(const std::string &destinatario, const std::string &mensaje, int origen_fd) {
    // Buscar el nombre del usuario emisor basado en su descriptor de socket.
    std::string nombreEmisor = "desconocido";
    for (const auto &par : usuarios) {
        if (par.second.socket_fd == origen_fd) {
            nombreEmisor = par.second.nombre;
            break;
        }
    }

    if (usuarios.find(destinatario) != usuarios.end()) {
        int dest_fd = usuarios[destinatario].socket_fd;
        std::string fullMsg = "[" + nombreEmisor + "]: " + mensaje;
        send(dest_fd, fullMsg.c_str(), fullMsg.length(), 0);
        std::cout << "Reenviado mensaje de " << nombreEmisor << " a " << destinatario << std::endl;
    } else {
        std::string errorMsg = "Usuario destinatario no registrado.\n";
        send(origen_fd, errorMsg.c_str(), errorMsg.length(), 0);
    }
}

void manejarCliente(int client_fd, sockaddr_in client_addr, char *buffer, int len) {
    buffer[len] = '\0';
    std::string input(buffer);
    std::istringstream iss(input);
    std::string comando;
    iss >> comando;

    if (comando == "REGISTRO") {
        std::string nombre;
        iss >> nombre;
        registrarUsuario(client_fd, client_addr, nombre);
    } else if (comando == "ENVIAR") {
        std::string destinatario;
        iss >> destinatario;
        std::string mensaje;
        getline(iss, mensaje);
        if (!mensaje.empty() && mensaje[0] == ' ')
            mensaje.erase(0,1);
        reenviarMensaje(destinatario, mensaje, client_fd);
    } else {
        std::string error = "Comando no reconocido.\n";
        send(client_fd, error.c_str(), error.length(), 0);
    }
}

void iniciarServidorTCP() {
    leerConfiguracion();
    if (SERVER_TCP_PORT == 0 || DISCOVERY_UDP_PORT == 0) {
        std::cerr << "Error: Los puertos no están configurados correctamente." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::cout << "Puerto TCP: " << SERVER_TCP_PORT << std::endl;
    std::cout << "Puerto UDP: " << DISCOVERY_UDP_PORT << std::endl;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Error al crear el socket TCP");
        exit(EXIT_FAILURE);
    }
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_TCP_PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind TCP");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Error en listen TCP");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::cout << "Servidor TCP escuchando en el puerto " << SERVER_TCP_PORT << std::endl;

    fd_set master_set, working_set;
    FD_ZERO(&master_set);
    FD_SET(server_fd, &master_set);
    int max_fd = server_fd;

    while (true) {
        working_set = master_set;
        int activity = select(max_fd + 1, &working_set, nullptr, nullptr, nullptr);
        if (activity < 0) {
            perror("Error en select TCP");
            break;
        }

        for (int fd = 0; fd <= max_fd; fd++) {
            if (FD_ISSET(fd, &working_set)) {
                if (fd == server_fd) {
                    sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
                    if (client_fd < 0) {
                        perror("Error en accept TCP");
                        continue;
                    }
                    FD_SET(client_fd, &master_set);
                    if (client_fd > max_fd)
                        max_fd = client_fd;
                    std::cout << "Nueva conexión aceptada (fd=" << client_fd << ") desde "
                              << inet_ntoa(client_addr.sin_addr) << std::endl;
                } else {
                    char buffer[MAX_BUFFER];
                    int bytes_recv = recv(fd, buffer, sizeof(buffer) - 1, 0);
                    if (bytes_recv <= 0) {
                        std::cout << "La conexión (fd=" << fd << ") fue cerrada." << std::endl;
                        close(fd);
                        FD_CLR(fd, &master_set);
                    } else {
                        sockaddr_in client_addr;
                        socklen_t len = sizeof(client_addr);
                        getpeername(fd, (struct sockaddr*)&client_addr, &len);
                        manejarCliente(fd, client_addr, buffer, bytes_recv);
                    }
                }
            }
        }
    }
    close(server_fd);
}

void iniciarDescubrimientoUDP() {
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0) {
        perror("Error al crear socket UDP");
        exit(EXIT_FAILURE);
    }

    sockaddr_in udp_addr;
    memset(&udp_addr, 0, sizeof(udp_addr));
    udp_addr.sin_family = AF_INET;
    udp_addr.sin_addr.s_addr = INADDR_ANY;
    udp_addr.sin_port = htons(DISCOVERY_UDP_PORT);

    if (bind(udpSock, (struct sockaddr*)&udp_addr, sizeof(udp_addr)) < 0) {
        perror("Error en bind UDP");
        close(udpSock);
        exit(EXIT_FAILURE);
    }

    std::cout << "Servidor UDP de descubrimiento escuchando en el puerto " << DISCOVERY_UDP_PORT << std::endl;

    char buffer[MAX_BUFFER];
    while (true) {
        memset(buffer, 0, MAX_BUFFER);
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int recvLen = recvfrom(udpSock, buffer, MAX_BUFFER - 1, 0,
                               (struct sockaddr*)&client_addr, &addr_len);
        if (recvLen < 0) {
            perror("Error recibiendo mensaje de descubrimiento");
            continue;
        }
        buffer[recvLen] = '\0';
        std::string mensaje(buffer);
        std::cout << "Recibido UDP: " << mensaje << " desde "
                  << inet_ntoa(client_addr.sin_addr) << std::endl;

        if (mensaje == DISCOVERY_REQUEST) {
            if (sendto(udpSock, DISCOVERY_RESPONSE, strlen(DISCOVERY_RESPONSE), 0,
                       (struct sockaddr*)&client_addr, addr_len) < 0) {
                perror("Error enviando respuesta de descubrimiento");
            } else {
                std::cout << "Enviado respuesta de descubrimiento a "
                          << inet_ntoa(client_addr.sin_addr) << std::endl;
            }
        }
    }

    close(udpSock);
}

int main() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Error al crear proceso para UDP");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        iniciarDescubrimientoUDP();
    } else {
        iniciarServidorTCP();
    }
    return 0;
}
