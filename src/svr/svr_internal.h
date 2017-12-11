#ifndef _SVR_INTERNAL_H
#define _SVR_INTERNAL_H

#include <pthread.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <aio.h>
#include <libaio.h>
#include <pthread.h>
#include <math.h>

#include "../fsvr_internal.h"
#include "../fsvr_log.h"
#include "../fsvr_msg.h"
#include "../fsvr_file.h"

#define PORTNO 9090
#define WTHR_NUM 10
#define STRIPE_UNIT 1048576 /* 1MB단위로 스트라이핑 */
#define MAX_CLIENT 10
#define MAX_FAILOVER 2
#define MAX_FILE 1024
#define MAX_AIO_CNT 10000


#define DIRECTORY "mnt/"
#define METAFILE ".meta"

void* svr_wthr_main(void *arg);
void* svr_sthr_main(void *arg);
void* svr_tthr_main(void *arg);

LOG_EXT

typedef struct pipe_s pipe_t;
struct pipe_s {
    int         rd;
    int         wr;
};

typedef struct svr_thr_arg_s svr_thr_arg_t;
struct svr_thr_arg_s {
    pipe_t      pfd;
};

typedef enum {false, true} bool_t;

enum ipc_type_e {
    IPC_SEND_MSG,   /* 0 */
    IPC_JOB_DIST,   /* 1 */
    IPC_JOB_DONE    /* 2 */
};
typedef enum ipc_type_e ipc_type_t;

struct ipc_msg_s {
    ipc_type_t  type;
    void       *data;
};
typedef struct ipc_msg_s ipc_msg_t;

static void inline
write_ipc(int fd, ipc_type_t type, void* data)
{
    ipc_msg_t *msg = (ipc_msg_t*) malloc(sizeof(ipc_msg_t));
    msg->type = type;
    msg->data = data;
    write(fd, &msg, sizeof(msg));
}

typedef struct svr_thr_s svr_thr_t;
struct svr_thr_s {
    pthread_t   tid;
    int         tnum;
    pipe_t      pfd;        /* wr: cthr->wthr  rd: wthr->cthr */
    bool_t      is_sleep;
};

extern svr_thr_t    svr_thr_wthrs[WTHR_NUM];
extern svr_thr_t    sthr;
extern int         *ipc_pipe;

#endif
