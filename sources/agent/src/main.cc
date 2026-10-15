#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

#include "client/volta_collector_client.h"
#include "collectors/collector.h"
#include "config/config.h"
#include "config/config_loader.h"
#include "config/config_resolution.h"
#include "control/control_command.h"
#include "control/control_socket.h"
#include "control/control_socket_path.h"
#include "scheduler.h"

using namespace volta::agent;
static void on_signal(int) { g_running = 0; }

void install_signal_handlers() {
  struct sigaction sa{};
  sa.sa_handler = on_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGINT, &sa, nullptr);
  sigaction(SIGTERM, &sa, nullptr);
}

int SendControlCommand(const control::ControlCommand& command) {
  try {
    auto path =
        control::ResolveControlSocketPath(control::ControlPathSide::kCli);
    control::ControlSocket socket(path.string(),
                                  control::ControlSocket::Role::kClient);
    auto payload = control::Encode(command);
    int err = 0;
    if (!socket.Send(payload, &err)) {
      std::cerr << "error: " << control::DescribeSendError(err) << "\n";
      return 1;
    }
    std::cout << "Message " << payload << " sent\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "error: " << ex.what() << "\n";
    return 1;
  }
}

struct CliCommand {
  std::string_view name;
  int (*run)(int argc, const char* argv[]);
};

int RunExportStart(int argc, const char* argv[]) {
  control::DumpStart command;
  if (argc >= 4) command.path = std::filesystem::absolute(argv[3]);
  return SendControlCommand(command);
}

int RunExportStop(int, const char**) {
  return SendControlCommand(control::DumpEnd{});
}

int RunExport(int argc, const char* argv[]) {
  if (argc < 3) return 1;
  static constexpr CliCommand kExport[] = {
      {"start", RunExportStart},
      {"stop", RunExportStop},
  };
  std::string_view name(argv[2]);
  for (const auto& subcommand : kExport) {
    if (name == subcommand.name) return subcommand.run(argc, argv);
  }
  return 1;
}

int cli_mode(int argc, const char* argv[]) {
  if (argc < 2) return 1;
  static constexpr CliCommand kCommands[] = {
      {"export", RunExport},
  };
  std::string_view name(argv[1]);
  for (const auto& command : kCommands) {
    if (name == command.name) return command.run(argc, argv);
  }
  return 1;
}

int agent_mode(int argc, const char* const* argv) {
  auto cli = config::ParseCli(argc, argv);
  if (!cli) {
    std::cerr << "error: invalid arguments (try --help)\n";
    return 2;
  }
  if (cli->show_help) {
    config::PrintAgentHelp(argv[0]);
    return 0;
  }

  config::ResolvedConfig resolved;
  try {
    resolved = config::ResolveConfig(*cli);
  } catch (const config::ResolutionError& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }

  if (resolved.source == config::ConfigSource::kBuiltinDefaults) {
    std::cout << "config source: "
              << config::ConfigSourceToString(resolved.source) << "\n";
  } else {
    std::cout << "config source: "
              << config::ConfigSourceToString(resolved.source) << " ("
              << resolved.path->string() << ")\n";
  }

  install_signal_handlers();
  try {
    auto agent_config = config::ConfigLoader::LoadConfig(resolved.path);

    auto active_collectors = collectors::CollectorRegistry::Instance().Resolve(
        agent_config.requestedMetrics);

    std::shared_ptr<MetricsBuffer> buffer =
        std::make_shared<MetricsBuffer>(agent_config);
    Scheduler scheduler(agent_config, std::move(active_collectors), buffer);

    std::jthread grpc_thread([&scheduler, &agent_config, &buffer]() {
      if (auto channel = client::Client::CreateChannel(
              agent_config.server_address + ":" +
              std::to_string(agent_config.server_port))) {
        client::Client grpc_client(channel, agent_config, buffer);
        grpc_client.Connect();
      } else {
        std::cerr
            << "Failed to create gRPC channel, falling back to local dashboard"
            << std::endl;
        scheduler.print_dashboard.store(true);
      }
    });

    scheduler.Run();

  } catch (const std::exception& e) {
    std::cerr << "CRITICAL ERROR: " << e.what() << std::endl;
    return 1;
  }
  std::cout << "Agent exited successfully" << std::endl;
  return 0;
}

int main(int argc, const char* argv[]) {
  std::string program_name(argv[0]);

  if (program_name.ends_with("volta"))
    return cli_mode(argc, argv);
  else if (program_name.ends_with("voltad"))
    return agent_mode(argc, argv);
  return 1;
}
