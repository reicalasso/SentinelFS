import React from 'react'
import ReactDOM from 'react-dom/client'
import App from './App'
import { ThemeProvider } from './context/ThemeContext'
import { AppearanceProvider } from './context/AppearanceContext'
import { NotificationProvider, NotificationToast } from './context/NotificationContext'
import './index.css'

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <ThemeProvider>
      <AppearanceProvider>
        <NotificationProvider>
          <App />
          <NotificationToast />
        </NotificationProvider>
      </AppearanceProvider>
    </ThemeProvider>
  </React.StrictMode>,
)
