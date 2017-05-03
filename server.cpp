#include <iostream>
#include <fstream>
#include <exception>
#include <map>
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
#include <netdb.h>
#include <signal.h>
#include <errno.h>

}

#define LISTEN_PORT 1733
#define BUFLEN 65536

int sockfd = 0;
char welcome_msg[] = "Welcome to my netprog hw3 chat server\n";

class client : public my_send_recv
{
public:
    std::string name;
    struct sockaddr_storage addr;

    client(int fd, std::string name, struct sockaddr_storage addr)
    : my_send_recv(fd), name(name), addr(addr)
    {

    }
};

int accept_connection(int sockfd, std::vector<client> &nonlogin_clients);
int serve_client(fd_set &set, std::map<std::string, client> &clients, std::map<int, client *> &fd_to_client);
int relay_msg(std::vector<std::string> &cmd, const char *cmd_orig, client &cl, std::map<std::string, client> &clients, std::map<int, client *> &fd_to_client);
static int client_leave(client &cl, std::map<std::string, client> &clients, std::map<int, client *> &fd_to_client);
int user_login(fd_set &set, std::vector<client> &nonlogin_clients, std::map<std::string, client> &clients, std::map<int, client *> &fd_to_client);
static void print_client(client &cl);
static std::string get_ip(client &cl);
static std::string get_port(client&cl);

/**
 * Descrption: Clean exit when SIGINT received.
 */
static void sigint_safe_exit(int sig);

/**
 * Descrption: Determine and return IPv4 or IPv6 address object.
 * Return: IPv4 or IPv6 address object (sin_addr or sin6_addr).
 */
static const void *get_in_addr(const struct sockaddr &sa);

/**
 * Descrption: Get port number from sockaddr object.
 * Return: Unsigned short, host byte order port number.
 */
static uint16_t get_in_port(const struct sockaddr &sa);

/**
 * Descrption: Start server and listening to clients.
 * Return: 0 if succeed, or -1 if fail.
 */
static int start_server();

/**
 * Descrption: Print client info and send welcome message to client.
 * Return: 0 if succeed, or -1 if fail.
 */
static int welcome(client &cl);

int main()
{
    // Handle SIGINT
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = sigint_safe_exit;
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, NULL);

    using namespace std;

    // Start server
    int status = start_server();
    if (status != 0) {
        if (sockfd > 2) {
            close(sockfd);
        }
        cerr << "Fail to start server." << endl;
        exit(1);
    }

    fd_set set;
    FD_ZERO(&set);
    FD_SET(sockfd, &set);
    int maxfd = sockfd + 1;

    vector<client> nonlogin_clients;
    map<string, client> clients;
    map<int, client *> fd_to_client;

    status = select(maxfd, &set, NULL, NULL, NULL);
    while (status >= 0) {
        if (FD_ISSET(sockfd, &set)) {
            status = accept_connection(sockfd, nonlogin_clients);
        }

        if (status < 0) {
            break;
        }

        status = serve_client(set, clients, fd_to_client);

        if (status < 0) {
            break;
        }

        status = user_login(set, nonlogin_clients, clients, fd_to_client);

        if (status < 0) {
            break;
        }

        FD_ZERO(&set);
        FD_SET(sockfd, &set);
        maxfd = sockfd + 1;

        for (auto &cl : clients) {
            if (cl.second.fd > 0) {
                FD_SET(cl.second.fd, &set);
                if (maxfd <= cl.second.fd) {
                    maxfd = cl.second.fd + 1;
                }
            }
        }

        for (auto &cl : nonlogin_clients) {
            if (cl.fd > 0) {
                FD_SET(cl.fd, &set);
                if (maxfd <= cl.fd) {
                    maxfd = cl.fd + 1;
                }
            }
        }

        status = select(maxfd, &set, NULL, NULL, NULL);
    }
    
    cout << "An error has occurred." << endl;

    close(sockfd);
    
    return 1;
}

static void sigint_safe_exit(int sig)
{
    if (sockfd > 2) {
        close(sockfd);
    }
    std::cerr << "Interrupt." << std::endl;
    exit(1);
}

int client_leave(client &cl, std::map<std::string, client> &clients, std::map<int, client *> &fd_to_client)
{
    using namespace std;

    int status = 0;

    string leave = "User " + cl.name + " is off-line.\n";
    for (auto it_lmsg = clients.begin(); it_lmsg != clients.end(); ++it_lmsg) {
        client &cl_lmsg = it_lmsg->second;
        if (cl_lmsg.fd < 0 || &cl_lmsg == &cl) {
            continue;
        }

        int len = static_cast<int>(leave.size());
        status = cl_lmsg.send(leave.c_str(), &len, MSG_NOSIGNAL);
        if (status < 0) {
            if (errno == EPIPE) {
                status = 0;
            }
            else {
                perror("my_send");
                return status;
            }
        }
    }

    fd_to_client.erase(cl.fd);
    close(cl.fd);
    cl.fd = -1;

    return status;
}

