#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "my_send_recv.hpp"

void my_send_recv::close()
{
    if (fd > 0) {
        ::close(fd);
    }
    fd = -1;
    in_buflen = 0;
}

int my_send_recv::recv(int flags)
{
    int received_val = ::recv(fd, in_buf, sizeof (in_buf), flags);
    if (received_val < 0) {
        in_buflen = 0;
        return received_val;
    }
    in_buflen = received_val;

    return in_buflen;
}

int my_send_recv::send(const void *buf, int *buflen, int flags)
{
    int sent = 0;
    int send_val = 0;

    if (fd < 0) {
        *buflen = 0;
        return -1;
    }

    while (*buflen > sent) {
        send_val = ::send(fd, reinterpret_cast<const char *>(buf) + sent, *buflen - sent, 0);
        if (send_val <= 0) {
            *buflen = sent;
            return send_val;
        }

        sent += send_val;
    }

    *buflen = sent;
    return 0;
}

int my_send_recv::recv_cmd(char *buf, int *buflen)
{
    if (fd < 0) {
        *buflen = 0;
        return -1;
    }

    int received_val;
    if (in_buflen == 0) {
        received_val = recv(0);
        if (received_val <= 0) {
            *buflen = 0;
            return (received_val == 0 ? 1 : -1);
        }
    }

    int received = 0;

    uint8_t *cmd_end = reinterpret_cast<uint8_t *>( memchr(in_buf, '\n', static_cast<size_t>(in_buflen)) );
    while (cmd_end == NULL) {
        size_t cpy_size = static_cast<size_t>( (*buflen - received >= in_buflen) ? in_buflen : (*buflen - received) );
        memcpy(buf + received, in_buf, cpy_size);
        received += cpy_size;
        if (*buflen <= received) {
            memmove(in_buf, in_buf + cpy_size, in_buflen - cpy_size);
            in_buflen -= cpy_size;
            *buflen = received;
            return 1;
        }

        received_val = recv(0);
        if (received_val <= 0) {
            *buflen = received;
            return (received_val == 0 ? 1 : -1);
        }
        cmd_end = reinterpret_cast<uint8_t *>( memchr(in_buf, '\n', static_cast<size_t>(in_buflen)) );
    }

    int end_index = static_cast<int>(cmd_end - in_buf) + 1;

    size_t cpy_size = static_cast<size_t>( (*buflen - received >= end_index) ? end_index : (*buflen - received) );
    memcpy(buf + received, in_buf, cpy_size);
    received += cpy_size;
    if (static_cast<size_t>(in_buflen) > cpy_size) {
        memmove(in_buf, in_buf + cpy_size, in_buflen - cpy_size);
    }
    in_buflen -= cpy_size;
    *buflen = received;
    if (cpy_size < static_cast<size_t>(end_index)) {
        return 1;
    }
    
    return 0;
}

int my_send_recv::recv_data(void *buf, int *buflen)
{
    if (fd < 0) {
        *buflen = 0;
        return -1;
    }

    int received_val;
    if (in_buflen == 0) {
        received_val = recv(0);
        if (received_val <= 0) {
            *buflen = 0;
            return received_val;
        }
    }

    int received = 0;
    while (received < *buflen) {
        size_t cpy_size = static_cast<size_t>( (*buflen - received >= in_buflen) ? in_buflen : (*buflen - received) );
        memcpy(reinterpret_cast<char *>(buf) + received, in_buf, cpy_size);
        received += cpy_size;
        if (*buflen <= received) {
            memmove(in_buf, in_buf + cpy_size, in_buflen - cpy_size);
            in_buflen -= cpy_size;
            *buflen = received;
            return 0;
        }

        received_val = recv(0);
        if (received_val < 0) {
            *buflen = received;
            return received_val;
        }
        if (received_val == 0) {
            break;
        }
    }

    *buflen = received;
    return 0;
}
