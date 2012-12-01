/* This file implements Go-Back-N transfer library */

#ifndef _RDT_H
#define _RDT_H

#include "udt.h"

void snd(channel_t udt, const void *data, size_t len);
void* rcv(channel_t udt, size_t& rcvlen);

#endif
