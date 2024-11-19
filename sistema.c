#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <time.h>

#define MAX_SUBSCRIBERS 10

// Estructura para almacenar información de cada suscriptor
typedef struct {
    int fd;                 // Descriptor del pipe del suscriptor
    char categories[6];     // Categorías de interés
    char pipeName[20];      // Nombre del pipe exclusivo
} Subscriber;

Subscriber subscribers[MAX_SUBSCRIBERS]; // Lista de suscriptores
int subscriberCount = 0;                 // Número actual de suscriptores

// Agregar un suscriptor a la lista
void addSubscriber(int fd, const char *categories, const char *pipeName) {
    if (subscriberCount < MAX_SUBSCRIBERS) {
        subscribers[subscriberCount].fd = fd;
        strncpy(subscribers[subscriberCount].categories, categories, sizeof(subscribers[subscriberCount].categories) - 1);
        strncpy(subscribers[subscriberCount].pipeName, pipeName, sizeof(subscribers[subscriberCount].pipeName) - 1);
        subscriberCount++;
        printf("Nuevo suscriptor añadido: FD=%d, Categorías=%s, Pipe=%s\n", fd, categories, pipeName);
    } else {
        printf("Límite de suscriptores alcanzado.\n");
    }
}

// Distribuir noticias a los suscriptores interesados
void distributeNews(const char *news) {
    char category = news[0]; // Categoría es el primer carácter
    for (int i = 0; i < subscriberCount; i++) {
        if (strchr(subscribers[i].categories, category)) {
            int fd = open(subscribers[i].pipeName, O_WRONLY | O_NONBLOCK);
            if (fd != -1) {
                write(fd, news, strlen(news));
                close(fd);
                printf("Noticia enviada a suscriptor %d (%s): %s\n", subscribers[i].fd, subscribers[i].pipeName, news);
            } else {
                perror("Error al abrir pipe del suscriptor");
            }
        }
    }
}

// Enviar mensaje de fin de emisión
void sendEndBroadcast() {
    const char *endMessage = "Fin de la emisión de noticias.\n";
    for (int i = 0; i < subscriberCount; i++) {
        int fd = open(subscribers[i].pipeName, O_WRONLY | O_NONBLOCK);
        if (fd != -1) {
            write(fd, endMessage, strlen(endMessage));
            close(fd);
            printf("Mensaje de fin enviado al suscriptor %d (%s).\n", subscribers[i].fd, subscribers[i].pipeName);
        } else {
            perror("Error al abrir pipe del suscriptor");
        }
    }
    printf("Fin de emisión completado.\n");
}

// Gestionar el sistema de comunicación
void manageSystem(const char *publisherPipe, const char *subscriberPipe, int timeoutSeconds) {
    if (mkfifo(publisherPipe, 0666) == -1 && access(publisherPipe, F_OK) == -1) {
        perror("Error al crear el pipe del publicador");
        return;
    }
    if (mkfifo(subscriberPipe, 0666) == -1 && access(subscriberPipe, F_OK) == -1) {
        perror("Error al crear el pipe del suscriptor");
        return;
    }

    int pubFd = open(publisherPipe, O_RDONLY | O_NONBLOCK);
    int subFd = open(subscriberPipe, O_RDONLY | O_NONBLOCK);
    if (pubFd == -1 || subFd == -1) {
        perror("Error al abrir pipes");
        exit(1);
    }

    char buffer[256];
    char news[256];
    int publishersActive = 1;
    time_t lastActivity = time(NULL);

    while (1) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(pubFd, &readSet);
        FD_SET(subFd, &readSet);

        int maxFd = (pubFd > subFd) ? pubFd : subFd;
        struct timeval timeout = { .tv_sec = timeoutSeconds, .tv_usec = 0 };

        int activity = select(maxFd + 1, &readSet, NULL, NULL, &timeout);
        if (activity == -1) {
            perror("Error en select");
            break;
        }

        if (FD_ISSET(pubFd, &readSet)) {
            int bytesRead = read(pubFd, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                strcpy(news, buffer);
                distributeNews(news);
                lastActivity = time(NULL);
                publishersActive = 1;
            } else {
                publishersActive = 0;
            }
        }

        if (FD_ISSET(subFd, &readSet)) {
            int bytesRead = read(subFd, buffer, sizeof(buffer) - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                char *categories = strtok(buffer, ":");
                char *pipeName = strtok(NULL, ":");
                if (categories && pipeName) {
                    addSubscriber(subFd, categories, pipeName);
                    lastActivity = time(NULL);
                }
            }
        }

        if (!publishersActive && difftime(time(NULL), lastActivity) >= timeoutSeconds) {
            sendEndBroadcast();
            break;
        }
    }

    close(pubFd);
    close(subFd);
}

int main(int argc, char *argv[]) {
    const char *publisherPipe = NULL;
    const char *subscriberPipe = NULL;
    int timeoutSeconds = 5;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            publisherPipe = argv[++i];
        } else if (strcmp(argv[i], "-s") == 0) {
            subscriberPipe = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0) {
            timeoutSeconds = atoi(argv[++i]);
        }
    }

    if (publisherPipe && subscriberPipe) {
        manageSystem(publisherPipe, subscriberPipe, timeoutSeconds);
    } else {
        fprintf(stderr, "Uso: %s -p <pipePublicador> -s <pipeSuscriptor> -t <timeout>\n", argv[0]);
    }

    return 0;
}
