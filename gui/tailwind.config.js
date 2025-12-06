/** @type {import('tailwindcss').Config} */
module.exports = {
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    fontFamily: {
      sans: ['"Pixellet TH"', 'system-ui', 'sans-serif'],
      mono: ['"Pixellet TH"', 'ui-monospace', 'monospace'],
      serif: ['"Pixellet TH"', 'Georgia', 'serif'],
    },
    extend: {
      colors: {
        background: "hsl(var(--background))",
        foreground: "hsl(var(--foreground))",
        card: {
          DEFAULT: "hsl(var(--card))",
          foreground: "hsl(var(--card-foreground))",
        },
        primary: {
          DEFAULT: "hsl(var(--primary))",
          foreground: "hsl(var(--primary-foreground))",
        },
        secondary: {
          DEFAULT: "hsl(var(--secondary))",
          foreground: "hsl(var(--secondary-foreground))",
        },
        muted: {
          DEFAULT: "hsl(var(--muted))",
          foreground: "hsl(var(--muted-foreground))",
        },
        accent: {
          DEFAULT: "hsl(var(--accent))",
          foreground: "hsl(var(--accent-foreground))",
        },
        destructive: {
          DEFAULT: "hsl(var(--destructive))",
          foreground: "hsl(var(--destructive-foreground))",
        },
        border: "hsl(var(--border))",
        input: "hsl(var(--input))",
        ring: "hsl(var(--ring))",
        // Semantic Status Colors
        success: {
          DEFAULT: "hsl(160 70% 45%)",
          light: "hsl(160 70% 60%)",
          dark: "hsl(160 70% 35%)",
          muted: "hsl(160 70% 45% / 0.15)",
        },
        warning: {
          DEFAULT: "hsl(45 100% 55%)",
          light: "hsl(45 100% 70%)",
          dark: "hsl(45 100% 45%)",
          muted: "hsl(45 100% 55% / 0.15)",
        },
        error: {
          DEFAULT: "hsl(0 72% 51%)",
          light: "hsl(0 72% 65%)",
          dark: "hsl(0 72% 41%)",
          muted: "hsl(0 72% 51% / 0.15)",
        },
        info: {
          DEFAULT: "hsl(180 60% 50%)",
          light: "hsl(180 60% 65%)",
          dark: "hsl(180 60% 40%)",
          muted: "hsl(180 60% 50% / 0.15)",
        },
        // Terminal Colors
        terminal: {
          bg: "hsl(180 50% 8%)",
          header: "hsl(180 60% 35%)",
          text: "hsl(180 60% 70%)",
          dim: "hsl(180 40% 55%)",
          prompt: "hsl(30 100% 55%)",
        },
      },
    },
  },
  plugins: [],
}
