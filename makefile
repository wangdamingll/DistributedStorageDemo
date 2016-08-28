.PHONY:all clean

WORKDIR=.
VPATH = ./src

CC = gcc
CFLAGS = -g -Wall
CPPFLAGS= -I $(WORKDIR)/inc
LIBS = -lfcgi -lhiredis -lm -lmysqlclient

BIN = demo upload data reg_cgi login_cgi 
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

reg_cgi:reg_cgi.o make_log.o  cJSON.o dao_mysql.o util_cgi.o
	$(CC) $^ $(LIBS) -o $@
	mv $@ ./bin

login_cgi:login_cgi.o make_log.o cJSON.o dao_mysql.o util_cgi.o
	$(CC) $^ $(LIBS) -o $@
	mv $@ ./bin

%.o:$.c
	$(CC) -c $< $(CFLAGS) $(CPPFLAGS) -o $@
	
clean:
	rm -rf *.o logs ./bin/logs
