#ifndef _SVR_JOB_H
#define _SVR_JOB_H
#include "svr_internal.h"
#include "svr_tcp.h"
#include "svr_aio.h"

enum job_op_e {
    OP_JOB_READ = 0,
    OP_JOB_READ_POST,           /* 1 */
    OP_JOB_WRITE,               /* 2 */
    OP_JOB_WRITE_POST,          /* 3 */
    OP_JOB_LIST,                /* 4 */
    OP_JOB_LIST_POST,           /* 5 */
    OP_JOB_GET_FILE,            /* 6 */
    OP_JOB_GET_FILE_POST,       /* 7 */
    OP_JOB_PUT_FILE,            /* 8 */
    OP_JOB_PUT_FILE_POST        /* 9 */
};
typedef enum job_op_e job_op_t;

struct svr_job_s {
    struct svr_job_s   *next;
    struct svr_job_s   *parent;         /* for child */
    struct svr_job_s  **child_jobs;     /* for parent */
    int                 num_child;
    job_op_t            op;
    fsvr_file_t        *file;           /* put */
    char               *filename;       /* get */
    char               *buf;
    uint64_t            size;           /* write */
    uint64_t            shard;
    uint64_t            fid;
    int                 ref_cnt;
    int                 ec;
    uint64_t            offset;
    svr_conn_t         *conn;
    void               *msg;            /* received msg */

    int                 aio_cnt;
    svr_iocb_t         *cb[MAX_FAILOVER];
};
typedef struct svr_job_s svr_job_t;

struct svr_jobq_s {
    queue_t            *q;
};
typedef struct svr_jobq_s svr_jobq_t;

extern svr_jobq_t *jobq;

void svr_jobq_init();
void svr_jobq_enq(svr_job_t *job);
svr_job_t* svr_jobq_deq();

svr_job_t* svr_job_alloc();
svr_job_t* svr_job_init(fsvr_msg_hdr_t *hdr);
void svr_job_free(svr_job_t *job);
#endif
