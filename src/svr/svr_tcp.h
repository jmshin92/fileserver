#ifndef _SVR_TCP_H
#define _SVR_TCP_H

#include "svr_internal.h"
#include "svr_queue.h"

typedef struct svr_conn_s svr_conn_t;
struct svr_conn_s {
    struct sockaddr_in cliaddr;
    int fd;
};

typedef struct svr_sock_s svr_sock_t;
struct svr_sock_s {
    struct sockaddr_in addr;
    int sockfd;
};

typedef struct svr_msg_s svr_msg_t;
struct svr_msg_s {
    svr_conn_t *conn;
    char       *msg;
    int         msglen;
    char       *buf;
    int         buflen;
};

/* for ipc */
struct send_msg_s {
    fsvr_msg_hdr_t *msg;
    svr_conn_t     *conn;
};
typedef struct send_msg_s send_msg_t;

extern svr_sock_t *svr_sock;

void svr_sock_init();
svr_conn_t* accept_new_conn();
void svr_send_msg(svr_conn_t *conn, char *msg, int msglen, char *buf, int buflen);
fsvr_msg_hdr_t* svr_recv_msg(svr_conn_t *conn);

#endif
