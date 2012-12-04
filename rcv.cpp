/* Receiver code, receiving file using rdt and writing it */
#include <cstdio>
#include <cstring>
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

    size_t max_filesize = 0x3FFFFFFF;
    char *buffer = (char*) rcv(udt, max_filesize);
    if (buffer == NULL)
        logger.raise("rcv returned NULL");
    char *ptr = buffer;

    int filename_len = *(int*) ptr;
    ptr += sizeof(int);

    char filename[filename_len];
    strcpy(filename, ptr);
    ptr += filename_len;
    if (filename_len <= 0)
        logger.raise("No filename received.");
    logger.print("Filename %s received.", filename);

    mode_t md = *(mode_t*) ptr;
    ptr += sizeof(mode_t);
    logger.print("The file's permission is %u", (int) md);

    off_t left_len = *(off_t*) ptr;
    ptr += sizeof(off_t);
    logger.print("The file's size is %d bytes.", (int) left_len);

    std::string new_filename;
#ifdef TEST
    new_filename += "bucket/";
#endif
    new_filename += filename;

    int fd = open(new_filename.c_str(), O_WRONLY | O_CREAT | O_EXCL, md);
    if (fd < 0)
        throw logger.errmsg("Counldn't create '%s'", filename);
    else
        logger.print("File created.", filename);

    created_file = new_filename;

    while (left_len > 0) {
        ssize_t ret = write(fd, ptr, left_len);
        if (ret < 0)
            throw logger.errmsg("Failed to write file into disk");
        ptr += ret;
        left_len -= ret;
    }

    close(fd);

    free(buffer);

    created_file = "";

    logger.print("File '%s' successfully received.", new_filename.c_str());
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

        receive_file(udt);

    } catch (const std::string& err) {
        logger.eprint("%s, program terminated.", err.c_str());

        int errno_tmp = errno;
        if (created_file.size() != 0)
            remove(created_file.c_str());
        errno = errno_tmp;
                
    }
    return 0;
}
