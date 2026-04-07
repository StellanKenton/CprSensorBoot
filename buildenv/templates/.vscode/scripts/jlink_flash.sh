#!/bin/zsh

set -euo pipefail

device="$1"
interface="$2"
speed="$3"
image="$4"
jlink_exe="${5:-JLinkExe}"

if [[ ! -f "$image" ]]; then
  echo "Firmware image not found: $image" >&2
  exit 1
fi

{
  print -r -- "r"
  print -r -- "h"
  print -r -- "loadfile $image"
  print -r -- "r"
  print -r -- "g"
  print -r -- "qc"
} | "$jlink_exe" -device "$device" -if "$interface" -speed "$speed"
