#include "cli_tcp.h"

int
connect_server(char *host, int portno)
{
    int sockfd;
    struct sockaddr_in serv_addr;

    portno = PORTNO;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0) {
        LOG_ERR("failed to socket: errno %d", errno);
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = inet_addr(host);
    serv_addr.sin_port          = htons(portno);

    if (connect(sockfd, (struct sockaddr *) &serv_addr,
                sizeof(serv_addr)) < 0) {
        LOG_ERR("failed to connect: errno %d", errno);
        goto errout;
    }

    return sockfd;

  errout:
    close(sockfd);
    return -1;
}

fsvr_reply_hdr_t*
cli_recv_msg(int sockfd)
{
    int bytes_recvd, bytes_remain, ret;
    char *buffer, *reply;
    fsvr_reply_hdr_t *hdr;

    bytes_recvd = 0;
    bytes_remain = sizeof(fsvr_reply_hdr_t);
    buffer = malloc(bytes_remain);

    /* recv msg hdr */
    while (bytes_recvd < bytes_remain) {
        ret = read(sockfd, buffer + bytes_recvd, bytes_remain - bytes_recvd);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            else {
                LOG_ERR("failed to recv msg:errno %d", errno);
                exit(-1);
            }
        }
        bytes_recvd += ret;
    }
    hdr = (fsvr_reply_hdr_t*) buffer;
    deserialize_reply_hdr(hdr);
    LOG("recved hdr:op %d,ec %d,msglen %d,buflen %d", hdr->op, hdr->ec, hdr->msglen, hdr->buflen);

    reply = malloc(hdr->msglen + hdr->buflen);
    memcpy(reply, buffer, bytes_recvd);
    hdr = (fsvr_reply_hdr_t*) reply;
    free(buffer);

    /* recv msg body & buffer */
    bytes_remain = hdr->msglen + hdr->buflen;
    while (bytes_recvd < bytes_remain) {
        ret = read(sockfd, reply + bytes_recvd, bytes_remain - bytes_recvd);
        if (ret < 0) {
            if (errno == EINTR)
                continue;
            else {
                LOG_ERR("failed to recv msg:errno %d", errno);
                exit(-1);
            }
        }

        bytes_recvd += ret;
    }

    LOG("recvd msg:msg %p,op %d, ec %d,recvd %d,msglen %d,buflen %d",
        reply, hdr->op, hdr->ec, bytes_recvd, hdr->msglen, hdr->buflen);

    fsvr_msg_deserialize(hdr);
    return hdr;
}

void
cli_send_msg(int sockfd, char* msg, int len, char *buf, int buflen)
{
    int bytes_sent = 0, ret, op;
    fsvr_msg_hdr_t *hdr = (fsvr_msg_hdr_t*) msg;

    op = hdr->op;
    fsvr_msg_serialize(msg);

    while (bytes_sent < len) {
        ret = write(sockfd, msg + bytes_sent, len - bytes_sent);

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

    bytes_sent = 0;
    while (bytes_sent < buflen) {
        ret = write(sockfd, buf + bytes_sent, buflen - bytes_sent);

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

    /* GC때 op를 보고 하기 때문에 op는 복구시켜줌 */
    hdr->op = op;
}
