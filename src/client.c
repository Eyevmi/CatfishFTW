#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "net.h"

int receive_line(int client, char *line, size_t capacity);
int send_text(int client, const char *text);

/* Convert text such as "5000" to a number and check its range. */
static int parse_number(const char *text, int min, int max, int *result)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value < min || value > max)
        return 0;

    *result = (int)value;
    return 1;
}

int main(int argc, char *argv[])
{
    if ((argc != 6 && argc != 7) || strcmp(argv[3], "submit") != 0) {
        fprintf(stderr,
                "Usage: %s HOST PORT submit \"COMMAND\" TIMEOUT [PRIORITY]\n",
                argv[0]);
        return 1;
    }

    const char *host = argv[1];
    const char *command = argv[4];
    int port;
    int timeout;
    int priority = 0;

    if (!parse_number(argv[2], 1, 65535, &port)) {
        fprintf(stderr, "Port must be between 1 and 65535\n");
        return 1;
    }

    if (!parse_number(argv[5], 1, INT_MAX, &timeout)) {
        fprintf(stderr, "Timeout must be a positive whole number\n");
        return 1;
    }

    if (argc == 7 && !parse_number(argv[6], 0, 9, &priority)) {
        fprintf(stderr, "Priority must be between 0 and 9\n");
        return 1;
    }

    if (command[0] == '\0' || strpbrk(command, "\r\n") != NULL) {
        fprintf(stderr, "Command must be one nonempty line\n");
        return 1;
    }

    /* Build exactly the line that server.c expects to receive. */
    char request[4096];
    int length = snprintf(request, sizeof request, "SUBMIT %d %d %s\n",
                          timeout, priority, command);

    if (length < 0 || (size_t)length >= sizeof request) {
        fprintf(stderr, "Request is too long\n");
        return 1;
    }

    int sock = client_connect(host, port);

    if (send_text(sock, request) < 0) {
        perror("send_text");
        close(sock);
        return 1;
    }

    char reply[256];
    int received = receive_line(sock, reply, sizeof reply);
    close(sock);

    if (received != 1) {
        fprintf(stderr, "Did not receive a complete reply\n");
        return 1;
    }

    puts(reply);
    return 0;
}