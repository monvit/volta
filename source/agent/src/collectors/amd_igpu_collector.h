#ifndef VOLTA_AGENT_SRC_COLLECTORS_AMD_IGPU_COLLECTOR_H_
#define VOLTA_AGENT_SRC_COLLECTORS_AMD_IGPU_COLLECTOR_H_

#include <optional>
#include <map>
#include <filesystem>

#include "collector.h"

namespace volta {
namespace agent {
namespace collectors {

class AmdIgpuCollector : public Collector {
public:
	AmdIgpuCollector();

	std::vector<Metric> Collect() override;

	bool IsAvailable() const;	

private:
	std::optional<std::filesystem::path> gpu_;
	std::optional<std::filesystem::path> hwmon_;

	void FindGpu();
	void FindHwmon();
	long ReadFromFile();
};

}	// collectors
}	// agent
}	// volta

#endif	// VOLTA_AGENT_SRC_COLLECTORS_AMD_IGPU_COLLECTOR_H_

