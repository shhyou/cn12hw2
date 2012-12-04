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

    size_t buffer_len = sizeof(int) + filename_len + sizeof(st.st_mode) + sizeof(st.st_size) + st.st_size;
    char* buffer = (char*)malloc(buffer_len);
    char* ptr = buffer;
    *(int*) ptr = filename_len;
    ptr += sizeof(int);

    memcpy(ptr, filename, filename_len);
    ptr += filename_len;

    *(mode_t*) ptr = st.st_mode;
    ptr += sizeof(st.st_mode);

    *(off_t*) ptr = st.st_size;
    ptr += sizeof(st.st_size);

    const int buf_len = 1024;
    for (off_t content_len = 0; content_len < st.st_size; ) {
        ssize_t ret = read(fd, ptr, buf_len);
        if (ret == 0)
            logger.raise("Got EOF earlier than excepted.");
        else if (ret < 0)
            throw logger.errmsg("Failed to read file");
        content_len += ret;
        ptr += ret;
    }

    snd(udt, buffer, buffer_len);
    logger.print("File sent.");

    /* end sending file */

    udt.close();
    free(buffer);
    close(fd);
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

