#!/usr/bin/perl
use strict;
use warnings;
my @CONSTANTS = qw(
SUCCESS
BUCKET_ENOENT
AUTH_ERROR
CONNECT_ERROR
NETWORK_ERROR
ENOMEM
KEY_ENOENT
ETIMEDOUT
ETMPFAIL
APPEND
PREPEND
SET
REPLACE
ADD
);

foreach (@CONSTANTS) {
	my $legacy = "LIBCOUCHBASE_$_";
	my $new = "LCB_$_";
	printf("#define %-30s %s\n", $legacy, $new);
}

printf("\n\n");

print "#define libcouchbase_t lcb_t\n";
my @LC_SYMS = qw(
size_t
cas_t
uint16_t
uint32_t
int64_t
time_t
error_t

get_cookie
set_cookie
get_version
destroy
connect
set_timeout
set_error_callback
set_touch_callback
set_remove_callback
set_get_callback
strerror
wait
storage_t
);

foreach (@LC_SYMS) {
	my $legacy = "libcouchbase_$_";
	my $new = "lcb_$_";
	printf("#define %-30s %s\n", $legacy, $new);
}
