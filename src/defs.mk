all :: 
JSONCPP_ROOT=$(shell pwd)/contrib/json-cpp
JSONCPP_LFLAGS=
JSONCPP_CPPFLAGS=-I$(JSONCPP_ROOT)/dist/

ifdef LCB_ROOT
	LCB_CPPFLAGS := -I$(LCB_ROOT)/include
	LCB_LFLAGS := -L$(LCB_ROOT)/lib -Wl,-rpath=$(LCB_ROOT)/lib -lcouchbase
else
	LCB_LFLAGS := -lcouchbase
	LCB_CPPFLAGS :=
endif

CXX := g++
CC := gcc

OBJECTS := \
	Message.o \
	Request.o \
	Dataset.o \
	Response.o \
	Handle.o \
	ResultSet.o \
	IODispatch.o \
	ThreadUnix.o \
	Worker.o \
	sockutil.o \
	\
	ViewExecutor.o \
	ViewLoader.o \
	views/viewrow.o \
	views/viewopts.o \
	\
	contrib/debug.o \
	contrib/jsonsl/jsonsl.o \
	contrib/json-cpp/dist/jsoncpp.o \
	contrib/cliopts/cliopts.o

LDFLAGS += $(LFLAGS)
LDFLAGS += -lpthread
LDFLAGS += $(JSONCPP_LFLAGS)
LDFLAGS += $(LCB_LFLAGS)
LDFLAGS += -lrt

ifdef PROFILE
	LDFLAGS += -lprofiler
endif

CPPFLAGS = -g -Wall \
		   -pthread \
		   -Icontrib/jsonsl -DJSONSL_STATE_GENERIC \
		   -Icontrib/cliopts

CPPFLAGS += $(LCB_CPPFLAGS)
CXXFLAGS += $(JSONCPP_CPPFLAGS) -Wno-reorder
