#include "svr_tcp.h"

svr_sock_t *svr_sock;

void
svr_sock_init()
{
    const int trueflag = 1;
    svr_sock = (svr_sock_t *) malloc(sizeof(svr_sock_t));

    svr_sock->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr_sock->sockfd < 0) {
        LOG_ERR("failed to open socket:fd %d,errno %d",
               svr_sock->sockfd, errno);
        return;
    }

    if (setsockopt(svr_sock->sockfd, SOL_SOCKET, SO_REUSEADDR, &trueflag,
                   sizeof(int)) < 0) {
        LOG_ERR("failed to set socket option");
        return;
    }

    LOG("created socket:socfd %d", svr_sock->sockfd);
    memset(&svr_sock->addr, 0, sizeof(struct sockaddr_in));
    svr_sock->addr.sin_family = AF_INET;
    svr_sock->addr.sin_addr.s_addr = htonl(INADDR_ANY);;
    svr_sock->addr.sin_port = htons(PORTNO);

    if (bind(svr_sock->sockfd, (struct sockaddr *) &svr_sock->addr,
             sizeof(struct sockaddr_in)) < 0) {
        LOG_ERR("failed to bind:fd %d,errno %d", svr_sock->sockfd, errno);
        goto errout;
    }

    if (listen(svr_sock->sockfd, SOMAXCONN) < 0) {
        LOG_ERR("failed to listen:fd %d,errno %d", svr_sock->sockfd, errno);
        goto errout;
    }

    return;

  errout:
    close(svr_sock->sockfd);
    free(svr_sock);
}

svr_conn_t *
accept_new_conn()
{
    svr_conn_t *conn;
    socklen_t clilen;
    conn = (svr_conn_t *) malloc(sizeof(svr_conn_t));

    clilen = sizeof(struct sockaddr_in);
    conn->fd = accept(svr_sock->sockfd, (struct sockaddr *) &conn->cliaddr,
                      &clilen);

    if (conn->fd < 0) {
        LOG_ERR("failed to accept:fd %d,errno %d", svr_sock->sockfd, errno);
        goto errout;
    }

    LOG("accepted a new conn:fd %d,addr %p", conn->fd, &conn->cliaddr);
    return conn;

  errout:
    free(conn);
    return NULL;
}

void
svr_send_msg(svr_conn_t *conn, char *msg, int msglen, char *buf, int buflen)
{
    int bytes_sent = 0, ret, op;
    fsvr_reply_hdr_t *hdr = (fsvr_reply_hdr_t*) msg;
    op = hdr->op;

    LOG("send msg:msg %p,op %d, ec %d", hdr, hdr->op, hdr->ec);
    fsvr_msg_serialize(msg);
    LOG("serialize msg:msg %p,op %d, ec %d", hdr, hdr->op, hdr->ec);
    /* send msg hdr, body first */
    while (bytes_sent < msglen) {
        ret = write(conn->fd, msg + bytes_sent, msglen - bytes_sent);

        if (ret < 0) {
            if (errno == EINTR)
                continue;
            else {
                LOG_ERR("failed to send msg:errno %d", errno);
                return;
            }
        }

        bytes_sent += ret;
    }

    /* send buffer(data) */
    bytes_sent = 0;
    while (bytes_sent < buflen) {
        ret = write(conn->fd, buf + bytes_sent, buflen - bytes_sent);

        if (ret < 0) {
            if (errno == EINTR)
                continue;
            else {
                LOG_ERR("failed to send buffer:errno %d", errno);
                return;
            }
        }

        bytes_sent += ret;
    }
    LOG("sent msg:msglen %d,buflen %d", msglen, buflen);

    hdr->op = op;
}

fsvr_msg_hdr_t *
svr_recv_msg(svr_conn_t *conn)
{
    int bytes_recvd, bytes_remain, ret;
    char *buffer, *msg;
    fsvr_msg_hdr_t *hdr;

    LOG("conn:fd %d", conn->fd);
    bytes_recvd = 0;
    bytes_remain = sizeof(fsvr_msg_hdr_t);
    buffer = (char*) malloc(bytes_remain);

    LOG("start to read:remain %d,fd %d,conn 0x%p", bytes_remain, conn->fd, conn);
    /* recv msg hdr */
    while (bytes_recvd < bytes_remain) {
        ret = read(conn->fd, buffer + bytes_recvd, bytes_remain - bytes_recvd);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            else {
                LOG_ERR("failed to recv msg:errno %d", errno);
            }
        }  else if (ret == 0) {
            printf("socket has closed\n");
            return NULL;
        }
        bytes_recvd += ret;
    }

    hdr = (fsvr_msg_hdr_t*) buffer;
    deserialize_msg_hdr(hdr);
    LOG("recved hdr:op %d,msglen %d,buflen %d",
        hdr->op, hdr->msglen, hdr->buflen);

    msg = (char*) malloc(hdr->msglen + hdr->buflen);
    hdr = (fsvr_msg_hdr_t*) msg;
    memcpy(msg, buffer, bytes_recvd);
    free(buffer);

    bytes_remain = hdr->msglen + hdr->buflen;
    /* recv msg body */
    while (bytes_recvd < bytes_remain) {
        ret = read(conn->fd, msg + bytes_recvd, bytes_remain - bytes_recvd);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            else {
                LOG_ERR("failed to recv msg: errno %d", errno);
            }
        }

        LOG("recv msg:offset %p, remain %d, recvd %d, ret %d",
            (msg + bytes_recvd), bytes_remain - bytes_recvd, bytes_recvd, ret);

        bytes_recvd += ret;
    }

    LOG("recved msg:msg %p,msglen %d,buflen %d", msg, hdr->msglen, hdr->buflen);
    fsvr_msg_deserialize(hdr);
    return hdr;
}
