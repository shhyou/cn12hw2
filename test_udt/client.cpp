#include <cstdio>
#include <cstring>

#include <string>

#include "../log.h"
#include "../udt.h"

using std::string;

void subrout(int port, const char *add) {
	__log;

    channel_t cnl = udt_new(port, add);
	char buf[180];
	for (int i = 1; i <= 1000; ++i) {
        sprintf(buf + 1, "%d", i);
        buf[0] = strlen(buf + 1) + 1;
        cnl.send(buf, buf[0] + 1);
	}
}

int main(int argc, char *argv[]) {
	__log;

    try {
        subrout(18018, argc > 1 ? argv[1] : "127.0.0.1");
    } catch (const string& err) {
        logger.eprint("Caught error '%s'\n", err.c_str());
    }
	return 0;
}
