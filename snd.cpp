/* Sender code, reading file and sending using rdt */
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mmap.h>
#include <errno.h>

#include "udt.h"
#include "rdt.h"
#include "log.h"

/* 240 is just a loose upper-bound, can change if needed */
const int filename_ubound = 240;

void send_file(const char *address, const int port, const char *pathname) {
    __log;
    
    int fd = open(pathname, O_RDONLY);
    if (fd < 0)
        throw logger.errmsg("Failed to open file");

    stat st;
    if (fstat(fd, &st) != 0)
        throw logger.errmsg("Failed to get status of '%s'", pathname);

    channel_t udt = udt_new(port, address);

    /* send filename */
    const char *filename = strrchr(pathname, '/');
    if (filename == NULL) filename = pathname;
    else filename += 1;
    
    size_t filename_len = strlen(filename);
    if (filename_len >= filename_ubound)
        logger.print("Filename '%s' is too long, will be truncated to %d\n", filename, filename_ubound);

    snd(udt, filename, filename_len);
    
    /* send filesize */
    snd(udt, &st.st_size, sizeof(st.st_size));

    /* send filedata */
    void *file_content;
    off_t content_len;
    /* Modify below if mmap crashes */
    file_content = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_content == MAP_FAILED)
        throw logger.eprint("Failed to read file into memory");
    content_len = st.st_size;

    snd(udt, file_content, content_len);

    munmap(file_content, st.st_size);
    /* end sending file */

    udt.close();
}

int main(int argc, char *argv[]) {
    __log;
    
    if (argc < 4) {
        puts("Usage:");
        puts("./snd target_ip target_port filename");
        return 0;
    }

    try {
        send_file(argv[1], atoi(argv[2]), argv[3]);
    } catch (const string& err) {
        logger.eprint("Caught error '%s'\n", err.c_str());
        logger.eprint("Program terminated.\n");
    }
    
    return 0;
}

