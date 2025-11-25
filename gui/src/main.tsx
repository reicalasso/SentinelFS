import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App'
import { ThemeProvider } from './context/ThemeContext'
import { NotificationProvider, NotificationToast } from './context/NotificationContext'
import './index.css'

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <ThemeProvider>
      <NotificationProvider>
        <App />
        <NotificationToast />
      </NotificationProvider>
    </ThemeProvider>
  </React.StrictMode>,
)
