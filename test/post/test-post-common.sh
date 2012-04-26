#!/bin/sh
#
# Common routines for the post tests
#

set -e

. "${MH_OBJ_DIR}/test/common.sh"

setup_test

localport=65412
testname="${MH_TEST_DIR}/$$"

#
# Set this for the EHLO command
#

echo "clientname: nosuchhost.example.com" >> ${MHMTSCONF}

#
# One "post" test run.  Ok, yeah, we're using "send", but that's just
# because it's easier.
#

test_post ()
{ "${MH_OBJ_DIR}/test/fakesmtp" "$1" $localport &
    pid="$!"

    send -draft -server 127.0.0.1 -port $localport || exit 1

    wait ${pid}

    #
    # It's hard to calculate the exact Date: header post is going to
    # use, so we'll just use sed to remove the actual date so we can easily
    # compare it against our "correct" output.
    #

    sed -e 's/^Date:.*/Date:/' "$1" > "$1".nodate
    rm -f "$1"

    check "$1".nodate "$2"
}
