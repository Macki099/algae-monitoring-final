import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './styles/global.css';

const ensureFavicon = () => {
  const href = '/favicon.svg?v=20260325-2';
  let link = document.querySelector("link[rel='icon']");
  if (!link) {
    link = document.createElement('link');
    link.setAttribute('rel', 'icon');
    document.head.appendChild(link);
  }
  link.setAttribute('type', 'image/svg+xml');
  link.setAttribute('href', href);
};

ensureFavicon();

const root = ReactDOM.createRoot(document.getElementById('root'));
root.render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);