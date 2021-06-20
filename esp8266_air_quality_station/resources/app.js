import './app.scss';

window._ = require('underscore')

window.switchState = false;

window.shellySwitch = function shellySwitch() {

    let _on = false;

    return {
        on: _on,
        error: false,
        setState (on) {
            this._on = on;

            const message = {"event": on ? "set-switch-on" : "set-switch-off"}

            socket.send(JSON.stringify(message))
        },
    }
}

window.settings = function settings() {
    return {
        ppm_limit: 0,
        shelly_ip: "192.168.0.1",
        auto_switch_enabled: true,
        measuring_frequency: 1,
        switch_back_time: 30,
        init () {
            window.addEventListener('config-update', (event) => {
                this.ppm_limit = event.detail.ppm_limit;
                this.shelly_ip = event.detail.shelly_ip;
                this.auto_switch_enabled = event.detail.auto_switch_enabled;
                this.measuring_frequency = event.detail.measuring_frequency;
                this.switch_back_time = event.detail.switch_back_time
            })
        },
        save () {
            const message = {
                "event": "save-settings",
                "data": {
                    "enabled": this.enabled,
                    "ppm_limit": this.ppm_limit,
                    "shelly_ip": this.shelly_ip,
                    "auto_switch_enabled": this.auto_switch_enabled,
                    "measuring_frequency": this.measuring_frequency,
                    "switch_back_time": this.switch_back_time,
                }
            }

            socket.send(JSON.stringify(message))
        }
    }
}

window.aqiSensor = function aqiSensor() {
    return {
        measuring: false,
        aqi: 0,
        pm25: 0,
        pm10: 0,
        error: false,
        aqiNextWakeup: 0,
        aqiNextRead: 0,
        nextReadCounter: null,
        nextWakeupCounter: null,
        init () {
            window.addEventListener('sensors-update', (event) => {
                this.aqi = event.detail.aqi;
                this.pm25 = event.detail.pm25;
                this.pm10 = event.detail.pm10;
                this.aqiNextWakeup = Math.ceil(event.detail.aqiNextWakeup / 1000);
                this.aqiNextRead = Math.ceil(event.detail.aqiNextRead / 1000);
                this.measuring = this.aqiNextWeakup <= 0;
                
                if (this.nextWakeupCounter) {
                    clearInterval(this.nextWakeupCounter);
                }      
                if (this.nextReadCounter) {
                    clearInterval(this.nextReadCounter);
                }

                this.measuring ? this.countdownToNextRead() : this.countdownToNextWakeup()
            })

            this.$watch("measuring", value => {
                this.measuring ? this.countdownToNextRead() : this.countdownToNextWakeup()
            })
        },
        measureNow () {
            socket.send(JSON.stringify({
                "event": "measure-aqi"
            }))

            this.measuring = true;
        },
        countdownToNextRead () {
            if (this.nextWakeupCounter) {
                clearInterval(this.nextWakeupCounter);
            }

            this.nextReadCounter = setInterval(() => {
                this.aqiNextRead--;

                if (this.aqiNextRead <= 0) {
                    clearInterval(this.nextReadCounter)
                }
            }, 1000)
        },
        countdownToNextWakeup () {      
            if (this.nextReadCounter) {
                clearInterval(this.nextReadCounter);
            }

            this.nextWakeupCounter = setInterval(() => {
                this.aqiNextWakeup--;

                if (this.aqiNextWakeup <= 0) {
                    this.measuring = true;
                    clearInterval(this.nextWakeupCounter)
                }
            }, 1000)
        },
    }
}

window.sensorHistory = function sensorHistory() {
    return {
        items: [],
        init () {
            window.addEventListener('sensors-update', (event) => {
                let found = _.find(this.items, (item, index) => {
                    return item.at == event.detail.at;
                })

                if (! found) {
                    this.items.push(event.detail)
                    this.items = this.items.sort(function (a, b) { return a.at - b.at })
                }

                if (this.items.length == 20) { 
                    this.items.pop()
                }
            })
        },
    }
}

