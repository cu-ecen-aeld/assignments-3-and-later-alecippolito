// Socket Server program
// Author: Alec Ippoltio

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <signal.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>
#include <pthread.h>
// #include <time.h>
#include <sys/queue.h>

// Define constants
#define PORT 9000
#define FILE_PATH "/dev/aesdchar"
#define BUFFER_SIZE 1024
#define PID_FILE "/var/run/aesdsocket.pid"

int sockfd = -1;

pthread_mutex_t file_mutex;
//timer_t timer_id;
struct thread_node {
    pthread_t thread_id;
    SLIST_ENTRY(thread_node) nodes;
};
SLIST_HEAD(slisthead, thread_node) head;

// Signal Handler
void handle_signal(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        syslog(LOG_INFO, "Caught signal %d, exiting", signal);

        // Cancel and join all threads
        struct thread_node *node;
        SLIST_FOREACH(node, &head, nodes) {
            pthread_cancel(node->thread_id);
        }
        SLIST_FOREACH(node, &head, nodes) {
            pthread_join(node->thread_id, NULL);
        }

        // Clean up linked list
        while (!SLIST_EMPTY(&head)) {
            node = SLIST_FIRST(&head);
            SLIST_REMOVE_HEAD(&head, nodes);
            free(node);
        }

        if (sockfd >= 0) {
            close(sockfd);
        }
        if (remove(FILE_PATH) != 0) {
            syslog(LOG_ERR, "Failed to delete the file %s: %s", FILE_PATH, strerror(errno));
        }
        pthread_mutex_destroy(&file_mutex); // Destroy the mutex
        closelog();
        syslog(LOG_INFO, "Sockets terminated");
        exit(0);
    }
}

// Setting up Signal Handlers
void setup_signals() {
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0) {
        syslog(LOG_ERR, "Error setting up signal handlers: %s", strerror(errno));
        exit(1);
    }
}

// Daemonize Process
void daemonize() {
    pid_t pid = fork();
    setup_signals();
    syslog(LOG_INFO, "Attemtping Daemon");
    if (pid < 0) {
        syslog(LOG_ERR, "Failed to fork: %s", strerror(errno));
        exit(1);
    } else if (pid > 0) {
        exit(0);
    }

    if (setsid() < 0) {
        syslog(LOG_ERR, "Failed to create new session id: %s", strerror(errno));
        exit(1);
    }

    pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Failed to fork: %s", strerror(errno));
        exit(1);
    } else if (pid > 0) {
        exit(0);
    }
}

