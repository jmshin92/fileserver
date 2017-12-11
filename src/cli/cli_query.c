#include "cli_query.h"

static void
print_file_info(fsvr_file_t* file, uint64_t cnt)
{
    uint32_t i;

    printf("%20s %20s %20s\n", "File name", "Size", "Owner name");
    printf("---------------------------------------------------------------\n");
    for (i = 0; i < cnt; i++)
        printf("%20s %20zu %20s \n",
               file[i].filename, file[i].size, file[i].ownername);
}

/* put msg를 통해 먼저 file server의 메타 정보를 업데이트한 후
 * stripe size 단위로 쪼개어 write msg를 보낸다.
 * file server는 put msg를 받은 후 파일의 ID를 돌려준다.
 * write msg는 파일의 ID와 데이터, 오프셋, 사이즈를 달아서 보낸다.*/
static void
process_put_query(int sockfd)
{
    int                     fd, ret;
    uint64_t                i, j, shard = 0, offset = 0;
    size_t                  written;
    fsvr_put_file_msg_t    *put_msg     = NULL;
    fsvr_put_file_reply_t  *put_reply   = NULL;
    fsvr_write_msg_t       *write_msg   = NULL;
    fsvr_write_reply_t     *write_reply = NULL;
    fsvr_file_t             file;

    /* send put_msg and recv fid */
    put_msg = (fsvr_put_file_msg_t*) fsvr_alloc_msg(OP_MSG_PUT_FILE);

    /* file stat */
    scanf("%s", file.filename);
    ret = cli_open_file(file.filename, true, &fd, file.ownername, &file.size);

    if (ret != 0) {
        LOG_ERR("failed to open file:errno %d", ret);
        printf("failed to open file\n");
        return;
    }

    memcpy(&put_msg->file, &file, sizeof(fsvr_file_t));

    LOG("send put msg:filename %s,size %zd,owner %s",
        file.filename, file.size, file.ownername);
    cli_send_msg(sockfd, (char *) put_msg, put_msg->msglen, NULL, 0);

    put_reply = (fsvr_put_file_reply_t*) cli_recv_msg(sockfd);

    if (put_reply->ec == EC_FILE_ALREADY_EXIST) {
        LOG_ERR("failed to put file(file already exist):filename %s, ec %d",
                file.filename, put_reply->ec);
        return;;
    }

    LOG("Recv put reply:filename %s,fid %zu", file.filename, put_reply->fid);

    /* send write msgs */
    write_msg = (fsvr_write_msg_t *) fsvr_alloc_msg(OP_MSG_WRITE);
    write_msg->buf = malloc(STRIPE_UNIT);
    written = 0;

    while (offset < file.size) {
        for (i = 0; i < 10 && offset < file.size; i++) {
            write_msg->op       = OP_MSG_WRITE;
            write_msg->fid      = put_reply->fid;
            write_msg->msglen   = sizeof(fsvr_write_msg_t);

            if (offset + STRIPE_UNIT > file.size)
                write_msg->buflen = file.size - offset;
            else
                write_msg->buflen = STRIPE_UNIT;

            write_msg->shard = shard++;

            cli_read_file(fd, write_msg->buf, write_msg->buflen, offset);

            written += write_msg->buflen;
            offset += STRIPE_UNIT;

            cli_send_msg(sockfd, (char *) write_msg, write_msg->msglen,
                         write_msg->buf, write_msg->buflen);

            LOG("send write msg:filename %s,written %zd",
                file.filename, written);
        }

        for (j = 0; j < i; j++) {
            write_reply = (fsvr_write_reply_t*) cli_recv_msg(sockfd);
            free(write_reply);
        }

        PROGRESS((written * 100) / file.size);
    }

    PROGRESS(100);

    if (put_msg)
        fsvr_msg_free(put_msg, 0);
    if (put_reply)
        fsvr_msg_free(put_reply, 0);
    if (write_msg)
        fsvr_msg_free(write_msg, 1);
}