const host = `ws://${window.location.hostname}/ws`;
//const host = "ws://192.168.0.128/ws";

const socket = new WebSocket(host);

window.socket = socket;

socket.onmessage = function (message) {

    console.log(message);

    const payload = JSON.parse(message.data);

    switch (payload.event) {
        case "sensors":
            window.dispatchEvent(new CustomEvent('sensors-update', {detail: payload.data}));
            break;
        case "config":
            window.dispatchEvent(new CustomEvent('config-update', {detail: payload.data}));
            break;
        case "info":
            switch (payload.data.code) {
                case "settings.saved":
                    displayInfo("Beállítások mentve!");
                    break;
                default:
                    displayInfo("Ismeretlen infó üzenet:" + payload.data.code);
                    break;
            }
            break;
        case "error":
            switch (payload.data.code) {
                case "shelly.notfound":
                    window.dispatchEvent(new CustomEvent('shelly-error'));
                    displayError("Nem található Shelly kapcsoló a megadott IP címen! Ellenőrizd, hogy áram alatt van és helyes-e a beállított IP címe.");
                    break;
                case "shelly.setstate.failed":
                    displayError("A Shelly kapcsolása meghíúsult! Ellenőrizd, hogy áram alatt van és helyes-e a beállított IP címe.");
                    break;
                case "aqs.notfound":
                    window.dispatchEvent(new CustomEvent('aqs-error'));
                    displayError("Levegőminőség szenzor hiba! Nincs csatlakoztatva.");
                    break;
                default:
                    displayError("Ismeretlen hibakód:" + payload.data.code);
                    break;
            }
    }
}

socket.onerror = function(error) {
    console.error("Websocket error:", error);
    displayError("Websocket kapcsolódási hiba.");
}

function displayError(message) {
    window.dispatchEvent(new CustomEvent('alert-message', { detail: {"message": message, "type": "error"} } ));
}

function displayInfo(message) {
    window.dispatchEvent(new CustomEvent('alert-message', { detail: {"message": message, "type": "info"} } ));
}

window.formatUnixTimestamp = function (unixTimestamp) {
    let t = new Intl.DateTimeFormat([], {
        timeStyle: "medium",
        dateStyle: "short"
    });

    return t.format(new Date(unixTimestamp*1000));
}

window.humanizeSeconds = function (seconds) {
    seconds = seconds * 1000;

    if (seconds > (60 * 60 * 1000)) {
        return new Date(seconds).toISOString().slice(11, -5); // 00:00:00
    }
    return new Date(seconds).toISOString().slice(14, -5); // 00:00
}

window.bg = {
    aqi: function (aqi)
    {
        return {
            'bg-aqi-very-high': aqi >= 100,
            'bg-aqi-high': aqi >= 75 && aqi < 100,
            'bg-aqi-medium': aqi >= 50 && aqi < 75,
            'bg-aqi-low': aqi >= 25 && aqi < 50,
            'bg-aqi-very-low': aqi < 25
        }
    },

    pm10: function (pm10)
    {
        return {
            'bg-aqi-very-high': pm10 >= 180,
            'bg-aqi-high': pm10 >= 90 && pm10 < 180,
            'bg-aqi-medium': pm10 >= 50 && pm10 < 90,
            'bg-aqi-low': pm10 >= 25 && pm10 < 50,
            'bg-aqi-very-low': pm10 < 25
        }
    },

    pm25: function (pm25)
    {
        return {
            'bg-aqi-very-high': pm25 >= 110,
            'bg-aqi-high': pm25 >= 55 && pm25 < 110,
            'bg-aqi-medium': pm25 >= 30 && pm25 < 55,
            'bg-aqi-low': pm25 >= 15 && pm25 < 30,
            'bg-aqi-very-low': pm25 < 15
        }
    }
}