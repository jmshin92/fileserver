#ifndef _SVR_AIO_H
#define _SVR_AIO_H

#include "svr_internal.h"

typedef struct iocb svr_iocb_t;

int svr_io_suspend(io_context_t ctx, int min, int max, struct io_event *events,
                   int ms);

void svr_prep_io(svr_iocb_t *iocb, int fd, char *buf, size_t size, off_t offset,
                 void *arg, int is_read);

void svr_io_submit(io_context_t ctx, int len, svr_iocb_t **cblist);

#endif
