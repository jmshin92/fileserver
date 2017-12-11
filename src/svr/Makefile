CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -pthread -laio -lm
TARGET = fsvr
OBJECTS = svr_cthr.o svr_aio.o svr_job.o svr_meta.o svr_queue.o svr_tcp.o svr_wthr.o

all : $(TARGET)

$(TARGET) : $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean : 
	rm *.o $(TARGET)

