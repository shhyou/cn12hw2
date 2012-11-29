#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../udt.h"

int main(int argc, char *argv[]) {
	int listen_port = 18000;
	if (argc >= 2) listen_port = atoi(argv[1]);
    channel_t cnl = udt_new(listen_port, NULL);

	char buf[1808];	int jizz;
	while ((jizz = cnl.recv(buf, 1808)) != -1)
		printf("%d => %s\n", jizz, buf + 1);
	return 0;
}
