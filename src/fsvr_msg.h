#ifndef _FSVR_MSG_H
#define _FSVR_MSG_H

#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <endian.h>

#include "fsvr_file.h"

enum fsvr_op_e {
    OP_MSG_READ = 0,
    OP_MSG_READ_REPLY,          /* 1 */
    OP_MSG_WRITE,               /* 2 */
    OP_MSG_WRITE_REPLY,         /* 3 */
    OP_MSG_LIST_FILE,           /* 4 */
    OP_MSG_LIST_FILE_REPLY,     /* 5 */
    OP_MSG_GET_FILE,            /* 6 */
    OP_MSG_GET_FILE_REPLY,      /* 7 */
    OP_MSG_PUT_FILE,            /* 8 */
    OP_MSG_PUT_FILE_REPLY       /* 9 */
};

enum fsvr_ec_e {
    EC_NONE = 0,
    EC_FILE_NOT_FOUND = 1,
    EC_FILE_ALREADY_EXIST = 2
};

typedef uint16_t fsvr_ec_t;
typedef uint16_t fsvr_op_t;

#define SVR_MSG_COMMON                                                         \
    fsvr_op_t       op;                                                        \
    char            hdr_filler[6];                                             \
    int32_t         msglen;                                                    \
    int32_t         buflen;

#define SVR_REPLY_COMMON                                                       \
    fsvr_op_t       op;                                                        \
    fsvr_ec_t       ec;                                                        \
    char            hdr_filler[4];                                             \
    int32_t         msglen;                                                    \
    int32_t         buflen;

struct fsvr_msg_hdr_s {
    SVR_MSG_COMMON
};
typedef struct fsvr_msg_hdr_s fsvr_msg_hdr_t;

struct fsvr_reply_hdr_s {
    SVR_REPLY_COMMON
};
typedef struct fsvr_reply_hdr_s fsvr_reply_hdr_t;

struct fsvr_read_msg_s {
    SVR_MSG_COMMON
    char            filename[PATH_MAX];
    uint64_t        offset;
};
typedef struct fsvr_read_msg_s fsvr_read_msg_t;

struct fsvr_read_reply_s {
    SVR_REPLY_COMMON
    uint64_t        offset;
    char           *buf;
};
typedef struct fsvr_read_reply_s fsvr_read_reply_t;

struct fsvr_write_msg_s {
    SVR_MSG_COMMON
    uint64_t        fid;
    uint64_t        shard;
    char            filler[4];
    char           *buf;
};
typedef struct fsvr_write_msg_s fsvr_write_msg_t;

struct fsvr_write_reply_s {
    SVR_REPLY_COMMON
};
typedef struct fsvr_write_reply_s fsvr_write_reply_t;

struct fsvr_put_file_msg_s {
    SVR_MSG_COMMON
    fsvr_file_t     file;
};
typedef struct fsvr_put_file_msg_s fsvr_put_file_msg_t;

struct fsvr_put_file_reply_s {
    SVR_REPLY_COMMON
    uint64_t        fid;
};
typedef struct fsvr_put_file_reply_s fsvr_put_file_reply_t;

struct fsvr_get_file_msg_s {
    SVR_MSG_COMMON
    char            filename[PATH_MAX];
};
typedef struct fsvr_get_file_msg_s fsvr_get_file_msg_t;

struct fsvr_get_file_reply_s {
    SVR_REPLY_COMMON
    fsvr_file_t     file;
};
typedef struct fsvr_get_file_reply_s fsvr_get_file_reply_t;

struct fsvr_list_file_msg_s {
    SVR_MSG_COMMON
};
typedef struct fsvr_list_file_msg_s fsvr_list_file_msg_t;

struct fsvr_list_file_reply_s {
    SVR_REPLY_COMMON
    uint64_t        cnt;
    char           *buf;
};
typedef struct fsvr_list_file_reply_s fsvr_list_file_reply_t;

