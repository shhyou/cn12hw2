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

std::string created_file;

void receive_file(channel_t udt) {
    __log;

    size_t filename_len = 1024;
    char *filename = (char*) rcv(udt, filename_len);
    if (filename_len <= 0)
        logger.raise("No filename received.");
    logger.print("Filename %s received.", filename);

    size_t dummy = sizeof(mode_t);
    mode_t *md = (mode_t*) rcv(udt, dummy);
    if (dummy != sizeof(mode_t))
        logger.raise("File permission size %u not valid (excepted %u).", dummy, sizeof(mode_t));
    logger.print("Filemode %d received.", (int) *md);

    dummy = sizeof(off_t);
    off_t *left_len = (off_t*) rcv(udt, dummy);
    logger.print("The file's size is %d bytes.", (int) *left_len);

    std::string new_filename;
#ifdef TEST
    new_filename += "bucket/";
#endif
    new_filename += filename;

    int fd = open(new_filename.c_str(), O_WRONLY | O_CREAT | O_EXCL, *md);
    if (fd < 0)
        throw logger.errmsg("Counldn't create '%s'", new_filename.c_str());
    else
        logger.print("File created.", filename);

    created_file = new_filename;

    void *buf;
    for (size_t chunk_len; *left_len > 0; *left_len -= chunk_len) {
        chunk_len = *left_len;
        buf = rcv(udt, chunk_len);
        write(fd, buf, chunk_len);
        free(buf);
    }

    close(fd);

    free(filename);
    free(md);
    free(left_len);

    created_file = "";

    logger.print("File '%s' successfully received.", new_filename.c_str());
    logger.print("Waiting for next file.");
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

        logger.print("Listening on port %d", atoi(argv[1]));

        while (true) receive_file(udt);

    } catch (const std::string& err) {
        logger.eprint("%s, program terminated.", err.c_str());

        int errno_tmp = errno;
        if (created_file.size() != 0)
            remove(created_file.c_str());
        errno = errno_tmp;
                
    }
    return 0;
}