static const void *get_in_addr(const struct sockaddr &sa)
{
  if (sa.sa_family == AF_INET) {
    return &(reinterpret_cast<const struct sockaddr_in *>(&sa)->sin_addr);
  }
  return &(reinterpret_cast<const struct sockaddr_in6 *>(&sa)->sin6_addr);
}

static uint16_t get_in_port(const struct sockaddr &sa)
{
    if (sa.sa_family == AF_INET) {
        return ntohs(reinterpret_cast<const struct sockaddr_in *>(&sa)->sin_port);
    }
    return ntohs(reinterpret_cast<const struct sockaddr_in6 *>(&sa)->sin6_port);
}

static int start_server()
{
    // Create socket
    int addr_family = AF_INET6;
    sockfd = socket(PF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0) {
        std::cout << "Fail to open IPv6 socket. Fallback to IPv4 socket." << std::endl;
        addr_family = AF_INET;
        sockfd = socket(PF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            perror("socket");
            return -1;
        }
    }

    int status;

    // Set IPv6 dual stack socket
    if (addr_family == AF_INET6) {
        int no = 0;
        status = setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (char *) &no, sizeof (no));
        if (status != 0) {
            std::cout << "Fail to set dual stack socket. Fallback to IPv4 socket." << std::endl;
            close(sockfd);

            addr_family = AF_INET;
            sockfd = socket(PF_INET, SOCK_STREAM, 0);
            if (sockfd < 0) {
                perror("socket");
                return -1;
            }
        }
    }

    if (addr_family == AF_INET6) {
        struct sockaddr_in6 any_addr = {};
        any_addr.sin6_family = AF_INET6;
        any_addr.sin6_port = htons(LISTEN_PORT);

        status = bind(sockfd, reinterpret_cast<const struct sockaddr *>(&any_addr), sizeof (any_addr));
    }
    else {
        struct sockaddr_in any_addr = {};
        any_addr.sin_family = AF_INET;
        any_addr.sin_port = htons(LISTEN_PORT);

        status = bind(sockfd, reinterpret_cast<const struct sockaddr *>(&any_addr), sizeof (any_addr));
    }
    
    if (status < 0) {
        perror("bind");
        return -1;
    }

    status = listen(sockfd, 10);
    if (status < 0) {
        perror("listen");
        return -1;
    }

    std::cout << "Start listening at port " << LISTEN_PORT << "." << std::endl;

    return 0;
}

static std::string get_port(client&cl)
{
    struct sockaddr &client_addr = *reinterpret_cast<struct sockaddr *>(&cl.addr);
    return std::to_string(get_in_port(client_addr));
}

static std::string get_ip(client &cl)
{
    struct sockaddr &client_addr = *reinterpret_cast<struct sockaddr *>(&cl.addr);
    char client_addr_p[INET6_ADDRSTRLEN] = {};
    if (inet_ntop(client_addr.sa_family, get_in_addr(client_addr),
    client_addr_p, sizeof (client_addr_p)) == NULL) {
        perror("inet_ntop");
        return "";
    }

    // Extract address from IPv4-mapped IPv6 address.
    int offset = (memcmp(client_addr_p, "::ffff:", 7) == 0) ? 7 : 0;

    return std::string(client_addr_p + offset);
}

static void print_client(client &cl)
{
    std::cout << "Connection from " << get_ip(cl) << " port " <<
    get_port(cl) << " protocol SOCK_STREAM(TCP) accepted." << std::endl;
}

static int welcome(client &cl)
{

    std::cout << "User " << cl.name << " from " << get_ip(cl) << " logged in." << std::endl;
    
    int msglen = static_cast<int>(strlen(welcome_msg));
    return cl.send(welcome_msg, &msglen);
}

int accept_connection(int sockfd, std::vector<client> &nonlogin_clients)
{
    struct sockaddr_storage client_addr = {};
    socklen_t client_addr_size = sizeof (client_addr);

    // Accept client connecting.
    int clientfd = accept(sockfd, reinterpret_cast<struct sockaddr *>(&client_addr), &client_addr_size);
    if (clientfd < 0) {
        perror("accept");
        return -1;
    }

    nonlogin_clients.push_back({ clientfd, "", client_addr });
    print_client(nonlogin_clients.back());

    return 0;
}

int serve_client(fd_set &set, std::map<std::string, client> &clients, std::map<int, client *> &fd_to_client)
{
    using namespace std;

    int status = 0;

    for (auto it = clients.begin(); it != clients.end(); ++it) {
        client &cl = it->second;
        if (cl.fd < 0 || !FD_ISSET(cl.fd, &set)) {
            continue;
        }

        char cmd_orig[MAX_CMD];
        int cmdlen = MAX_CMD;
        status = cl.recv_cmd(cmd_orig, &cmdlen);
        if (status < 0) {
            perror("my_recv_cmd");
        }
        if (status > 0) {
            if (cmdlen == 0) {
                cout << "Connection closed by user " << cl.name << "." << endl;
            }
            else {
                cout << "Invalid command received from user " << cl.name << ". Terminating connection..." << endl;
            }

            client_leave(cl, clients, fd_to_client);
            continue;
        }
        
        vector<string> cmd = parse_command(cmd_orig);
        if (cmd.size() == 0) {
            continue;
        }
        
        if (cmd[0] == "chat") {
            status = relay_msg(cmd, cmd_orig, cl, clients, fd_to_client);
        }

        if (status < 0) {
            break;
        }
    }

    return status;
}

