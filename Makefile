CC = gcc
OUT_EXE1 = optimizer
OUT_EXE2 = deoptimizer
CFLAGS = -g -Wall
SRC = optimizer.c include/dedup01.c include/dedup01.h\
	include/as.c include/as.h include/dedup02.c include/dedup02.h\
	include/inout.c include/inout.h\
	include/murmurhash.c include/murmurhash.h include/solowan.c\
	include/solowan.h include/uncomp.c
OBJ = include/dedup01.o include/as.o include/dedup02.o\
	 include/inout.o include/murmurhash.o include/solowan.o
OBJ_OPTIM = optimizer.o $(OBJ)
OBJ_DEOPTIM = deoptimizer.o $(OBJ)

all: optimizer deoptimizer

optimizer: $(OBJ_OPTIM)
	$(CC) $(CFLAGS) -o $(OUT_EXE1) $(OBJ_OPTIM)
deoptimizer: $(OBJ_DEOPTIM)
	$(CC) $(CFLAGS) -o $(OUT_EXE2) $(OBJ_DEOPTIM)

clean:
	rm -f $(OBJ) optimizer deoptimizer optimizer.o deoptimizer.o

rebuild:	clean build
