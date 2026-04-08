#!/bin/zsh

set -euo pipefail

device="$1"
interface="$2"
speed="$3"
jlink_exe="${4:-JLinkExe}"

{
  print -r -- "r"
  print -r -- "g"
  print -r -- "qc"
} | "$jlink_exe" -device "$device" -if "$interface" -speed "$speed"
