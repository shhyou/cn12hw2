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
		size_t siz = sizeof(unsigned int)*1;
		channel_t udt = udt_new(51451, "localhost");
		snd(udt, data, siz);
		logger.print("data should be sent now?");
	} catch (const string& err) {
		logger.eprint("%s", err.c_str());
	}
	return 0;
}