static void
process_get_query(int sockfd)
{
    fsvr_get_file_msg_t     *get_msg     = NULL;
    fsvr_get_file_reply_t   *get_reply   = NULL;
    fsvr_read_reply_t       *read_reply  = NULL;
    fsvr_reply_hdr_t        *hdr         = NULL;
    off_t                   total_size = 0, recved_size = 0;
    size_t                  dummy;
    int                     fd;

    get_msg = (fsvr_get_file_msg_t*) fsvr_alloc_msg(OP_MSG_GET_FILE);
    scanf("%s", get_msg->filename);

    LOG("send get_msg:file %s,sockfd %d", get_msg->filename, sockfd);

    cli_send_msg(sockfd, (char *) get_msg, get_msg->msglen, NULL, 0);

    if (cli_open_file(get_msg->filename, false, &fd, NULL, &dummy) < 0) {
        LOG_ERR("cannot open file:filename %s", get_msg->filename);
        goto cleanup;
    }

    /* DB는 get msg를 받으면 meta 정보를 보냄과 동시에 바로 파일을 읽어
     * 메세지를 보낸다. 따라서 파일 데이터가 담긴 메세지가 먼저올 수도 있다.
     * 메타 정보에는 파일의 전체 사이즈가 담겨있으므로 받은 데이터의 크기가
     * 파일 사이즈와 같아지면 끝남.*/
    while (total_size == 0 || recved_size < total_size) {
        hdr = cli_recv_msg(sockfd);

        if (hdr->op == OP_MSG_GET_FILE_REPLY) {
            get_reply = (fsvr_get_file_reply_t *) hdr;
            if (get_reply->ec != EC_NONE) {
                if (get_reply->ec == EC_FILE_NOT_FOUND)
                    printf("file is not found: %s\n",
                           get_msg->filename);
                LOG_ERR("get_msg with error:ec %d", get_reply->ec);
                return;
            }
            total_size = get_reply->file.size;
        } else if (hdr->op == OP_MSG_READ_REPLY) {
            read_reply = (fsvr_read_reply_t *) hdr;

            if (read_reply->ec != EC_NONE) {
                LOG_ERR("failed to read:ec %d", read_reply->ec);
                return;
            }

            read_reply->buf = (char*) read_reply + hdr->msglen;

            cli_write_file(fd, read_reply->buf, read_reply->buflen,
                           read_reply->offset);
            recved_size += read_reply->buflen;

            if (total_size > 0)
                PROGRESS((recved_size *100) /  total_size);
            free(hdr);
        }
    }

    print_file_info(&get_reply->file, 1);

  cleanup:
    if (get_msg)
        fsvr_msg_free(get_msg, 0);
    if (get_reply)
        fsvr_msg_free(get_reply, 0);
}

static void
process_list_query(int sockfd)
{
    char                   *ptr;
    fsvr_list_file_msg_t    *list_msg;
    fsvr_list_file_reply_t  *list_reply;

    list_msg = (fsvr_list_file_msg_t*) fsvr_alloc_msg(OP_MSG_LIST_FILE);

    cli_send_msg(sockfd, (char *) list_msg, list_msg->msglen, NULL, 0);
    LOG("sent list_msg");

    list_reply = (fsvr_list_file_reply_t*) cli_recv_msg(sockfd);

    if (list_reply->cnt == 0) {
        LOG("no files");
        printf("no files\n");
        goto cleanup;
    }

    list_reply->buf = (char*) list_reply + list_reply->msglen;
    ptr = list_reply->buf;

    print_file_info((fsvr_file_t*) ptr, list_reply->cnt);

  cleanup:
    fsvr_msg_free(list_msg, 0);
    fsvr_msg_free(list_reply, 0);
}

int
cli_get_query(int sockfd)
{
    char str_op[10];

  retry:
    printf("CLI> ");
    scanf("%s", str_op);
    fflush(stdin);

    if (strcmp(str_op, "put") == 0) {
        process_put_query(sockfd);
    } else if (strcmp(str_op, "get") == 0) {
        process_get_query(sockfd);
    } else if (strcmp(str_op, "list") == 0) {
        process_list_query(sockfd);
    } else if (strcmp(str_op, "hello") == 0) {
        printf(" world!\n");
    } else if (strcmp(str_op, "q") == 0 || strcmp(str_op, "exit") == 0) {
        printf("disconnected!\n");
        return -1;
    } else {
        printf("the cmd is not supported\n");
        LOG("unhandled command");
        goto retry;
    }

    return 0;
}

