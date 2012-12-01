/* udt.h provides unreliable data transfer utilities */

#ifndef _UDT_H
#define _UDT_H

#include <stdio.h> // for NULL
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

struct channel_t {
    int fd;
    timeval timeout;
    channel_t();
    ~channel_t();
    sockaddr_in dest;
    ssize_t send(const void* buf, size_t len) const; /* may throw error (string)*/
    ssize_t recv(void* buf, size_t maxlen); /* may throw error (string) */
    /* note that recv changes dest to the incomer's IP nomatter it succeesed or not*/
    void close();
};

/* NULL is for receiving. channel_t::recv should fill the src ip */
/* throws std::strings on error */
channel_t udt_new(unsigned short port, const char *addr = NULL);

#endif
