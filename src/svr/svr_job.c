#include "svr_job.h"

svr_jobq_t *jobq;
int total;

void
svr_jobq_init()
{
    jobq = (svr_jobq_t*) malloc(sizeof(svr_jobq_t));
    jobq->q = (queue_t*) malloc(sizeof(queue_t));
    queue_init(jobq->q);
    total = 0;

    LOG("jobq init done!");
}

void
svr_jobq_enq(svr_job_t *job)
{
    enqueue(jobq->q, job);
}

svr_job_t*
svr_jobq_deq()
{
    svr_job_t *job;

    job = (svr_job_t *) dequeue(jobq->q);
    return job;
}

svr_job_t*
svr_job_init(fsvr_msg_hdr_t *hdr)
{
    svr_job_t *job;
    fsvr_write_msg_t *write_msg;
    fsvr_put_file_msg_t *put_msg;
    fsvr_get_file_msg_t *get_msg;

    job = svr_job_alloc();

    LOG("job allocation:job %p,hdr %p,op %d", job, hdr, hdr->op);
    job->msg = hdr;
    switch(hdr->op) {
    case OP_MSG_READ:
        break;
    case OP_MSG_WRITE:
        write_msg       = (fsvr_write_msg_t*) hdr;
        job->op         = OP_JOB_WRITE;
        job->fid        = write_msg->fid;
        job->shard      = write_msg->shard;
        job->buf        = (char *) write_msg + write_msg->msglen;
        job->size       = write_msg->buflen;
        break;
    case OP_MSG_LIST_FILE:
        job->op         = OP_JOB_LIST;
        break;
    case OP_MSG_GET_FILE:
        get_msg         = (fsvr_get_file_msg_t*) hdr;
        job->op         = OP_JOB_GET_FILE;
        job->filename   = get_msg->filename;
        break;
    case OP_MSG_PUT_FILE:
        put_msg         = (fsvr_put_file_msg_t*) hdr;
        job->op         = OP_JOB_PUT_FILE;
        job->file       = &put_msg->file;
        break;
    default:
        LOG_ERR("unhandled msg op:op %d", hdr->op);
        return NULL;
        break;
    }

    job->msg = hdr;
    return job;
}

svr_job_t*
svr_job_alloc()
{
    int i;
    svr_job_t *job;

    job = (svr_job_t*) malloc(sizeof(svr_job_t));
    job->next       = NULL;
    job->parent     = NULL;
    job->child_jobs = NULL;
    job->msg        = NULL;
    job->ref_cnt    = 1;
    job->ec         = 0;
    job->aio_cnt    = 0;
    job->num_child  = 0;

    for (i = 0; i < MAX_FAILOVER; i++) {
        job->cb[i] = NULL;
    }
    total++;
    return job;
}

void
svr_job_free(svr_job_t* job)
{
    int i;

    if (--job->ref_cnt > 0)
        return;

    if (job->parent != NULL)
        svr_job_free(job->parent);

    if (job->msg) {
        LOG("free msg");
        free(job->msg);
    }

    for (i = 0; i < MAX_FAILOVER; i++) {
        if (job->cb[i]) {
            LOG("free cb");
            free(job->cb[i]);
        }
    }

    total--;
    printf("job free: %d\n", total);
    LOG("free job %p", job);
    free(job);
}
