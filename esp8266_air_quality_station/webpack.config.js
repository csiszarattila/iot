const { CleanWebpackPlugin } = require('clean-webpack-plugin')
const HtmlWebpackPlugin = require('html-webpack-plugin')
const HtmlWebpackInlineSourcePlugin = require('html-webpack-inline-source-plugin');
const MiniCssExtractPlugin = require("mini-css-extract-plugin");

const BuildWebpageWebpackPlugin = require('./resources/BuildWebpageWebpackPlugin');

module.exports = {
    mode: 'production',
    plugins: [
        new CleanWebpackPlugin(),
        new MiniCssExtractPlugin({
            filename: "app.css",
        }),
        new HtmlWebpackPlugin({
            template: './resources/index.html',
            inlineSource: '.(js)$',
            cache: false,
        }),
        new HtmlWebpackInlineSourcePlugin(HtmlWebpackPlugin),
        new BuildWebpageWebpackPlugin({
            indexFilename: 'index.html',
            outputname: 'webpage.h'
        }),
    ],
    entry: './resources/app.js',
    output: {
        publicPath: '/',
        path: __dirname + '/build',
        filename: '[name].js'
    },
    module: {
        rules: [
            {
                test: /\.s[ac]ss$/,
                use: [
                    MiniCssExtractPlugin.loader,
                    'css-loader',
                    'postcss-loader',
                    'sass-loader'
                ]
            },
            {
                test: /\.js$/,
                use: [
                    {
                        loader: "babel-loader",
                        options: {
                            presets: [
                                [
                                  "@babel/preset-env",
                                  {
                                    useBuiltIns: "entry",
                                    corejs: 3,
                                    targets: {
                                        "chrome": 58
                                    }
                                  }
                                ]
                            ]
                        }
                    }
                ]
            }
        ],
    }
}