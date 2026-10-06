#!/bin/bash
set -e

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
make -C "$repo_dir/src" clean
