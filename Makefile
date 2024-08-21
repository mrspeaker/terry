.PHONY: all
all: terry demo

CC = gcc
CFLAGS = -Wall -O2

%: %.c
	gcc -o $@ $<
