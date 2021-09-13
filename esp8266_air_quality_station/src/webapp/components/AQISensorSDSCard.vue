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
                <p class="p-2 rounded" :class="pm25ToColor(data.pm25)">
                    <strong>PM<sub>2,5</sub> - <span>{{ data.pm25 }}</span></strong> <small>µg/m3</small>
                </p>
            </div>
            <small><a href="https://en.wikipedia.org/wiki/Air_quality_index#CAQI" target="_blank">skála</a></small>
            <div class="mt-2">
                <button
                    type="submit"
                    class="btn"
                    :class="{ 'btn-primary' : !isWaitingForUpdate(), 'btn-secondary' : isWaitingForUpdate() }"
                    :disabled="sensor.measuring"
                    @click.prevent="$emit('measure-now')"
                >
                    <span
                        class="spinner-border spinner-border-sm"
                        role="status"
                        aria-hidden="true"
                        v-show="sensor.measuring"
                    ></span>
                    <span v-text="(sensor.measuring ? ' Mérés... ' : 'Mérés most')"></span>
                    <span v-show="sensor.measuring && nextReadSec">{{ humanizeSeconds(nextReadSec) }}</span>
                </button>
            </div>
            <div class="mt-2" v-show="!isWaitingForUpdate() && !sensor.measuring">
                <strong>Következő mérés: </strong>
                <span> {{ humanizeSeconds(nextWakeupSec) }}</span>
            </div>
            <div class="mt-2" v-show="isWaitingForUpdate()">
                <strong>Várakozás frissítésre</strong>
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
    emits: [
        'measure-now'
    ],
    setup(props) {

        let nextWakeupCounter: ReturnType<typeof setInterval>
        let nextReadCounter: ReturnType<typeof setInterval>

        const nextReadSec = ref(0)
        const nextWakeupSec = ref(0)

        watch(() => props.sensor.statusChangedAt, () => {

            if (nextReadCounter) {
                clearInterval(nextReadCounter)
            }

            if (nextWakeupCounter) {
                clearInterval(nextWakeupCounter);
            }

            if (props.sensor.measuring) {
                nextReadSec.value = props.sensor.nextRead

                nextReadCounter = setInterval(() => {
                    nextReadSec.value--;

                    if (nextReadSec.value <= 0) {
                        clearInterval(nextReadCounter)
                    }
                }, 1000)
            } else {
                nextWakeupSec.value = props.sensor.nextWakeup

                nextWakeupCounter = setInterval(() => {
                    nextWakeupSec.value--;

                    if (nextWakeupSec.value <= 0) {
                        clearInterval(nextWakeupCounter)
                    }
                }, 1000)
            }
        })

        const isWaitingForUpdate = () => {
            return (props.sensor.measuring && nextReadSec.value <= 0)
                || (!props.sensor.measuring && nextWakeupSec.value <= 0)
        }

        return {
            nextWakeupSec,
            nextReadSec,
            aqiToColor,
            pm10ToColor,
            pm25ToColor,
            isWaitingForUpdate,
            humanizeSeconds,
        }
    }
})
</script>