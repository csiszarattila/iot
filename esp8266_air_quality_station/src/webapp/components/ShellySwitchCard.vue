<template>
    <div 
        class="card text-dark bg-light text-center h-100"
        :class="{'border-danger': shelly.error}"
    >
        <h4 class="card-header" :class="{'bg-danger': shelly.error}"><i class="fas fa-bolt"></i> Kapcsoló</h4>
        <div class="card-body h4 d-flex align-items-center justify-content-center">                        
            <div class="form-check form-switch">
                <input 
                    class="form-check-input"
                    type="checkbox"
                    name="switch_on"
                    value="1"
                    :checked="shelly.on ? true : false"
                    @click="$emit('switched', !shelly.on)"
                    :disabled="shelly.error"
                >
                <label class="form-check-label" for="switch_on" v-text="shelly.on ? 'Bekapcsolva': 'Kikapcsolva'"></label>
            </div>
        </div>
    </div>    
</template>

<script lang="ts">
import { ShellySwitch } from './../types'
import { defineComponent, PropType } from 'vue'

export default defineComponent({
    name: "ShellySwitchCard",
    props: {
        shelly: {
            type: Object as PropType<ShellySwitch>,
            required: true
        }
    },
    emits: [
        'switched',
    ],
    setup() {
        
    },
})
</script>
