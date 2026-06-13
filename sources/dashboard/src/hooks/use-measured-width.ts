import { useEffect, useRef, useState } from "react";

/** Tracks an element's content-box width via ResizeObserver */
export function useMeasuredWidth() {
  const ref = useRef<HTMLDivElement>(null);
  const [width, setWidth] = useState(0);

  useEffect(() => {
    const el = ref.current;
    if (!el) return;

    const observer = new ResizeObserver((entries) => {
      const width = Math.floor(entries[0].contentRect.width);
      setWidth(width);
    });

    observer.observe(el);

    return () => observer.disconnect();
  }, []);

  return [ref, width] as const;
}
