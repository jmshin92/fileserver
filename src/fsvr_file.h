#ifndef _FSVR_FILE_H
#define _FSVR_FILE_H
#include <sys/types.h>
#include <limits.h>

struct fsvr_file_s {
    char        filename[PATH_MAX];
    uint64_t    size;
    char        ownername[32]; /* user name limit */
};
typedef struct fsvr_file_s fsvr_file_t;

#endif
