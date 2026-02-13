#!/usr/bin/env bash

set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format not found in PATH" >&2
  exit 1
fi

mapfile -t files < <(
  git -C "$repo_root" status --porcelain \
    | awk '{print substr($0,4)}' \
    | grep -E '^clox/src/.*\.(c|h)$' || true
)

if [ "${#files[@]}" -eq 0 ]; then
  echo "No modified clox/src C/C header files to format."
  exit 0
fi

for i in "${!files[@]}"; do
  files[$i]="$repo_root/${files[$i]}"
done

clang-format -i "${files[@]}"

echo "Formatted files:"
printf '%s\n' "${files[@]#$repo_root/}"
