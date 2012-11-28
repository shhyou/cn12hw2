/* This file implements Go-Back-N transfer library */

#ifndef _RDT_H
#define _RDT_H

#include "udt.h"

void snd(channel_t udt, void *data, size_t len);
size_t rcv(channel_t udt, void *&data);

#endif
