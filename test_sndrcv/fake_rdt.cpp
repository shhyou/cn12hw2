/* This file implements Go-Back-N transfer library */

#include <cassert>
#include <cstdlib>
#include <cstring>

#include <deque>
#include <string>

#include "log.h"
#include "udt.h"
#include "rdt.h"

using std::string;

void snd(channel_t udt, const void *data, size_t len) {
	__log;

    udt.send(&len, sizeof(len));

    for (size_t head = 0, sent; head < len; head += sent) {
        sent = udt.send((char*) data + head, 200);
    }
}

void* rcv(channel_t udt, size_t &rcvlen) {
	__log;

    void* buf = malloc(rcvlen);
    if (buf == NULL)
        logger.raise("malloc failed.");
    
    size_t actual_len;
    while (udt.recv(&actual_len, sizeof(actual_len)) == 0) ;
    
    if (actual_len < rcvlen) rcvlen = actual_len;
    //logger.print("Package length = %d", rcvlen);

    for (size_t head = 0, got; head < rcvlen; head += got) {
        got = udt.recv((char*)buf + head, rcvlen - head);
    }

	return buf;
}
