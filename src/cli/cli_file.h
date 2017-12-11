#ifndef _CLI_FILE_H
#define _CLI_FILE_H

#include "cli_internal.h"

int cli_open_file(char *filename, int read, int *fd, char *ownername,
                  size_t *size);
void cli_read_file(int fd, char *buf, size_t size, off_t offset);
void cli_write_file(int fd, char *buf, size_t size, off_t offset);

#endif
