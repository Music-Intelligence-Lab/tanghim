import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// Generate build timestamp
const buildTimestamp = new Date().toLocaleString('en-US', {
  month: 'short',
  day: '2-digit',
  year: 'numeric',
  hour: '2-digit',
  minute: '2-digit',
  second: '2-digit',
  hour12: false,
})

export default defineConfig({
  plugins: [react()],
  define: {
    __BUILD_TIMESTAMP__: JSON.stringify(buildTimestamp),
  },
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
