#!/bin/sh
#
# Common routines for the post tests
#

set -e

. "${MH_OBJ_DIR}/test/common.sh"

setup_test

arith_eval 64000 + $$ % 1000
localport=$arith_val
testname="${MH_TEST_DIR}/$$"

#
# Set this for the EHLO command
#

echo "clientname: nosuchhost.example.com" >> ${MHMTSCONF}

#
# One "post" test run.  Ok, yeah, we're using "send", but that's just
# because it's easier.
# $1: output filename for fakesmtp, i.e., the sent message
# $2: expected output
# $3: optional switches for send

test_post ()
{ pid=`"${MH_OBJ_DIR}/test/fakesmtp" "$1" $localport`

    run_prog send -draft -server 127.0.0.1 -port $localport $3

    #
    # It's hard to calculate the exact Date: header post is going to
    # use, so we'll just use sed to remove the actual date so we can easily
    # compare it against our "correct" output.
    #

    sed -e 's/^Date:.*/Date:/' "$1" > "$1".nodate
    rm -f "$1"

    check "$1".nodate "$2"
}
