#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_TOPICS 5
#define MAX_NEWS_LENGTH 80
#define PIPE_NAME_LENGTH 50

void subscribe_to_topics(const char *pipeSSC, char *unique_pipe) {
    int fd = open(pipeSSC, O_WRONLY);
    if (fd == -1) {
        perror("[Suscriptor]: Error al abrir el pipeSSC para escribir");
        exit(EXIT_FAILURE);
    }

    char topics[MAX_TOPICS + 1];
    printf("Ingrese los tópicos de suscripción (A, E, C, P, S): ");
    fgets(topics, sizeof(topics), stdin);
    topics[strcspn(topics, "\n")] = '\0';

    if (strlen(topics) == 0 || strspn(topics, "AECPS") != strlen(topics)) {
        fprintf(stderr, "[Suscriptor]: Tópicos inválidos ingresados. Terminando.\n");
        close(fd);
        exit(EXIT_FAILURE);
    }

    if (write(fd, topics, strlen(topics) + 1) == -1) {
        perror("[Suscriptor]: Error al enviar los tópicos al SC");
    } else {
        printf("[Suscriptor]: Suscripción enviada: %s\n", topics);
    }
    close(fd);

    // Recibir el nombre del pipe único del sistema
    fd = open(pipeSSC, O_RDONLY);
    if (fd == -1) {
        perror("[Suscriptor]: Error al abrir el pipeSSC para leer respuesta");
        exit(EXIT_FAILURE);
    }
    if (read(fd, unique_pipe, PIPE_NAME_LENGTH) <= 0) {
        perror("[Suscriptor]: Error al leer el pipe único del sistema");
        close(fd);
        exit(EXIT_FAILURE);
    }
    close(fd);

    printf("[Suscriptor]: Pipe único recibido: %s\n", unique_pipe);
}

void listen_to_news(const char *unique_pipe) {
    int fd = open(unique_pipe, O_RDONLY);
    if (fd == -1) {
        perror("[Suscriptor]: Error al abrir el pipe único para leer noticias");
        exit(EXIT_FAILURE);
    }

    char news[MAX_NEWS_LENGTH + 1];
    printf("[Suscriptor]: Esperando noticias en %s...\n", unique_pipe);

    while (read(fd, news, sizeof(news)) > 0) {
        if (strcmp(news, "FIN") == 0) {
            printf("[Suscriptor]: Sistema de Comunicación ha finalizado.\n");
            break;
        }
        printf("[Suscriptor]: ¡Noticia recibida!: %s\n", news);
    }

    close(fd);
    printf("[Suscriptor]: Finalizado.\n");
}

int main(int argc, char *argv[]) {
    char *pipeSSC = NULL;
    char unique_pipe[PIPE_NAME_LENGTH];

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            pipeSSC = argv[++i];
        }
    }

    if (!pipeSSC) {
        fprintf(stderr, "Uso: %s -s pipeSSC\n", argv[0]);
        return EXIT_FAILURE;
    }

    subscribe_to_topics(pipeSSC, unique_pipe);
    listen_to_news(unique_pipe);

    return 0;
}
