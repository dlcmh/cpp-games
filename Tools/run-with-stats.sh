#!/usr/bin/env bash
# Run a command and print its CPU and resident-memory usage once per second.
# Used by each game's `run.sh --stats`.
set -euo pipefail

if [[ $# -eq 0 ]]; then
    echo "Usage: Tools/run-with-stats.sh <executable> [args...]" >&2
    exit 2
fi

"$@" &
child_pid=$!
stopped=0

stop_child() {
    if [[ "${stopped}" -eq 0 ]]; then
        stopped=1
        if kill -0 "${child_pid}" 2>/dev/null; then
            kill "${child_pid}" 2>/dev/null || true
        fi
    fi
}

trap 'stop_child; exit 130' INT
trap 'stop_child; exit 143' TERM
trap 'stop_child' HUP
trap 'stop_child' EXIT

printf '[stats] pid %d; sampling CPU and RSS every second (Ctrl-C to stop)\n' "${child_pid}"

while kill -0 "${child_pid}" 2>/dev/null; do
    sleep 1
    if ! kill -0 "${child_pid}" 2>/dev/null; then
        break
    fi

    sample="$(ps -o %cpu= -o rss= -p "${child_pid}" 2>/dev/null |
        awk 'NF >= 2 { printf "CPU %5.1f%%  RSS %6.1f MB", $1, $2 / 1024; exit }')"
    if [[ -n "${sample}" ]]; then
        printf '[stats] %s  %s\n' "$(date '+%H:%M:%S')" "${sample}"
    fi
done

set +e
wait "${child_pid}"
status=$?
set -e

printf '[stats] process exited with status %d\n' "${status}"
exit "${status}"
