#!/usr/bin/perl

package LCB_Dep;
use strict;
use warnings;

use Getopt::Long;
use Cwd qw(getcwd);
use Class::Struct;

struct __PACKAGE__, [
    'release' => '$',
    'url' => '$',
    'urlbase' => '$',
    'name' => '$',
    'configure_flags' => '$',
];


GetOptions(
    'p|prefix=s' => \my $Prefix,
    'r|release=s' => \my $Release,
    'b|vbrel=s' => \my $Vbucket
);

if ( (!$Prefix) || (!$Release) ) {
    die ("Must have prefix and release");
}

my $p_prefix = "$Prefix/$Release";

my $need_deps = 
    !((-e "$p_prefix/lib/libcouchbase.so" && 
        -e "$p_prefix/lib/libvbucket.so"));

sub sys_or_die {
    my @cmds = shift;
    system(@cmds) == 0 or die "@cmds";
}


if (!$Vbucket) {
    $Vbucket = "1.8.0.4";
}
my $CBURL = "http://packages.couchbase.com/clients/c";


my $old_dir = getcwd();

sub append_to_env {
    my ($var,$add) = @_;
    $ENV{$var} = "$ENV{$var}:$add";
}

append_to_env("LD_LIBRARY_PATH", "$p_prefix/lib");
append_to_env("C_INCLUDE_PATH", "$p_prefix/include");
append_to_env("CPLUS_INCLUDE_PATH", "$p_prefix/include");
append_to_env("LD_RUN_PATH", "$p_prefix/lib");
$ENV{LDFLAGS} = "-L$p_prefix/lib";
$ENV{CPPFLAGS} = "-I$p_prefix/include";

my @pkgs = (
    LCB_Dep->new(
        name => "libvbucket",
        release => $Vbucket,
        urlbase => $CBURL,
    ),
    LCB_Dep->new(
        name => "libevent",
        release => "2.0.19-stable",
        urlbase => "https://github.com/downloads/libevent/libevent",
    ),
    LCB_Dep->new(
        name => "libcouchbase",
        release => $Release,
        urlbase => $CBURL,
        configure_flags => 
        ' --disable-tools --disable-couchbasemock'
    )
);

sub build_deps {
    system("rm -rf $p_prefix");

    foreach my $pkg (@pkgs) {
        chdir $old_dir;
        my $dir = sprintf("%s-%s",  $pkg->name, $pkg->release);
        my $tarball = "$dir.tar.gz";

        if (!-e $tarball) {
            my $url = sprintf("%s/$tarball", $pkg->urlbase);
            sys_or_die("wget $url");
        }

        sys_or_die("rm -rf $dir");
        sys_or_die("tar xf $tarball");

        chdir($dir) or die "chdir: $!";
        sys_or_die("./configure --prefix=$p_prefix " .
            $pkg->configure_flags);
        sys_or_die("make");
        sys_or_die("make install");
    }
}

if ($need_deps) {
    build_deps();
}

chdir $old_dir;

my $fname = "sdkd-cpp-env.sh";
open my $fh, ">", $fname or die "$fname: $!";
print $fh <<"EOF";
export CI_ROOT=$p_prefix/
EOF
