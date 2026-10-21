#!/usr/bin/env bash
# Capture a screenshot of each graphical example using xdotool and ImageMagick.
#
# Usage: tools/capture_examples.sh [-b build_dir] [-o out_dir] [-t table] [name[:suffix] ...]
#
# The table (tools/capture_examples.tsv) has tab-separated columns:
#   output-name  executable  args  startup-delay  steps
# Use "-" for empty args (tab is whitespace, so empty fields would collapse).
# `steps` is a ';'-separated list of: key:<xdotool key>, keys:<text to type>, click:<x>,<y>,
# move:<x>,<y>, drag:<x1>,<y1>,<x2>,<y2>, hold:<key>,<seconds>, sleep:<seconds>.
# Coordinates are relative to the example window. Needs an X display.
set -u

root="$(cd "$(dirname "$0")/.." && pwd)"
build="$root/build"
out="$root/docs/examples/img"
table="$root/tools/capture_examples.tsv"

while getopts "b:o:t:" opt; do
    case $opt in
        b) build=$OPTARG ;;
        o) out=$OPTARG ;;
        t) table=$OPTARG ;;
        *) exit 2 ;;
    esac
done
shift $((OPTIND - 1))

for tool in xdotool import; do
    command -v "$tool" >/dev/null || { echo "missing $tool" >&2; exit 1; }
done
mkdir -p "$out"

wait_window() {
    local pid=$1 wid=""
    for _ in $(seq 1 100); do
        kill -0 "$pid" 2>/dev/null || return 1
        wid=$(xdotool search --pid "$pid" 2>/dev/null | tail -n 1)
        if [ -n "$wid" ] && xdotool getwindowgeometry "$wid" >/dev/null 2>&1; then
            echo "$wid"
            return 0
        fi
        sleep 0.1
    done
    return 1
}

run_steps() {
    local wid=$1 steps=$2 step kind arg
    local IFS=';'
    for step in $steps; do
        kind=${step%%:*}
        arg=${step#*:}
        case $kind in
            key) xdotool key --window "$wid" $arg ;;
            keys) xdotool type --window "$wid" --delay 40 "$arg" ;;
            sleep) sleep "$arg" ;;
            move) IFS=, read -r x y <<<"$arg"; xdotool mousemove --window "$wid" "$x" "$y" ;;
            click) IFS=, read -r x y <<<"$arg"; xdotool mousemove --window "$wid" "$x" "$y" click 1 ;;
            drag)
                IFS=, read -r x1 y1 x2 y2 <<<"$arg"
                xdotool mousemove --window "$wid" "$x1" "$y1" mousedown 1
                sleep 0.2
                xdotool mousemove --window "$wid" "$x2" "$y2"
                sleep 0.2
                xdotool mouseup 1
                ;;
            hold)
                IFS=, read -r k secs <<<"$arg"
                xdotool keydown --window "$wid" "$k"; sleep "$secs"; xdotool keyup --window "$wid" "$k"
                ;;
        esac
    done
}

capture_one() {
    local name=$1 exe=$2 args=$3 delay=$4 steps=$5
    if [ ! -x "$build/$exe" ]; then
        echo "SKIP $name: $build/$exe not built" >&2
        return 1
    fi
    (cd "$root" && exec "$build/$exe" $args) >/dev/null 2>&1 &
    local pid=$!
    local wid
    if ! wid=$(wait_window "$pid"); then
        echo "FAIL $name: no window" >&2
        kill "$pid" 2>/dev/null
        return 1
    fi
    sleep "$delay"
    xdotool windowfocus "$wid" 2>/dev/null
    xdotool mousemove --window "$wid" 400 300 2>/dev/null
    [ -n "$steps" ] && run_steps "$wid" "$steps"
    sleep 0.5
    if import -window "$wid" "$out/$name.png" 2>/dev/null; then
        echo "OK   $name"
    else
        echo "FAIL $name: capture" >&2
    fi
    kill "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null
}

selected=("$@")
while IFS=$'\t' read -r name exe args delay steps; do
    [ -z "$name" ] && continue
    case $name in \#*) continue ;; esac
    if [ ${#selected[@]} -gt 0 ]; then
        match=0
        for s in "${selected[@]}"; do [ "$s" = "$name" ] && match=1; done
        [ $match -eq 1 ] || continue
    fi
    [ "$args" = "-" ] && args=""
    capture_one "$name" "$exe" "$args" "${delay:-2}" "${steps:-}"
done <"$table"
