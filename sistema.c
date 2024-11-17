#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>

#define MAX_NEWS_LENGTH 80
#define MAX_TOPICS 5
#define PIPE_NAME_LENGTH 50

typedef struct News {
    char topic;
    char content[MAX_NEWS_LENGTH + 1];
    struct News *next;
} News;

typedef struct Subscriber {
    int fd;
    char pipe_name[PIPE_NAME_LENGTH];
    char topics[MAX_TOPICS + 1];
    struct Subscriber *next;
} Subscriber;

News *news_head = NULL;
Subscriber *subscribers_head = NULL;
pthread_mutex_t news_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t subs_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t timer_mutex = PTHREAD_MUTEX_INITIALIZER;

int running = 1;
time_t last_publication_time;

pthread_t timer_thread, news_processor_thread;

// Función para limpiar el sistema al cerrar
void cleanup() {
    pthread_mutex_lock(&news_mutex);
    while (news_head) {
        News *temp = news_head;
        news_head = news_head->next;
        free(temp);
    }
    pthread_mutex_unlock(&news_mutex);

    pthread_mutex_lock(&subs_mutex);
    while (subscribers_head) {
        Subscriber *temp = subscribers_head;
        subscribers_head = subscribers_head->next;
        close(temp->fd);
        unlink(temp->pipe_name);
        free(temp);
    }
    pthread_mutex_unlock(&subs_mutex);

    unlink("pipePSC");
    unlink("pipeSSC");
    printf("[Sistema]: Recursos liberados y pipes eliminados.\n");
}

void handle_sigint(int sig) {
    (void)sig;
    printf("\n[Sistema]: Señal de interrupción recibida. Finalizando...\n");
    running = 0;

    pthread_cancel(timer_thread);
    pthread_cancel(news_processor_thread);

    unlink("pipePSC");
    unlink("pipeSSC");
}

void enqueue_news(char topic, const char *content) {
    News *new_news = malloc(sizeof(News));
    if (!new_news) {
        perror("[Sistema]: Error al asignar memoria para noticia");
        return;
    }
    new_news->topic = topic;
    strncpy(new_news->content, content, MAX_NEWS_LENGTH);
    new_news->next = NULL;

    pthread_mutex_lock(&news_mutex);
    if (!news_head) {
        news_head = new_news;
    } else {
        News *current = news_head;
        while (current->next) current = current->next;
        current->next = new_news;
    }
    pthread_mutex_unlock(&news_mutex);

    pthread_mutex_lock(&timer_mutex);
    last_publication_time = time(NULL);  // Reinicia el temporizador
    pthread_mutex_unlock(&timer_mutex);

    printf("[Sistema]: Noticia almacenada: Tópico %c, Contenido '%s'\n", topic, content);
}

News *dequeue_news() {
    pthread_mutex_lock(&news_mutex);
    if (!news_head) {
        pthread_mutex_unlock(&news_mutex);
        return NULL;
    }
    News *news = news_head;
    news_head = news_head->next;
    pthread_mutex_unlock(&news_mutex);
    return news;
}

void add_subscriber(int fd, const char *topics) {
    Subscriber *new_sub = malloc(sizeof(Subscriber));
    if (!new_sub) {
        perror("[Sistema]: Error al asignar memoria para suscriptor");
        return;
    }

    snprintf(new_sub->pipe_name, PIPE_NAME_LENGTH, "pipe_sub_%d", fd);
    if (mkfifo(new_sub->pipe_name, 0666) == -1) {
        perror("[Sistema]: Error creando pipe único para suscriptor");
        free(new_sub);
        return;
    }

    if (write(fd, new_sub->pipe_name, strlen(new_sub->pipe_name) + 1) == -1) {
        perror("[Sistema]: Error enviando nombre de pipe único al suscriptor");
        unlink(new_sub->pipe_name);
        free(new_sub);
        return;
    }

    new_sub->fd = open(new_sub->pipe_name, O_WRONLY);
    if (new_sub->fd == -1) {
        perror("[Sistema]: Error al abrir el pipe único para el suscriptor");
        unlink(new_sub->pipe_name);
        free(new_sub);
        return;
    }

    strncpy(new_sub->topics, topics, MAX_TOPICS);
    new_sub->next = NULL;

    pthread_mutex_lock(&subs_mutex);
    if (!subscribers_head) {
        subscribers_head = new_sub;
    } else {
        Subscriber *current = subscribers_head;
        while (current->next) current = current->next;
        current->next = new_sub;
    }
    pthread_mutex_unlock(&subs_mutex);

    printf("[Sistema]: Nuevo suscriptor añadido. Pipe único: %s, Tópicos: %s\n", new_sub->pipe_name, topics);
}

