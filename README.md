# LAN Torrent

Our SOC 3010 project is a C job runner for a local network. The goal is for a client to submit a download, a server to schedule it, and a worker to fetch file pieces from peers and verify them with SHA-256.

## Current progress • November 26, 2026

The first TCP round trip works. The server listens on port `5000`, handles one client at a time, and reads one newline-terminated request per connection. It prints the request in the server terminal, sends `OK` to the client, and closes the connection. If the client disconnects halfway through a line, the server discards it.

`server.c` handles connections using the course’s `net.h`. The two helpers in `net.c`, `receive_line()` and `send_text()`, handle reading the request and writing the reply. At this stage, `OK` confirms receipt of a complete line. Job creation comes in the next milestone.

## Try the prototype on Ubuntu

From the directory containing `server.c` and `net.c`, build and start the server:

```sh
gcc -Wall -Wextra server.c net.c -o server
./server
```

In a second terminal, open the extracted course `example_socket` directory and run:

```sh
gcc -Wall -Wextra client.c -o example_client
./example_client 127.0.0.1 "SUBMIT 10 0 echo hello"
```

The server prints `Received: SUBMIT 10 0 echo hello`. The example client prints `client: the server said "OK"`.