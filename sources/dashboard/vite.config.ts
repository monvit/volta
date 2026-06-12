import { defineConfig } from "vite-plus";
import tailwindcss from "@tailwindcss/vite";
import react, { reactCompilerPreset } from "@vitejs/plugin-react";
import babel from "@rolldown/plugin-babel";

// https://vite.dev/config/
export default defineConfig({
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
      "react/only-export-components": [
        "error",
        {
          allowConstantExport: true,
        },
      ],
    },
  },
  plugins: [react(), babel({ presets: [reactCompilerPreset()] }), tailwindcss()],
});
