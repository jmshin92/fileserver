#ifndef _FSVR_LOG_H
#define _FSVR_LOG_H
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <unistd.h>

#define LOG_EXT\
    extern FILE *log_file;\
    extern time_t log_time;\
    extern struct tm *log_tm;

#define LOG_DEC\
    FILE *log_file;\
    time_t log_time;\
    struct tm *log_tm;

#define GET_TIME\
    time(&log_time);\
    log_tm = localtime(&log_time);

#define LOG_INIT(filename)                                                     \
    log_file = fopen(filename, "a");

#define TIME_FORMAT\
    log_tm->tm_mday, log_tm->tm_mon + 1, log_tm->tm_hour, log_tm->tm_min, log_tm->tm_sec

#define LOG_ERR(format, ...)                                                   \
    GET_TIME                                                                   \
    fprintf(log_file, "[%2d.%2d %2d:%2d:%2d] %s:%d\t[ERROR]" format "\n",      \
            TIME_FORMAT,                                                       \
            __FILE__, __LINE__, ##__VA_ARGS__);                                \
    fflush(log_file);

#define LOG_SUC(format, ...)                                                   \
    GET_TIME                                                                   \
    fprintf(log_file, "[%2d.%2d %2d:%2d:%2d] %2d %s:%d\t[SUCCESS]"format "\n",     \
            TIME_FORMAT, pthread_self(),                                                       \
            __FILE__, __LINE__, ##__VA_ARGS__);                                \
    fflush(log_file);

#define LOG(format, ...)                                                       \
    GET_TIME                                                                   \
    fprintf(log_file, "[%2d.%2d %2d:%2d:%2d] %s:%d\t[LOG]"format "\n",         \
            TIME_FORMAT,                                                       \
            __FILE__, __LINE__, ##__VA_ARGS__);                                \
    fflush(log_file);

#ifdef _CLI_INTERNAL_H
#define PROGRESS(per) printfProgress(per)

static void
printfProgress(int percentage) 
{
    struct winsize w;
    int lpad, rpad, width, i;

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    width = w.ws_col - 7;
    
    lpad = percentage * width / 100;
    rpad = width - lpad;
    printf("\r%3d%% [", percentage);
    for (i = 0; i < lpad; i++)
        printf("%s", "=");
    for (i = 0; i < rpad; i++)
        printf(" ");
    printf("]");
    fflush(stdout);
    /*printf("\r%3d%% [%.*s%*s]", percentage, lpad,*/
}
#endif
#endif /* no _LOG_H */
