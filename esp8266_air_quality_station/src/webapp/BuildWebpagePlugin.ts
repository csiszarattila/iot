import { IndexHtmlTransformResult, IndexHtmlTransformContext } from "vite"
import { Plugin } from "vite"
import { OutputChunk, OutputAsset } from "rollup"

/**
 * https://github.com/ayushsharma82/ESP-DASH/blob/master/vue-frontend/finalize.js
 */
const { gzip } = require('@gfx/zopfli');
const FS = require('fs');
const path = require('path');

function chunkArray(myArray: any, chunk_size: number): any {
  let index = 0;
  const arrayLength = myArray.length;
  const tempArray = [];
  for (index = 0; index < arrayLength; index += chunk_size) {
    const myChunk = myArray.slice(index, index + chunk_size);
    // Do something if you want with the group
    tempArray.push(myChunk);
  }
  return tempArray;
}

function addLineBreaks(buffer: any) {
  let data = '';
  const chunks = chunkArray(buffer, 30);
  chunks.forEach((chunk: any, index: number) => {
    data += chunk.join(',');
    if (index + 1 !== chunks.length) {
      data += ',\n';
    }
  });
  return data;
}

export default function BuildWebpagePlugin(): Plugin {
  return {
    name: 'transform-file',
    transformIndexHtml: {
      enforce: "post",
      transform(html, ctx) {
          // Only use this plugin during build
          if (!ctx || !ctx.bundle)
              return html;

          gzip(html, { numiterations: 15 }, (err: any, output: any) => {
            if (err) {
              return console.error(err);
            }
    
            const FILE = `#ifndef Webpage_h
            #define Webpage_h
            const uint32_t WEBPAGE_HTML_SIZE = ${output.length};
            const uint8_t WEBPAGE_HTML[] PROGMEM = { 
            ${addLineBreaks(output)} 
            };
            #endif
            `;
    
            FS.writeFileSync(path.resolve(__dirname, './../firmware/webpage.h'), FILE);
            console.log(`[COMPRESS] Compressed Build Files to webpage.h: ${ (output.length/1024).toFixed(2) }KB`);
          });
      },
    }
  }
}
