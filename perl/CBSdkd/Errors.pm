################################################################################
################################################################################
################################################################################
### Constants/Error Codes                                                    ###
################################################################################
################################################################################
################################################################################

# Define the error codes. The boring part is generated automatically via
# the srcutil/errdefs.pl and copy-pasted here.

# Error codes are a bitflag of two integers, the subsystem and the minor code.
# The subsystem is a 'SUBSYSf_' constant, the error code is prefixed by the name
# of the subsystem..

package CBSdkd::Errors;
use strict;
use warnings;
use Couchbase::Client::Errors;

use base qw(Exporter);
our @EXPORT;
use Constant::Generate {
    SUBSYSf_UNKNOWN      => 0x1,
    SUBSYSf_CLUSTER      => 0x2,
    SUBSYSf_CLIENT       => 0x4,
    SUBSYSf_MEMD         => 0x8,
    SUBSYSf_NETWORK      => 0x10,
    SUBSYSf_SDKD         => 0x20,
    SUBSYSf_KVOPS        => 0x40,
    
    SDKD_EINVAL          => 0x200,
    SDKD_ENOIMPL         => 0x300,
    SDKD_ENOHANDLE       => 0x400,
    SDKD_ENODS           => 0x500,
    SDKD_ENOREQ          => 0x600,
    
    ERROR_GENERIC        => 0x100,
    
    CLIENT_ETMO          => 0x200,
    
    CLUSTER_EAUTH        => 0x200,
    CLUSTER_ENOENT       => 0x300,
    
    MEMD_ENOENT          => 0x200,
    MEMD_ECAS            => 0x300,
    MEMD_ESET            => 0x400,
    MEMD_EVBUCKET        => 0x500,
    
    KVOPS_EMATCH         => 0x200,
    
}, export => 1, type => 'bit';

our %MutationMap = (
    APPEND => 'append',
    PREPEND => 'prepend',
    SET => 'set',
    REPLACE => 'replace',
);

# Map Couchbase::Client error codes unto ones SDKD understands.
our %ERRMAP = (
    COUCHBASE_BUCKET_ENOENT,    SUBSYSf_CLUSTER | CLUSTER_ENOENT,
    COUCHBASE_AUTH_ERROR,       SUBSYSf_CLUSTER | CLUSTER_EAUTH,
    
    COUCHBASE_CONNECT_ERROR,    SUBSYSf_NETWORK | ERROR_GENERIC,
    COUCHBASE_NETWORK_ERROR,    SUBSYSf_NETWORK | ERROR_GENERIC,
    
    COUCHBASE_ENOMEM,           SUBSYSf_MEMD    | ERROR_GENERIC,
    
    
    COUCHBASE_KEY_ENOENT,       SUBSYSf_MEMD    | MEMD_ENOENT,
    
    COUCHBASE_ETIMEDOUT,        SUBSYSf_CLIENT  | CLIENT_ETMO,
    
    # This could either be a network error, or a client error :/
    COUCHBASE_ETMPFAIL,         SUBSYSf_CLIENT  | ERROR_GENERIC | SUBSYSf_NETWORK
);

push @EXPORT, 'extract_error_common';

sub extract_error_common {
    my $cberr = shift;
    my $ecode = $ERRMAP{$cberr};
    if (!$ecode) {
        warn "Couldn't map error $cberr";
    }
    return $ecode;
}
