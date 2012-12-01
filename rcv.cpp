/* Receiver code, receiving file using rdt and writing it */
#include <cstdio>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "udt.h"
#include "rdt.h"
#include "log.h"

void receive_file(channel_t udt) {
    __log;

    size_t filename_len;
    char *filename = (char*) rcv(udt, filename_len);
    if (filename_len <= 0)
        logger.raise("No filename received.");

    size_t dummy;
    mode_t *md = (mode_t*) rcv(udt, dummy);
    if (dummy != sizeof(mode_t))
        logger.raise("File permission size not valid.");

    off_t *left_len = (off_t*) rcv(udt, dummy);

    int fd = open(filename, O_WRONLY | O_APPEND | O_CREAT | O_EXCL, *md);
    if (fd < 0)
        throw logger.errmsg("Counldn't create '%s'", filename);
    else
        logger.print("File '%s' created.", filename);

    void *buf;
    for (size_t chunk_len; *left_len > 0; *left_len -= chunk_len) {
        buf = rcv(udt, chunk_len);
        write(fd, buf, chunk_len);
        free(buf);
    }

    close(fd);

    free(filename);
    free(md);
    free(left_len);

    logger.print("File '%s' successfully received.\n", filename);
}

int main(int argc, char *argv[]) {
    __log;
    
    if (argc == 1) {
        puts("Usage:");
        puts("./rcv listen_port");
        puts("Receives files to current directory.");
        return 0;
    }

    try {
        channel_t udt = udt_new(atoi(argv[1]));

        while (true) receive_file(udt);

    } catch (const std::string& err) {
        logger.eprint("    %s, program terminated.", err.c_str());
    }
    return 0;
}
