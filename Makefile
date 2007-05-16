CFLAGS=-Wall -g
CPPFLAGS=-Wall -g

.PHONY: clean

all: ftcat


ftcat: ftcat.o
	cc -o ftcat ftcat.o

clean: 
	rm -rf *.o ftcat
