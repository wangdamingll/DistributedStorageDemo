.PHONY:all clean

WORKDIR=.
VPATH = ./src

CC = gcc
CFLAGS = -g -Wall
CPPFLAGS= -I $(WORKDIR)/inc
LIBS = -lfcgi -lhiredis -lm

BIN = demo upload data 
all:$(BIN)

demo:demo.o make_log.o  redis_op.o cJSON.o
	$(CC) $^ $(LIBS) -o $@
	mv $@ ./bin
	
upload:upload.o make_log.o redis_op.o cJSON.o
	$(CC) $^ $(LIBS) -o $@
	mv $@ ./bin

data:data.o make_log.o redis_op.o cJSON.o
	$(CC) $^ $(LIBS) -o $@
	mv $@ ./bin

%.o:$.c
	$(CC) -c $< $(CFLAGS) $(CPPFLAGS) -o $@
	
clean:
	#rm -rf *.o $(BIN) logs
	rm -rf *.o logs ./bin/logs
