import type { ReactNode } from "react";
import { ThemeProvider } from "@/components/theme-provider";

export function AppProvider({ children }: { children: ReactNode }) {
  return (
    <ThemeProvider defaultTheme="system" storageKey="volta-ui-theme">
      {children}
    </ThemeProvider>
  );
}
