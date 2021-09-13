<template>
    <div class="card">
        <h4 class="card-header"><i class="fas fa-cog"></i> Legutóbbi mérések</h4>
        <div class="card-body px-0 text-sm">
            <table class="table align-middle">
                <thead>
                    <tr>
                        <th>Időpont</th>
                        <th class="text-end">PM<sub>10</sub></th>
                        <th class="text-end">PM<sub>4</sub></th>
                        <th class="text-end">PM<sub>2,5</sub></th>
                        <th class="text-end">PM<sub>1</sub></th>
                        <th class="text-end">Index</th>
                    </tr>
                </thead>
                <tbody>
                    <tr v-for="item in items" :key="item.at">
                        <td v-text="formatUnixTimestamp(item.at)"></td>
                        <td class="text-end" :class="pm10ToColor(item.pm10)" >{{ item.pm10 }}</td>
                        <td class="text-end" :class="pm25ToColor(item.pm4)" >{{ item.pm4 }}</td>
                        <td class="text-end" :class="pm25ToColor(item.pm25)" >{{ item.pm25 }}</td>
                        <td class="text-end" :class="pm25ToColor(item.pm1)" >{{ item.pm1 }}</td>
                        <td class="text-end" :class="aqiToColor(item.aqi)" >{{ item.aqi }}</td>
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
