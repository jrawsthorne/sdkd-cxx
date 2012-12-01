all: test

# test: main.c viewrow.o viewopts.o jsonsl.o

LCB_DIR=/sources/libcouchbase/inst
CPPFLAGS=-g -ggdb3 -O0 -Wall \
		 -I$(LCB_DIR)/include \
		 -I../contrib/jsonsl \
		 -DJSONSL_STATE_GENERIC \
		 -pthread

LDFLAGS=-L$(LCB_DIR)/lib -Wl,-rpath=$(LCB_DIR)/lib \
		-lcouchbase

jsonsl.o: ../contrib/jsonsl/jsonsl.c
	$(CC) -c -o $@ $(CPPFLAGS) $^

test: main.c viewrow.o viewopts.o jsonsl.o
	$(CC) $(CPPFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f main *.o
