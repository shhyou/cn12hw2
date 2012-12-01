#include <cstdio>
#include <cstdlib>

#include <string>

#include "../log.h"
#include "../udt.h"
#include "../rdt.h"

using namespace std;

#define MAX 1048576

unsigned int data[MAX];

int main() {
	__log;

	for (int i = 0; i != MAX; i++)
		data[i] = i;
	try {
		void *buf;
		unsigned int *pdata;
		size_t siz = sizeof(unsigned int)*1;
		channel_t udt = udt_new(51451);
		buf = rcv(udt, siz);
		logger.print("received data of length %lu\n", siz);
		pdata = (unsigned int*)buf;
		for (int i = 0; i*sizeof(unsigned int)!=siz; i++)
			printf("%u\t", pdata[i]);
		free(buf);
	} catch (const string& err) {
		logger.eprint("%s", err.c_str());
	}
	return 0;
}

