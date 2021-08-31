<template>
    <div class="card">
        <h4 class="card-header"><i class="fas fa-cog"></i> Legutóbbi mérések</h4>
        <div class="card-body">
            <table class="table">
                <thead class="">
                    <tr>
                        <th>Időpont</th>
                        <th>PM<sub>10</sub></th>
                        <th>PM<sub>2,5</sub></th>
                        <th>Index</th>
                    </tr>
                </thead>
                <tbody>
                    <tr v-for="item in items" :key="item.at">
                        <td v-text="formatUnixTimestamp(item.at)"></td>
                        <td v-text="item.pm10" :class="pm10ToColor(item.pm10)"></td>
                        <td v-text="item.pm25" :class="pm25ToColor(item.pm25)"></td>
                        <td v-text="item.aqi" :class="aqiToColor(item.aqi)"></td>
                    </tr>
                </tbody>
            </table>
        </div>
    </div> 
</template>

<script lang="ts">
import { defineComponent } from 'vue'
import { formatUnixTimestamp, aqiToColor, pm10ToColor, pm25ToColor } from '../Utils'

export default defineComponent({
    name: "SensorHistoryCard",
    props: [
        'items'
    ],
    setup() {
        return {
            formatUnixTimestamp,
            pm10ToColor,
            pm25ToColor,
            aqiToColor,
        }
    },
})
</script>
