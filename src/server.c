#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
//first server prototype
int main(void){
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        perror("socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    struct sockaddr_in address = {
        .sin_family = AF_INET,
        .sin_port = htons(5000),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };

    if (bind(listener, (struct sockaddr*)&address, sizeof address) < 0) {
        perror("bind");
        close(listener);
        return 1;
    }

    if (listen(listener, 1) < 0) {
        perror("listen");
        close(listener);
        return 1;
    }

    puts("Server listening on 127.0.0.1 port 5000");

    for(;;){
        int client = accept(listener, NULL, NULL);
        if (client < 0) {
            perror("accept");
            break;
        }
        char buffer[4096];
        if (fgetc(line, sizeof(line), input) && strchr(line, '\n')){
            printf("Received: %s", line);
            if(send(client, line, strlen(line), 0) < 0){
                perror("send");
            }
        }

        fclose(input);
    }

    close(listener);
    return 0;
}