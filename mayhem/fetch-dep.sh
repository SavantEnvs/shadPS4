#!/usr/bin/env bash
# fetch-dep.sh <name> <git url> <commit>: shallow-fetch one pinned commit into
# /opt/toolchains/deps/<name>-<commit7>/src (used only by mayhem/Dockerfile).
set -euo pipefail
name=$1 url=$2 sha=$3
d=/opt/toolchains/deps/${name}-${sha:0:7}/src
mkdir -p "$d" && cd "$d"
git init -q && git remote add origin "$url"
git fetch -q --depth 1 origin "$sha" && git checkout -q FETCH_HEAD
[ "$(git rev-parse HEAD)" = "$sha" ]
rm -rf .git