int relay_msg(std::vector<std::string> &cmd, const char *cmd_orig, client &cl, std::map<std::string, client> &clients, std::map<int, client *> &fd_to_client)
{
    using namespace std;

    int status = 0;

    size_t cmd_len = strlen(cmd_orig);
    unsigned int msg_start = 0;
    unsigned int msg_end = 0;
    for (unsigned int i = 6; i < cmd_len && cmd_orig[i] != '\n'; ++i) {
        if (cmd_orig[i] == '"') {
            if (msg_start > 0) {
                msg_end = i;
                break;
            }
            msg_start = i;
        }
    }
    if (msg_start == 0 || msg_end == 0) {
        cout << "Invalid command received. Terminating connection..." << endl;
        client_leave(cl, clients, fd_to_client);
        return status;
    }

    string msg(cmd_orig + msg_start + 1, msg_end - msg_start - 1);
    vector<client *> msg_peers;

    for (unsigned int i = 1; i < cmd.size(); ++i) {
        if (cmd[i][0] == '"') {
            break;
        }

        auto peer = clients.find(cmd[i]);
        if (peer == clients.end()) {
            string nonexist = "User " + cmd[i] + " does not exist.\n";
            int len = static_cast<int>(nonexist.size());
            cl.send(nonexist.c_str(), &len, MSG_NOSIGNAL);
            msg_peers.clear();
            break;
        }

        msg_peers.push_back(&peer->second);
    }

    msg = "message " + to_string(time(NULL)) + " " + cl.name + " \"" + msg + "\"\n";
    for (auto peer : msg_peers) {
        if (peer->fd > 0) {
            int len = static_cast<int>(msg.size());
            status = peer->send(msg.c_str(), &len, MSG_NOSIGNAL);
            if (status < 0) {
                perror("my_send");
                break;
            }
        }
        if (peer->fd < 0 || (status < 0 && errno == EPIPE)) {
            string offline = "User " + peer->name + " is off-line. The message will be passed when he comes back.\n";
            int len = static_cast<int>(offline.size());
            status = cl.send(offline.c_str(), &len, MSG_NOSIGNAL);
            if (status < 0) {
                perror("my_send");
                break;
            }
            else {
                status = 0;
            }
        }
    }

    return status;
}

int user_login(fd_set &set, std::vector<client> &nonlogin_clients, std::map<std::string, client> &clients, std::map<int, client *> &fd_to_client)
{
    using namespace std;

    int status = 0;

    for (auto it = nonlogin_clients.begin(); it != nonlogin_clients.end(); ) {
        client &cl = *it;
        if (!FD_ISSET(it->fd, &set)) {
            ++it;
            continue;
        }

        char cmd_orig[MAX_CMD];
        int cmdlen = MAX_CMD;
        status = cl.recv_cmd(cmd_orig, &cmdlen);
        if (status < 0) {
            perror("my_recv_cmd");
        }
        if (status > 0) {
            if (cmdlen == 0) {
                cout << "Connection closed by peer." << endl;
            }
            else {
                cout << "Invalid command received. Terminating connection..." << endl;
            }
            close(cl.fd);
            it = nonlogin_clients.erase(it);
            continue;
        }
        
        vector<string> cmd = parse_command(cmd_orig);
        if (cmd.size() == 0) {
            ++it;
            continue;
        }
        if (cmd[0] == "user") {
            cl.name = cmd[1];
            auto inserted = clients.insert(make_pair(cmd[1], cl));

            // user exists
            if (!inserted.second) {
                if (inserted.first->second.fd > 0) {
                    string reason = "User " + cmd[1] + " has logged in.\n";
                    int len = reason.size();
                    cl.send(reason.c_str(), &len, MSG_NOSIGNAL);

                    close(cl.fd);
                    it = nonlogin_clients.erase(it);
                    continue;
                }
                inserted.first->second = cl;
            }

            fd_to_client.insert(make_pair(cl.fd, &(inserted.first->second)));

            welcome(cl);

            string login_notify = "User " + cmd[1] + " is on-line, IP address: " + get_ip(cl) + "\n";
            for (auto it = clients.begin(); it != clients.end(); ++it) {
                int len = static_cast<int>(login_notify.size());
                it->second.send(login_notify.c_str(), &len, MSG_NOSIGNAL);
            }

            it = nonlogin_clients.erase(it);
            continue;
        }

        ++it;
    }

    return status;
}