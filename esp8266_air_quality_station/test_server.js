var express = require("express");
var expressStaticGzip = require("express-static-gzip");
var app = express();

app.use("/", expressStaticGzip(__dirname+"/build/"));

app.listen(5000);

console.log("Running at Port 5000");