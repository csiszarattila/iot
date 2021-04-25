const purgecss = require('@fullhuman/postcss-purgecss')
const comments = require('postcss-discard-comments')

module.exports = {
  plugins: [
    comments({ removeAll: true }),
    purgecss({
      content: ['./**/*.html']
    }),
    require('autoprefixer'),
  ]
} 