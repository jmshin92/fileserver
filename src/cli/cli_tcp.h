#ifndef _CLI_TCP_H
#define _CLI_TCP_H

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "cli_internal.h"

#define MAXSLEEP 128

int connect_server(char *host, int portno);

void cli_send_msg(int sockfd, char* msg, int len, char* buf, int buflen);
fsvr_reply_hdr_t* cli_recv_msg(int sockfd);

#endif
