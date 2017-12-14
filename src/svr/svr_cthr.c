#include "svr_internal.h"
#include "svr_tcp.h"
#include "svr_job.h"
#include "svr_meta.h"

#define FD_MAX_SIZE 32

LOG_DEC

svr_thr_t wthrs[WTHR_NUM];      /* working thread */
svr_thr_t sthr;                 /* suspend thread */
int       *ipc_pipe;

int
svr_thread_create(svr_thr_t *thr, int tnum, void *(*start_routine) (void *))
{
    int ret, wpipe[2], rpipe[2];
    svr_thr_arg_t *thr_arg;

    thr_arg = (svr_thr_arg_t*) malloc(sizeof(svr_thr_arg_t));

    if (pipe(wpipe) == -1 || pipe(rpipe) == -1) {
        LOG_ERR("failed to create pipe fd:tnum %d,errno %d", tnum, errno);
        goto err_out;
    }

    thr->tnum       = tnum;
    thr->pfd.wr     = wpipe[1]; /* cthr(wr) ->  thr      */
    thr_arg->pfd.rd = wpipe[0]; /* cthr     ->  thr(rd)  */
    thr->pfd.rd     = rpipe[0]; /* thr      ->  cthr(rd) */
    thr_arg->pfd.wr = rpipe[1]; /* thr(wr)  ->  cthr     */
    LOG("thr rd %d, wr %d", thr_arg->pfd.rd, thr_arg->pfd.wr);
    ret = pthread_create(&thr->tid, NULL, start_routine, thr_arg);
    if (ret != 0) {
        LOG_ERR("failed to create thread:tnum %d,errno %d", tnum, errno);
        goto err_out;
    }

    return 0;

  err_out:
    free(thr_arg);
    return -1;
}

int
svr_cthr_create_peer_thrs() {
    int i;

    for (i = 0; i < WTHR_NUM; i++) {
        svr_thread_create(&wthrs[i], i, &svr_wthr_main);
        wthrs[i].is_sleep = true;
    }

    svr_thread_create(&sthr, ++i, &svr_sthr_main);
    return 0;
}

int
svr_cthr_init() {
    ipc_pipe = (int*) malloc(sizeof(int) *  2);

    if (pipe(ipc_pipe) == -1) {
        LOG_ERR("failed to create ipc pipe fd:errno %d", errno);
        return -1;
    }

    LOG("ipc pipe created:rd %d,wr %d", ipc_pipe[0], ipc_pipe[1]);

    return svr_cthr_create_peer_thrs();
}

svr_job_t*
svr_cthr_recv_msg(svr_conn_t *conn) {
    fsvr_msg_hdr_t *hdr;
    svr_job_t *job;
    hdr = svr_recv_msg(conn);
    if (hdr == NULL)
        return NULL;
    LOG("recved msg:buflen %d", hdr->buflen);
    job = svr_job_init(hdr);
    job->conn = conn;
    return job;
}

void
svr_cthr_distribute_job(svr_job_t *job) {
    int i;

    /* distribute job */
    for (i = 0; i < WTHR_NUM; i++) {
        if (wthrs[i].is_sleep == true) {
            printf("job to wthrs[%d]\n", i);
            wthrs[i].is_sleep = false;
            LOG("gave %d thr, job %p", i, job);
            write(wthrs[i].pfd.wr, &job, sizeof(job));
            return;
        }
    }

    svr_jobq_enq(job);
}

static void
svr_cthr_send_msg(send_msg_t *send_msg)
{
    fsvr_reply_hdr_t       *msg = (fsvr_reply_hdr_t*) send_msg->msg;
    fsvr_read_reply_t      *read_reply;
    fsvr_list_file_reply_t *list_reply;

    LOG("send msg:msg %p,op %d, ec %d, len %d",msg, msg->op, msg->ec, msg->msglen);

    switch (msg->op) {
    case OP_MSG_GET_FILE_REPLY:
    case OP_MSG_PUT_FILE_REPLY:
    case OP_MSG_WRITE_REPLY:
        svr_send_msg(send_msg->conn, (char*) msg, msg->msglen, NULL, 0);
        fsvr_msg_free(msg, 0);
        break;

    case OP_MSG_LIST_FILE_REPLY:
        list_reply = (fsvr_list_file_reply_t*) msg;
        svr_send_msg(send_msg->conn, (char*) list_reply, list_reply->msglen,
                     list_reply->buf, list_reply->buflen);
        fsvr_msg_free(list_reply, 1);
        break;
    case OP_MSG_READ_REPLY:
        read_reply = (fsvr_read_reply_t*) msg;
        svr_send_msg(/*job->parent*/send_msg->conn, (char*) read_reply, read_reply->msglen,
                     read_reply->buf, read_reply->buflen);
        fsvr_msg_free(read_reply, 1);
        break;
    default:
        break;
    }
    free(send_msg);
}

static void
svr_cthr_handle_job(svr_job_t *job)
{
    int i = 0;

    if (job->num_child > 0) {
        for (i = 0; i < job->num_child; i++)
            svr_cthr_distribute_job(job->child_jobs[i]);
    }

    svr_job_free(job);
}

