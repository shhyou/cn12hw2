#include <cstring>
#include <cstdarg>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "udt.h"

char* error_message(const char *format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args);
    vsprintf(buffer, format, args);
    va_end(args);
    sprintf(buffer + strlen(buffer), ": %s", strerror(errno));
    return buffer;
}

char* herror_message(const char *format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args);
    vsprintf(buffer, format, args);
    va_end(args);
    sprintf(buffer + strlen(buffer), ": %s", hstrerror(h_errno));
    return buffer;
}

channel_t::channel_t() {
}

channel_t::~channel_t() {
    this->close();
}

size_t send(void* buf, size_t len) {
    size_t ret = sendto(this->fd, buf, len, 0 (sockaddr*) &this->dest, sizeof(this->dest));
    if (ret < 0)
        throw error_message("failed to send UDP package");
    return ret;
}

size_t recv(void* buf, size_t maxlen) {
    size_t ret = recvfrom(this->fd, biuf, maxlen, 0, (sockaddr*) &this->dest, sizeof(this->dest));
    if (ret < 0)
        throw error_message("failed to receive UDP package");
    return ret;
}

void channel_t::close() {
    if (close(this->fd) != 0)
        perror("failed to close channel");
}

channel_t udt_new(unsigned short port, const char *addr) {
    channel_t cnl;
    cnl.fd = socket(AF_INET, SOCK_DGRAM, 0);
    bzero(&cnl.dest, sizeof(cnl.dest));
    cnl.dest.sin_family = AF_INET;
    cnl.dest.sin_port = htons(port);

    if (addr == NULL) {
        
        cnl.dest.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(fd, (sockaddr*) &cnl.dest, sizeof(cnl.dest)) != 0)
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
