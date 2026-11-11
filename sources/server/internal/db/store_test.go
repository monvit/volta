package db

import (
	"testing"
	"time"

	pbt "github.com/monvit/volta/server/pb/types"
)

func TestPointsFromBatch(t *testing.T) {
	batch := &pbt.MetricBatch{
		TimestampsNs: []uint64{1_000_000_000, 2_000_000_000},
		Values:       []float64{1.5, 2.5},
		Header: &pbt.BatchHeader{
			MetricType: pbt.MetricType_METRIC_TYPE_CPU_UTILIZATION,
			DeviceId: &pbt.DeviceID{
				Device: &pbt.DeviceID_Cpu{Cpu: &pbt.CpuID{SocketIndex: 0, CoreIndex: 2}},
			},
		},
	}

	points := pointsFromBatch("agent-1", batch)
	if len(points) != 2 {
		t.Fatalf("expected 2 points, got %d", len(points))
	}

	if points[0].TS != time.Unix(1, 0) {
		t.Errorf("unexpected ts: %v", points[0].TS)
	}
	if points[0].AgentID != "agent-1" {
		t.Errorf("unexpected agent id: %v", points[0].AgentID)
	}
	if points[0].MetricType != 103 {
		t.Errorf("unexpected metric type: %v", points[0].MetricType)
	}
	if points[0].DeviceID == nil || *points[0].DeviceID != "cpu:s0-c2" {
		t.Errorf("unexpected device id: %v", points[0].DeviceID)
	}
	if points[0].Value != 1.5 || points[1].Value != 2.5 {
		t.Errorf("unexpected values: %v %v", points[0].Value, points[1].Value)
	}
}

func TestPointsFromBatchTruncatesToValues(t *testing.T) {
	batch := &pbt.MetricBatch{
		Header: &pbt.BatchHeader{
			MetricType: pbt.MetricType_METRIC_TYPE_RAM_USED,
		},
		TimestampsNs: []uint64{1, 2, 3},
		Values:       []float64{1.0},
	}

	points := pointsFromBatch("agent-1", batch)
	if len(points) != 1 {
		t.Fatalf("expected 1 point, got %d", len(points))
	}
}

func TestPointsFromBatchNoHeader(t *testing.T) {
	points := pointsFromBatch("agent-1", &pbt.MetricBatch{})
	if len(points) != 0 {
		t.Fatalf("expected no points, got %d", len(points))
	}
}

func TestDeviceIDString(t *testing.T) {
	cases := []struct {
		name string
		id   *pbt.DeviceID
		want *string
	}{
		{
			name: "gpu",
			id: &pbt.DeviceID{Device: &pbt.DeviceID_Gpu{
				Gpu: &pbt.GpuID{PciDomain: 0, PciBus: 1, PciDevice: 2, PciFunction: 3},
			}},
			want: strptr("gpu:0000:01:02.3"),
		},
		{
			name: "cpu-socket",
			id: &pbt.DeviceID{Device: &pbt.DeviceID_Cpu{
				Cpu: &pbt.CpuID{SocketIndex: 1},
			}},
			want: strptr("cpu:s1"),
		},
		{
			name: "network",
			id: &pbt.DeviceID{Device: &pbt.DeviceID_Network{
				Network: &pbt.NetInterfaceID{Ifindex: 5},
			}},
			want: strptr("net:ifindex5"),
		},
		{
			name: "disk",
			id: &pbt.DeviceID{Device: &pbt.DeviceID_Disk{
				Disk: &pbt.DiskID{Major: 8, Minor: 0},
			}},
			want: strptr("disk:8:0"),
		},
		{
			name: "nil",
			id:   nil,
			want: nil,
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			got := deviceIDString(tc.id)
			if tc.want == nil {
				if got != nil {
					t.Errorf("expected nil, got %v", *got)
				}
				return
			}
			if got == nil || *got != *tc.want {
				t.Errorf("expected %q, got %v", *tc.want, got)
			}
		})
	}
}

func strptr(s string) *string {
	return &s
}
