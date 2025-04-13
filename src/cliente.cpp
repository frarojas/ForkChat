#include <iostream>
#include <cstring>
#include <sstream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cstdlib>
#include <fstream>

// Definiciones de colores ANSI
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[1;32m"  // Para mensajes recibidos
#define COLOR_YELLOW  "\033[1;33m"  // Para el prompt de entrada
#define COLOR_CYAN    "\033[1;36m"  // Para avisos del sistema

#define MAX_BUFFER 1024

// Puertos definidos para descubrimiento y conexión TCP
int DISCOVERY_UDP_PORT = 5001;   // Puerto UDP para detección del servidor
int SERVER_TCP_PORT = 5000;  // Puerto TCP para la mensajería

const char* DISCOVERY_MESSAGE = "DISCOVER_SERVER";

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

// Función para descubrir la IP del servidor mediante broadcast UDP
std::string discoverServerIP() {
    int udpSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSock < 0) {
        perror("Error creando socket UDP");
        exit(EXIT_FAILURE);
    }
    
    int broadcastEnable = 1;
    if (setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0) {
        perror("Error habilitando broadcast");
        close(udpSock);
        exit(EXIT_FAILURE);
    }
    
    sockaddr_in broadcastAddr;
    memset(&broadcastAddr, 0, sizeof(broadcastAddr));
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(DISCOVERY_UDP_PORT);
    broadcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");
    
    if (sendto(udpSock, DISCOVERY_MESSAGE, strlen(DISCOVERY_MESSAGE), 0,
               (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr)) < 0) {
        perror("Error enviando mensaje de descubrimiento");
        close(udpSock);
        exit(EXIT_FAILURE);
    }
    
    sockaddr_in serverResp;
    socklen_t addrLen = sizeof(serverResp);
    char buffer[MAX_BUFFER];
    int recvLen = recvfrom(udpSock, buffer, MAX_BUFFER - 1, 0,
                           (struct sockaddr*)&serverResp, &addrLen);
    if (recvLen < 0) {
        perror("No se recibió respuesta de descubrimiento");
        close(udpSock);
        exit(EXIT_FAILURE);
    }
    buffer[recvLen] = '\0';
    
    std::string serverIP = inet_ntoa(serverResp.sin_addr);
    close(udpSock);
    
    std::cout << COLOR_CYAN << "Servidor detectado en IP: " << serverIP 
              << COLOR_RESET << std::endl;
    return serverIP;
}

// Función para enviar el mensaje de registro con el nombre de usuario
void registrarUsuario(int sockfd, const std::string &nombre) {
    std::string mensaje = "REGISTRO " + nombre;
    if (send(sockfd, mensaje.c_str(), mensaje.length(), 0) < 0) {
        perror("Error enviando el registro");
        exit(EXIT_FAILURE);
    }
}

// Función para recibir mensajes del servidor de forma continua (proceso hijo)
void recibirMensajes(int sockfd) {
    char buffer[MAX_BUFFER];
    while (true) {
        memset(buffer, 0, MAX_BUFFER);
        int bytesRecibidos = recv(sockfd, buffer, MAX_BUFFER - 1, 0);
        if (bytesRecibidos <= 0) {
            std::cout << COLOR_CYAN << "\nConexión cerrada por el servidor o error." 
                      << COLOR_RESET << std::endl;
            close(sockfd);
            exit(EXIT_FAILURE);
        }
        buffer[bytesRecibidos] = '\0';
        // Mostrar el mensaje recibido en color verde
        std::cout << "\n" << COLOR_GREEN << "[Mensaje recibido]: " 
                  << buffer << COLOR_RESET << std::endl;
        std::cout << COLOR_YELLOW << ">> " << COLOR_RESET;
        fflush(stdout);
    }
}

int main() {
    // Leer la configuración de los puertos desde el archivo
    leerConfiguracion();
    
    std::string nombreUsuario;
    
    // Se solicita el nombre de usuario
    std::cout << COLOR_CYAN << "Ingrese su nombre de usuario: " 
              << COLOR_RESET;
    std::getline(std::cin, nombreUsuario);
    
    // Descubre la IP del servidor usando broadcast UDP (la IP se obtiene automáticamente)
    std::string serverIP = discoverServerIP();
    
    // Crear el socket TCP del cliente
    int tcpSock = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSock < 0) {
        perror("Error al crear el socket TCP");
        exit(EXIT_FAILURE);
    }
    
    // Configurar la dirección del servidor
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_TCP_PORT);
    if (inet_pton(AF_INET, serverIP.c_str(), &server_addr.sin_addr) <= 0) {
        perror("Dirección IP inválida o no soportada");
        close(tcpSock);
        exit(EXIT_FAILURE);
    }
    
    // Conectarse al servidor vía TCP
    if (connect(tcpSock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en la conexión TCP");
        close(tcpSock);
        exit(EXIT_FAILURE);
    }
    
    std::cout << COLOR_CYAN << "Conectado al servidor " << serverIP 
              << " en el puerto " << SERVER_TCP_PORT << COLOR_RESET << std::endl;
    
    // Registrar el usuario en el servidor
    registrarUsuario(tcpSock, nombreUsuario);
    
    // Fork para crear dos procesos:
    // - El proceso hijo se encarga de recibir mensajes
    // - El proceso padre se encarga de enviar mensajes
    pid_t pid = fork();
    
    if (pid < 0) {
        perror("Error al crear el proceso");
        close(tcpSock);
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        // Proceso hijo: recibir mensajes del servidor
        recibirMensajes(tcpSock);
    } else {
        // Proceso padre: enviar mensajes escritos por el usuario
        std::string entrada;
        std::cout << COLOR_CYAN << "Para enviar un mensaje, escriba en el formato: " 
                  << "<destinatario> <mensaje>" << COLOR_RESET << std::endl;
        while (true) {
            std::cout << COLOR_YELLOW << ">> " << COLOR_RESET;
            std::getline(std::cin, entrada);
            if (!entrada.empty()) {
                std::istringstream iss(entrada);
                std::string destinatario;
                iss >> destinatario;
                std::string mensajeTexto;
                std::getline(iss, mensajeTexto);
                if (!mensajeTexto.empty() && mensajeTexto[0] == ' ')
                    mensajeTexto.erase(0, 1);
                std::string comando = "ENVIAR " + destinatario + " " + mensajeTexto;
                if (send(tcpSock, comando.c_str(), comando.length(), 0) < 0) {
                    perror("Error al enviar el mensaje");
                }
            }
        }
    }
    
    close(tcpSock);
    return EXIT_SUCCESS;
}
