all: main

LDFLAGS=$(shell pkg-config --libs jsoncpp) -lcouchbase -lpthread
CPPFLAGS=-g -Wall $(shell pkg-config --cflags jsoncpp)
CPPFLAGS += -Wno-reorder -pthread
CXX := clang++

OBJECTS =Message.o Request.o Dataset.o Response.o
OBJECTS += Handle.o
OBJECTS += IODispatch.o

main: main.cpp $(OBJECTS)
	$(CXX) -o $@ $(CPPFLAGS) $^ $(LDFLAGS)


%.o: %.cpp
	$(CXX) -c -o $@ $^ $(CPPFLAGS)

clean:
	rm -f main $(OBJECTS)
