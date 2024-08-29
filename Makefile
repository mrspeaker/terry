.PHONY: all
all: terry demo keys_test test

CC = gcc
CFLAGS = -Wall -O2 -I.

%: %.c
	$(CC) -o $@ $(CFLAGS) ansi_parse.c $<
