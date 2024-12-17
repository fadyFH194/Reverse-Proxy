# Reverse Proxy Project

## Overview
This project implements a Reverse Proxy in C using low-level system calls and sockets. The proxy listens for HTTP requests on a specified port, determines the correct backend server to forward the request to, and returns the backend’s response to the client. It supports dynamic route registration, concurrency limits, and comprehensive testing.

### Key Features:
- **Reverse Proxying**: Forwards requests based on path prefixes.
- **Dynamic Route Registration**: Add new routes at runtime via HTTP POST `/register` requests.
- **Concurrency Limits**: Uses semaphores to limit the number of concurrent connections.
- **Error Handling**: Returns appropriate HTTP error responses for missing routes, backend failures, or invalid requests.
- **Performance Testing**: Evaluate how the application performs under load.

This project meets the requirements of using low-level C sockets (no libraries that abstract away networking) and thoroughly testing and evaluating the implementation.

## Prerequisites
- **Operating System**: Tested on macOS and Linux.
- **Compiler**: `gcc` or `clang`.
- **Build Tools**: `make`.
- **Python 3**: Used to run backend servers with `python3 -m http.server`.
- **Curl**: For testing routes and registration.

### Optional:
- `wrk` or `ab` (Apache Benchmark) for performance testing.

Ensure these tools are installed on your system. On macOS, you can use Homebrew:
```bash
brew install gcc 
brew install make 
brew install curl
brew install wrk
brew install coreutils
```

## Directory Structure
Your project’s files are organized as follows:
```
reverse_proxy/
├── bin/
│   └── proxy_server               # Compiled binary
├── build/
│   └── *.o                       # Object files after building
├── config/
│   ├── 4000.json                 # Route config for backend 4000
│   ├── 4001.json
│   └── 4002.json
├── include/
│   ├── json_parser.h
│   ├── proxy.h
│   └── route_manager.h
├── logs/
│   ├── proxy_server.log
│   ├── backend_4000.log
│   ├── backend_4001.log
│   └── backend_4002.log
├── scripts/
│   ├── build_and_run.sh          # Cleans, builds, and starts services
│   └── manage_services.sh        # Start, test, and cleanup services
├── src/
│   ├── main.c
│   ├── proxy.c
│   ├── route_manager.c
│   └── json_parser.c
├── tests/
│   ├── Makefile
│   ├── test_route_manager.c     # Test binary for route manager
│   └── test_json_parser.c      # Test binary for JSON parser
│   └── test_proxy.c            # Test binary for proxy
├── Makefile
└── README.md                      
```

## Quick Start

1. **Build the Project**
   ```bash
   make clean
   make
   ```
   This will:
   - Create `build/` and `bin/` directories if not present.
   - Compile source files into `bin/proxy_server`.

2. **Run and Test**

   **Option 1: Automated Script**
   ```bash
   ./scripts/build_and_run.sh
   ```
   Cleans, builds, and prompts you to start the proxy and backends. If you type `y`, it will start the services and attempt to register routes.

   **Option 2: Manual Steps**

   - **Start services:**
     ```bash
     ./scripts/manage_services.sh start
     ```
     This will:
     - Start backend servers on ports 4000, 4001, and 4002.
     - Start the proxy server on port 8080.
     - Register the routes using the JSON files in `config/`.

   - **Test routes:**
     ```bash
     ./scripts/manage_services.sh test
     ```
     This sends requests to `http://localhost:8080/4000`, .../4001, and .../4002 to verify they are working.

   - **Stop all services:**
     ```bash
     ./scripts/manage_services.sh cleanup
     ```

## How to Access and Test the Reverse Proxy

### Dynamic Route Registration
Send a POST request with an HTML page to register a new route:
```bash
curl -H "Content-Type: application/HTML" \
-d '{"prefix":"/tests","host":"127.0.0.1","port":4000}' \
http://localhost:8080/register && \
echo '<h1>Route /test Registered Successfully!</h1>' > tests/index.html
```
If successful, you’ll see a `Registered.` response.

### Forwarded Requests
After registration, test the route:
```bash
curl -v http://localhost:8080/tests/
```
You should see a response from the backend server with the Index file we created.

### Error Cases
- **Non-existent route:**
  ```bash
  curl -v http://localhost:8080/nonexisting/
  ```
  Expect 404 Not Found.

- **Backend down:** Stop a backend server and request its route again. Expect 502 Bad Gateway.

### Performance Testing
Use `wrk` or `ab` to test performance:
Example:
```bash
wrk -t4 -c100 -d30s http://localhost:8080/4000
```
- `-t4`: 4 threads.
- `-c100`: 100 concurrent connections.
- `-d30s`: Run for 30 seconds.

Check `logs/proxy_server.log` and `backend_*.log` for details.

### Concurrency Limits
The proxy uses a semaphore to limit the number of concurrent connections. By default, it allows up to `MAX_CONCURRENT_CONNECTIONS` (e.g., 100).

To see concurrency limits in action, run a load test:
```bash
wrk -t4 -c300 -d30s http://localhost:8080/4000
```
If `MAX_CONCURRENT_CONNECTIONS` is 100, extra connections beyond this limit will wait until slots free up.

## Binary Testing

### Individual Component Tests

- **Proxy Tests**: Validates proxy functionality, including request forwarding, route registration, and error handling. Run:
  ```bash
  ./scripts/manage_services.sh proxy-tests
  ```

- **Route Manager Tests**: Tests adding and retrieving routes, including handling a full route table. Run:
  ```bash
  ./scripts/manage_services.sh route-tests
  ```

- **JSON Parser Tests**: Validates JSON parsing and route registration. Run:
  ```bash
  ./scripts/manage_services.sh json-parser-tests
  ```

### Run All Tests

To execute all tests (proxy, route manager, and concurrency) in a single command:
```bash
./scripts/manage_services.sh alltests
```

## Code Coverage
To measure code coverage:

Clean and rebuild with coverage flags:
```bash
make clean
make CFLAGS="-Wall -Wextra -pthread -fprofile-arcs -ftest-coverage"
```
Run your tests (e.g. `./scripts/manage_services.sh alltests`).

Generate coverage reports in a new directory:
```bash
gcov -o build src/*.c && mkdir -p gcov_output && mv *.gcov gcov_output/
```
This produces `.gcov` files indicating line-by-line coverage.

## Troubleshooting
- **Address Already in Use**: If 8080 or backend ports are taken, stop any processes using them:
  ```bash
  lsof -i :8080
  kill <PID>
  ```
  Then restart services.

- **Route Registration Fails**: Check `proxy_server.log` and `backend_*.log`. Ensure JSON syntax is correct and the proxy is running.

- **Permission Denied**: Make sure scripts are executable:
  ```bash
  chmod +x scripts/*.sh
  ```