/*
void timer_handler(union sigval sv) {
    (void)sv; // Mark sv as intentionally unused
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[100];
    strftime(timestamp, sizeof(timestamp), "timestamp:%Y-%m-%d %H:%M:%S\n", tm_info);

    pthread_mutex_lock(&file_mutex);
    int file_fd = open(FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (file_fd >= 0) {
        if(write(file_fd, timestamp, strlen(timestamp)) == -1){
            syslog(LOG_ERR, "Error writing timer to file: %s", strerror(errno));
        }
        close(file_fd);
    }
    pthread_mutex_unlock(&file_mutex);
}

void setup_timer() {
    struct sigevent sev;
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = timer_handler;
    sev.sigev_notify_attributes = NULL;
    if (timer_create(CLOCK_REALTIME, &sev, &timer_id) == -1) {
        syslog(LOG_ERR, "Failed to create timer: %s", strerror(errno));
        exit(1);
    }
    struct itimerspec its;
    its.it_value.tv_sec = 10;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 10;
    its.it_interval.tv_nsec = 0;
    if (timer_settime(timer_id, 0, &its, NULL) == -1) {
        syslog(LOG_ERR, "Failed to set timer: %s", strerror(errno));
        exit(1);
    }
}
*/
void *handle_connection(void *arg) {
    int client_sockfd = *(int *)arg;
    free(arg); // Free the allocated memory
    char client_ip[INET_ADDRSTRLEN];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    getpeername(client_sockfd, (struct sockaddr *)&client_addr, &client_len);
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    syslog(LOG_INFO, "Accepted connection from %s", client_ip);

    // Open file for writing
    int file_fd = open(FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (file_fd < 0) {
        syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
        close(client_sockfd);
        return NULL;
    } else {
        syslog(LOG_INFO, "Successfully opened file: %s", FILE_PATH);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = recv(client_sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        pthread_mutex_lock(&file_mutex); // Lock mutex
        if (write(file_fd, buffer, bytes_read) != bytes_read) {
            syslog(LOG_ERR, "Failed to write received data to file: %s", strerror(errno));
            pthread_mutex_unlock(&file_mutex); // Unlock mutex
            break;
        } else {
            syslog(LOG_INFO, "Read in %ld bytes: %s", bytes_read, buffer);
        }
        pthread_mutex_unlock(&file_mutex); // Unlock mutex
        if (buffer[bytes_read - 1] == '\n') {
            break;
        }
    }
    fsync(file_fd);
    close(file_fd);

    if (bytes_read < 0) {
        syslog(LOG_ERR, "Failed to receive data: %s", strerror(errno));
    }

    file_fd = open(FILE_PATH, O_RDONLY);
    if (file_fd < 0) {
        syslog(LOG_ERR, "Failed to open file for reading: %s", strerror(errno));
        close(client_sockfd);
        return NULL;
    }

    while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
        if (send(client_sockfd, buffer, bytes_read, 0) < 0) {
            syslog(LOG_ERR, "Failed to send data to client: %s", strerror(errno));
            break;
        }
    }
    close(file_fd);
    close(client_sockfd);

    syslog(LOG_INFO, "Closed connection from %s", client_ip);
    return NULL;
}

int main(int argc, char *argv[]) {
    // Init Variables
    int daemon_mode = 0;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Open System Log
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Starting aesdsocket.c -- EXECUTION STARTING");

    // Attach Signal Handlers
    setup_signals();

    // Create Socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        syslog(LOG_ERR, "Error creating socket: %s", strerror(errno));
        closelog();
        return -1;
    }
    int optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    syslog(LOG_INFO, "Successfully created socket with id: %d", sockfd);

    // Bind Socket to port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "Error binding socket: %s", strerror(errno));
        close(sockfd);
        closelog();
        return -1;
    } else {
        syslog(LOG_INFO, "Successfully bound socket to server");
    }

    // Daemon Mode
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            daemon_mode = 1;
        }
    }
    if (daemon_mode) {
        daemonize();
        syslog(LOG_INFO, "Daemon mode activated");
    } else {
        syslog(LOG_INFO, "Running in normal mode");
    }

    // Listen
    if (listen(sockfd, 10) < 0) {
        syslog(LOG_ERR, "Error listening on socket: %s", strerror(errno));
        close(sockfd);
        closelog();
        return -1;
    }
    syslog(LOG_INFO, "Listening on port %d", PORT);

    pthread_mutex_init(&file_mutex, NULL);
    SLIST_INIT(&head);
    // setup_timer();

    // Accept
    while (true) {
        int client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sockfd < 0) {
            syslog(LOG_ERR, "Failed accept to connect: %s", strerror(errno));
            break;
        }

        // Create thread
        pthread_t thread_id;
        int *client_sockfd_ptr = malloc(sizeof(int));
        *client_sockfd_ptr = client_sockfd;
        if (pthread_create(&thread_id, NULL, handle_connection, client_sockfd_ptr) != 0) {
            syslog(LOG_ERR, "Failed to create thread: %s", strerror(errno));
            close(client_sockfd);
            free(client_sockfd_ptr);
            continue;
        }

        // Add thread to linked list
        struct thread_node *node = malloc(sizeof(struct thread_node));
        node->thread_id = thread_id;
        SLIST_INSERT_HEAD(&head, node, nodes);
    }

    if (remove(FILE_PATH) != 0) {
        syslog(LOG_ERR, "Failed to delete the file %s: %s", FILE_PATH, strerror(errno));
    }
    syslog(LOG_INFO, "Successfully ran aesdsocket.c -- EXECUTION OVER");
    return 0;
}
