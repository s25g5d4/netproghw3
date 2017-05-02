#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "commons.hpp"
#include "my_huffman.hpp"

extern "C" {
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>
#include <libgen.h>

#include "my_send_recv.h"
}

int sockfd = 0;

/**
 * Descrption: Clean exit when SIGINT received.
 */
static void sigint_safe_exit(int sig);

/**
 * Descrption: Connect and login to server.
 * Return: 0 if succeed, or -1 if fail.
 */
static int login(const char *addr, const char *port);

/**
 * Descrption: Check and parse user input and run login command.
 * Return: 0 if succeed, or -1 if fail.
 */
static int run_login(std::vector<std::string> &cmd);

/**
 * Descrption: Check and parse user input and send file to server.
 * Return: 0 if succeed, or -1 if fail.
 */
static int run_send(std::vector<std::string> &cmd, std::string &orig_cmd);

int main()
{
    // Handle SIGINT
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigint_safe_exit;
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);

    using namespace std;

    // Prompt for user command
    while (true) {
        cout << "> " << flush;
        string orig_cmd;
        getline(cin, orig_cmd);
        vector<string> cmd = parse_command(orig_cmd);
        if (cmd.size() == 0) {
            continue;
        }

        // Run command
        if (cmd[0] == "login") {
            run_login(cmd);
        }
        else if (cmd[0] == "send") {
            if (run_send(cmd, orig_cmd) < 0) {
                break;
            }
        }
        else if (cmd[0] == "logout" || cmd[0] == "exit") {
            break;
        }
        else {
            cout << "Invalid command." << endl;
        }

    }

    // Clean exit
    if (sockfd > 2) {
        close(sockfd);
    }

    cout << "Goodbye." << endl;
    return 0;
}

static void sigint_safe_exit(int sig)
{
    if (sockfd > 2) {
        close(sockfd);
    }
    fprintf(stderr, "Interrupt.\n");
    exit(1);
}

static int login(const char *addr, const char *port)
{
    // Resolve hostname and connect
    struct addrinfo hints = {};
    struct addrinfo *res;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int status = getaddrinfo(addr, port, &hints, &res);
    if (status != 0) {
        std::cerr << gai_strerror(status) << std::endl;
        return -1;
    }

    struct addrinfo *p;
    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd > 0) {
            if (connect(sockfd, p->ai_addr, p->ai_addrlen) == 0) {
                break;
            }

            perror("connect");
            close(sockfd);
            sockfd = 0;
            continue;
        }

        perror("socket");
    }

    freeaddrinfo(res);

    if (p == NULL) {
        return -1;
    }

    // Read welcome message
    char welcome_msg[64] = {};
    int msglen = (int) sizeof (welcome_msg);
    status = my_recv_cmd(sockfd, welcome_msg, &msglen);
    if (status < 0) {
        perror("my_recv_cmd");
        return -1;
    }
    else if (status > 0) {
        std::cout << "Invalid command received." << std::endl;
        return -1;
    }

    welcome_msg[msglen - 1] = '\0';
    std::cout << welcome_msg << std::endl;

    return 0;
}


static int run_login(std::vector<std::string> &cmd)
{
    if (cmd.size() < 3) {
        std::cout << "Invalid command.";
        return -1;
    }

    if (sockfd > 2) {
        std::cout << "The connection has been established already." << std::endl;
        return -1;
    }

    std::cout << "Connecting to " << cmd[1] << ":" << cmd[2] << std::endl;
    if (login(cmd[1].c_str(), cmd[2].c_str()) < 0) {
        std::cout << "Fail to login." << std::endl;
        return -1;
    }

    return 0;
}

static int run_send(std::vector<std::string> &cmd, std::string &orig_cmd)
{
    using namespace std;

    if (sockfd <= 2) {
        cout << "You are not logged in yet." << endl;
        return 1;
    }

    if (cmd.size() < 2) {
        cout << "Please provide file name." << endl;
        return 1;
    }

    // Get file path
    string::size_type n = orig_cmd.find(cmd[1]);
    if (n == string::npos) {
        cout << "Command error." << endl;
        return 1;
    }

    string pathname(orig_cmd.begin() + n, orig_cmd.end());

    ifstream file(pathname, fstream::in | fstream::binary);
    if (!file.is_open()) {
        cout << "Failed to open file." << endl;
        return 1;
    }

    // Get filename
    // Both dirname() and basename() may modify the contents of path, so it
    // may be desirable to pass a copy when calling one of these functions.
    char *pathname_c_str = new char[pathname.size() + 1];
    memcpy(pathname_c_str, pathname.c_str(), pathname.size() + 1);

    string filename = basename(pathname_c_str);

    delete pathname_c_str;

    // Encode with Huffman Coding
    my_huffman::huffman_encode encoded_file(file);

    uint8_t *buf;
    int buflen;

    file.seekg(0);
    encoded_file.write(file, &buf, &buflen);

    // Send command to server
    string send_cmd = "send " + to_string(buflen) + " " + filename + "\n";
    int sendlen = static_cast<int>(send_cmd.size());
    int status = my_send(sockfd, send_cmd.c_str(), &sendlen);
    if (status < 0) {
        perror("my_send");
        cout << "Send failed. Terminate conneciton." << endl;
        return -1;
    }

    // Send file to server
    status = my_send(sockfd, buf, &buflen);
    if (status < 0) {
        perror("my_send");
        cout << "Send failed. Terminate conneciton." << endl;
        return -1;
    }

    file.clear();
    file.seekg(0, file.end);

    cout << "Original file size: " << file.tellg() << "bytes, compressed size: " << buflen << " bytes." << endl;
    cout.precision(2);
    cout.setf(ios::fixed);
    cout << "Compression ratio: " << static_cast<double>(buflen)*100.0 / static_cast<double>(file.tellg()) << "%." << endl;

    // Get response
    char msg[MAX_CMD];
    int msglen = MAX_CMD - 1;
    status = my_recv_cmd(sockfd, msg, &msglen);
    if (status > 0) {
        cout << "Invalid response. Terminate conneciton." << endl;
        return -1;
    }
    else if (status < 0) {
        perror("my_recv_cmd");
        cout << "Invalid response. Terminate conneciton." << endl;
        return -1;
    }

    vector<string> res = parse_command(msg);
    if (res.size() < 2 || res[0] != "OK") {
        cout << "Invalid response. Terminate conneciton." << endl;
        return -1;
    }
    
    int sent;
    try {
        sent = stoi(res[1]);
    }
    catch (exception &e) {
        cout << "Invalid response. Terminate conneciton." << endl;
        return -1;
    }

    cout << "OK " << sent << " bytes sent." << endl;
    return 0;
}
