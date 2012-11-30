#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../log.h"
#include "../udt.h"

using std::string;

int main(int argc, char *argv[]) {
	__log;

	int listen_port = 18000;
	if (argc >= 2) listen_port = atoi(argv[1]);
    channel_t cnl = udt_new(listen_port, NULL);

	char buf[1808];	int jizz, cnt = 0;
	while (true) {
        try {
            jizz = cnl.recv(buf, 1808);
        } catch (const string& err) {
            puts(err.c_str()); break;
        }
        if (jizz < 0) break;
        if (jizz == 0)
            if (++cnt > 20) break;
            else puts("Timecheck");
        else
            printf("%d => %s\n", jizz, buf + 1), cnt = 0;
    }
	return 0;
}
