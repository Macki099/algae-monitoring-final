const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const CopyPlugin = require('copy-webpack-plugin');
const webpack = require('webpack');
const dotenv = require('dotenv');

// Load env vars from the appropriate .env file at build time
const envFile = process.env.NODE_ENV === 'production' ? '.env.production' : '.env.local';
dotenv.config({ path: path.resolve(__dirname, envFile) });

module.exports = {
  entry: './src/index.js',
  output: {
    path: path.resolve(__dirname, 'dist'),
    filename: 'bundle.[contenthash].js',
    chunkFilename: '[name].[contenthash].js',
    clean: true,
  },
  module: {
    rules: [
      {
        test: /\.(js|jsx)$/,
        exclude: /node_modules/,
        use: {
          loader: 'babel-loader',
          options: {
            presets: ['@babel/preset-env', '@babel/preset-react'],
          },
        },
      },
      {
        test: /\.css$/i,
        use: ['style-loader', 'css-loader'],
      },
    ],
  },
  plugins: [
    new HtmlWebpackPlugin({
      template: './public/index.html',
    }),
    new CopyPlugin({
      patterns: [
        {
          from: 'public',
          globOptions: {
            ignore: ['**/index.html'],
          },
          noErrorOnMissing: true,
        },
      ],
    }),
    new webpack.DefinePlugin({
      'process.env.REACT_APP_API_URL': JSON.stringify(process.env.REACT_APP_API_URL || 'http://localhost:5000/api/sensor-data'),
    }),
  ],
  devServer: {
    static: {
      directory: path.join(__dirname, 'dist'),
    },
    compress: true,
    port: 3000,
    host: '0.0.0.0', // Allow external access
    hot: true,
    allowedHosts: 'all', // Allow access from any host
  },
  resolve: {
    extensions: ['.js', '.jsx'],
  },
};