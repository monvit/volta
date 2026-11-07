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
#include "logger/logger_builder.h"
#include "logger/stderr_logger.h"
#include "scheduler.h"

using namespace volta::agent;
using logging::LogLevel;

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
    auto logger = std::make_unique<logging::StderrLogger>();
    control::ControlSocket socket(path.string(),
                                  control::ControlSocket::Role::kClient,
                                  logger->WithName("ControlSocket"));
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
  config::Config default_config;
  auto logger = logging::BuildLogger(default_config);

  auto cli = config::ParseCli(argc, argv);
  if (!cli) {
    logger->Log(LogLevel::ERROR_LEVEL, "Invalid arguments (try --help)");
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
    logger->Log(LogLevel::ERROR_LEVEL, "{}", e.what());
    return 1;
  }

  install_signal_handlers();
  try {
    auto agent_config = config::ConfigLoader::LoadConfig(resolved.path);
    logger = logging::BuildLogger(agent_config);

    if (resolved.source == config::ConfigSource::kBuiltinDefaults) {
      logger->Log(LogLevel::INFO_LEVEL, "Config source: {}",
                  config::ConfigSourceToString(resolved.source));
    } else {
      logger->Log(LogLevel::INFO_LEVEL, "Config source: {} ({})",
                  config::ConfigSourceToString(resolved.source),
                  resolved.path->string());
    }

    collectors::CollectorRegistry::Instance().SetLoggers(*logger);

    auto active_collectors = collectors::CollectorRegistry::Instance().Resolve(
        agent_config.requestedMetrics);

    std::shared_ptr<MetricsBuffer> buffer =
        std::make_shared<MetricsBuffer>(agent_config);
    Scheduler scheduler(agent_config, std::move(active_collectors), buffer,
                        logger->WithName("Scheduler"));

    auto grpc_logger = logger->WithName("gRPC");
    std::jthread grpc_thread(
        [logger = std::move(grpc_logger), &agent_config, &buffer]() {
          if (auto channel = client::Client::CreateChannel(
                  agent_config.server_address + ":" +
                  std::to_string(agent_config.server_port))) {
            client::Client grpc_client(channel, agent_config, buffer,
                                       logger->WithName("gRPC Client"));
            grpc_client.Connect();
          } else {
            logger->Log(LogLevel::ERROR_LEVEL, "Failed to create gRPC channel");
          }
        });

    scheduler.Run();

  } catch (const std::exception& e) {
    logger->Log(LogLevel::ERROR_LEVEL, "Critical error: {}", e.what());
    return 1;
  }
  logger->Log(LogLevel::INFO_LEVEL, "Agent exited successfully");
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
