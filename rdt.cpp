/* This file implements Go-Back-N transfer library */

#include <cassert>
#include <cstdlib>

#include "udt.h"
#include "rdt.h"

const int N = 512;

/* package processing; Need this be moved to another file? */
struct __attribute__((packed)) pkg_t {
    unsigned char len;
    unsigned short seq; /* I have no idea how long this should be */
    unsigned int crc;
    unsigned char data[1];
};

static unsigned int crc32(void* data, size_t len) {
    unsigned int tbl32[256];
    if (!tbl32[0]) {
        for (int i = 0; i < 256; i++) {
            int poly = i;
            for (int j = 0; j < 8; j++) {
                if (poly&1) poly = (poly>>1)^0xEDB88320;
                else poly >>= 1;
            }
            tbl32[i] = poly;
        }
    }
    unsigned char *buf = (unsigned char*)data;
	unsigned int crc = 0, idx;
	for (size_t i = 0; i != len; i++) {
		idx = (crc^buf[i])&0xff;
		crc = (crc>>8) ^ crc_tbl32[idx];
	}
    return crc;
}

static pkg_t *mkpkt(unsigned short seq, void* data, size_t len) {
    pkg_t *p;
    assert(0<len && len+sizeof(pkg_t)-1<=256);
    p = (pkg_t*)malloc(len + sizeof(pkg_t) - 1);
    p->len = len + sizeof(pkg_t) - 1;
    p->seq = seq;
    p->crc = 0;
    memcpy(p->data, data, len);
    p->crc = crc32(p, p->len);
    return p;
}

static bool unpkt(unsigned short &seq, void* data, size_t len, pkg_t *p) {
    int rcv_crc = p->crc;
    p->crc = 0;
    if (crc32(p, p->len) != rcv_crc)
        return false;
    len = p->len - sizeof(pkg_t) + 1;
    seq = p->seq;
    memcpy(data, p->data, len);
    return true;
}

void snd(channel_t udt, void *data, size_t len) {
}

size_t rcv(channel_t udt, void *&data) {
}
