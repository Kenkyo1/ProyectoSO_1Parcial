#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define BROKER_IP "127.0.0.1"
#define BROKER_PORT 5000
#define BUFFER_SIZE 1024

int MY_PORT = 6000;
char MY_TOPIC[50] = "sensor/general";

void forward_to_broker(char *message) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[ERROR] Creación socket broker\n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(BROKER_PORT);

    if (inet_pton(AF_INET, BROKER_IP, &serv_addr.sin_addr) <= 0) {
        printf("[ERROR] Dirección Broker inválida\n");
        close(sock);
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[ERROR] No se pudo conectar al Broker en %d\n", BROKER_PORT);
        close(sock);
        return;
    }

    send(sock, message, strlen(message), 0);
    close(sock);
}

void *handle_publisher(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(client_sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        char formatted_msg[BUFFER_SIZE + 128]; 
        
        snprintf(formatted_msg, sizeof(formatted_msg), "PUB|%s|%s", MY_TOPIC, buffer);
        
        printf("[GATEWAY %d] Recibido: (%s) -> Reenviando al Broker...\n", MY_PORT, buffer);
        forward_to_broker(formatted_msg);
    }

    close(client_sock);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc == 3) {
        MY_PORT = atoi(argv[1]);
        strncpy(MY_TOPIC, argv[2], 49);
    } else {
        printf("Uso: ./gateway <PUERTO> <TOPICO>\n");
        printf("Usando valores por defecto: Puerto %d, Topico '%s'\n", MY_PORT, MY_TOPIC);
    }

    int server_fd, *new_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Fallo socket");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(MY_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Fallo bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Fallo listen");
        exit(EXIT_FAILURE);
    }

    printf("****************************************\n");
    printf("[GATEWAY INICIADO]\n");
    printf(" -> Puerto de Escucha: %d\n", MY_PORT);
    printf(" -> Tópico Objetivo:   %s\n", MY_TOPIC);
    printf("****************************************\n");

    while (1) {
        new_sock = malloc(sizeof(int));
        if ((*new_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            free(new_sock);
            continue;
        }
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_publisher, (void *)new_sock);
        pthread_detach(thread_id);
    }
    return 0;
}