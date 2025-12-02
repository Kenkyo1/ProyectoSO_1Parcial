#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BROKER_IP "127.0.0.1"
#define BROKER_PORT 5000
#define BUFFER_SIZE 1024

// Función para enviar la petición de suscripción
void subscribe_to(int sock, char *topic) {
    char sub_msg[BUFFER_SIZE];
    // Protocolo: SUB|TOPICO|
    snprintf(sub_msg, sizeof(sub_msg), "SUB|%s|", topic);
    send(sock, sub_msg, strlen(sub_msg), 0);
    printf(" -> Solicitando suscripción a: '%s'\n", topic);
}

int main(int argc, char *argv[]) {
    // Verificamos que el usuario ingrese el tópico al ejecutar
    if (argc != 2) {
        printf("Uso incorrecto.\n");
        printf("Ejecutar como: ./subscriber <TOPICO_A_ESCUCHAR>\n");
        printf("Ejemplo: ./subscriber sensor/dht\n");
        return -1;
    }

    char *my_topic = argv[1];
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Error creando socket\n");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(BROKER_PORT);

    if (inet_pton(AF_INET, BROKER_IP, &serv_addr.sin_addr) <= 0) {
        printf("Dirección inválida\n");
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Conexión fallida con Broker (¿Está encendido?)\n");
        return -1;
    }

    printf("****************************************\n");
    printf("[SUBSCRIBER INICIADO]\n");
    
    // Nos suscribimos SOLAMENTE al tópico pasado por argumento
    subscribe_to(sock, my_topic);

    printf("Esperando datos de %s...\n", my_topic);
    printf("****************************************\n");

    // Bucle de lectura
    while (1) {
        int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_read <= 0) {
            printf("Desconectado del Broker.\n");
            break;
        }
        buffer[bytes_read] = '\0';

        // Parseo visual del mensaje recibido
        char *first_pipe = strchr(buffer, '|');
        if (first_pipe) {
            char *second_pipe = strchr(first_pipe + 1, '|');
            if (second_pipe) {
                *first_pipe = '\0';
                *second_pipe = '\0';
                
                char *topic = first_pipe + 1;
                char *payload = second_pipe + 1;

                // Solo mostramos si realmente es el tópico (seguridad extra visual)
                if (strcmp(topic, my_topic) == 0) {
                    printf("\n[NUEVO DATO RECIBIDO]\n");
                    printf(" > Tópico: %s\n", topic);
                    printf(" > Valor:  %s\n", payload);
                }
            }
        }
    }
    close(sock);
    return 0;
}