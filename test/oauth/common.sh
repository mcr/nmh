# Common routines for OAuth tests

. "${MH_OBJ_DIR}/test/common.sh"

setup_test

if [ "${OAUTH_SUPPORT}" -eq 0 ]; then
    test_skip 'no oauth support'
fi

testname="${MH_TEST_DIR}/$$"

arith_eval 64001 + $$ % 1000
http_port=${arith_val}

arith_eval ${http_port} + 1
pop_port=${arith_val}

arith_eval ${pop_port} + 1
smtp_port=${arith_val}

cat >> ${MH} <<EOF
oauth-test-scope: test-scope
oauth-test-client_id: test-id
oauth-test-client_secret: test-secret
oauth-test-auth_endpoint: http://127.0.0.1:${http_port}/oauth/auth
oauth-test-token_endpoint: http://127.0.0.1:${http_port}/oauth/token
oauth-test-redirect_uri: urn:ietf:wg:oauth:2.0:oob
EOF

setup_pop() {
    pop_message=${MHTMPDIR}/testmessage
    cat > "${pop_message}" <<EOM
Received: From somewhere
From: No Such User <nosuch@example.com>
To: Some Other User <someother@example.com>
Subject: Hello
Date: Sun, 17 Dec 2006 12:13:14 -0500

Hey man
EOM
}

setup_draft() {
    cat > "${MH_TEST_DIR}/Mail/draft" <<EOF
From: Mr Nobody <nobody@example.com>
To: Somebody Else <somebody@example.com>
Subject: Test
MIME-Version: 1.0
Content-Type: text/plain; charset="us-ascii"

This is a test
EOF
}

start_fakehttp() {
    "${MH_OBJ_DIR}/test/fakehttp" "${testname}.http-req" ${http_port} \
      "${testname}.http-res" > /dev/null
}

start_pop() {
    "${MH_OBJ_DIR}/test/fakepop" "${pop_port}" "$1" "$2" "${pop_message}" \
        > /dev/null
}

start_pop_xoauth() {
    start_pop XOAUTH \
        'dXNlcj1ub2JvZHlAZXhhbXBsZS5jb20BYXV0aD1CZWFyZXIgdGVzdC1hY2Nlc3MBAQ=='
}

start_fakesmtp() {
    "${MH_OBJ_DIR}/test/fakesmtp" "${testname}.smtp-req" ${smtp_port} \
        > /dev/null
}

clean_fakesmtp() {
    rm "${testname}.smtp-req"
}

fake_creds() {
    cat > "${MHTMPDIR}/oauth-test"
}

fake_http_response() {
    echo "HTTP/1.1 $1" > "${testname}.http-res"
    cat >> "${testname}.http-res"
}

clean_fakehttp() {
    rm -f "${testname}.http-res"
}

fake_json_response() {
    (echo 'Content-Type: application/json';
     echo;
     cat) | fake_http_response '200 OK'
}

# The format of the POST request is mostly dependent on curl, and could possibly
# change with newer or older curl versions, or by configuration.  curl 7.39.0
# makes POST requests like this on FreeBSD 10 and Ubuntu 14.04.  If you find
# this failing, you'll need to make this a smarter comparison.  Note that it is
# sorted, so that it doesn't depend on the JSON being in a specific order.
expect_http_post() {
    sort > "${testname}.expected-http-req" <<EOF
POST /oauth/token HTTP/1.1
User-Agent: nmh/${MH_VERSION} ${CURL_USER_AGENT}
Host: 127.0.0.1:${http_port}
Accept: */*
Content-Length: $1
Content-Type: application/x-www-form-urlencoded

$2
EOF
}

expect_http_post_code() {
    expect_http_post 132 'code=code&grant_type=authorization_code&redirect_uri=urn%3Aietf%3Awg%3Aoauth%3A2.0%3Aoob&client_id=test-id&client_secret=test-secret'
}

expect_http_post_refresh() {
    expect_http_post 95 'grant_type=refresh_token&refresh_token=test-refresh&client_id=test-id&client_secret=test-secret'
}

expect_http_post_old_refresh() {
    expect_http_post 94 'grant_type=refresh_token&refresh_token=old-refresh&client_id=test-id&client_secret=test-secret'
}

expect_creds() {
    cat > "${MHTMPDIR}/$$.expected-creds"
}

test_inc() {
    run_test "inc -host 127.0.0.1 -port ${pop_port} -sasl -saslmech xoauth2 -authservice test -user nobody@example.com -width 80" "$@"
}

test_inc_success() {
    test_inc 'Incorporating new mail into inbox...

  11+ 12/17 No Such User       Hello<<Hey man >>'
    check "${pop_message}" "`mhpath +inbox 11`" 'keep first'
}

test_send_no_servers() {
    run_test "send -draft -server 127.0.0.1 -port ${smtp_port} -sasl -saslmech xoauth2 -authservice test -user nobody@example.com" "$@"
}

test_send_only_fakesmtp() {
    start_fakesmtp
    test_send_no_servers "$@"
}

test_send() {
    start_fakehttp
    test_send_only_fakesmtp "$@"
    sort -o "${testname}.http-req.sorted" "${testname}.http-req"
    rm "${testname}.http-req"
    check "${testname}.http-req.sorted" "${testname}.expected-http-req"
}

check_http_req() {
    sort -o "${testname}.http-req.sorted" "${testname}.http-req"
    rm "${testname}.http-req"
    check "${testname}.http-req.sorted" "${testname}.expected-http-req"
}

check_creds_private() {
    f="${MHTMPDIR}/oauth-test"
    if ls -dl "$f" | grep '^-rw-------' > /dev/null 2>&1; then
        :
    else
        echo "$f permissions not private"
        failed=`expr ${failed:-0} + 1`
    fi
}

check_creds() {
    # It's hard to calculate the exact expiration time mhlogin is going to use,
    # so we'll just use sed to remove the actual time so we can easily compare
    # it against our "correct" output.
    f="${MHTMPDIR}/oauth-test"

    sed 's/^\(expire.*:\).*/\1/' "$f" > "$f".notime
    check "$f".notime "${MHTMPDIR}/$$.expected-creds"
    rm "$f"
}
