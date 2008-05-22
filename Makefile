CFLAGS=-Wall -g -Iinclude/ -m64
CPPFLAGS=-Wall -g

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
