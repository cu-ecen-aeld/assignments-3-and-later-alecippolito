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

// Define constants
#define PORT 9000
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 512
#define PID_FILE "/var/run/aesdsocket.pid"

int client_sockfd = -1;
int sockfd = -1;


void handle_signal(int signal) {
if (signal == SIGINT || signal == SIGTERM) {
        syslog(LOG_INFO, "Caught signal %d, exiting", signal);
        if (client_sockfd >= 0) {
            close(client_sockfd);
        }
        if (sockfd >= 0) {
            close(sockfd);
        }
        if (remove(FILE_PATH) != 0) {
            syslog(LOG_ERR, "Failed to delete the file %s: %s", FILE_PATH, strerror(errno));
        }
        closelog();
        syslog(LOG_INFO, "Sockets terminated");
        exit(0);
    }
}

void setup_signals(){
	struct sigaction sa;
	sa.sa_handler = handle_signal;
	sa.sa_flags = SA_RESTART | SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGINT, &sa, NULL) < 0 || sigaction(SIGTERM, &sa, NULL) < 0) {
		syslog(LOG_ERR, "Error setting up signal handlers: %s", strerror(errno));
		exit(1);
	}
}


void daemonize() {
    pid_t pid = fork();
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

    if (chdir("/") < 0) {
        syslog(LOG_ERR, "Changing directory failed: %s", strerror(errno));
        exit(1);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDWR);
    dup(0);
    dup(0);


}

int main(int argc, char *argv[]) {
	// Init Variables 
	int daemon_mode = 0;
	char client_ip[INET_ADDRSTRLEN];
	struct sockaddr_in server_addr, client_addr;
	socklen_t client_len = sizeof(client_addr);

	
	// Open System Log
	openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
	syslog(LOG_INFO, "Starting aesdsocket.c -- EXECUTION STARTING");

	// Attach Signl Handlerss
	setup_signals();

	// Create Socket
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
	syslog(LOG_ERR, "Error creating socket: %s", strerror(errno));
	closelog();
	return -1;
	}
	syslog(LOG_INFO, "Successfully created socket with id: %d", sockfd);

	// Bind Socket to port
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(PORT);
	if (bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
		syslog(LOG_ERR, "Error binding socket: %s", strerror(errno));
		close(sockfd);
		closelog();
		return -1;
	} else {
		syslog(LOG_INFO, "Successfully bound socket to server");
	}


	
	// Deamon Mode
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


	// Accept
	while (true) {
		client_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
		if (client_sockfd < 0) {
			syslog(LOG_ERR, "Failed accept to connect: %s", strerror(errno));
			break;
		} else {
			inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
			syslog(LOG_INFO, "Accepted connection from %s", client_ip);
		}

		// Open file for writing
		int file_fd = open(FILE_PATH, O_RDWR | O_CREAT | O_APPEND, 0644);
		if (file_fd < 0) {
			syslog(LOG_ERR, "Failed to open file: %s", strerror(errno));
			close(client_sockfd);
			continue;
		} else {
			syslog(LOG_INFO, "Successfully opened file: %s", FILE_PATH);
		}	
		
		char buffer[BUFFER_SIZE];
		ssize_t bytes_read;
		while ((bytes_read = recv(client_sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
			if (write(file_fd, buffer, bytes_read) != bytes_read) {
				syslog(LOG_ERR, "Failed to write received data to file: %s", strerror(errno));
				break;
			} else {
				syslog(LOG_INFO, "Read in %ld bytes: %s", bytes_read, buffer);
			}
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
			continue;
		}

		while ((bytes_read = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
			if (send(client_sockfd, buffer, bytes_read, 0) < 0) {
				syslog(LOG_ERR, "Failed to send data to client: %s", strerror(errno));
				break;
			}
		}
		
		close(file_fd);
		close(client_sockfd);
		client_sockfd = -1;

		syslog(LOG_INFO, "Closed connection from %s", client_ip);
	}
        if (client_sockfd >= 0) {
    		close(client_sockfd);
        }
        if (sockfd >= 0) {
		close(sockfd);
        }
        if (remove(FILE_PATH) != 0) {
        	syslog(LOG_ERR, "Failed to delete the file %s: %s", FILE_PATH, strerror(errno));
        }
	syslog(LOG_INFO, "Successfully ran aesdsocket.c -- EXECUTION OVER");
	return 0;
}

