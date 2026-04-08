#!/bin/zsh

set -euo pipefail

host="$1"
port="$2"

exec nc "$host" "$port"
