#!/usr/bin/env bash
set -euo pipefail

if [[ ! -x ./qopt ]]; then
  echo "qopt not found, run make first"
  exit 1
fi

echo "Running starter benchmark script"
echo "BENCH run" | ./qopt
