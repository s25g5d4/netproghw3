#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "commons.hpp"
#include "my_send_recv.hpp"

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
#include <pthread.h>
}

struct my_send_recv client(-1);

/**
 * Descrption: Clean exit when SIGINT received.
 */
static void sigint_safe_exit(int sig);

/**
 * Descrption: Connect and login to server.
 * Return: 0 if succeed, or -1 if fail.
 */
static int login(const char *addr, const char *port, std::string name);

/**
 * Descrption: Check and parse user input and run connect command.
 * Return: 0 if succeed, or -1 if fail.
 */
static int run_connect(std::vector<std::string> &cmd);

/**
 * Descrption: Send chatting.
 * Return: 0 if succeed, or -1 if fail.
 */
static int run_chat(std::vector<std::string> &cmd, std::string &cmd_orig);

void *print_msg(void *);

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
        string cmd_orig;
        getline(cin, cmd_orig);
        vector<string> cmd = parse_command(cmd_orig);
        if (cmd.size() == 0) {
            continue;
        }

        // Run command
        if (cmd[0] == "connect") {
            if (run_connect(cmd) == 0) {
                pthread_t tid;
                pthread_create(&tid, NULL, print_msg, NULL);
            }
        }
        else if (cmd[0] == "chat") {
            if (run_chat(cmd, cmd_orig) < 0) {
                break;
            }
        }
        else if (cmd[0] == "help") {
            cout << "Available commands:" << endl;
            cout << endl;
            cout << "connect <IP> <port> <username>" << endl;
            cout << "chat <user>[ user[ user]...] \"message\"" << endl;
            cout << "bye" << endl;
            cout << endl;
        }
        else if (cmd[0] == "bye" || cmd[0] == "exit") {
            break;
        }
        else {
            cout << "Invalid command." << endl;
        }

    }

    // Clean exit
    if (client.fd > 2) {
        close(client.fd);
    }

    cout << "Goodbye." << endl;
    return 0;
}

static void sigint_safe_exit(int sig)
{
    if (client.fd > 2) {
        close(client.fd);
    }
    std::cerr << "Interrupt.\n" << std::endl;
    exit(1);
}

static int login(const char *addr, const char *port, std::string name)
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

    int sockfd;
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

    client.fd = sockfd;

    std::string login_cmd = "user " + name + "\n";
    int len = static_cast<int>(login_cmd.size());
    status = client.send(login_cmd.c_str(), &len);
    if (status < 0) {
        perror("my_send");
        close(client.fd);
        return -1;
    }

    // Read welcome message
    char welcome_msg[MAX_CMD] = {};
    int msglen = static_cast<int>(sizeof (welcome_msg));
    status = client.recv_cmd(welcome_msg, &msglen);
    if (status < 0) {
        perror("my_recv_cmd");
        close(client.fd);
        client.fd = -1;
        return -1;
    }
    else if (status > 0) {
        std::cout << "Invalid command received." << std::endl;
        close(client.fd);
        client.fd = -1;
        return -1;
    }

    welcome_msg[msglen - 1] = '\0';
    std::cout << welcome_msg << std::endl;

    // test whether connection is closed.
    const char *peek = "\n";
    len = 1;
    status = client.send(peek, &len, MSG_NOSIGNAL);
    if (status < 0) {
        if (errno == EPIPE) {
            std::cout << "Connection closed by peer." << std::endl;
        }
        else {
            perror("my_send");
        }
        close(client.fd);
        client.fd = -1;
        return -1;
    }

    return 0;
}

static int run_connect(std::vector<std::string> &cmd)
{
    if (cmd.size() < 4) {
        std::cout << "Invalid command.";
        return -1;
    }

    if (client.fd > 2) {
        std::cout << "The connection has been established already." << std::endl;
        return -1;
    }

    std::cout << "Connecting to " << cmd[1] << ":" << cmd[2] << std::endl;
    if (login(cmd[1].c_str(), cmd[2].c_str(), cmd[3]) < 0) {
        std::cout << "Fail to login." << std::endl;
        return -1;
    }

    return 0;
}

static int run_chat(std::vector<std::string> &cmd, std::string &cmd_orig)
{
    using namespace std;

    if (client.fd <= 2) {
        cout << "You are not logged in yet." << endl;
        return 1;
    }

    if (cmd.size() < 3) {
        cout << "Please provide receiver and message." << endl;
        return 1;
    }

    size_t msg_start = cmd_orig.find('"', 6);
    size_t msg_end = msg_start == string::npos ? string::npos : cmd_orig.find('"', msg_start + 1);

    if (msg_start == string::npos || msg_end == string::npos) {
        cout << "Invalid message." << endl;
        return 1;
    }

    string msg = "chat ";
    for (unsigned int i = 1; i < cmd.size(); ++i) {
        if (cmd[i][0] == '"') {
            break;
        }
        msg += cmd[i] + " ";
    }

    msg += "\"";
    msg.append(cmd_orig.begin() + msg_start + 1, cmd_orig.begin() + msg_end);
    msg += "\"\n";

    // Send msg to server
    int sendlen = static_cast<int>(msg.size());
    int status = client.send(msg.c_str(), &sendlen);
    if (status < 0) {
        perror("my_send");
        cout << "Send failed. Terminate conneciton." << endl;
        return -1;
    }

    return 0;
}

void *print_msg(void *)
{
    using namespace std;

    while (true) {
        if (client.fd < 0) {
            pthread_exit(NULL);
        }

        char msg_orig[MAX_CMD];
        int msglen = static_cast<int>(sizeof (msg_orig));
        int status = client.recv_cmd(msg_orig, &msglen);
        if (status < 0) {
            perror("my_recv");
            pthread_exit(NULL);
        }
        else if (status > 0) {
            if (msglen == 0) {
                cerr << "Connection closed by peer. Terminate connection." << endl;
            }
            else {
                cerr << "Invalid command received. Terminate connection." << endl;
            }
            close(client.fd);
            exit(1);
        }

        msg_orig[msglen - 1] = '\0';

        if (memcmp(msg_orig, "message ", 8) == 0) {
            vector<string> cmd = parse_command(msg_orig);

            unsigned int msg_start = 0;
            unsigned int msg_end = 0;
            for (unsigned int i = 6; i < sizeof (msg_orig) && msg_orig[i] != '\n'; ++i) {
                if (msg_orig[i] == '"') {
                    if (msg_start > 0) {
                        msg_end = i;
                        break;
                    }
                    msg_start = i;
                }
            }
            if (msg_start == 0 || msg_end == 0) {
                cout << "Invalid command received. Terminating connection..." << endl;
                close(client.fd);
                exit(1);
            }

            string msg(msg_orig + msg_start + 1, msg_end - msg_start - 1);

            time_t msg_time = static_cast<time_t>(stoul(cmd[1]));
            string time_str = ctime(&msg_time);
            time_str.pop_back();
            cout << "\r" << time_str << " " << cmd[2] << ": " << msg << endl << "> " << flush;
        }
        else {
            cout << "\r" << msg_orig << endl << "> " << flush;
        }
    }
}
