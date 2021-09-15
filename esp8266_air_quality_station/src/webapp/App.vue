<template>
  <div class="container">
    <div v-for="alert in alerts.visible()" :key="alert.at">
        <p
            class="alert"
            :class="{'alert-success': alert.type == 'info', 'alert-danger': alert.type == 'error'}"
        >
            {{ alert.message }}
            <small class="text-muted d-block text-sm">{{ formatUnixTimestamp(alert.at) }}</small>
        </p>
    </div>

    <div class="row">
        <div class="col-sm-6" v-if="settings.aqi_sensor_type == 'sds'">
            <AQISensorSDSCard 
                :data="sensorsData"
                :sensor="aqiSensor"
                @measure-now="measureNow"
            />
        </div>
        <div class="col-sm-6"  v-if="settings.aqi_sensor_type == 'sps030'">
            <AQISensorSPS030Card 
                :data="sensorsData"
                :sensor="aqiSensor"
            />
        </div>

        <div class="col-sm-6 mt-4 mt-sm-0">
            <ShellySwitchCard 
                :shelly="shelly"
                @switched="switchShelly($event.on)"
            />
        </div>
    </div>

    <div class="row">
        <div class="col mt-4">
            <SensorHistoryCard
                :items="sensorsHistory"
            />
        </div>
    </div>
        
    <div class="row mt-4">
        <div class="col">
            <SettingsCard 
              v-model:settings="settings"
              :saving="saving"
              :restarting="restarting"
              @save-settings="saveSettings"
              @restart="restart"
            />
        </div>
    </div>
  </div>
</template>

<script lang="ts">

declare global {
    interface Window {
        socket: WebSocket;
    }
}

import { Alert, SDSSensor, SensorsData, Settings, ShellySwitch } from './types'
import { formatUnixTimestamp } from './Utils'
import _ from "lodash"
import { defineComponent, ref } from 'vue'
import SettingsCard from './components/SettingsCard.vue'
import AQISensorSDSCard from './components/AQISensorSDSCard.vue'
import AQISensorSPS030Card from './components/AQISensorSPS030Card.vue'
import ShellySwitchCard from './components/ShellySwitchCard.vue'
import SensorHistoryCard from './components/SensorHistoryCard.vue'

const host = `ws://${window.location.hostname}/ws`;

const socket = new WebSocket(host);

window.socket = socket;

