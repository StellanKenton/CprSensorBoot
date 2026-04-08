#!/bin/zsh

set -euo pipefail

device="$1"
interface="$2"
speed="$3"
gdb_port="$4"
swo_port="$5"
telnet_port="$6"
rtt_port="$7"
server_bin="${8:-JLinkGDBServer}"
host="127.0.0.1"
server_pid=""
server_started=0
server_log=""

cleanup() {
  if [[ "$server_started" -eq 1 && -n "$server_pid" ]]; then
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi

  if [[ -n "$server_log" && -f "$server_log" ]]; then
    rm -f "$server_log"
  fi
}

trap cleanup EXIT INT TERM

port_listening() {
  lsof -tiTCP:"$1" -sTCP:LISTEN >/dev/null 2>&1
}

for port in "$gdb_port" "$swo_port" "$telnet_port" "$rtt_port"; do
  pids="$(lsof -tiTCP:"$port" -sTCP:LISTEN 2>/dev/null || true)"
  if [[ -n "$pids" ]]; then
    kill $pids 2>/dev/null || true
  fi
done

sleep 0.3

server_log="$(mktemp -t cpr-rtt.XXXXXX.log)"

"$server_bin" \
  -device "$device" \
  -if "$interface" \
  -speed "$speed" \
  -nohalt \
  -port "$gdb_port" \
  -swoport "$swo_port" \
  -telnetport "$telnet_port" \
  -RTTTelnetPort "$rtt_port" \
  >"$server_log" 2>&1 &

server_pid="$!"
server_started=1

for _ in {1..50}; do
  if port_listening "$rtt_port"; then
    break
  fi

  if ! kill -0 "$server_pid" 2>/dev/null; then
    cat "$server_log"
    exit 1
  fi

  sleep 0.2
done

if ! port_listening "$rtt_port"; then
  cat "$server_log"
  exit 1
fi

print "RTT console ready on ${host}:${rtt_port}"
print "Type directly in this terminal and press Enter to send data. Press Ctrl+C to close RTT."

nc "$host" "$rtt_port"
