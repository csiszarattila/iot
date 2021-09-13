<template>
    <div 
        class="card text-dark bg-light text-center h-100"
        :class="{'border-danger': sensor.error}"
    >
        <h4 class="card-header"><i class="fas fa-smog"></i> LM Index</h4>
        <div class="card-body">    
            <div class="h2">
                <p class="p-2 rounded" :class="aqiToColor(data.aqi)">
                    <strong>{{ data.aqi }}</strong>
                </p>
            </div>
            <div class="h5">
                <p class="p-2 rounded" :class="pm10ToColor(data.pm10)">
                    <strong>PM<sub>10</sub> - <span>{{ data.pm10 }}</span></strong> <small>µg/m3</small>
                </p>
                <p class="p-2 rounded" :class="pm25ToColor(data.pm4)">
                    <strong>PM<sub>4</sub> - <span>{{ data.pm4 }}</span></strong> <small>µg/m3</small>
                </p>
                <p class="p-2 rounded" :class="pm25ToColor(data.pm25)">
                    <strong>PM<sub>2,5</sub> - <span>{{ data.pm25 }}</span></strong> <small>µg/m3</small>
                </p>
                <p class="p-2 rounded" :class="pm25ToColor(data.pm1)">
                    <strong>PM<sub>1</sub> - <span>{{ data.pm1 }}</span></strong> <small>µg/m3</small>
                </p>
            </div>
            <small><a href="https://en.wikipedia.org/wiki/Air_quality_index#CAQI" target="_blank">skála</a></small>
            <div class="mt-2">
                <button
                    type="submit"
                    class="btn btn-primary"
                    disabled
                >
                    <span
                        class="spinner-border spinner-border-sm"
                        role="status"
                    ></span>
                    <span v-show="!nextWakeupSec"> Mérés...</span>
                    <span v-show="nextWakeupSec"> Következő mérés... {{ humanizeSeconds(nextWakeupSec) }}</span>
                </button>
            </div>
        </div>
    </div>
</template>

<script lang="ts">
import { SDSSensor, SensorsData } from '../types'
import { defineComponent, watch, ref, PropType } from "vue";
import { aqiToColor, pm10ToColor, pm25ToColor, humanizeSeconds } from "../Utils";

export default defineComponent({
    props: {
        data: {
            type: Object as PropType<SensorsData>,
            required: true
        },
        sensor: {
            type: Object as PropType<SDSSensor>,
            required: true
        }
    },
    setup(props) {

        let nextWakeupCounter: ReturnType<typeof setInterval>

        const nextWakeupSec = ref(0)

        watch(() => props.sensor.statusChangedAt, () => {

            if (nextWakeupCounter) {
                clearInterval(nextWakeupCounter);
            }

            if (!props.sensor.measuring) {
                nextWakeupSec.value = props.sensor.nextWakeup

                nextWakeupCounter = setInterval(() => {
                    nextWakeupSec.value--;

                    if (nextWakeupSec.value <= 0) {
                        clearInterval(nextWakeupCounter)
                    }
                }, 1000)
            }
        })

        return {
            nextWakeupSec,
            aqiToColor,
            pm10ToColor,
            pm25ToColor,
            humanizeSeconds,
        }
    }
})
</script>