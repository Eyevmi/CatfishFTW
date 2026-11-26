#include <stddef.h>
#include <stdio.h>
#include <unistd.h>
#include "net.h"

int receive_line(int client, char *line, size_t capacity);
int send_text(int client, const char *text);

static void handle_client(int client)
{
    char request[4096];
    int result = receive_line(client, request, sizeof request);

    if (result < 0) {
        perror("receive_line");
        return;
    }

    if (result == 0)
        return;

    printf("Received: %s\n", request);

    if (send_text(client, "OK\n") < 0)
        perror("send_text");
}

int main(void)
{
    int listener = server_listen(5000);
    puts("Server listening on port 5000");

    for (;;) {
        int client = server_accept(listener);
        handle_client(client);
        close(client);
    }
}