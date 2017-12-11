#ifndef _CLI_INTERNAL_H
#define _CLI_INTERNAL_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <time.h>

#include "../fsvr_log.h"
#include "../fsvr_msg.h"
#include "../fsvr_msg.h"

#define PORTNO 9090
#define STRIPE_UNIT 1048576 /* 1MB단위로 스트라이핑 */
#define false 0
#define true 1

LOG_EXT
#endif
