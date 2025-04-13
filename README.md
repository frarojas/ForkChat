# 🛰️ ForkChat

**ForkChat** es un sistema de mensajería entre clientes en C utilizando sockets y procesos (`fork()`), desarrollado como parte del curso de Paradigmas de Programación en el paradigma imperativo.

## 🚀 Funcionalidades
- Registro de usuario con IP y nombre.
- Envío de mensajes entre clientes a través de un servidor.
- Recepción de mensajes en procesos independientes.
- Configuración dinámica vía archivo `config.txt`.

## 🧾 Requisitos

- Linux
- Compilador `g++`
- Terminal compatible con ANSI para colores (si se usa junto al cliente con interfaz coloreada)

## 📦 Archivos

- `servidor.cpp`: Código fuente del servidor.
- `config.txt`: Archivo de configuración de puertos.
- `README.md`: Este archivo.

## ⚙️ Configuración

Crea un archivo llamado `config.txt` en la raíz del proyecto con el siguiente contenido:

Puedes cambiar los valores de los puertos sin necesidad de recompilar.

```bash
TCP_PORT=5000
UDP_PORT=5001
```

## 🛠️ Compilación

```bash
g++ -o servidor servidor.cpp
g++ -o cliente cliente.cpp
```

## 🚀 Ejecución

```bash
./servidor
./cliente
```

El servidor iniciará dos procesos:

1. UDP Discovery en el puerto especificado para recibir solicitudes de broadcast (DISCOVER_SERVER).

2. Servidor TCP en el puerto configurado para aceptar conexiones de clientes y reenviar mensajes.

## 🧠 Funcionalidades Técnicas

- Manejo de múltiples clientes concurrentes con select().

- Bifurcación de procesos (fork()) para manejo paralelo de TCP y UDP.

- Estructura std::map para registrar usuarios y almacenar mensajes en espera.

- Comunicación en texto plano simple.

