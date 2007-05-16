CFLAGS=-Wall -g -Iinclude/
CPPFLAGS=-Wall -g

vpath %.h include/
vpath %.c src/

.PHONY: clean

all: ftcat

ftcat: ftcat.o
	${CC} -o ftcat ftcat.o

clean: 
	rm -rf *.o ftcat
