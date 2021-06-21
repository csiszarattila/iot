# Before building

## Install NPM packages

    npm install

## Install Arduino libraries

    arduino-cli lib install RemoteDebug
    arduino-cli lib install "DHT sensor library"
    arduino-cli lib install "Nova Fitness Sds dust sensors library"
    arduino-cli lib install AsyncElegantOTA

Some libraries require manual install

    cd ~/Arduino/libraries
    git clone https://github.com/me-no-dev/ESPAsyncWebServer.git
    git clone https://github.com/me-no-dev/ESPAsyncTCP.git

# Compile and Upload

    npm run release

# Setup

Running on first time connect to the device via the Captive Portal or opening http://192.168.4.1 in a browser.
Select the wifi network and set the password.
On successful setup the device restarts, connect to the wifi network, and start working.

# Monitoring

Open the device's ip address in a browser. You can find the ip address on the list of your routers DHCP clients.

# Debugging

## Watch console via USB

stty 115200 -F /dev/ttyUSB0 raw -echo | cat /dev/ttyUSB0

# Remote Debug

telnet 192.168.x.x 23

# Resources and ideas that helped to create this project

https://chewett.co.uk/blog/1066/pin-numbering-for-wemos-d1-mini-esp8266/
https://github.com/ayushsharma82/ESP-DASH

## Esp filesystem SPIFFS

https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html

https://github.com/esp8266/arduino-esp8266fs-plugin/issues/51#issuecomment-739433154

https://github.com/earlephilhower/arduino-esp8266littlefs-plugin

### Upload With Arduino IDE
https://github.com/esp8266/arduino-esp8266fs-plugin