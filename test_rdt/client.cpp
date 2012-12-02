#include <cstdio>
#include <cstdlib>

#include <string>

#include "../log.h"
#include "../udt.h"
#include "../rdt.h"

#include "test.h"

using namespace std;

unsigned int data[MAX];

int main(int argc, char *argv[]) {
	__log;

    //int port = 51451;
	int port = 5000;
    if (argc >= 2) port = atoi(argv[1]);

	for (int i = 0; i != MAX; i++)
		data[i] = i;
	try {
		size_t siz = sizeof(unsigned int)*SIZ;
		channel_t udt = udt_new(port, "localhost");
		snd(udt, data, siz);
		logger.print("data should be sent now?");
	} catch (const string& err) {
		logger.eprint("%s", err.c_str());
	}
	return 0;
}

