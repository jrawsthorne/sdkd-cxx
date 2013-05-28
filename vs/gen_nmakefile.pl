#!/usr/bin/perl
use strict;
use warnings;

my $OBJS = [
    ["Message.cpp"],
    ["Request.cpp"],
    ["Dataset.cpp"],
    ["Response.cpp"],
    ["Handle.cpp"],
    ["ResultSet.cpp"],
    ["IODispatch.cpp"],
    ["ThreadWin32.cpp"],
    ["Worker.cpp"],

    ["sockutil.c"],
    ["win32-gettimeofday.c"],

    ["ViewExecutor.cpp"],
    ["ViewLoader.cpp"],

    ["views", "viewrow.c"],
    ["views", "viewopts.c"],
    ["contrib", "debug.c"],
    ["contrib", "jsonsl", "jsonsl.c"],
    ["contrib", "cliopts", "cliopts.c"],
    ["contrib", "json-cpp", "dist", "jsoncpp.cpp"],

    ["sdkd_lcb.cpp"]
];

my $TEMPLATE = <<'EOF';
SDKD_SRC=..\src
LCB_HDRS = lib\libcouchbase-2.0.6_x86_vc10\include
DIR_OUT = bin
DIR_OBJS = objs

LDFLAGS = /LIBPATH:$(LCB_HDRS)\..\lib
LD_DEPS = ws2_32.lib kernel32.lib user32.lib libcouchbase.lib

CPPFLAGS=/I$(LCB_HDRS) \
	/I$(SDKD_SRC) \
	/I$(SDKD_SRC)\views \
	/I$(SDKD_SRC)\contrib \
	/I$(SDKD_SRC)\contrib\jsonsl \
	/I$(SDKD_SRC)\contrib\cliopts \
	/I$(SDKD_SRC)\contrib\json-cpp\dist \
	-nologo \
	/MP \
	/MTd

CXXFLAGS=/EHsc
EOF

print $TEMPLATE . "\n";

my $objfiles = [];

foreach my $o (@$OBJS) {
    my $srcfile = join('\\', @$o);
    my $objfile = $srcfile;
    $objfile =~ s/\.c.*$/.obj/;
    $objfile = '$(DIR_OBJS)\\' . $objfile;
    my $flags = '$(CPPFLAGS)';
    if ($srcfile =~ /\.cpp$/) {
        $flags .= ' $(CXXFLAGS)';
    }

    print <<"EOF";

$objfile: \$(SDKD_SRC)\\$srcfile
	CL /c /Fo"$objfile" \$** $flags

EOF
    
    push @$objfiles, $objfile;
}

print "OBJ_FILES= \\\n";
foreach my $of (@$objfiles) {
    print "\t$of \\\n"
}
print "\n";

print <<'EOF';
APPFILE=bin\sdkd-lcb.exe

$(APPFILE): $(OBJ_FILES)
	LINK /out:$@ \
		$(LDFLAGS) \
		/MACHINE:X86 \
		/SUBSYSTEM:CONSOLE \
		$(OBJ_FILES) $(LD_DEPS)

clean:
	FOR %O IN ($(OBJ_FILES)) DO DEL %O
	DEL $(APPFILE)

all: $(APPFILE)
EOF

