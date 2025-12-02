#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 5000
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100

typedef struct {
    int sockfd;
    char topic[50];
    int active;
} Subscriber;

Subscriber subscribers[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_to_subscribers(char *topic, char *message) {
    pthread_mutex_lock(&clients_mutex);
    char formatted_msg[BUFFER_SIZE];
    snprintf(formatted_msg, sizeof(formatted_msg), "EVENTO|%s|%s", topic, message);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (subscribers[i].active && strcmp(subscribers[i].topic, topic) == 0) {
            if (send(subscribers[i].sockfd, formatted_msg, strlen(formatted_msg), 0) < 0) {
                close(subscribers[i].sockfd);
                subscribers[i].active = 0;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg) {
    int client_sock = *(int *)arg;
    free(arg);
    char buffer[BUFFER_SIZE];
    int bytes_read;

    while ((bytes_read = recv(client_sock, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        char temp_buf[BUFFER_SIZE];
        strcpy(temp_buf, buffer);
        
        char *type = strtok(temp_buf, "|");
        char *topic = strtok(NULL, "|");
        char *payload = strtok(NULL, "|"); 

        if (type && topic) {
            if (strcmp(type, "SUB") == 0) {
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (!subscribers[i].active) {
                        subscribers[i].sockfd = client_sock;
                        strncpy(subscribers[i].topic, topic, 49);
                        subscribers[i].active = 1;
                        printf("[SUSCRIPCION] Cliente %d suscrito a %s\n", client_sock, topic);
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
            } else if (strcmp(type, "PUB") == 0 && payload) {
                printf("[DATOS RECIBIDOS] Tópico: %s -> %s\n", topic, payload);
                broadcast_to_subscribers(topic, payload);
            }
        }
    }

    close(client_sock);
    pthread_mutex_lock(&clients_mutex);
    for(int i=0; i<MAX_CLIENTS; i++) {
        if(subscribers[i].sockfd == client_sock) subscribers[i].active = 0;
    }
    pthread_mutex_unlock(&clients_mutex);
    return NULL;
}

int main() {
    int server_fd, *new_sock;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    for(int i=0; i<MAX_CLIENTS; i++) subscribers[i].active = 0;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Fallo socket");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Fallo bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Fallo listen");
        exit(EXIT_FAILURE);
    }

    printf("[BROKER INICIADO] Escuchando en puerto %d...\n", PORT);

    while (1) {
        new_sock = malloc(sizeof(int));
        if ((*new_sock = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            free(new_sock);
            continue;
        }
        
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, handle_client, (void *)new_sock);
        pthread_detach(thread_id); // Liberar recursos al terminar el hilo automáticamente
    }
    return 0;
}