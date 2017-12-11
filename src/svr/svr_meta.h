#ifndef _SVR_META_H
#define _SVR_META_H

#include "svr_internal.h"
#include "svr_job.h"

struct file_elem_s {
    fsvr_file_t         file;
    uint64_t            fid;
    struct file_elem_s *next;
    uint64_t            on_write;
    pthread_mutex_t     lock;
};
typedef struct file_elem_s file_elem_t;

struct file_list_s {
    file_elem_t        *head;
    file_elem_t        *tail;
    uint32_t            cnt;
    pthread_mutex_t     lock;
};
typedef struct file_list_s file_list_t;

struct file_pool_s {
    file_elem_t       **files;
    int                 cnt;
    pthread_mutex_t     lock;
};
typedef struct file_pool_s file_pool_t;

void svr_meta_list_init();

uint64_t svr_meta_create_elem(fsvr_file_t *file, uint64_t on_write);
void svr_meta_done_write(uint64_t fid);

int svr_meta_list_add(file_elem_t *elem);

int svr_meta_list_get_cnt();
file_elem_t* svr_meta_list_get_next(file_elem_t *elem);
file_elem_t* svr_meta_get_elem_by_name(char *filename);
file_elem_t* svr_meta_get_elem_by_fid(uint64_t fid);

#endif
