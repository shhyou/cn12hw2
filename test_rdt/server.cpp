#include <cstdio>
#include <cstdlib>

#include <string>

#include "../log.h"
#include "../udt.h"
#include "../rdt.h"

#include "test.h"

using namespace std;

unsigned int data[MAX];

int main() {
	__log;

	for (int i = 0; i != MAX; i++)
		data[i] = i;
	try {
		void *buf;
		unsigned int *pdata;
		size_t siz = sizeof(unsigned int)*SIZ;
		channel_t udt = udt_new(51451);
		buf = rcv(udt, siz);
		logger.print("received data of length %lu", siz);
		pdata = (unsigned int*)buf;
		for (unsigned int i = 0; i != siz/sizeof(unsigned int); i++)
			if (pdata[i] != i) logger.eprint("data %u corrupt: expected %u but got %u",
					i, data[i], pdata[i]);
		free(buf);
	} catch (const string& err) {
		logger.eprint("%s", err.c_str());
	}
	return 0;
}