static inline void
fsvr_msg_free(void *arg, int freebuf)
{
    fsvr_msg_hdr_t         *hdr = arg;
    fsvr_list_file_msg_t   *list_msg;
    fsvr_list_file_reply_t *list_reply;
    fsvr_put_file_msg_t    *put_msg;
    fsvr_put_file_reply_t  *put_reply;
    fsvr_get_file_msg_t    *get_msg;
    fsvr_get_file_reply_t  *get_reply;
    fsvr_read_msg_t        *read_msg;
    fsvr_read_reply_t      *read_reply;
    fsvr_write_msg_t       *write_msg;
    fsvr_write_reply_t     *write_reply;

    switch (hdr->op) {
    case OP_MSG_LIST_FILE:
        list_msg = (fsvr_list_file_msg_t*) arg;
        free(list_msg);
        break;
    case OP_MSG_LIST_FILE_REPLY:
        list_reply = (fsvr_list_file_reply_t*) arg;
        if (freebuf && list_reply->buf != NULL)
            free(list_reply->buf);
        free(list_reply);
        break;
    case OP_MSG_GET_FILE:
        get_msg = (fsvr_get_file_msg_t*) arg;
        free(get_msg);
        break;
    case OP_MSG_GET_FILE_REPLY:
        get_reply = (fsvr_get_file_reply_t*) arg;
        free(get_reply);
        break;
    case OP_MSG_PUT_FILE:
        put_msg = (fsvr_put_file_msg_t*) arg;
        free(put_msg);
        break;
    case OP_MSG_PUT_FILE_REPLY:
        put_reply = (fsvr_put_file_reply_t*) arg;
        free(put_reply);
        break;
    case OP_MSG_READ:
        read_msg = (fsvr_read_msg_t*) arg;
        free(read_msg);
        break;
    case OP_MSG_READ_REPLY:
        read_reply = (fsvr_read_reply_t*) arg;
        if (freebuf && read_reply->buf != NULL)
            free(read_reply->buf);
        free(read_reply);
        break;
    case OP_MSG_WRITE:
        write_msg = (fsvr_write_msg_t*) arg;
        if (freebuf && write_msg->buf != NULL)
            free(write_msg->buf);
        free(write_msg);
        break;
    case OP_MSG_WRITE_REPLY:
        write_reply = (fsvr_write_reply_t*) arg;
        free(write_reply);
        break;
    default:
        printf("free: unknown msg type(%d)\n", hdr->op);
        break;
    }
}

static inline fsvr_msg_hdr_t*
fsvr_alloc_msg(fsvr_op_t op)
{
    fsvr_msg_hdr_t *msg = NULL;

    switch(op) {
    case OP_MSG_LIST_FILE:
        msg         = malloc(sizeof(fsvr_list_file_msg_t));
        msg->op     = op;
        msg->msglen = sizeof(fsvr_list_file_msg_t);
        msg->buflen = 0;
        break;
    case OP_MSG_LIST_FILE_REPLY:
        msg         = malloc(sizeof(fsvr_list_file_reply_t));
        msg->op     = op;
        msg->msglen = sizeof(fsvr_list_file_reply_t);
        msg->buflen = 0;
        break;
    case OP_MSG_GET_FILE:
        msg         = malloc(sizeof(fsvr_get_file_msg_t));
        msg->op     = op;
        msg->msglen = sizeof(fsvr_get_file_msg_t);
        msg->buflen = 0;
        break;
    case OP_MSG_GET_FILE_REPLY:
        msg         = malloc(sizeof(fsvr_get_file_reply_t));
        msg->op     = op;
        msg->msglen = sizeof(fsvr_get_file_reply_t);
        msg->buflen = 0;
        break;
    case OP_MSG_PUT_FILE:
        msg         = malloc(sizeof(fsvr_put_file_msg_t));
        msg->op     = op;
        msg->msglen = sizeof(fsvr_put_file_msg_t);
        msg->buflen = 0;
        break;
    case OP_MSG_PUT_FILE_REPLY:
        msg         = malloc(sizeof(fsvr_put_file_reply_t));
        msg->op     = op;
        msg->msglen = sizeof(fsvr_put_file_reply_t);
        msg->buflen = 0;
        break;
    case OP_MSG_WRITE:
        msg         = malloc(sizeof(fsvr_write_msg_t));
        msg->op     = op;
        msg->msglen = sizeof(fsvr_write_msg_t);
        msg->buflen = 0;
        break;
    case OP_MSG_READ_REPLY:
        msg         = malloc(sizeof(fsvr_read_reply_t));
        msg->op     = op;
        msg->msglen = sizeof(fsvr_read_reply_t);
        msg->buflen = 0;
        break;
    case OP_MSG_WRITE_REPLY:
        msg         = malloc(sizeof(fsvr_write_reply_t));
        msg->op     = op;
        msg->msglen = sizeof(fsvr_write_reply_t);
        msg->buflen = 0;
    default:
        break;
    }
    return msg;
}

static inline void
serialize_msg_hdr(fsvr_msg_hdr_t* hdr)
{
    hdr->op     = htobe16(hdr->op);
    hdr->msglen = htobe32(hdr->msglen);
    hdr->buflen = htobe32(hdr->buflen);
}

static inline void
deserialize_msg_hdr(fsvr_msg_hdr_t* hdr)
{
    hdr->op     = be16toh(hdr->op);
    hdr->msglen = be32toh(hdr->msglen);
    hdr->buflen = be32toh(hdr->buflen);
}

static inline void
serialize_reply_hdr(fsvr_reply_hdr_t* hdr)
{
    hdr->op     = htobe16(hdr->op);
    hdr->ec     = htobe16(hdr->ec);
    hdr->msglen = htobe32(hdr->msglen);
    hdr->buflen = htobe32(hdr->buflen);
}

