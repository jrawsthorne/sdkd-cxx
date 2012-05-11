package CBSdkd::Dataset::Iterator;
use strict;
use warnings;

package CBSdkd::Dataset::Iterator::Inline;
use strict;
use warnings;


sub new {
    my ($cls,$parent) = @_;
    my $ref = $parent->{Items};
    
    # 'Parent' here is simply a hashref of stuff..
    my $o = {
        ref => $ref,
        idx => 0,
        cur => undef,
    };
    bless $o, $cls;
}

sub next {
    my $self = shift;
    use Data::Dumper;
    #print STDERR Dumper($self);
    if ($self->{idx} > $#{$self->{ref}}) {
        $self->{cur} = [];
        return undef;
    }
    my $ret = $self->{ref}->[$self->{idx}];
    $self->{idx}++;
    $self->{cur} = $ret;
    return $ret;
}

sub key {
    my $self = shift;
    if (ref $self->{cur}) {
        return $self->{cur}->[0];
    } else {
        return $self->{cur};
    }
}

sub value {
    my $self = shift;
    if (ref ($self->{cur})) {
        return $self->{cur}->[1];
    }
    return undef;
}

package CBSdkd::Dataset::Iterator::Seeded;
use strict;
use warnings;
use Log::Fu;
sub new {    
    my ($cls,$parent) = @_;
    my $self = {
        idx => 0,
        curk => undef,
        curv => undef,
        parent => $parent
    };
    bless $self, $cls;
}

sub next {
    my $self = shift;
    $self->{curk} = $self->{curv} = undef;
    
    if ($self->{idx} > $self->{parent}->{Count}-1) {
        $self->{cur} = [];
        return undef;
    }
    $self->{idx}++;
    return [];
}

sub _fill_repeat {
    my ($self,$idx, $limit, $repeat_char,$base) = @_;
    
    my $filler = sprintf("%s%d", $repeat_char, $idx);
    # Now figure out how many times we need to repeat it, in order to
    # efficiently append..
    my $blen = length($filler);
    my $multiplier = 1;
    while ($blen * $multiplier < $limit) {
        $multiplier++;
    }
    
    $base .= $filler x $multiplier;
    
    return $base;
    
}

sub key {
    my $self = shift;
    if (defined $self->{curk}) {
        return $self->{curk};
    }
    
    $self->{curk} = $self->_fill_repeat($self->{idx},
                                        $self->{parent}->{KSize},
                                        $self->{parent}->{Repeat},
                                        $self->{parent}->{KSeed}
                                        );
    
    return $self->{curk};
}

sub value {
    my $self = shift;
    if (defined $self->{curv}) {
        return $self->{curv};
    }
    $self->{curv} = $self->_fill_repeat($self->{idx},
                                        $self->{parent}->{VSize},
                                        $self->{parent}->{Repeat},
                                        $self->{parent}->{VSeed});
    return $self->{curv};
}

package CBSdkd::Dataset;
use strict;
use warnings;
use Data::Dumper;
use base qw(Exporter);
our %DSCache;

use CBSdkd::Errors;

our @EXPORT;
push @EXPORT, qw(ref2ds ds_verify_common);


sub new {
    my ($cls,$type,$data) = @_;
    #print STDERR Dumper($data);
    my $o = { %$data };
    if ($type eq 'DSTYPE_SEEDED') {
        $o->{_itercls} = 'CBSdkd::Dataset::Iterator::Seeded';
    } elsif ($type eq 'DSTYPE_INLINE') {
        $o->{_itercls} = 'CBSdkd::Dataset::Iterator::Inline';
    } else {
        die 'NOT IMPLEMENTED ($type)';
    }
    bless $o, $cls;
}

sub iter {
    my $self = shift;
    return $self->{_itercls}->new($self);
}


sub ref2ds {
    my $ref = shift;
    my $ret = [];
    
    if (ref $ref eq 'HASH') {
        while (my ($k,$v) = each %$ref) {
            push @$ret, [ $k, $v ];
        }
    } elsif (ref $ref eq 'ARRAY') {
        foreach (@$ref) {
            push @$ret, [ $_ ];
        }
    } else {
        die "Unsupported reference type " . ref $ref;
    }
    return $ret;
}

sub ds_decode {
    my ($cls,$payload) = @_;
    my $type = $payload->{DSType};
    my $data = $payload->{DS};
    my $dsid = $data->{ID};
    if ($dsid) {
        if (scalar(keys %$data) == 1) {
            my $ds = $DSCache{$data->{ID}};
            if (!$ds) {
                die "SDKD_ERROR: " . SUBSYSf_SDKD|SDKD_ENODS;
            }
            print STDERR "Using Cached DS\n";
            return $ds;
        }
    }
    my $ret = $cls->new($type,$data);
    if ($dsid) {
        $DSCache{$dsid} = $ret;
    }
    return $ret;
}

# returns ($ok,$dataset,$cbo)
# or (undef,$error_response)
sub ds_verify_common {
    my ($msg,$dshash) = @_;
    
    my $params = $msg->payload;
    my $hid = $msg->handle_id;
    my $dataset;
    
    eval {
        $dataset = __PACKAGE__->ds_decode($params);
    }; if ($@) {
        my ($status) = ($@ =~ /SDKD_ERROR:.+(\d+)/);
        $status ||= SUBSYSf_SDKD | ERROR_GENERIC;
        return (undef, $msg->create_response(status => $status,
                                             errstr => $@,
                                             payload => {}));        
    }
        
    eval {
        $msg->cbo
    }; if ($@) {
        
        return (undef, $msg->create_response(
            status => SUBSYSf_SDKD | SDKD_ENOHANDLE,
            errstr => "Can't find handle"
        ));
    }
    
    return (1, $msg->cbo, $dataset);
}

