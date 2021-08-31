<template>
    <div class="card">
        <h4 class="card-header"><i class="fas fa-cog"></i> Beállítások</h4>
        <div class="card-body">
            <form>
                <div class="form-check form-switch mb-3">
                    <input class="form-check-input" type="checkbox" id="flexSwitchCheckDefault" v-model="settings.auto_switch_enabled">
                    <label class="form-check-label" for="flexSwitchCheckDefault">Automatikus ki/bekapcsolás</label>
                </div>
                <div class="mb-3">
                    <label for="measuring_frequency" class="form-label">Mérési gyakoriság (perc):</label>
                    <strong v-text="settings.measuring_frequency"></strong>
                    <input type="range" class="form-range" min="1" max="120" step="1" name="measuring_frequency" v-model="settings.measuring_frequency">
                </div>
                <div class="mb-3">
                    <label for="ppm_limit" class="form-label">Lekapcsolási határ (LM Index):</label>
                    <strong v-text="settings.ppm_limit"></strong>
                    <input type="range" class="form-range" min="0" max="100" step="1" name="ppm_limit" v-model="settings.ppm_limit">
                </div>
                <div class="mb-3">
                    <label for="switch_back_time" class="form-label">Lekapcsolás utáni legkorábbi visszakapcsolás (perc):</label>
                    <strong v-text="settings.switch_back_time"></strong>
                    <input type="range" class="form-range" min="0" max="120" step="1" name="switch_back_time" v-model="settings.switch_back_time">
                </div>
                <div class="mb-3">
                    <label for="shelly_ip" class="form-label">Shelly kapcsoló ip címe:</label>
                    <input type="text" class="form-control" name="shelly_ip" v-model="settings.shelly_ip">
                </div>
                <button type="submit" class="btn btn-primary" :disabled="saving" @click.prevent="$emit('save-settings')">
                    <span
                        class="spinner-border spinner-border-sm"
                        role="status"
                        aria-hidden="true"
                        v-show="saving"
                    ></span>
                    Beállítások mentése
                </button>

                <div class="mt-5 mb-3">
                    <div>
                        <label for="version" class="form-label">Jelenlegi verzió: <span v-text="settings.version"></span></label>
                    </div>
                </div>

                <div class="text-center">
                    <a href="/update" class="btn btn-primary">Frissítés feltöltése</a>
                </div>
                
                <div class="text-center">
                    <button type="submit" class="btn btn-danger mt-2" :disabled="restarting" @click.prevent="$emit('restart')">
                        <span
                            class="spinner-border spinner-border-sm"
                            role="status"
                            aria-hidden="true"
                            v-show="restarting"
                        ></span>
                        Újraindítás
                    </button>
                </div>
            </form>
        </div>
    </div>
</template>

<script lang="ts">
import { Settings } from './../types'
import { defineComponent, PropType, ref } from 'vue'

export default defineComponent({
    name: "SettingsCard",
    props: {
        settings: {
            type: Object as PropType<Settings>,
            required: true,
        },
        saving: Boolean,
        restarting: Boolean,
    },
    emits: [
        'save-settings',
        'restart'
    ],
    setup(props) {
    }
})
</script>
