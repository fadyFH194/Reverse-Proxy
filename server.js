// Import required modules
const http = require('http');
const url = require('url');

// Configuration for server
const PORT = process.env.PORT || 4000;

// Create an HTTP server
const server = http.createServer((req, res) => {
    const parsedUrl = url.parse(req.url, true);

    // Handle a basic health check endpoint
    if (parsedUrl.pathname === '/health') {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ status: 'OK', port: PORT }));
        return;
    }

    // Echo back any request with details for testing
    let body = '';
    req.on('data', chunk => {
        body += chunk;
    });

    req.on('end', () => {
        res.writeHead(200, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({
            message: `Response from backend server on port ${PORT}`,
            method: req.method,
            headers: req.headers,
            body: body,
        }));
    });

    req.on('error', () => {
        res.writeHead(500, { 'Content-Type': 'application/json' });
        res.end(JSON.stringify({ error: 'Internal server error' }));
    });
});

// Start listening
server.listen(PORT, '127.0.0.1', () => {
    console.log(`Backend server is running on http://127.0.0.1:${PORT}`);
});