void *publisher_handler(void *arg) {
    int fd = *((int *)arg);
    free(arg);

    char buffer[MAX_NEWS_LENGTH + 3];
    while (running) {
        int bytesRead = read(fd, buffer, sizeof(buffer));
        if (bytesRead > 0) {
            char topic = buffer[0];
            if (strchr("AECPS", topic)) {
                enqueue_news(topic, buffer + 2);
            } else {
                fprintf(stderr, "[Sistema]: Tópico inválido recibido: %c\n", topic);
            }
        } else if (bytesRead <= 0) {
            break;
        }
    }

    close(fd);
    return NULL;
}

void *listener_for_publishers(void *arg) {
    char *pipePSC = (char *)arg;
    while (running) {
        int fd = open(pipePSC, O_RDONLY);
        if (fd == -1) {
            perror("[Sistema]: Error abriendo pipePSC para lectura");
            break;
        }

        int *new_fd = malloc(sizeof(int));
        *new_fd = fd;

        pthread_t publisher_thread;
        pthread_create(&publisher_thread, NULL, publisher_handler, new_fd);
        pthread_detach(publisher_thread);
    }
    return NULL;
}

void process_news() {
    while (running) {
        News *news = dequeue_news();
        if (!news) {
            sleep(1);
            continue;
        }

        pthread_mutex_lock(&subs_mutex);
        Subscriber *current = subscribers_head;
        while (current) {
            if (strchr(current->topics, news->topic)) {
                printf("[Sistema]: Enviando noticia '%s' al suscriptor por pipe %s\n", news->content, current->pipe_name);
                if (write(current->fd, news->content, strlen(news->content) + 1) == -1) {
                    perror("[Sistema]: Error enviando noticia al suscriptor");
                } else {
                    printf("[Sistema]: Noticia enviada correctamente al suscriptor por pipe %s\n", current->pipe_name);
                }
            }
            current = current->next;
        }
        pthread_mutex_unlock(&subs_mutex);

        free(news);
    }
}

void *timer_handler(void *arg) {
    int *timeF = (int *)arg;
    while (running) {
        sleep(1);

        pthread_mutex_lock(&timer_mutex);
        if (difftime(time(NULL), last_publication_time) >= *timeF) {
            printf("[Sistema]: Tiempo finalizado sin nuevas publicaciones. Cerrando sistema...\n");
            pthread_mutex_unlock(&timer_mutex);
            raise(SIGINT);
            break;
        }
        pthread_mutex_unlock(&timer_mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    char *pipePSC = NULL, *pipeSSC = NULL;
    int timeF = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0) pipePSC = argv[++i];
        else if (strcmp(argv[i], "-s") == 0) pipeSSC = argv[++i];
        else if (strcmp(argv[i], "-t") == 0) timeF = atoi(argv[++i]);
    }

    if (!pipePSC || !pipeSSC || timeF <= 0) {
        fprintf(stderr, "Uso: %s -p pipePSC -s pipeSSC -t timeF\n", argv[0]);
        return EXIT_FAILURE;
    }

    unlink(pipePSC);
    unlink(pipeSSC);
    if (mkfifo(pipePSC, 0666) == -1 || mkfifo(pipeSSC, 0666) == -1) {
        perror("[Sistema]: Error creando pipes");
        exit(EXIT_FAILURE);
    }
    printf("[Sistema]: Pipes %s y %s recreados exitosamente.\n", pipePSC, pipeSSC);

    signal(SIGINT, handle_sigint);
    last_publication_time = time(NULL);

    pthread_t listener_thread;
    pthread_create(&listener_thread, NULL, listener_for_publishers, pipePSC);

    pthread_create(&timer_thread, NULL, timer_handler, &timeF);
    pthread_create(&news_processor_thread, NULL, (void *)process_news, NULL);

    pthread_join(listener_thread, NULL);
    pthread_join(timer_thread, NULL);
    pthread_join(news_processor_thread, NULL);

    cleanup();
    printf("[Sistema]: Finalizado correctamente.\n");
    return 0;
}