export default defineComponent({
  name: 'App',
  components: {
    SettingsCard,
    AQISensorSDSCard,
    AQISensorSPS030Card,
    ShellySwitchCard,
    SensorHistoryCard,
  },
  setup() {

    const settings = ref<Settings>({
      ppm_limit: 0,
      shelly_ip: "192.168.0.1",
      auto_switch_enabled: true,
      measuring_frequency: 1,
      switch_back_time: 30,
      version: '-',
      aqi_sensor_type: 'sps030',
      demo_mode: false,
    })
    
    const sensorsData = ref<SensorsData>({
      aqi: 0,
      pm1: 0,
      pm25: 0,
      pm4: 0,
      pm10: 0,
      at: 0,
    })

    const sensorsHistory = ref<Array<SensorsData>>([])

    const saving = ref(false)
    const restarting = ref(false)

    const aqiSensor = ref<SDSSensor>({
        error: false,
        measuring: false,
        nextRead: 0,
        nextWakeup: 0,
        statusChangedAt: 0,
    })

    const shelly = ref<ShellySwitch>({
        error: false,
        on: false,
    })

    class Alerts {

        messages: Alert[] = []

        info(message: String) {
            this.messages.push({ type: "info", message: message, at: Date.now() / 1000 })
        }

        error(message: String) {
            this.messages.push({ type: "error", message: message, at: Date.now() / 1000 })
        }

        visible() {
            this.clearExpired()

            return this.messages
        }

        clearExpired() {
            _.remove(this.messages, (alert) => {
                return alert.at + 5000 <= (Date.now() / 1000)
            })
        }
    }

    const alerts = ref(new Alerts())

    socket.onerror = function(error) {
        alerts.value.error("Websocket kapcsolódási hiba.");
    }

    socket.onmessage = (message) => {
        const payload = JSON.parse(message.data);

        switch (payload.event) {
            case "sensors":
                sensorsData.value = payload.data

                let found = _.find(sensorsHistory.value, (item, index) => {
                    return item.at == payload.data.at;
                })

                if (! found) {
                    sensorsHistory.value.push(payload.data)
                    sensorsHistory.value = sensorsHistory.value.sort(function (a, b) { return b.at - a.at })
                }

                if (sensorsHistory.value.length == 20) { 
                    sensorsHistory.value.pop()
                }

                break;
            case "config":
                settings.value = payload.data
                break;
            case "measuring":
                aqiSensor.value = {
                    measuring: true,
                    error: false,
                    nextWakeup: 0,
                    nextRead: Math.ceil(payload.data.nextReadAt / 1000),
                    statusChangedAt: Date.now(),
                }
                break;
            case "sleeping":
                aqiSensor.value = {
                    measuring: false,
                    error: false,
                    nextWakeup: Math.ceil(payload.data.nextWakeupAt / 1000),
                    nextRead: 0,
                    statusChangedAt: Date.now(),
                }
                break;
            case "shelly":
                shelly.value = {
                    on: payload.data.state,
                    error: false
                }
                break;
            case "info":
                switch (payload.data.code) {
                    case "settings.saved":
                        saving.value = false
                        break;
                    default:
                        alerts.value.info("Ismeretlen infó üzenet:" + payload.data.code);
                        break;
                }
                break;
            case "error":
                switch (payload.data.code) {
                    case "shelly.notfound":
                        shelly.value.error = true
                        alerts.value.error("Nem található Shelly kapcsoló a megadott IP címen! Ellenőrizd, hogy áram alatt van és helyes-e a beállított IP címe.");
                        break;
                    case "shelly.setstate.failed":
                        shelly.value.error = true
                        alerts.value.error("A Shelly kapcsolása meghíúsult! Ellenőrizd, hogy áram alatt van és helyes-e a beállított IP címe.");
                        break;
                    case "aqs.notfound":
                        aqiSensor.value.error = true
                        alerts.value.error("Levegőminőség szenzor hiba! Nincs csatlakoztatva.");
                        break;
                    default:
                        alerts.value.error("Ismeretlen hibakód: " + payload.data.code);
                        break;
                }
        }
    }

    const saveSettings = () => {
        saving.value = true

        socket.send(JSON.stringify({
            "event": "save-settings",
            "data": {
                "ppm_limit": settings.value.ppm_limit,
                "shelly_ip": settings.value.shelly_ip,
                "auto_switch_enabled": settings.value.auto_switch_enabled,
                "measuring_frequency": settings.value.measuring_frequency,
                "switch_back_time": settings.value.switch_back_time,
            }
        }))
    }

    const switchShelly = (on: Boolean) => {
        socket.send(JSON.stringify({
            "event": on ? "set-switch-on" : "set-switch-off"
        }))
    }

    const restart = () => {
        restarting.value = true

        socket.send(JSON.stringify({
            "event": "restart"
        }))

        setInterval(() => window.location.reload(), 2000);
    }

    const measureNow = () => {
      aqiSensor.value.measuring = true;

      socket.send(JSON.stringify({
          "event": "measure-aqi"
      }))
    }

    return {
      alerts,
      settings,
      saving,
      restarting,
      sensorsData,
      sensorsHistory,
      shelly,
      aqiSensor,
      saveSettings,
      restart,
      switchShelly,
      measureNow,
      formatUnixTimestamp,
    }
  }
})
</script>

<style lang="scss">
@import "bootstrap";
@import "bootstrap/scss/_functions";
@import "bootstrap/scss/_variables";
@import "bootstrap/scss/_mixins";
@import "bootstrap/scss/_reboot";

.bg-aqi-very-low {
    background-color: #79bc6a !important;
 }
 .bg-aqi-low {
    background-color: #bbcf4c !important;
 }
 .bg-aqi-medium {
    background-color: #eec20b !important;
 }
 .bg-aqi-high {
    background-color: #f29305 !important;
 }
 .bg-aqi-very-high {
    background-color: #e8416f !important;
 }

 .text-sm {
     font-size: 0.75em !important;
 }
</style>
