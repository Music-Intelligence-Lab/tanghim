import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  // Build output goes to dist/ — CMake embeds this as BinaryData in release builds
  build: {
    outDir: 'dist',
    // Inline assets below 100kb to reduce number of BinaryData entries
    assetsInlineLimit: 100_000,
  },
  server: {
    port: 5173,
    // Allow connections from the WebView (localhost)
    host: 'localhost',
  },
})
