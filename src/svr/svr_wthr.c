#include "svr_internal.h"
#include "svr_job.h"
#include "svr_meta.h"

struct svr_ctx_s {
    io_context_t    ioctx;
    uint32_t        aio_cnt;
};
typedef struct svr_ctx_s svr_ctx_t;

svr_ctx_t ctx;

static void inline
svr_wthr_send_msg_ipc(svr_conn_t *conn, void *msg)
{
    send_msg_t *ipcmsg = (send_msg_t*) malloc(sizeof(send_msg_t));
    ipcmsg->msg = msg;
    ipcmsg->conn = conn;
    write_ipc(ipc_pipe[1], IPC_SEND_MSG, ipcmsg);
}

static void
svr_wthr_read(svr_job_t *job)
{
    int fd, retry = 0;
    char path[PATH_MAX];
    fsvr_read_reply_t *reply;

  retry:
    sprintf(path, "mnt%d/%s_%zu", retry, job->filename, job->shard);

    LOG("read file:filename %s,size %zu", path, job->size);
    fd = open(path, O_RDONLY|O_SYNC, 0644);

    if (fd < 0) {
        LOG_ERR("cannot open file:path %s,errno %d", path, errno);
        if (errno == EMFILE) {
            /* XXX sleep and retry?*/
            LOG_ERR("too many fd opened");
            goto retry;
        }

        if (++retry < MAX_FAILOVER)
            goto retry;

        reply = (fsvr_read_reply_t*) fsvr_alloc_msg(OP_MSG_READ_REPLY);
        reply->buf      = NULL;
        reply->buflen   = 0;
        reply->ec       = EC_FILE_NOT_FOUND;
        svr_wthr_send_msg_ipc(job->parent->conn, reply);
    } else {
        job->ref_cnt++;
        job->op = OP_JOB_READ_POST;
        job->buf = malloc(job->size);
        job->aio_cnt = 1;
        job->cb[0] = malloc(sizeof(svr_iocb_t));
        svr_prep_io(job->cb[0], fd, job->buf, job->size, 0, job, true);
        svr_io_submit(ctx.ioctx, 1, job->cb);
    }
}

static void
svr_wthr_read_post(svr_job_t *job)
{
    fsvr_read_reply_t *reply;
    printf("read post \n");
    close(job->cb[0]->aio_fildes);
    reply = (fsvr_read_reply_t*) fsvr_alloc_msg(OP_MSG_READ_REPLY);
    reply->buflen   = job->size;
    reply->offset   = job->offset;
    reply->ec       = EC_NONE;
    reply->buf      = job->buf;

    job->buf        = NULL;
    svr_wthr_send_msg_ipc(job->parent->conn, reply);
}

static void
svr_wthr_write(svr_job_t *job)
{
    int fd, i;
    fsvr_file_t *file;
    char path[PATH_MAX];

    LOG("write file:fid %zu", job->fid);
    file    = &svr_meta_get_elem_by_fid(job->fid)->file;

    job->op = OP_JOB_WRITE_POST;
    job->ref_cnt++;

    for (i = 0; i < MAX_FAILOVER; i++) {
        sprintf(path, "mnt%d/%s_%zu", i, file->filename, job->shard);

        fd = open(path, O_CREAT|O_WRONLY|O_SYNC|O_TRUNC, 0644);

        LOG("write file:filename %s,size %zu, fd %d", path, job->size, fd);
        if (fd < 0) {
            LOG_ERR("cannot open file:fd %d,errno %d", fd, errno);
            if (errno == EMFILE) {
                LOG_ERR("too many file opened");
                i--;
            }
        } else {
            job->cb[i] = malloc(sizeof(svr_iocb_t));
            svr_prep_io(job->cb[i], fd, job->buf, job->size, 0, job, false);
            job->aio_cnt++;
        }
    }
    svr_io_submit(ctx.ioctx, job->aio_cnt, job->cb);
}

static void
svr_wthr_write_post(svr_job_t *job)
{
    int                 i, ret;
    fsvr_write_reply_t  *reply;

    /* close fds */
    for (i = 0; i < MAX_FAILOVER; i++) {
        ret = close(job->cb[i]->aio_fildes);
        if (ret < 0) {
            LOG_ERR("close failed:ret %d", ret);
        }
    }

    svr_meta_done_write(job->fid);

    reply = (fsvr_write_reply_t*) fsvr_alloc_msg(OP_MSG_WRITE_REPLY);
    reply->ec = EC_NONE;
    svr_wthr_send_msg_ipc(job->conn, reply);
}

static void
svr_wthr_list(svr_job_t *job)
{
    fsvr_list_file_reply_t  *reply;
    file_elem_t            *elem = NULL;
    fsvr_file_t            *file = NULL;
    char                   *buf;

    reply = (fsvr_list_file_reply_t*) malloc(sizeof(fsvr_list_file_reply_t));
    reply->op       = OP_MSG_LIST_FILE_REPLY;
    reply->msglen   = sizeof(fsvr_list_file_reply_t);
    reply->cnt      = svr_meta_list_get_cnt();

    if (reply->cnt == 0)
        goto send_list_reply;

    reply->buflen   = reply->cnt * sizeof(fsvr_file_t);
    reply->buf      = malloc(reply->buflen);
    memset(reply->buf, 0, reply->buflen);

    buf = reply->buf;
    while ((elem = svr_meta_list_get_next(elem)) != NULL) {
        file = &elem->file;
        LOG("list file:path %s,size %zu,owner %s",
            file->filename, file->size, file->ownername);
        memcpy(buf, file, sizeof(fsvr_file_t));
        buf += sizeof(fsvr_file_t);
    }

  send_list_reply:
    svr_wthr_send_msg_ipc(job->conn, reply);
}


