import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import { viteSingleFile } from "vite-plugin-singlefile"
import BuildWebpagePlugin from "./BuildWebpagePlugin"

export default defineConfig({
	plugins: [vue(), viteSingleFile(), BuildWebpagePlugin()],
	build: {
    outDir: './../../build/',
		target: "esnext",
		assetsInlineLimit: 100000000,
		chunkSizeWarningLimit: 100000000,
		cssCodeSplit: false,
		brotliSize: false,
		rollupOptions: {
			inlineDynamicImports: true,
			output: {
				manualChunks: () => "everything.js",
			},
		},
	},
})
