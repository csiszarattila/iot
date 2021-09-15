export interface SensorsData {
    aqi: number
    pm1: number,
    pm25: number,
    pm4: number,
    pm10: number,
    at: number,
}

export interface SDSSensor {
    error: boolean,
    measuring: boolean,
    nextRead: number,
    nextWakeup: number,
    statusChangedAt: number,
}

export interface Settings {
    ppm_limit: number,
    shelly_ip: String,
    auto_switch_enabled: boolean,
    measuring_frequency: number,
    switch_back_time: number,
    version: String,
    aqi_sensor_type: String,
    demo_mode: boolean,
}

export interface ShellySwitch {
    error: boolean,
    on: boolean,
}

export interface Alert {
    type: String,
    message: String,
    at: number,
}