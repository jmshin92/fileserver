#include "cli_file.h"

/*
 * PARAMS
 * in: filename, read
 * out: fd, ownername, size
 **/
int
cli_open_file(char *filename, int read, int* fd, char *ownername, size_t *size)
{
    char            path[PATH_MAX];
    struct stat     stat_buf;
    struct passwd  *pwd;

    if (read) {
        if ((*fd = open(filename, O_RDONLY)) == -1) {
            LOG_ERR("cannot open file:filename %s,errno %d", filename, errno);
            *fd = -1;
            return errno;
        }
        stat(filename, &stat_buf);
        *size = stat_buf.st_size;
        pwd = getpwuid(stat_buf.st_uid);
        strcpy(ownername, pwd->pw_name);
    } else {
        sprintf(path, "download/%s", filename);
        if ((*fd = open(path, O_CREAT|O_WRONLY|O_TRUNC,
                        S_IRUSR|S_IWUSR|S_IROTH)) == -1) {
            LOG_ERR("cannot open file:filename %s,errno %d", filename, errno);
            *fd = -1;
            return errno;
        }
    }

    LOG("opend a file:name %s,read %d", filename, read);

    return 0;
}

void
cli_write_file(int fd, char *buf, size_t size, off_t offset)
{
    ssize_t bytes_written;

    bytes_written = 0;

    LOG("write file:fd %d,size:%zu, offset: %zu",
        fd, size, offset);

    while (bytes_written < size) {
        bytes_written = pwrite(fd, buf, size, offset);

        LOG("written %zd bytes[remain %zu]", bytes_written, size- bytes_written);
    }
    LOG("written %zd", bytes_written);
}

void
cli_read_file(int fd, char *buf, size_t size, off_t offset)
{
    int bytes_read, ret;

    bytes_read = 0;

    LOG("read file:fd %d,size:%zu, offset: %zu",
        fd, size, offset);

    while(bytes_read < size) {
        ret = pread(fd, buf + bytes_read, size - bytes_read,
                    offset + bytes_read);
        bytes_read += ret;
    }
}

