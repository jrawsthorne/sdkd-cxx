#!/bin/sh
# This acts "like" a compiler..
set -e

# Check if we've compiled stuff already
STATUS_FILE=".have_views_status"
if [ -e $STATUS_FILE ]; then
    cat $STATUS_FILE
    exit 0
fi

echo "$@" > TEST_COMPILE_CMD
PROG='int main(void) { lcb_get_version(); return 0; }'
echo $PROG | $@
echo "Have views" > $STATUS_FILE
cat $STATUS_FILE

