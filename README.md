# netproghw2

The purpose of this homework is to write a (fake) Huffman-coded FTP server &
client. There are three commands can be used in the client:

```
login <IP> <port>
send <filename>
logout
```

For server, it does not interact with user.

## Features

* Server
  - IPv6 capable
  - Print message when connection established, terminated and file received
  - Send welcome message to client
  - Uncompress and save file sent from client
  - Save Huffman coding table to file

* Client
  - Connect to server via hostname or IP address (v6 capable)
  - File tansmitting is compressed using Huffman coding

* Common
  - Both binary and ASCII text can be transferred correctly
  - Fixed-length Huffman coding is **not** implemented
  - Variable-length Huffman coding is implemented instead

## Build

`$ make`

## Usage

Server:

```
$ ./server
Start listening at port 1732.
Connection from ::1 port 49390 protocol SOCK_STREAM(TCP) accepted.
Receiving LICENSE ...
OK 1165 bytes received.
Uncompressed file size: 1064 bytes. Compression ratio: 109.49%.
Huffman coding table is saved in LICENSE.code .
Connection terminated.
```

Client:

```
$ ./client
> login ::1 1732
Connecting to ::1:1732
Welcome to my netprog hw2 FTP server
> send LICENSE
Original file size: 1064bytes, compressed size: 1165 bytes.
Compression ratio: 109.49%.
OK 1165 bytes sent.
> logout
Goodbye.
```

## Organization

```
netproghw2
 ├── README.md - The file you're reading.
 ├── LICENSE - The license of this project.
 ├── Makefile - Directives for GNU make build automation tool.
 ├── server.cpp - The main body of server.
 ├── client.cpp - The main body of client.
 ├── commons.hpp - Header of common functions and variables.
 ├── commons.cpp - Common functions and variables.
 ├── my_huffman.hpp - Header of Huffman coding library.
 ├── my_huffman.cpp - Huffman coding library.
 ├── my_send_recv.h - Header of custom send and recv functions.
 └── my_send_recv.c - Custom send and recv functions, written in C.
```

## Protocol

Every message or command sent to remote host must ended with newline ('\n')
character, and has a max length limit of 512 bytes.

Send:

  `send <length> <filename>\n`

  Client issues `send` command with total transmitting length and filename,
  and followed by the data with exactly that length.

## Program Procedure

Server:

  1. Accept and send welcome message (end with '\n') to client.
  2. Read and process client command repeatedly.
      - Currently only `send` command is implemented. Data sent from client
        is expected compressed using Huffman coding, with file length and code
        table embeded.
  3. After client terminate connection, back to step 1.
  4. If any invalid command received, terminate connection immediately and
      return to step 1.

Client:

  1. Connect to server.
  2. Read and print welcome message.
  3. Read and process user command repeatedly.
      - Currently only `send` command is implemented. Client read and compress
        file, calculate length including Huffman tree embedded, then send
        command to server.
  4. logout and exit process.
