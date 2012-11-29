/* udt.h provides unreliable data transfer utilities */

#ifndef _UDT_H
#define _UDT_H

#include <sys/socket.h>
#include <netinet/in.h>

struct channel_t {
    int fd;
    channel_t();
    ~channel_t();
    sockaddr_in dest;
    size_t send(void* buf, size_t len); /* may throw error */
    size_t recv(void* buf, size_t maxlen); /* may throw error */
    void close();
};

/* NULL is for receiving. channel_t::recv should fill the src ip */
/* throws strings (char*) on error */
channel_t udt_new(unsigned short port, const char *addr);

#endif
