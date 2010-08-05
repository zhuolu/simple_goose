
 
TARGET = gs_tran gs_recv

CC := gcc
SRCS := gs_tran.c gs_recv.c nl_if_goose.c
OBJS = $(SRCS:.c=.o)

INC_PATH = ../src

CFLAGS = -fmessage-length=0 -Wall -O2 -g
LFLAGS = -lpthread

all:	$(TARGET)

$(TARGET):	$(OBJS)
	$(CC) $(CFLAGS) $(LFLAGS) -o $(PWD)/gs_tran gs_tran.o nl_if_goose.o
	$(CC) $(CFLAGS) $(LFLAGS) -o $(PWD)/gs_recv gs_recv.o nl_if_goose.o

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS) -I$(INC_PATH) -c $<

.PONY: clean run

clean:
	@rm -f $(OBJS) $(TARGET)
