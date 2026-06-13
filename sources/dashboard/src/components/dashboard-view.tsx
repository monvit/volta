import { Suspense, lazy } from "react";
import { useView } from "@/stores/ui-store";

const DetailPage = lazy(() =>
  import("@/app/pages/detail-page").then((m) => ({ default: m.DetailPage })),
);

const DashboardPage = lazy(() =>
  import("@/app/pages/dashboard-page").then((m) => ({ default: m.DashboardPage })),
);

export function DashboardView() {
  const view = useView();

  return (
    <Suspense fallback={null}>
      {view === "detail" && <DetailPage />}
      {view === "dashboard" && <DashboardPage />}
    </Suspense>
  );
}
