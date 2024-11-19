#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

// Función para enviar noticias desde un archivo a través de un pipe
void enviarNoticias(const char *pipePSC, const char *fileName, int intervalo) {
    // Verificar si el pipe existe; si no, crearlo con permisos 0666
    if (access(pipePSC, F_OK) == -1) {
        if (mkfifo(pipePSC, 0666) == -1) {
            perror("Error al crear el pipe");
            exit(1);
        }
    }

    // Abrir el pipe en modo de solo escritura
    int pipeFd = open(pipePSC, O_WRONLY);
    if (pipeFd == -1) {
        perror("Error al abrir el pipe");
        exit(1);
    }

    // Abrir el archivo de noticias en modo de solo lectura
    FILE *file = fopen(fileName, "r");
    if (!file) {
        perror("Error al abrir el archivo de noticias");
        close(pipeFd);
        exit(1);
    }

    char buffer[100]; // Buffer para almacenar cada línea de noticias
    // Leer y enviar línea por línea desde el archivo a través del pipe
    while (fgets(buffer, sizeof(buffer), file)) {
        write(pipeFd, buffer, strlen(buffer)); // Enviar la línea a través del pipe
        printf("Noticia enviada: %s", buffer); // Mostrar la noticia enviada
        sleep(intervalo); // Esperar el tiempo especificado entre envíos
    }

    // Cerrar el pipe y el archivo al finalizar
    fclose(file);
    close(pipeFd);
}

// Función principal
int main(int argc, char *argv[]) {
    const char *pipePSC = NULL; // Nombre del pipe para comunicación con el sistema
    const char *fileName = NULL; // Nombre del archivo con noticias
    int intervalo = 1;          // Tiempo de espera entre envíos (por defecto: 1 segundo)

    // Procesar los argumentos de la línea de comandos
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) {
            pipePSC = argv[++i];
        } else if (strcmp(argv[i], "-f") == 0) {
            fileName = argv[++i];
        } else if (strcmp(argv[i], "-t") == 0) {
            intervalo = atoi(argv[++i]);
        }
    }

    // Verificar que se proporcionaron los argumentos obligatorios
    if (pipePSC && fileName) {
        enviarNoticias(pipePSC, fileName, intervalo); // Iniciar el envío de noticias
    } else {
        // Mostrar un mensaje de uso si faltan argumentos
        fprintf(stderr, "Uso: %s -p <pipePSC> -f <archivo> -t <intervalo>\n", argv[0]);
    }

    return 0;
}
