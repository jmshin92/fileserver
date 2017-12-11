#include "svr_aio.h"

static struct timespec *
ms2ts(int ms, struct timespec *ts)
{
    ts->tv_sec = ms / 1000;
    ts->tv_nsec = (ms % 1000) * 1000000;
    return ts;
}

int
svr_io_suspend(io_context_t ctx, int min, int max, struct io_event *events,
               int ms)
{
    int ret;
    struct timespec ts;

  retry:
    ret = io_getevents(ctx, min, max, events, ms2ts(ms, &ts));

    if (ret < 0) {
        if (ret == -EINTR)
            goto retry;
        LOG_ERR("Failed to suspend aio:err %d", ret);
    }

    return ret;
}

void
svr_prep_io(svr_iocb_t *iocb, int fd, char *buf, size_t size, off_t offset,
            void *data, int is_read)
{
    if (is_read)
        io_prep_pread(iocb, fd, buf, size, offset);
    else
        io_prep_pwrite(iocb, fd, buf, size, offset);

    iocb->data = data;
}

void
svr_io_submit(io_context_t ctx, int len, svr_iocb_t **cblist)
{
    int ret;

    while((ret = io_submit(ctx, len, (struct iocb **) cblist)) < len) {
        if (ret > 0)
            continue;
        if (errno == EAGAIN)
            continue;
        LOG_ERR("failed to submit io:err %d %d", ret, errno);
        break;
    }

    LOG("aio submit:ret %d, len %d", ret, len);
}


