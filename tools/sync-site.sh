#!/bin/sh
# Mirrors the simlib arcade demo pages and downloads plus the simlib SDK from the local web root to a remote host.
# Only the listed paths are sent; the rest of the web root (kosmos, large files, ...) is never touched.
#   tools/sync-site.sh [--dry-run] [--host user@host]
set -e

web=${WEB_ROOT:-/var/www/html}
host=${HOST:-dave@gemini}
dry=""
while [ $# -gt 0 ]; do
    case "$1" in
        --dry-run) dry="--dry-run" ;;
        --host) host=$2; shift ;;
        *) echo "usage: $0 [--dry-run] [--host user@host]" >&2; exit 2 ;;
    esac
    shift
done

ssh -o BatchMode=yes -o ConnectTimeout=10 "$host" true ||
    { echo "cannot reach $host with passwordless ssh" >&2; exit 1; }

opts="-rlt --omit-dir-times --partial -i $dry"
for d in simlib exinvaders exbomberman exdefender exasteroids; do
    [ -d "$web/$d" ] || continue
    ssh "$host" "mkdir -p '$web/$d'"
    rsync $opts "$web/$d/" "$host:$web/$d/"
done
for f in index.html img/card-exinvaders.jpg img/card-exbomberman.jpg img/card-exdefender.jpg img/card-exasteroids.jpg img/card-simlib.jpg; do
    [ -f "$web/$f" ] || continue
    ssh "$host" "mkdir -p '$web/$(dirname "$f")'"
    rsync $opts "$web/$f" "$host:$web/$f"
done
echo "synced to $host:$web"
