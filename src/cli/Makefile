CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -pthread
TARGET = cli_ex
OBJECTS = cli_file.o cli_main.o cli_query.o cli_tcp.o

all : $(TARGET)

$(TARGET) : $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean :
	rm *.o $(TARGET)
