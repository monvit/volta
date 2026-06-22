import { defineConfig } from "vite-plus";
import tailwindcss from "@tailwindcss/vite";
import react, { reactCompilerPreset } from "@vitejs/plugin-react";
import babel from "@rolldown/plugin-babel";
import path from "path";

// https://vite.dev/config/
export default defineConfig({
  fmt: {
    ignorePatterns: [],
  },
  lint: {
    plugins: ["react"],
    env: {
      browser: true,
    },
    ignorePatterns: ["dist"],
    options: {
      typeAware: true,
      typeCheck: true,
    },
    rules: {
      "react/rules-of-hooks": "error",
      "react/exhaustive-deps": "warn",
    },
  },
  plugins: [react(), babel({ presets: [reactCompilerPreset()] }), tailwindcss()],
  resolve: {
    alias: {
      "@": path.resolve(__dirname, "./src"),
    },
  },
  build: {
    rolldownOptions: {
      output: {
        // Split big, independently-cacheable vendors out of the app chunk so no
        // single file trips the 500 kB warning and updates don't bust the lot.
        codeSplitting: {
          groups: [
            { name: "react", test: /node_modules[\\/](react|react-dom|scheduler)[\\/]/ },
            { name: "uplot", test: /node_modules[\\/]uplot[\\/]/ },
            { name: "base-ui", test: /node_modules[\\/]@base-ui[\\/]/ },
            { name: "protobuf", test: /node_modules[\\/]@bufbuild[\\/]/ },
          ],
        },
      },
    },
  },
});
