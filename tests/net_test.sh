#!/usr/bin/env bash
# Network integration test: starts examples/simple_api_server (port 8000),
# queries it with curl and with Copihue HTTP clients, then stops it.
set -u
cd "$(dirname "$0")/.."
ROOT=$PWD
CC=$ROOT/compiler/bootstrap-c/copihuec
WORK=$(mktemp -d)
gcc -O2 -o "$CC" compiler/bootstrap-c/copihuec.c || exit 1
for p in examples/simple_api_server tests/test_http_simple examples/realtime_api_monitor; do
  "$CC" -O2 -o "$WORK/$(basename $p)" "$p.cph" || { echo "FAIL compile $p"; exit 1; }
done

"$WORK/simple_api_server" > "$WORK/server.log" 2>&1 &
SERVER=$!
cleanup() { kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null; rm -rf "$WORK" "$CC"; }
trap cleanup EXIT

for i in $(seq 1 50); do curl -s -o /dev/null http://127.0.0.1:8000/ && break; sleep 0.1; done

fail=0
expect() {  # $1 = description, $2 = haystack, $3 = needle
  if printf '%s' "$2" | grep -qF -- "$3"; then echo "ok   $1"; else echo "FAIL $1: expected '$3' in:"; printf '%s\n' "$2" | head -20; fail=1; fi
}

out=$(curl -s -i --max-time 5 http://127.0.0.1:8000/api/status)
expect "curl status line"   "$out" "HTTP/1.1 200 OK"
expect "curl content type"  "$out" "Content-Type: application/json"
expect "curl JSON body"     "$out" '"status":"ok"'

out=$(curl -s --max-time 5 http://127.0.0.1:8000/api/status)
expect "request counter (3rd request)"    "$out" '"count":3'

out=$(timeout 10 "$WORK/test_http_simple" 2>&1)
expect "Copihue HTTP client" "$out" '"status":"ok"'

out=$(timeout 30 "$WORK/realtime_api_monitor" 2>&1)
expect "realtime_api_monitor" "$out" "ok"
if printf '%s' "$out" | grep -q "Connection failed"; then echo "FAIL realtime_api_monitor reported a connection failure"; fail=1; fi

expect "server log" "$(cat "$WORK/server.log")" "Request received"
exit $fail
