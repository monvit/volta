import { DashboardView } from "@/components/dashboard-view";
import { DashboardLayout } from "@/components/layout/dashboard-layout";
import { ConnectionPage } from "@/app/pages/connection-page";
import { useIsConnected } from "@/stores/connection-store";

export function AppRouter() {
  const connected = useIsConnected();

  if (!connected) {
    return <ConnectionPage />;
  }

  return (
    <DashboardLayout>
      <DashboardView />
    </DashboardLayout>
  );
}
