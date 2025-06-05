CC = gcc

CFLAGS = -Wall -g -O2

SRCS = uds_hal.c util.c uds.c main.c fw_update.c

OBJS = $(SRCS:.c=.o)

TARGET = uds_fw_update

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)