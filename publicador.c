#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#define MAX_NEWS_LENGTH 80

// Validar formato de las noticias
int validate_news_format(const char *news) {
    if (strlen(news) < 3 || strlen(news) > MAX_NEWS_LENGTH) return 0; // Longitud válida
    if (!isupper(news[0]) || news[1] != ':') return 0;                // Categoría válida
    if (!strchr("AECPS", news[0])) return 0;                          // Tópico permitido
    return 1;
}

// Publicar noticias desde el archivo al sistema de comunicación
void publish_news(const char *pipePSC, const char *file, int timeN) {
    // Abrir el pipePSC para escritura
    int fd = open(pipePSC, O_WRONLY);
    if (fd == -1) {
        perror("[Publicador]: Error al abrir el pipePSC");
        exit(EXIT_FAILURE);
    }

    // Abrir el archivo de noticias
    FILE *fp = fopen(file, "r");
    if (!fp) {
        perror("[Publicador]: Error al abrir el archivo de noticias");
        close(fd);
        exit(EXIT_FAILURE);
    }

    char news[MAX_NEWS_LENGTH + 1];
    while (fgets(news, sizeof(news), fp)) {
        // Eliminar salto de línea
        news[strcspn(news, "\n")] = '\0';

        // Validar formato de la noticia
        if (validate_news_format(news)) {
            // Enviar la noticia al pipe
            if (write(fd, news, strlen(news) + 1) == -1) {
                perror("[Publicador]: Error al enviar la noticia");
            } else {
                printf("[Publicador]: Noticia enviada: %s\n", news);
            }
            sleep(timeN); // Esperar `timeN` segundos antes de enviar la siguiente
        } else {
            fprintf(stderr, "[Publicador]: Formato inválido: %s\n", news);
        }
    }

    fclose(fp); // Cerrar el archivo
    close(fd);  // Cerrar el pipe
    printf("[Publicador]: Finalizado.\n");
}

// Función principal
int main(int argc, char *argv[]) {
    char *pipePSC = NULL;
    char *file = NULL;
    int timeN = 0;

    // Parsear argumentos
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            pipePSC = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0) {
            file = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0) {
            timeN = atoi(argv[++i]);
        }
    }

    // Validar argumentos
    if (!pipePSC || !file || timeN <= 0) {
        fprintf(stderr, "Uso: %s -p pipePSC -f file -t timeN\n", argv[0]);
        return EXIT_FAILURE;
    }

    publish_news(pipePSC, file, timeN); // Llamar a la función para publicar noticias
    return 0;
}
