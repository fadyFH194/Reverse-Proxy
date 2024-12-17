#!/usr/bin/env bash

# Stop on error
set -e

echo "Cleaning up build..."
make clean

echo "Building project..."
make

echo "Build complete."

read -p "Do you want to start the proxy and backends now? (y/n): " answer
if [[ $answer == "y" ]]; then
    ./scripts/manage_services.sh start
    echo "Services started. Run './scripts/manage_services.sh test' to test routes."
    echo "Use './scripts/manage_services.sh cleanup' to stop services."
fi