void
svr_cthr_main()
{
    svr_conn_t     *conn[MAX_CLIENT] = { NULL };
    struct pollfd   fdset[MAX_CLIENT + WTHR_NUM + 2];
    int             nreads, maxfd, i, soff, maxconn;
    svr_job_t      *job;
    ipc_msg_t      *ipc_msg;

    /* polling fd for peer threads */
    for (i = 0; i < WTHR_NUM; i++) {
        fdset[i].fd = wthrs[i].pfd.rd;
        fdset[i].events = POLLIN;
    }

    fdset[i].fd = sthr.pfd.rd;
    fdset[i].events = POLLIN;

    fdset[i+1].fd = ipc_pipe[0];
    fdset[i+1].events = POLLIN;

    maxconn = 0;
    soff = i + 2;
    maxfd = i + 2;
    fdset[soff].fd = svr_sock->sockfd;
    fdset[soff].events = POLLIN;
    printf("Server init done! fd num %d\n", maxfd);
    while (1) {
        nreads = poll(fdset, maxfd + 1, -1);
        /* new client */
        if (fdset[soff].revents & POLLIN) {
            printf("new connection!\n");
            for (i = 0; i < MAX_CLIENT; i++) {
                if (conn[i] == NULL) {
                    conn[i] = accept_new_conn();
                    break;
                }
            }

            if (conn[i] == NULL)
                continue;

            if (i > MAX_CLIENT) {
                LOG_ERR("too many connections");
                continue;
            }

            fdset[soff + i + 1].fd = conn[i]->fd;
            fdset[soff + i + 1].events = POLLIN;

            if (soff + i + 1 > maxfd)
                maxfd = soff + i + 1;

            if (i + 1 > maxconn)
                maxconn = i + 1;

            if (--nreads <= 0)
                continue;
        }

        /* client socket: read msg from client*/
        for (i = 0; i < maxconn; i++) {
            if (nreads <= 0)
                break;

            if (conn[i] == NULL)
                continue;

            if (fdset[soff + i + 1].revents & (POLLIN | POLLERR)) {
                nreads--;
                job = svr_cthr_recv_msg(conn[i]);

                if (job == NULL) {
                    conn[i] = NULL;
                    fdset[soff + i + 1].fd = -1;
                    continue;
                }
                svr_cthr_distribute_job(job);
                job = NULL;
            }
        }

        /* recv msg from ipc pipe */
        if (fdset[WTHR_NUM+1].revents & (POLLIN | POLLERR)) {
            read(ipc_pipe[0], &ipc_msg, sizeof(ipc_msg));
            if (ipc_msg->type != IPC_SEND_MSG) {
                LOG_ERR("wrong ipc msg:op %d", ipc_msg->type);
            } else {
                svr_cthr_send_msg((send_msg_t*) ipc_msg->data);
                free(ipc_msg);
            }
            ipc_msg = NULL;
        }

        /* event from working threads */
        for (i = 0; i < WTHR_NUM; i++) {
            if (fdset[i].revents & (POLLIN | POLLERR)) {
                read(wthrs[i].pfd.rd, &ipc_msg, sizeof(ipc_msg));

                switch (ipc_msg->type) {
                case IPC_SEND_MSG:
                    svr_cthr_send_msg((send_msg_t*) ipc_msg->data);
                    break;
                case IPC_JOB_DONE:
                    svr_cthr_handle_job((svr_job_t*) ipc_msg->data);
                    job = svr_jobq_deq();
                    LOG("job done:%d wthr, job %p, next job %p", i, ipc_msg->data, job);
                    if (job == NULL)
                        wthrs[i].is_sleep = true;
                    else
                        write(wthrs[i].pfd.wr, &job, sizeof(job));
                    break;
                default:
                    break;
                }

                free(ipc_msg);
                ipc_msg = NULL;
            }
            job = NULL;
        }

        /* events from suspend thread*/
        if (fdset[WTHR_NUM].revents & (POLLIN | POLLERR)) {
            read(sthr.pfd.rd, &ipc_msg, sizeof(ipc_msg));
            if (ipc_msg->type != IPC_JOB_DIST) {
                LOG_ERR("not supported ipc msg:wanted %d,op %d", IPC_JOB_DIST, ipc_msg->type);
            } else {
                svr_cthr_distribute_job((svr_job_t*) ipc_msg->data);
            }
            free(ipc_msg);
            ipc_msg = NULL;
            job = NULL;
        }
    }
}

int
main()
{
    LOG_INIT("log/svr_log.txt");

    /* 저장된 메타 파일을 읽음 */
    svr_meta_list_init();
    /* job queue 초기화 */
    svr_jobq_init();
    /* socket을 열어서 클라이언트 받을 준비 */
    svr_sock_init();

    /* worker thread, suspend thread 생성 후 메세지 받음 */
    if (!svr_cthr_init())
        svr_cthr_main();

    return 0;
}
