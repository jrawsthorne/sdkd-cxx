#!/usr/bin/perl
use strict;
use warnings;

# This generates error definitions for multiple languaes
use Getopt::Long;
GetOptions(
    'l|lang=s' => \my $Language,
    'p|prefix=s' => \my $Prefix
);


$Prefix ||= "";
$Language ||= "python";

my ($kvsep,$pairdelim,$str_decorate);
if ($Language =~ /perl/) {
    $kvsep = '=>';
    $pairdelim = ",";
    $str_decorate = sub { $_[0] };

} elsif ($Language =~ /python/) {
    $kvsep = ':';
    $pairdelim = ",";
    $str_decorate = sub { sprintf("'%s'", $_[0] ) };

} elsif (uc($Language) eq 'C') {
    $kvsep = '=';
    $pairdelim = ',';
    $str_decorate = sub { $_[0] };
}

my %Subsystems = (
    UNKNOWN => 0x1,
    CLUSTER => 0x2,
    CLIENT  => 0x4,
    MEMD    => 0x8,
    NETWORK => 0x10,
    SDKD    => 0x20,
    KVOPS   => 0x40,
);


my %ErrHash = (
    'ERROR' => {
        GENERIC => 0x100
    },
    'MEMD' => {
        ENOENT  => 0x200,
        ECAS    => 0x300,
        ESET    => 0x400,
        EVBUCKET=> 0x500,
    },
    'CLUSTER' => {
        EAUTH   => 0x200,
        ENOENT  => 0x300
    },
    'CLIENT' => {
        ETMO    => 0x200,
    },
    'SDKD'  => {
        EINVAL  => 0x200,
        ENOIMPL => 0x300,
        ENOHANDLE=> 0x400,
        ENODS   => 0x500,
        ENOREQ  => 0x600,
    },
    'KVOPS' => {
        'EMATCH' => 0x200
    }
);


sub _print_hash {
    my ($prefix,$href) = @_;
    my @order = sort { $href->{$a} <=> $href->{$b} } keys %$href;

    foreach my $k (@order) {
        my $v = $href->{$k};
        printf("%-20s %s 0x%x%s\n",
               $str_decorate->($prefix.$k), $kvsep, $v, $pairdelim);
    }
}

_print_hash($Prefix.'SUBSYSf_', \%Subsystems);
printf("\n");

while (my ($k,$v) = each %ErrHash) {
    _print_hash($Prefix.$k."_", $v);
    printf("\n");
}
