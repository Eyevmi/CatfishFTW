#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

int receive_line(int client, char *line, size_t capacity)
{
    size_t used = 0;
    for (;;) {
        char ch;
        ssize_t count = recv(client, &ch, 1, 0);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return (int)count;  /* EOF also discards an unfinished line. */

        if (ch == '\n' && used < capacity) {
            line[used] = '\0';
            return 1;
        }
        if (ch == '\0' || used + 1 >= capacity) {
            errno = ch == '\0' ? EINVAL : EMSGSIZE;
            return -1;
        }
        line[used++] = ch;
    }
}

int send_text(int client, const char *text)
{
    size_t left = strlen(text);
    while (left > 0) {
        ssize_t count = send(client, text, left, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return -1;
        text += count;
        left -= (size_t)count;
    }
    return 0;
}