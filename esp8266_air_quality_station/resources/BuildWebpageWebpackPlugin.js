/**
 * https://github.com/ayushsharma82/ESP-DASH/blob/master/vue-frontend/finalize.js
 */
const { gzip } = require('@gfx/zopfli');
const FS = require('fs');
const path = require('path');
const { Compilation } = require('webpack');

class BuildWebpageWebpackPlugin {

  constructor(options)
  {
    this.options = options;
  }

  apply(compiler) {

    compiler.hooks.compilation.tap("BuildWebpageWebpackPlugin", compilation => {
      compilation.hooks.processAssets.tap({
        name: "BuildWebpageWebpackPlugin",
        stage: Compilation.PROCESS_ASSETS_STAGE_ADDITIONAL,
        additionalAssets: true,
      },
      (assets) => {
          if (assets[this.options.indexFilename] != undefined) {
              const indexFile = assets[this.options.indexFilename];

              gzip(indexFile.source(), { numiterations: 15 }, (err, output) => {
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

                FS.writeFileSync(path.resolve(__dirname, './../webpage.h'), FILE);
                console.log(`[COMPRESS] Compressed Build Files to webpage.h: ${ (output.length/1024).toFixed(2) }KB`);
              });
          }
      })
    });

    return;

    let HTML = FS.readFileSync(path.resolve(__dirname, './build/index.html'), {'encoding': 'utf-8'});

    function chunkArray(myArray, chunk_size) {
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

    function addLineBreaks(buffer) {
      let data = '';
      const chunks = chunkArray(buffer, 30);
      chunks.forEach((chunk, index) => {
        data += chunk.join(',');
        if (index + 1 !== chunks.length) {
          data += ',\n';
        }
      });
      return data;
    }


    gzip(HTML, { numiterations: 15 }, (err, output) => {
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

      FS.writeFileSync(path.resolve(__dirname, './webpage.h'), FILE);
      console.log(`[COMPRESS] Compressed Build Files to webpage.h: ${ (output.length/1024).toFixed(2) }KB`);
    });
  }
}

module.exports = BuildWebpageWebpackPlugin


