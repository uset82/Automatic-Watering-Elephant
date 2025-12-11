import { defineConfig } from 'vite'

export default defineConfig({
  server: {
    fs: {
      // Allow serving from this project directory
      allow: ['D:/HVL2025/ADA525/mesarota/PLOTTER ELEFANTE', 'd:/HVL2025/ADA525/uno']
    }
  }
})