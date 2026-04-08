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

exec "$server_bin" \
  -device "$device" \
  -if "$interface" \
  -speed "$speed" \
  -nohalt \
  -port "$gdb_port" \
  -swoport "$swo_port" \
  -telnetport "$telnet_port" \
  -RTTTelnetPort "$rtt_port"
