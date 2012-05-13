all: main

LDFLAGS=$(shell pkg-config --libs jsoncpp) -lcouchbase
CPPFLAGS=-g -Wall $(shell pkg-config --cflags jsoncpp)
CXX := clang++

OBJECTS=Message.o Request.o Dataset.o Response.o
OBJECTS+= Handle.o

main: main.cpp $(OBJECTS)
	$(CXX) -o $@ $(CPPFLAGS) $^ $(LDFLAGS)


%.o: %.cpp
	$(CXX) -c -o $@ $^ $(CPPFLAGS)

clean:
	rm -f main $(OBJECTS)
