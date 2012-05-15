all: main sdkd_lcb

LDFLAGS=$(shell pkg-config --libs jsoncpp) -lcouchbase -lpthread
CPPFLAGS=-g -Wall $(shell pkg-config --cflags jsoncpp)
CPPFLAGS += -Wno-reorder -pthread

BINLFLAGS := -Wl,-rpath='$$ORIGIN' -L$(shell pwd) -lcbsdkd
CXX := clang++

OBJECTS =Message.o Request.o Dataset.o Response.o contrib/debug.o
OBJECTS += Handle.o
OBJECTS += IODispatch.o

libcbsdkd.so: $(OBJECTS)
	$(CXX) -shared -o $@ $^ $(LDFLAGS)

main: main.cpp libcbsdkd.so
	$(CXX) -o $@ $(CPPFLAGS) $< $(BINLFLAGS) $(LDFLAGS)


%.o: %.cpp
	$(CXX) -fPIC -c -o $@ $^ $(CPPFLAGS)

contrib/debug.o: contrib/debug.c
	$(CC) -g -fPIC -c -o $@ $^

sdkd_lcb: sdkd_lcb.cpp libcbsdkd.so
	$(CXX) -o $@ $(CPPFLAGS) $< $(BINLFLAGS)

clean:
	rm -f main $(OBJECTS) *.so *.o main sdkd_lcb