static inline void
deserialize_reply_hdr(fsvr_reply_hdr_t* hdr)
{
    hdr->op     = be16toh(hdr->op);
    hdr->ec     = be16toh(hdr->ec);
    hdr->msglen = be32toh(hdr->msglen);
    hdr->buflen = be32toh(hdr->buflen);
}

static inline void
fsvr_msg_serialize(void *arg)
{
    fsvr_msg_hdr_t         *hdr = arg;
    fsvr_reply_hdr_t       *rhdr = arg;
    fsvr_put_file_msg_t    *put_msg;
    fsvr_read_msg_t        *read_msg;
    fsvr_write_msg_t       *write_msg;

    fsvr_list_file_reply_t *list_reply;
    fsvr_put_file_reply_t  *put_reply;
    fsvr_get_file_reply_t  *get_reply;
    fsvr_read_reply_t      *read_reply;

    switch (hdr->op) {
    case OP_MSG_LIST_FILE:
    case OP_MSG_GET_FILE:
        serialize_msg_hdr(hdr);
        break;
    case OP_MSG_PUT_FILE:
        serialize_msg_hdr(hdr);
        put_msg             = (fsvr_put_file_msg_t*) arg;
        put_msg->file.size  = htobe64(put_msg->file.size);
        break;
    case OP_MSG_READ:
        serialize_msg_hdr(hdr);
        read_msg            = (fsvr_read_msg_t*) arg;
        read_msg->offset    = htobe64(read_msg->offset);
        break;
    case OP_MSG_WRITE:
        serialize_msg_hdr(hdr);
        write_msg           = (fsvr_write_msg_t*) arg;
        write_msg->fid      = htobe64(write_msg->fid);
        write_msg->shard    = htobe64(write_msg->shard);
        break;

    case OP_MSG_LIST_FILE_REPLY:
        serialize_reply_hdr(rhdr);
        list_reply          = (fsvr_list_file_reply_t*) arg;
        list_reply->cnt     = htobe64(list_reply->cnt);
        break;
    case OP_MSG_GET_FILE_REPLY:
        serialize_reply_hdr(rhdr);
        get_reply           = (fsvr_get_file_reply_t*) arg;
        get_reply->file.size = htobe64(get_reply->file.size);
        break;
    case OP_MSG_PUT_FILE_REPLY:
        serialize_reply_hdr(rhdr);
        put_reply           = (fsvr_put_file_reply_t*) arg;
        put_reply->fid      = htobe64(put_reply->fid);
        break;
    case OP_MSG_READ_REPLY:
        serialize_reply_hdr(rhdr);
        read_reply          = (fsvr_read_reply_t*) arg;
        read_reply->offset  = htobe64(read_reply->offset);
        break;
    case OP_MSG_WRITE_REPLY:
        serialize_reply_hdr(rhdr);
        break;
    default:
        printf("serialize: unknown msg type(%d)\n", hdr->op);
        break;
    }
}

static inline void
fsvr_msg_deserialize(void *arg)
{
    fsvr_msg_hdr_t         *hdr = arg;
    fsvr_put_file_msg_t    *put_msg;
    fsvr_read_msg_t        *read_msg;
    fsvr_write_msg_t       *write_msg;

    fsvr_list_file_reply_t *list_reply;
    fsvr_put_file_reply_t  *put_reply;
    fsvr_get_file_reply_t  *get_reply;
    fsvr_read_reply_t      *read_reply;

    switch (hdr->op) {
    case OP_MSG_LIST_FILE:
    case OP_MSG_GET_FILE:
    case OP_MSG_WRITE_REPLY:
        break;
        break;
    case OP_MSG_PUT_FILE:
        put_msg             = (fsvr_put_file_msg_t*) arg;
        put_msg->file.size  = be64toh(put_msg->file.size);
        break;
    case OP_MSG_READ:
        read_msg            = (fsvr_read_msg_t*) arg;
        read_msg->offset    = be64toh(read_msg->offset);
        break;
    case OP_MSG_WRITE:
        write_msg           = (fsvr_write_msg_t*) arg;
        write_msg->fid      = be64toh(write_msg->fid);
        write_msg->shard    = be64toh(write_msg->shard);
        break;
    case OP_MSG_LIST_FILE_REPLY:
        list_reply          = (fsvr_list_file_reply_t*) arg;
        list_reply->cnt     = be64toh(list_reply->cnt);
        break;
    case OP_MSG_GET_FILE_REPLY:
        get_reply           = (fsvr_get_file_reply_t*) arg;
        get_reply->file.size = be64toh(get_reply->file.size);
        break;
    case OP_MSG_PUT_FILE_REPLY:
        put_reply           = (fsvr_put_file_reply_t*) arg;
        put_reply->fid      = be64toh(put_reply->fid);
        break;
    case OP_MSG_READ_REPLY:
        read_reply          = (fsvr_read_reply_t*) arg;
        read_reply->offset  = be64toh(read_reply->offset);
        break;
    default:
        printf("deserialize: unknown msg type\n");
        break;
    }
}

#endif /* no _MSG_H */
