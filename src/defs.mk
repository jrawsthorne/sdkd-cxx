JSONCPP_LFLAGS := $(shell pkg-config jsoncpp --libs)
JSONCPP_CPPFLAGS := $(shell pkg-config jsoncpp --cflags)

LCB_LFLAGS := -lcouchbase
LCB_CPPFLAGS :=

CXX := g++
CC := gcc

OBJECTS := \
	Message.o \
	Request.o \
	Dataset.o \
	Response.o \
	contrib/debug.o \
	Handle.o \
	ResultSet.o \
	IODispatch.o


LDFLAGS = -lpthread
LDFLAGS += $(JSONCPP_LFLAGS)
LDFLAGS += $(LCB_LFLAGS)

CPPFLAGS = -g -Wall
CPPFLAGS += -Wno-reorder -pthread
CPPFLAGS += $(JSONCPP_CPPFLAGS) $(LCB_CPPFLAGS)
