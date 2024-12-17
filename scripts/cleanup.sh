#!/usr/bin/env bash

if [[ -f "backend_pids.txt" ]]; then
    while IFS= read -r pid; do
        kill "$pid" 2>/dev/null && echo "Killed backend server (PID: $pid)"
    done < backend_pids.txt
    rm backend_pids.txt
fi

if [[ -f "proxy_pid.txt" ]]; then
    kill "$(cat proxy_pid.txt)" 2>/dev/null && echo "Killed proxy server"
    rm proxy_pid.txt
fi
