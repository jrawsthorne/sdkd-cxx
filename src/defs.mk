all :: 


JSONCPP_LFLAGS := $(shell pkg-config jsoncpp --libs)
JSONCPP_CPPFLAGS := $(shell pkg-config jsoncpp --cflags)

LCB_LFLAGS := -lcouchbase
LCB_CPPFLAGS :=

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


LDFLAGS = -lpthread
LDFLAGS += $(JSONCPP_LFLAGS)
LDFLAGS += $(LCB_LFLAGS)

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

