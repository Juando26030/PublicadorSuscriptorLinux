#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

// Función para recibir noticias desde el pipe exclusivo
void recibirNoticias(const char *pipeName, const char *categorias) {
    // Verificar si el pipe exclusivo existe
    if (access(pipeName, F_OK) == -1) {
        perror("Error al acceder al pipe de noticias");
        return;
    }

    // Abre el pipe en modo de solo lectura y no bloqueante
    int fd = open(pipeName, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        perror("Error al abrir pipe de noticias");
        return;
    }

    char buffer[256];

    // Bucle infinito para leer noticias del pipe
    while (1) {
        memset(buffer, 0, sizeof(buffer)); // Limpia el buffer
        int bytesRead = read(fd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0'; // Agrega terminador de cadena
            char *noticia = strtok(buffer, "\n"); // Divide las noticias en líneas
            while (noticia != NULL) {
                // Verifica si se ha recibido el mensaje de fin de emisión
                if (strcmp(noticia, "Fin de la emisión de noticias.") == 0) {
                    printf("Notificación de fin: %s\n", noticia);
                    close(fd); // Cierra el pipe
                    return; // Termina la recepción
                }

                // Muestra la noticia si pertenece a las categorías seleccionadas
                char categoriaNoticia = noticia[0];
                if (strchr(categorias, categoriaNoticia)) {
                    printf("Noticia recibida: %s\n", noticia);
                }

                noticia = strtok(NULL, "\n"); // Procesa la siguiente noticia
            }
        } else if (bytesRead == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Error al leer del pipe de noticias");
            break;
        }

        usleep(500000); // Pausa de medio segundo entre lecturas
    }

    close(fd); // Cierra el pipe al salir del bucle
}

// Función para enviar suscripción con categorías y nombre del pipe exclusivo
void enviarSuscripcion(const char *pipeSSC, const char *categorias, const char *pipeName) {
    // Verifica si el pipe de suscripción existe o lo crea
    if (access(pipeSSC, F_OK) == -1) {
        if (mkfifo(pipeSSC, 0666) == -1) {
            perror("Error al crear pipe de suscripción");
            return;
        }
    }

    // Abre el pipe de suscripción en modo de solo escritura
    int fd = open(pipeSSC, O_WRONLY);
    if (fd == -1) {
        perror("Error al abrir pipe para enviar suscripción");
        exit(1);
    }

    char mensaje[256];
    // Prepara el mensaje con las categorías y el nombre del pipe
    snprintf(mensaje, sizeof(mensaje), "%s:%s", categorias, pipeName);
    write(fd, mensaje, strlen(mensaje)); // Envía el mensaje
    close(fd); // Cierra el descriptor
}

int main(int argc, char *argv[]) {
    const char *pipeSSC = NULL;  // Nombre del pipe de suscripción
    char pipeName[20];          // Nombre único para el pipe exclusivo
    char categorias[6] = {0};   // Categorías de interés

    // Procesa los argumentos de línea de comandos
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            pipeSSC = argv[++i];
        }
    }

    if (pipeSSC) {
        // Genera un nombre único para el pipe exclusivo basado en el PID
        snprintf(pipeName, sizeof(pipeName), "pipeNoticias_%d", getpid());

        // Crea el pipe exclusivo para noticias
        if (mkfifo(pipeName, 0666) == -1) {
            perror("Error al crear pipe exclusivo para noticias");
            exit(1);
        }

        // Solicita las categorías al usuario
        printf("Ingrese las categorías de suscripción (A, E, C, P, S): ");
        fgets(categorias, sizeof(categorias), stdin);
        categorias[strcspn(categorias, "\n")] = '\0'; // Remueve el salto de línea

        // Envia la suscripción al sistema
        enviarSuscripcion(pipeSSC, categorias, pipeName);

        // Recibe las noticias en el pipe exclusivo
        recibirNoticias(pipeName, categorias);
    } else {
        // Mensaje de uso si no se proporciona el pipe de suscripción
        fprintf(stderr, "Uso: suscriptor -s <pipeSSC>\n");
    }

    return 0;
}
