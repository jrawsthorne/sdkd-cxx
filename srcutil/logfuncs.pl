#!/usr/bin/perl
use strict;
use warnings;

# generate explicit and implicit logging functions..

foreach my $base (qw(trace info debug warn error crit)) {
    printf("#define cbsdkd_log_%s(ctx, fmt, ...) \\\n", $base);
    printf("    cbsdkd_logger(ctx, CBSDKD_LOGLVL_%s, ".
        "__LINE__, __func__, fmt, ## __VA_ARGS__)\n",
        uc($base));

    printf("#define log_%s(fmt, ...) \\\n", $base);
    printf("    cbsdkd_log_%s(&this->cbsdkd__debugctx, fmt, ## __VA_ARGS__)\n",
        $base);

    printf("#define log_noctx_%s(fmt, ...) \\\n", $base);
    printf("    cbsdkd_log_%s(&CBsdkd_Global_Debug_Context.cbsdkd__debugctx, \\\n".
        "    fmt, ## __VA_ARGS__)\n",
            $base);
    printf("\n");
}
