#include "udt.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>

#include "log.h"

channel_t::channel_t() {
    this->fd = -1;
    this->timeout.tv_sec = 0;
    this->timeout.tv_usec = 500 * 1000;
}

channel_t::~channel_t() {
}

ssize_t channel_t::send(const void* buf, size_t len) const {
    __logc;

    ssize_t ret = sendto(this->fd, buf, len, 0, (sockaddr*) &this->dest, sizeof(this->dest));
    if (ret < 0)
        throw logger.errmsg("failed to send UDP package");
    return ret;
}

ssize_t channel_t::recv(void* buf, size_t maxlen) {
    __logc;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(this->fd, &readfds);
    
    timeval ltimeout = this->timeout;

    int srep = select(this->fd + 1, &readfds, NULL, NULL, &ltimeout);

    if (srep == 0) // timeout
        return 0;
    else if (srep < 0)
        throw logger.errmsg("select failed");
    
    socklen_t len = sizeof(this->dest);
    ssize_t ret = recvfrom(this->fd, buf, maxlen, 0, (sockaddr*) &this->dest, &len);

    if (ret < 0)
        throw logger.errmsg("failed to receive UDP package");
        
    return ret;
}

void channel_t::close() {
    if (this->fd != -1 && ::close(this->fd) != 0)
        perror("failed to close channel");
    else
        this->fd = -1;
}

channel_t udt_new(unsigned short port, const char *addr) {
    __log;

    channel_t cnl;
    cnl.fd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&cnl.dest, sizeof(cnl.dest));
    cnl.dest.sin_family = AF_INET;
    cnl.dest.sin_port = htons(port);

    if (addr == NULL) {
        
        cnl.dest.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(cnl.fd, (sockaddr*) &cnl.dest, sizeof(cnl.dest)) != 0)
            throw logger.errmsg("binding error");
        
    } else {

        hostent *at = gethostbyname(addr); // may point to static data
        if (at == NULL)
            throw logger.herrmsg("Couldn't get address of '%s'", addr);
        
        memcpy(&cnl.dest.sin_addr.s_addr, at->h_addr, at->h_length);
        if (connect(cnl.fd, (sockaddr*) &cnl.dest, sizeof(cnl.dest)) != 0)
            throw logger.errmsg("connecting error in udt_new");
        
    }

    return cnl;
}
