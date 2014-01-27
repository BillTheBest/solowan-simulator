CC = gcc
OUT_OPTIM = optimizer
OUT_DEOPTIM = deoptimizer
CFLAGS = -g -Wall
SRC = optimizer.c include/hashtable.c include/hashtable.h\
	include/as.c include/as.h include/solowan_rolling.c include/uncomp.c include/solowan_rolling.h\
	include/inout.c include/inout.h\
	include/MurmurHash3.c include/MurmurHash3.h include/solowan.c\
	include/solowan.h
OBJ = include/hashtable.o include/as.o include/solowan_rolling.o\
	 include/inout.o include/MurmurHash3.o include/solowan.o include/uncomp.o
OBJ_OPTIM = optimizer.o $(OBJ)
OBJ_DEOPTIM = deoptimizer.o $(OBJ)

ROLLING_FLAG = -D ROLLING
BASIC_FLAG = -D BASIC

all: rolling

rolling: CFLAGS += $(ROLLING_FLAG)
rolling: optimizer deoptimizer

basic: CFLAGS += $(BASIC_FLAG)
basic: optimizer deoptimizer

optimizer: $(OBJ_OPTIM)
	$(CC) $(CFLAGS) -o $(OUT_OPTIM) $(OBJ_OPTIM)
	
deoptimizer: $(OBJ_DEOPTIM)
	$(CC) $(CFLAGS) -o $(OUT_DEOPTIM) $(OBJ_DEOPTIM)

clean:
	rm -f $(OBJ) optimizer deoptimizer optimizer.o deoptimizer.o

rebuild:	clean all
