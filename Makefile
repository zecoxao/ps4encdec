CC=gcc
CFLAGS = 

ifeq ($(DEBUG), 1)
CFLAGS+=-g -O0
else
CFLAGS+=-O2
endif

OUT=ps4encdec

OBJ=main.o aes.o aes_xts.o util.o

all: $(OBJ)
	$(CC) $(CFLAGS)	-o $(OUT) $(OBJ)

clean:
	rm -f *.o $(OUT) *~