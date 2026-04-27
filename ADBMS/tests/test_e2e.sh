#!/usr/bin/env bash
set -euo pipefail

echo "SELECT * FROM customers LIMIT 5;" | ./qopt >/dev/null
echo "test_e2e: ok"
