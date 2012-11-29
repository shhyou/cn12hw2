#include <cstdio>
#include <cstring>
#include "../udt.h"

int main(int argc, char *argv[]) {
    channel_t cnl = udt_new(18018, argc > 1 ? argv[1] : "127.0.0.1");  
	char buf[180];
	for (int i = 1; i <= 1000; ++i) {
        sprintf(buf + 1, "%d", i);
        buf[0] = strlen(buf + 1) + 1;
        cnl.send(buf, buf[0] + 1);
	}
	return 0;
}
