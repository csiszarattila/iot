/**
 * https://github.com/ayushsharma82/ESP-DASH/blob/master/vue-frontend/finalize.js
 */
const { gzip } = require('@gfx/zopfli');
const FS = require('fs');
const path = require('path');

const BUNDLE_JS = FS.readFileSync(path.resolve(__dirname, './build/app.js'), {'encoding': 'utf-8'});
let HTML = FS.readFileSync(path.resolve(__dirname, './resources/index.html'), {'encoding': 'utf-8'});

HTML = HTML.replace('<script defer src="app.js"></script>', '<script defer>' + BUNDLE_JS + '</script>');

function chunkArray(myArray, chunk_size) {
  let index = 0;
  const arrayLength = myArray.length;
  const tempArray = [];
  for (index = 0; index < arrayLength; index += chunk_size) {
    myChunk = myArray.slice(index, index + chunk_size);
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