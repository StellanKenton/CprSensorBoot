#!/bin/zsh

set -euo pipefail

host="$1"
port="$2"
payload="$3"

print -r -- "$payload" | nc "$host" "$port"
