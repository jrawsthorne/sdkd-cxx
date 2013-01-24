all :: 

JSONCPP_HAVE_DIST := $(shell pkg-config --exists jsoncpp && echo OK)
JSONCPP_ROOT=$(shell pwd)/contrib/json-cpp

JSONCPP_CPPFLAGS := \
	$(if $(JSONCPP_HAVE_DIST), \
		$(shell pkg-config --cflags jsoncpp), \
		-I"$(JSONCPP_ROOT)/include")

JSONCPP_LFLAGS := \
	$(if $(JSONCPP_HAVE_DIST),\
		$(shell pkg-config --libs jsoncpp),\
		-L"$(JSONCPP_ROOT)/lib" -Wl,-rpath="$(JSONCPP_ROOT)/lib" -ljson-cpp)

ifdef LCB_ROOT
	LCB_CPPFLAGS := -I$(LCB_ROOT)/include
	LCB_LFLAGS := -L$(LCB_ROOT)/lib -Wl,-rpath=$(LCB_ROOT)/lib -lcouchbase
else
	LCB_LFLAGS := -lcouchbase
	LCB_CPPFLAGS :=
endif

CXX := g++
CC := gcc

VIEWOBJECTS := \
	ViewExecutor.o \
	ViewLoader.o \
	views/viewrow.o \
	views/viewopts.o

OBJECTS := \
	Message.o \
	Request.o \
	Dataset.o \
	Response.o \
	Handle.o \
	ResultSet.o \
	IODispatch.o \
	\
	contrib/debug.o \
	contrib/jsonsl/jsonsl.o \
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

CPPFLAGS += -Wno-reorder 
CPPFLAGS += $(JSONCPP_CPPFLAGS) $(LCB_CPPFLAGS)

_HAVE_VIEWS := $(shell build_helpers/have_views.sh $(CC) -xc - $(CPPFLAGS) $(LCB_LFLAGS))

ifdef _HAVE_VIEWS
	OBJECTS += $(VIEWOBJECTS)
endif

clean ::
	rm -f .have_views_status

