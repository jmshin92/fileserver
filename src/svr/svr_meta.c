#include "svr_meta.h"

FILE *metafile;
file_list_t *list;
file_pool_t *fp;

void
svr_meta_list_init()
{
    char           *buf, *tmp, *ptr;
    int             buflen;
    file_elem_t    *elem;
    fsvr_file_t    *file;

    if (access(METAFILE, F_OK) != -1) {
        metafile = fopen(METAFILE, "r+");
    } else {
        metafile = fopen(METAFILE, "w+");
    }

    if (metafile == NULL) {
        LOG_ERR("cannot open metadata file");
        return;
    }

    list = (file_list_t*) malloc(sizeof(file_list_t));
    list->head  = NULL;
    list->tail  = NULL;
    list->cnt   = 0;

    if (pthread_mutex_init(&list->lock, NULL) != 0) {
        LOG_ERR("cannot init lock");
        free(list);
        exit(-1);
    }

    fp = (file_pool_t*) malloc(sizeof(file_pool_t));
    fp->files   = (file_elem_t**) malloc(sizeof(file_elem_t*) * MAX_FILE);
    fp->cnt     = 0;

    if (pthread_mutex_init(&fp->lock, NULL) != 0) {
        LOG_ERR("cannot init fp lock");
        exit(-1);
    }

    /* READ METAFILE */
    buflen = PATH_MAX + 1 + 32 + 1 + 32 +1; /* path + size + owner + 3*delim */
    buf = malloc(buflen);

    LOG("meta init");
    while (fgets(buf, buflen, metafile)) {
        buf[strlen(buf) - 1] = '\0';

        elem = (file_elem_t*) malloc(sizeof(file_elem_t));
        elem->next = NULL;

        /* read meta data: filename/size/ownername */
        file = &elem->file;
        tmp = buf;
        if ((ptr = strsep(&tmp, "/")) == NULL)
            goto errout;
        strcpy(file->filename, ptr);

        if ((ptr = strsep(&tmp, "/")) == NULL)
            goto errout;
        file->size = atoi(ptr);

        if ((ptr = strsep(&tmp, "/")) == NULL)
            goto errout;
        strcpy(file->ownername, ptr);

        elem->fid = fp->cnt++;
        fp->files[elem->fid] = elem;

        svr_meta_list_add(elem);

        LOG("Reading metadata: filename %s,size %zu,ownername %s,fid: %zu",
            file->filename, file->size, file->ownername, elem->fid);
    }

    return;

  errout:
    free(buf);
    free(elem);
    return;
}

uint64_t
svr_meta_create_elem(fsvr_file_t *file, uint64_t on_write)
{
    file_elem_t *elem;

    if (!file)
        return FID_NULL;

    if (svr_meta_get_elem_by_name(file->filename))
        return FID_NULL;

    elem = (file_elem_t*) malloc(sizeof(file_elem_t));
    elem->next      = NULL;
    elem->on_write  = on_write;
    pthread_mutex_init(&elem->lock, NULL);

    if (file)
        memcpy(&elem->file, file, sizeof(fsvr_file_t));

    pthread_mutex_lock(&fp->lock);
    elem->fid = fp->cnt++;
    fp->files[elem->fid] = elem;
    pthread_mutex_unlock(&fp->lock);
    return elem->fid;
}

void
svr_meta_done_write(uint64_t fid)
{
    file_elem_t *elem = fp->files[fid];
    uint64_t on_write;

    pthread_mutex_lock(&elem->lock);
    on_write = --elem->on_write;
    pthread_mutex_unlock(&elem->lock);

    if (on_write == 0)
        svr_meta_list_add(elem);
}

int
svr_meta_list_add(file_elem_t *elem)
{
    pthread_mutex_lock(&list->lock);


    if (list->cnt == 0) {
        list->head = elem;
    } else {
        pthread_mutex_lock(&list->tail->lock);
        list->tail->next = elem;
        pthread_mutex_unlock(&list->tail->lock);
    }

    list->tail = elem;

    list->cnt++;

    pthread_mutex_unlock(&list->lock);

    fprintf(metafile, "%s/%zu/%s\n", elem->file.filename,  elem->file.size,
            elem->file.ownername);
    fflush(metafile);

    return elem->fid;
}

int
svr_meta_list_get_cnt()
{
    return list->cnt;
}

file_elem_t*
svr_meta_list_get_next(file_elem_t *pelem)
{
    file_elem_t *elem;
    if (list->head == NULL)
        return NULL;

    if (pelem == NULL) {
        if (list->head->on_write == 0)
            return list->head;
        else
            elem = list->head;
    } else {
        elem = pelem;
    }

    elem = elem->next;

    if (elem == NULL)
        return NULL;

    return elem;
}

file_elem_t*
svr_meta_get_elem_by_name(char *filename)
{
    file_elem_t *elem;
    int i;

    if (fp->cnt == 0)
        return NULL;

    for (i = 0; i < fp->cnt; i++) {
        elem = fp->files[i];
        if (strcmp(elem->file.filename, filename) == 0)
            return elem;
    }

    return NULL;
}

file_elem_t*
svr_meta_get_elem_by_fid(uint64_t fid)
{
    file_elem_t *elem;
    if (fp->cnt == 0)
        return NULL;

    if (fp->cnt < fid || fid == FID_NULL)
        return NULL;

    if ((elem = fp->files[fid]) == NULL)
        return NULL;

    return elem;
}
