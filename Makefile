ARCH=$(shell uname -m | sed -e s/i.86/i386/)

INC= -Iinclude

ifeq ($(ARCH),sparc64)
  CFLAGS= -Wall -g ${INC} -mcpu=v9 -m64
else
  CFLAGS= -Wall -g ${INC}
endif

vpath %.h include/
vpath %.c src/

.PHONY: clean

TOOLS = ftcat ft2csv

all: ${TOOLS}

FTCAT_OBJ = ftcat.o timestamp.o
ftcat: ${FTCAT_OBJ}
	${CC} ${CFLAGS} -o ftcat ${FTCAT_OBJ}

FT2CSV_OBJ = ft2csv.o mapping.o timestamp.o
ft2csv: ${FT2CSV_OBJ}
	${CC} ${CFLAGS} -o ft2csv ${FT2CSV_OBJ}

clean: 
	rm -rf *.o ${TOOLS}
