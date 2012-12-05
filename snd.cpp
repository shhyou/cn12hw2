/* Sender code, reading file and sending using rdt */
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <string>

#include "udt.h"
#include "rdt.h"
#include "log.h"

/* 240 is just a loose upper-bound, can change if needed */
const unsigned int filename_ubound = 1024;

void send_file(const char *address, const int port, const char *pathname) {
    __log;
    
    int fd = open(pathname, O_RDONLY);
    if (fd < 0)
        throw logger.errmsg("Failed to open file");

    struct stat st;
    if (fstat(fd, &st) != 0)
        throw logger.errmsg("Failed to get status of '%s'", pathname);

    channel_t udt = udt_new(port, address);

    /* send filename */
    const char *filename = strrchr(pathname, '/');
    if (filename == NULL) filename = pathname;
    else filename += 1;

    size_t filename_len = strlen(filename) + 1;
    if (filename_len >= filename_ubound)
        logger.print("Filename '%s' is too long, will be truncated.", filename, filename_ubound);

    snd(udt, filename, filename_len);
    logger.print("Filename sent.");
    
    /* send file protection */
    snd(udt, &st.st_mode, sizeof(st.st_mode));
    logger.print("File permission sent.");

    /* send filesize */
    snd(udt, &st.st_size, sizeof(st.st_size));
    logger.print("%s's size is %d bytes.", filename, st.st_size);

    /* send filedata */
    void *file_content;
    off_t content_len;
    /* Modify below if mmap crashes */
    file_content = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_content == MAP_FAILED)
        throw std::string("Failed to read file into memory");
    content_len = st.st_size;

    snd(udt, file_content, content_len);
    logger.print("File content sent.");

    munmap(file_content, st.st_size);
    /* end sending file */

    udt.close();
    close(fd);
}

int main(int argc, char *argv[]) {
    __log;
    
    if (argc < 4) {
        puts("Usage:");
        puts("./snd target_hostname target_port filename");
        return 0;
    }

    try {
        send_file(argv[1], atoi(argv[2]), argv[3]);

#ifdef TEST
        std::string cmd = "diff ";
        cmd += argv[3];
        cmd += " bucket/";
        cmd += argv[3];
        logger.print("%s", cmd.c_str());
        system(cmd.c_str());
#endif
    } catch (const std::string& err) {
        logger.eprint("%s, program terminated.", err.c_str());
    }
    
    return 0;
}

