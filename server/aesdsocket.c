#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>         // for open() and O_* flags
#include <netinet/in.h>
#include <netinet/tcp.h>  // for TCP_NODELAY
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

#define PORT 9000
#define BACKLOG 1
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

static int listen_fd = -1;
static int conn_fd = -1;
static volatile sig_atomic_t exit_flag = 0;

static void signal_handler(int signum)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    exit_flag = 1;
    if (listen_fd != -1) close(listen_fd);
    if (conn_fd != -1) close(conn_fd);
    unlink(DATA_FILE);
}

static void daemonize(void)
{
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Failed to fork for daemon mode: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        /* parent exits after creating listener */
        exit(EXIT_SUCCESS);
    }
    /* child continues */
    if (setsid() < 0) {
        syslog(LOG_ERR, "Failed to setsid for daemon mode: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    if (chdir("/") < 0) {
        syslog(LOG_ERR, "Failed to chdir to / for daemon mode: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    umask(0);
    /* Redirect stdio to /dev/null */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);
}

int main(int argc, char *argv[])
{
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char addr_str[INET_ADDRSTRLEN];
    int opt_val = 1;
    int daemon_mode = 0;
    int opt;

    while ((opt = getopt(argc, argv, "d")) != -1) {
        if (opt == 'd') daemon_mode = 1;
        else {
            fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    openlog(NULL, LOG_PID|LOG_PERROR, LOG_USER);

    /* create, reuse, bind socket */
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        syslog(LOG_ERR, "socket() failed: %s", strerror(errno));
        return -1;
    }
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val)) < 0) {
        syslog(LOG_ERR, "setsockopt() failed: %s", strerror(errno));
        return -1;
    }
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_ERR, "bind() failed: %s", strerror(errno));
        return -1;
    }

    /* start listening before daemonizing */
    if (listen(listen_fd, BACKLOG) < 0) {
        syslog(LOG_ERR, "listen() failed: %s", strerror(errno));
        return -1;
    }

    /* fork after bind and listen if daemon mode */
    if (daemon_mode) daemonize();

    /* install signal handlers */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* accept loop */
    while (!exit_flag) {
        conn_fd = accept(listen_fd,
                         (struct sockaddr *)&client_addr,
                         &client_addr_len);
        if (conn_fd < 0) {
            if (errno == EINTR && exit_flag) break;
            syslog(LOG_ERR, "accept() failed: %s", strerror(errno));
            continue;
        }

        // disable Nagle to minimize delays under high-latency/Valgrind environments
        int flag = 1;
        if (setsockopt(conn_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
            syslog(LOG_ERR, "Failed to disable Nagle (TCP_NODELAY): %s", strerror(errno));
        }

        inet_ntop(AF_INET, &client_addr.sin_addr,
                  addr_str, sizeof(addr_str));
        syslog(LOG_INFO, "Accepted connection from %s", addr_str);

        char buf[BUFFER_SIZE];
        char *packet = NULL;
        size_t pkt_len = 0;
        ssize_t rlen;
        while (!exit_flag &&
               (rlen = recv(conn_fd, buf,
                            sizeof(buf), 0)) > 0) {
            syslog(LOG_DEBUG, "receiving %zd bytes", rlen);
            for (ssize_t i = 0; i < rlen; i++) {
                char byte = buf[i];
                char *tmp = realloc(packet, pkt_len + 1);
                if (!tmp) {
                    syslog(LOG_ERR, "realloc() failed");
                    free(packet);
                    packet = NULL;
                    pkt_len = 0;
                    continue;
                }
                packet = tmp;
                packet[pkt_len++] = byte;
                if (byte == '\n') {
                	syslog(LOG_DEBUG, "\\n received, writing %zd bytes (\"%.10s\") to '%s'.", 
                		pkt_len, packet, DATA_FILE);
                    /* append packet to file */
                    FILE *f = fopen(DATA_FILE, "a");
                    if (f) {
                        fwrite(packet, 1, pkt_len, f);
                        fclose(f);
                    } else {
                        syslog(LOG_ERR, "fopen append failed: %s", strerror(errno));
                    }
                    /* send full file back */
                    FILE *r = fopen(DATA_FILE, "r");
                    if (r) {
                        char sendb[BUFFER_SIZE];
                        size_t n;
                        while ((n = fread(sendb, 1, sizeof(sendb), r)) > 0) {
                            send(conn_fd, sendb, n, 0);
                        }
                    } else {
                        syslog(LOG_ERR, "fopen read failed: %s", strerror(errno));
                    }
                    fclose(r);
                    // after sending full file, close write side so client sees EOF immediately
                    shutdown(conn_fd, SHUT_WR);
                    free(packet);
                    packet = NULL;
                    pkt_len = 0;
                }
            }
        }
        free(packet);
        syslog(LOG_INFO, "Closed connection from %s", addr_str);
        close(conn_fd);
    }

    if (listen_fd != -1) close(listen_fd);
    unlink(DATA_FILE);
    closelog();
    return 0;
}

