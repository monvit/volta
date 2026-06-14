import { formatClockTime } from "@/lib/format";

const TOOLTIP_SHIFT = 10;

export function chartTooltipPlugin(formatValue: (value: number) => string): uPlot.Plugin {
  let tooltip: HTMLDivElement | null = null;
  let overLeft = 0;
  let overTop = 0;

  function hideTooltip() {
    if (tooltip) tooltip.style.display = "none";
  }

  function setTooltip(u: uPlot, idx: number) {
    if (!tooltip) return;

    const tsSec = u.data[0][idx] as number;
    const val = u.data[1][idx] as number | null | undefined;
    const top = val == null ? (u.cursor.top ?? 0) : u.valToPos(val, "y");
    const left = u.valToPos(tsSec, "x");

    tooltip.innerHTML = [
      `<div style="color:var(--muted-foreground)">${formatClockTime(tsSec * 1000)}</div>`,
      `<div style="font-weight:500">${val == null ? "—" : formatValue(val)}</div>`,
    ].join("");
    tooltip.style.display = "block";

    const x = overLeft + left;
    const y = overTop + top;
    const { offsetWidth: tw, offsetHeight: th } = tooltip;

    let tooltipLeft = x + TOOLTIP_SHIFT;
    if (tooltipLeft + tw > u.width) tooltipLeft = x - TOOLTIP_SHIFT - tw;

    let tooltipTop = y + TOOLTIP_SHIFT;
    if (tooltipTop + th > u.height) tooltipTop = y - TOOLTIP_SHIFT - th;

    tooltip.style.left = `${tooltipLeft}px`;
    tooltip.style.top = `${tooltipTop}px`;
  }

  return {
    hooks: {
      ready: [
        (u) => {
          tooltip = document.createElement("div");
          Object.assign(tooltip.style, {
            position: "absolute",
            display: "none",
            pointerEvents: "none",
            zIndex: "10",
            padding: "4px 8px",
            borderRadius: "6px",
            fontSize: "12px",
            lineHeight: "1.4",
            border: "1px solid var(--border)",
            background: "var(--popover)",
            color: "var(--popover-foreground)",
            boxShadow: "0 1px 2px rgb(0 0 0 / 0.05)",
            fontVariantNumeric: "tabular-nums",
          });
          overLeft = Number.parseFloat(u.over.style.left);
          overTop = Number.parseFloat(u.over.style.top);
          u.root.querySelector(".u-wrap")!.appendChild(tooltip);
        },
      ],
      setCursor: [
        (u) => {
          const idx = u.cursor.idx;
          if (idx == null) hideTooltip();
          else setTooltip(u, idx);
        },
      ],
      destroy: [() => hideTooltip()],
    },
  };
}
