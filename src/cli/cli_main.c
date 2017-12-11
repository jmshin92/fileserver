#include <signal.h>

#include "cli_internal.h"
#include "cli_file.h"
#include "cli_tcp.h"
#include "cli_query.h"

LOG_DEC

int fd;

void sig_handler(int sig)
{
    shutdown(fd, SHUT_RDWR);
    printf("disconnected!\n");
    exit(0);
}

int cli_main(char* host, int port)
{
    int ret;

    fd = connect_server(host, port);

    while(1) {
        if (fd < 0) {
            printf("failed to connect to server\n");
            return -1;
        }

        ret = cli_get_query(fd);

        if (ret < 0)
            return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        LOG_ERR("not enough params");
        printf("USAGE: [host addr] [port]\n");
        return -1;
    }

    signal(SIGINT, sig_handler);

    LOG_INIT("log/cli_log.txt");

    cli_main(argv[1], atoi(argv[2]));

    return -1;
}