static void
svr_wthr_get_file(svr_job_t *job)
{
    fsvr_get_file_reply_t   *reply;
    file_elem_t            *elem;
    uint64_t i;
    uint64_t offset = 0;

    reply = (fsvr_get_file_reply_t*) fsvr_alloc_msg(OP_MSG_GET_FILE_REPLY);
    reply->ec       = EC_NONE;

    elem = svr_meta_get_elem_by_name(job->filename);

    if (!elem || elem->on_write > 0) {
        reply->ec = EC_FILE_NOT_FOUND;
        goto send_get_reply;
    }
    job->file       = &elem->file;

    memcpy(&reply->file, job->file, sizeof(fsvr_file_t));

    job->num_child = ceil((double) job->file->size / STRIPE_UNIT);
    job->child_jobs = (svr_job_t**) malloc(sizeof(svr_job_t*) * job->num_child);

    for (i = 0; offset < job->file->size; i++) {
        job->ref_cnt++;
        job->child_jobs[i]              = svr_job_alloc();
        job->child_jobs[i]->op          = OP_JOB_READ;
        job->child_jobs[i]->filename    = job->filename;
        job->child_jobs[i]->parent      = job;
        job->child_jobs[i]->shard       = i;
        job->child_jobs[i]->offset      = offset;
        job->child_jobs[i]->size = (offset + STRIPE_UNIT > job->file->size ?
                                job->file->size - offset : STRIPE_UNIT);
        LOG("create child job:job %p,%zu th, offset %zu", job->child_jobs[i], i, offset);
        offset += STRIPE_UNIT;
    }

  send_get_reply:
    LOG("send msg:msg %p, conn %p", reply, job->conn);
    svr_wthr_send_msg_ipc(job->conn, reply);
}

/* put msg를 받아서 meta 정보를 업데이트하고 해당 파일의 file ID를 답장*/
static void
svr_wthr_put_file(svr_job_t *job)
{
    uint64_t                fid;
    uint64_t                num_shard;
    fsvr_put_file_reply_t  *reply;

    num_shard = ceil((double)job->file->size / STRIPE_UNIT);
    fid = svr_meta_create_elem(job->file, num_shard);
    job->ref_cnt++;

    reply = (fsvr_put_file_reply_t*) fsvr_alloc_msg(OP_MSG_PUT_FILE_REPLY);
    reply->ec   = (fid == FID_NULL ? EC_FILE_ALREADY_EXIST : EC_NONE);
    reply->fid  = fid;
    svr_wthr_send_msg_ipc(job->conn, reply);
}

static void
handle_job(svr_job_t *job)
{
    LOG("handle job:job %p,op %d", job, job->op);
    switch (job->op) {
    case OP_JOB_READ:
        svr_wthr_read(job);
        break;
    case OP_JOB_READ_POST:
        svr_wthr_read_post(job);
        break;
    case OP_JOB_WRITE:
        svr_wthr_write(job);
        break;
    case OP_JOB_WRITE_POST:
        svr_wthr_write_post(job);
        break;
    case OP_JOB_LIST:
        svr_wthr_list(job);
        break;
    case OP_JOB_GET_FILE:
        svr_wthr_get_file(job);
        break;
    case OP_JOB_PUT_FILE:
        svr_wthr_put_file(job);
        break;
    default:
        LOG_ERR("Unknown job op");
    }
}

void*
svr_wthr_main(void *arg)
{
    svr_job_t      *job;
    svr_thr_arg_t  *thr_arg = (svr_thr_arg_t*) arg;
    pipe_t         *pfd = &thr_arg->pfd;
    struct pollfd   cthr_fd[1];

    cthr_fd[0].fd = pfd->rd;
    cthr_fd[0].events = POLLIN;

    LOG("ipc pipe test:wr %d", ipc_pipe[1]);
    LOG("wthr pipe: rd %d", pfd->rd);

    while(1) {
        poll(cthr_fd, 1, 1000);

        if (cthr_fd[0].revents & POLLIN) {
            read(pfd->rd, &job, sizeof(job));

            if (job == NULL)
                continue;

            handle_job(job);
            write_ipc(pfd->wr, IPC_JOB_DONE, job);
        }
        job = NULL;
    }
}

void*
svr_sthr_main(void *arg)
{
    int                 i, ret;
    svr_job_t          *job;
    struct io_event    *events;
    svr_thr_arg_t      *thr_arg = (svr_thr_arg_t*) arg;
    pipe_t             *pfd = &thr_arg->pfd;

    ctx.aio_cnt = 0;

    if (io_setup(MAX_AIO_CNT, &ctx.ioctx) != 0) {
        LOG_ERR("AIO setup failed:errno %d", errno);
        exit(-1);
    }

    LOG("sthr pipe: rd %d", pfd->rd);
    LOG("sthr:ioctx %p", &ctx);

    events = (struct io_event*) malloc(sizeof(struct io_event) * MAX_AIO_CNT);

    while(1) {
        ret = svr_io_suspend(ctx.ioctx, 1, MAX_AIO_CNT, events, 3000);

        if (ret < 0) {
            LOG_ERR("svr_io_expend failed:ret %d", ret);
            io_destroy(ctx.ioctx);
            return NULL;
        }

        for (i = 0; i < ret; i++) {
            job = (svr_job_t*) events[i].data;

            if (--job->aio_cnt <= 0) {
                LOG("aio job suspended:job %p,op %d", job, job->op);
                write_ipc(pfd->wr, IPC_JOB_DIST, job);
            }
        }
    }
}
