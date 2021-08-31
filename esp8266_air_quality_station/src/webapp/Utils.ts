export function aqiToColor(aqi: number) {
    return {
        'bg-aqi-very-high': aqi >= 100,
        'bg-aqi-high': aqi >= 75 && aqi < 100,
        'bg-aqi-medium': aqi >= 50 && aqi < 75,
        'bg-aqi-low': aqi >= 25 && aqi < 50,
        'bg-aqi-very-low': aqi < 25
    }
}

export function pm10ToColor(pm10: number) {
    return {
        'bg-aqi-very-high': pm10 >= 180,
        'bg-aqi-high': pm10 >= 90 && pm10 < 180,
        'bg-aqi-medium': pm10 >= 50 && pm10 < 90,
        'bg-aqi-low': pm10 >= 25 && pm10 < 50,
        'bg-aqi-very-low': pm10 < 25
    }
}

export function pm25ToColor(pm25: number) {
    return {
        'bg-aqi-very-high': pm25 >= 110,
        'bg-aqi-high': pm25 >= 55 && pm25 < 110,
        'bg-aqi-medium': pm25 >= 30 && pm25 < 55,
        'bg-aqi-low': pm25 >= 15 && pm25 < 30,
        'bg-aqi-very-low': pm25 < 15
    }
}

export function formatUnixTimestamp (unixTimestamp: number) {
    let t = new Intl.DateTimeFormat([], {
        timeStyle: "medium",
        dateStyle: "short"
    });

    return t.format(new Date(unixTimestamp*1000));
}

export function humanizeSeconds (seconds: number) {
    seconds = seconds * 1000;

    if (seconds > (60 * 60 * 1000)) {
        return new Date(seconds).toISOString().slice(11, -5); // 00:00:00
    }
    return new Date(seconds).toISOString().slice(14, -5); // 00:00
}