CC = gcc
OUT_EXE1 = optimizer
OUT_EXE2 = deoptimizer
CFLAGS = -g -Wall
SRC = optimizer.c include/hashtable.c include/hashtable.h\
	include/as.c include/as.h include/solowan_rolling.c include/uncomp.c include/solowan_rolling.h\
	include/inout.c include/inout.h\
	include/murmurhash.c include/murmurhash.h include/solowan.c\
	include/solowan.h
OBJ = include/hashtable.o include/as.o include/solowan_rolling.o\
	 include/inout.o include/murmurhash.o include/solowan.o include/uncomp.o
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
