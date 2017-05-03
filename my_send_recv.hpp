#ifndef __MY_SEND_RECV_HPP__
#define __MY_SEND_RECV_HPP__

class my_send_recv
{
    uint8_t in_buf[1024];
    int in_buflen;

    int recv(int flags);

public:
    int fd;

    my_send_recv(int fd) : in_buflen(0), fd(fd)
    {
    }

    /**
    * Description: Clean internal buffer and set fd to -1. Must be called if a
    *              client exited.
    */
    void close();

    /**
    * Description: Try to write `buflen` bytes of message of `buf` to `fd`.
    *              Actual bytes written will be stored in `buflen`.
    * Return: -1 if fail, and errno set to appropriate value.
    *         0 if succeed.
    */
    int send(const void *buf, int *buflen, int flags = 0);

    /**
    * Description: Try to read command from `fd` with a newline character ('\n').
    *              Read up to `buflen` bytes and write to `buf`.
    *              Actual bytes written will be stored in `buflen`.
    * Return: -1 if fail, and errno set to appropriate value.
    *         0 if read succeed and command is valid (end with '\n').
    *         1 if read succeed but command is not valid (not end with '\n') or
    *          read exact `buflen` bytes but no '\n' received.
    */
    int recv_cmd(char *buf, int *buflen);

    /**
    * Description: Read binary data.
    *              Read up to `buflen` bytes and write to `buf`.
    *              Actual bytes written will be stored in `buflen`.
    * Return: -1 if fail, and errno set to appropriate value.
    *         0 if read succeed.
    */
    int recv_data(void *buf, int *buflen);
};

#endif