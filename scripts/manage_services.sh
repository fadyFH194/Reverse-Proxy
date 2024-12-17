#!/usr/bin/env bash

# Configuration
PROXY_PORT=8080
BACKEND_PORTS=(4000 4001 4002)
BACKEND_HOST="127.0.0.1"
LOGDIR="logs"
CONFIGDIR="config"
BACKEND_SCRIPT="server.js"  # Node.js backend file

function start_backends() {
    echo "Starting backend servers..."
    > backend_pids.txt
    for port in "${BACKEND_PORTS[@]}"; do
        PORT=$port node $BACKEND_SCRIPT > "$LOGDIR/backend_$port.log" 2>&1 &
        SERVER_PID=$!
        echo $SERVER_PID >> backend_pids.txt
        echo "Started backend on port $port (PID: $SERVER_PID)"
    done
    sleep 2
}


function start_proxy() {
    echo "Starting proxy server on port $PROXY_PORT..."
    ./bin/proxy_server > "$LOGDIR/proxy_server.log" 2>&1 &
    PROXY_PID=$!
    echo $PROXY_PID > proxy_pid.txt
    echo "Proxy server started (PID: $PROXY_PID)"
    sleep 2
}

function test_routes() {
    echo "Testing routes..."
    for port in "${BACKEND_PORTS[@]}"; do
        RESPONSE=$(curl -s "http://localhost:$PROXY_PORT/$port")
        if [[ -n "$RESPONSE" ]]; then
            echo "Route /$port is working correctly."
        else
            echo "Route /$port failed. Response: $RESPONSE"
        fi
    done
}

function register_routes() {
    echo "Registering routes..."
    for port in "${BACKEND_PORTS[@]}"; do
        prefix="/$port"
        json_file="$CONFIGDIR/$port.json"
        if [[ -f "$json_file" ]]; then
            response=$(curl -s -H "Content-Type: application/json" -d @$json_file "http://$BACKEND_HOST:$PROXY_PORT/register")
            # Trim whitespace and newline characters
            trimmed_response=$(echo "$response" | tr -d '\n' | tr -d '\r')
            if [[ "$trimmed_response" == "Registered." ]]; then
                echo "Route for $port registered successfully."
            else
                echo "Failed to register route for $port. Response: $response"
            fi
        else
            echo "Configuration file for port $port not found: $json_file"
        fi
    done
}


function cleanup() {
    echo "Cleaning up..."
    if [[ -f "backend_pids.txt" ]]; then
        while IFS= read -r pid; do
            kill "$pid" 2>/dev/null && echo "Killed backend server (PID: $pid)"
        done < backend_pids.txt
        rm backend_pids.txt
    fi
    if [[ -f "proxy_pid.txt" ]]; then
        PROXY_PID=$(cat proxy_pid.txt)
        kill $PROXY_PID 2>/dev/null && echo "Terminated proxy server (PID: $PROXY_PID)"
        wait $PROXY_PID 2>/dev/null
        rm proxy_pid.txt
    fi
}

function run_route_tests() {
    echo "Running route manager tests..."
    if make -C tests test_route_manager && ./tests/test_route_manager; then
        echo "Route manager tests passed."
    else
        echo "Route manager tests failed."
        return 1
    fi
    return 0
}

function run_json_parser_tests() {
    echo "Running JSON parser tests..."
    if make -C tests test_json_parser && ./tests/test_json_parser; then
        echo "JSON parser tests passed."
    else
        echo "JSON parser tests failed."
        return 1
    fi
    return 0
}

function run_proxy_tests() {
    echo "Running proxy tests..."
    if make -C tests test_proxy && ./tests/test_proxy; then
        echo "Proxy tests passed."
    else
        echo "Proxy tests failed."
        return 1
    fi
    return 0
}


function all_tests() {
    mkdir -p "$LOGDIR"
    start_backends
    start_proxy
    register_routes

    RESULT=0

    run_route_tests || RESULT=1
    run_json_parser_tests || RESULT=1
    run_proxy_tests || RESULT=1

    cleanup

    if [[ $RESULT -eq 0 ]]; then
        echo "All tests passed successfully!"
    else
        echo "Some tests failed. Check logs and output above."
    fi
}

case $1 in
    start)
        mkdir -p "$LOGDIR"
        start_backends
        start_proxy
        register_routes
        ;;
    test)
        test_routes
        ;;
    cleanup)
        cleanup
        ;;
    restart)
        cleanup
        start_backends
        start_proxy
        register_routes
        ;;
    alltests)
        all_tests
        ;;
    route-tests)
        run_route_tests
        ;;
    json-parser-tests)
        run_json_parser_tests
        ;;
    proxy-tests)
        mkdir -p "$LOGDIR"
        start_backends
        start_proxy
        register_routes
        run_proxy_tests
        cleanup
        ;;
    *)
        echo "Usage: $0 {start|test|cleanup|restart|alltests|route-tests|json-parser-tests|proxy-tests}"
        exit 1
        ;;
esac
