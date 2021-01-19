#!/usr/bin/env node

var WebSocketServer = require('websocket').server
var http = require('http');

var httpServer = http.createServer(function(request, response) {
    console.log((new Date()) + " request: ", request.url);
    response.writeHead(404);
    response.end();
});

httpServer.listen(8080, function () {
    console.log((new Date()) + ' Server is listening on port 8080');
});

var wsServer = new WebSocketServer({
    httpServer: httpServer,
    autoAcceptConnections: true
});

var fakeData = function () {
    return {
        sensors: {
            ppm: Math.floor(Math.random() * (5000 - 1000) + 1000),
        },
        shelly: {
            on: true,
        },
        settings: {
            enabled: true,
            ppm_limit: Math.floor(Math.random() * (5000 - 1000) + 1000),
            shelly_ip: "192.168.0.128"
        }
    }
}

wsServer.on('connect', function (connection) {

    console.log((new Date()) + ' Connection accepted.');

    connection.send(JSON.stringify({
        event: "status",
        data: fakeData()
    }));

    connection.on('message', (message) => {
        console.log((new Date()) + ' Message received: ' + message.utf8Data);

        connection.send(JSON.stringify({
            event: "status",
            data: fakeData()
        }));
    })
});
