#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s \"command\" timeout\n", argv[0]);
        return 1;
    }

    /* 1. Create a TCP socket. */
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    /* 2. Connect to the address and port used by your server. */
    struct sockaddr_in server = {
        .sin_family = AF_INET,
        .sin_port = htons(5000),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };

    if (connect(sock, (struct sockaddr *)&server, sizeof server) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    /* 3. Send one request. The server waits for the newline. */
    if (dprintf(sock, "SUBMIT %s 0 %s\n", argv[2], argv[1]) < 0) {
        perror("send request");
        close(sock);
        return 1;
    }

    /* 4. Read one complete reply and print it. */
    FILE *input = fdopen(sock, "r");
    if (input == NULL) {
        perror("fdopen");
        close(sock);
        return 1;
    }

    char reply[256];
    if (fgets(reply, sizeof reply, input) == NULL ||
        strchr(reply, '\n') == NULL) {
        fprintf(stderr, "Server closed without a complete reply\n");
        fclose(input);
        return 1;
    }

    fputs(reply, stdout);
    fclose(input);  /* Also closes sock. */
    return 0;
}