# LAN Torrent

Our SOC 3010 project is a C job runner for a local network. The goal is for a client to submit a download, a server to schedule it, and a worker to fetch file pieces from peers and verify them with SHA-256.

## Current progress • November 27, 2026

The project client accepts `./client HOST PORT submit "COMMAND" TIMEOUT [PRIORITY]`. It checks the port (1–65535), timeout (positive whole seconds), and priority (0–9). Leaving out priority selects 0. The client sends one line in the form `SUBMIT timeout priority command` and prints the server’s reply.

The server listens on port `5000`, handles one client at a time, and reads one newline-terminated request per connection. It prints the request in the server terminal, sends `OK` to the client, and closes the connection. If the client disconnects halfway through a line, the server discards it.

`server.c` handles connections using the course’s `net.h`. The two helpers in `net.c`, `receive_line()` and `send_text()`, handle reading the request and writing the reply. At this stage, `OK` confirms receipt of a complete line. Job creation comes in the next milestone.

## Try the prototype on Ubuntu

From the directory containing `server.c` and `net.c`, build and start the server:

```sh
gcc -Wall -Wextra server.c net.c -o server
./server
```

In a second terminal, use the project’s submit-enabled `client.c` alongside `net.c` and `net.h`. Build the client and try the same command with two priorities.

```sh
gcc -Wall -Wextra client.c net.c -o client
./client 127.0.0.1 5000 submit "echo hello" 10
./client 127.0.0.1 5000 submit "echo hello" 10 7
```

Both calls should print `OK`. The server terminal should show the default priority on the first request and the explicit priority on the second.

```text
Received: SUBMIT 10 0 echo hello
Received: SUBMIT 10 7 echo hello
```