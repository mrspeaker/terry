.PHONY: all
all: terry demo keys

CC = gcc
CFLAGS = -Wall -O2

%: %.c
	gcc -o $@ $<
