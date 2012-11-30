#include "udt.h"

#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netinet/in.h>

char* error_message(const char *format, ...) {
    char* buffer = new char[1024];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);
    sprintf(buffer + strlen(buffer), ": %s", strerror(errno));
    return buffer;
}

char* herror_message(const char *format, ...) {
    char* buffer = new char[1024];
    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);
    sprintf(buffer + strlen(buffer), ": %s", hstrerror(h_errno));
    return buffer;
}

channel_t::channel_t() {
    this->fd = -1;
    this->timeout.tv_sec = 0;
    this->timeout.tv_usec = 500 * 1000;
}

channel_t::~channel_t() {
}

ssize_t channel_t::send(void* buf, size_t len) {
    ssize_t ret = sendto(this->fd, buf, len, 0, (sockaddr*) &this->dest, sizeof(this->dest));
    if (ret < 0)
        throw error_message("failed to send UDP package");
    return ret;
}

ssize_t channel_t::recv(void* buf, size_t maxlen) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(this->fd, &readfds);
    
    timeval ltimeout = this->timeout;

    int srep = select(this->fd + 1, &readfds, NULL, NULL, &ltimeout);

    if (srep == 0) // timeout
        return 0;
    else if (srep < 0)
        throw error_message("select failed");
    
    socklen_t len = sizeof(this->dest);
    ssize_t ret = recvfrom(this->fd, buf, maxlen, 0, (sockaddr*) &this->dest, &len);

    if (ret < 0)
        throw error_message("failed to receive UDP package");
        
    return ret;
}

void channel_t::close() {
    if (this->fd != -1 && ::close(this->fd) != 0)
        perror("failed to close channel");
    else
        this->fd = -1;
}

channel_t udt_new(unsigned short port, const char *addr) {
    channel_t cnl;
    cnl.fd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&cnl.dest, sizeof(cnl.dest));
    cnl.dest.sin_family = AF_INET;
    cnl.dest.sin_port = htons(port);

    if (addr == NULL) {
        
        cnl.dest.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(cnl.fd, (sockaddr*) &cnl.dest, sizeof(cnl.dest)) != 0)
            throw error_message("binding error in udt_new");
        
    } else {

        hostent *at = gethostbyname(addr); // may point to static data
        if (at == NULL)
            throw herror_message("Couldn't get address of '%s'", addr);
        
        memcpy(&cnl.dest.sin_addr.s_addr, at->h_addr, at->h_length);
        if (connect(cnl.fd, (sockaddr*) &cnl.dest, sizeof(cnl.dest)) != 0)
            throw error_message("connecting error in udt_new");
        
    }

    return cnl;
}
