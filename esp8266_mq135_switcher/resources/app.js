import 'alpinejs';
import { initializeComponent } from 'alpinejs';

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
        init () {
            window.addEventListener('config-update', (event) => {
                this.ppm_limit = event.detail.ppm_limit;
                this.shelly_ip = event.detail.shelly_ip;
                this.auto_switch_enabled = event.detail.auto_switch_enabled;
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
                }
            }

            socket.send(JSON.stringify(message))
        }
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

// setInterval(function() {
//     const message = {"event": "sensor-status"}

//     socket.send(JSON.stringify(message))
// }, 2000);