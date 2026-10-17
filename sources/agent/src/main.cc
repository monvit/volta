#include <iostream>
#include <string>

#include "client/volta_collector_client.h"
#include "collectors/collector.h"
#include "config/config.h"
#include "config/config_loader.h"
#include "config/config_resolution.h"
#include "logger/logger_builder.h"
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

int cli_mode(int argc, const char* argv[]) {
  if (argc < 2) return 1;

  MessageQueue msq("/volta_agent_command_queue", MessageQueue::Role::Sender,
                   {});
  std::string command(argv[1]);

  if (command == "export") {
    if (argc < 3) return 1;
    std::string subcommand(argv[2]);

    if (subcommand == "start") {
      std::string file("");
      if (argc >= 4) {
        std::filesystem::path fp(argv[3]);
        file = std::filesystem::absolute(fp).string();
      }
      std::string message = std::format("dump_start;{}", file);
      // std::string message = "dump_start";
      msq.send(message);
      std::cout << "Message " << message << " sent" << std::endl;
      return 0;
    }

    if (subcommand == "stop") {
      msq.send("dump_end");
      std::cout << "Message dump_end sent" << std::endl;
      return 0;
    }

    return 1;
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

  if (resolved.source == config::ConfigSource::kBuiltinDefaults) {
    logger->Log(LogLevel::INFO_LEVEL, "Config source: {}",
                config::ConfigSourceToString(resolved.source));
  } else {
    logger->Log(LogLevel::INFO_LEVEL, "Config source: {} ({})",
                config::ConfigSourceToString(resolved.source),
                resolved.path->string());
  }

  install_signal_handlers();
  try {
    auto agent_config = config::ConfigLoader::LoadConfig(resolved.path);
    logger = logging::BuildLogger(agent_config);

    collectors::CollectorRegistry::Instance().SetLoggers(*logger);

    auto active_collectors = collectors::CollectorRegistry::Instance().Resolve(
        agent_config.requestedMetrics);

    std::shared_ptr<MetricsBuffer> buffer =
        std::make_shared<MetricsBuffer>(agent_config);
    Scheduler scheduler(agent_config, std::move(active_collectors), buffer,
                        logger->WithName("Scheduler"));

    auto grpc_logger = logger->WithName("gRPC");
    std::jthread grpc_thread([logger = std::move(grpc_logger), &scheduler,
                              &agent_config, &buffer]() {
      if (auto channel = client::Client::CreateChannel(
              agent_config.server_address + ":" +
              std::to_string(agent_config.server_port))) {
        client::Client grpc_client(channel, agent_config, buffer,
                                   logger->WithName("gRPC Client"));
        grpc_client.Connect();
      } else {
        logger->Log(
            LogLevel::ERROR_LEVEL,
            "Failed to create channel, falling back to local dashboard");
        // scheduler.print_dashboard.store(true);
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
