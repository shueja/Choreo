#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "choreo/document/application_api.hpp"

namespace {

volatile std::sig_atomic_t g_shutdownRequested = 0;

void RequestShutdown(int) {
  g_shutdownRequested = 1;
}

struct Arguments {
  std::uint16_t port = 0;
  std::filesystem::path projectPath;
  std::filesystem::path webRoot;
  std::string generator;
  std::vector<std::string> allowedOrigins;
  bool showHelp = false;
};

bool ParsePort(std::string_view value, std::uint16_t& port) {
  unsigned int parsed = 0;
  const auto [end, error] =
      std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (error != std::errc{} || end != value.data() + value.size() ||
      parsed > UINT16_MAX) {
    return false;
  }
  port = static_cast<std::uint16_t>(parsed);
  return true;
}

bool ParseArguments(int argc, char** argv, Arguments& arguments,
                    std::string& error) {
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    if (argument == "--help" || argument == "-h") {
      arguments.showHelp = true;
      continue;
    }
    if (argument != "--port" && argument != "--project" &&
        argument != "--generator" &&
        argument != "--allowed-origin" && argument != "--web-root") {
      error = "unknown argument: " + std::string{argument};
      return false;
    }
    if (index + 1 >= argc) {
      error = "missing value for " + std::string{argument};
      return false;
    }
    const std::string_view value{argv[++index]};
    if (value.empty()) {
      error = "empty value for " + std::string{argument};
      return false;
    }
    if (argument == "--port") {
      if (!ParsePort(value, arguments.port)) {
        error = "port must be an integer from 0 to 65535";
        return false;
      }
    } else if (argument == "--project") {
      arguments.projectPath = value;
    } else if (argument == "--generator") {
      arguments.generator = value;
    } else if (argument == "--web-root") {
      arguments.webRoot = value;
    } else {
      arguments.allowedOrigins.emplace_back(value);
    }
  }
  return true;
}

void PrintUsage(std::ostream& output) {
  output << "Usage: choreo-server [--port PORT] [--project PATH] "
            "[--generator PATH] "
            "[--web-root PATH] [--allowed-origin ORIGIN...]\n"
            "Starts the localhost Choreo application API. Port 0 selects "
            "an available port.\n";
}

}  // namespace

int main(int argc, char** argv) {
  Arguments arguments;
  std::string error;
  if (!ParseArguments(argc, argv, arguments, error)) {
    std::cerr << "choreo-server: " << error << '\n';
    PrintUsage(std::cerr);
    return 2;
  }
  if (arguments.showHelp) {
    PrintUsage(std::cout);
    return 0;
  }

  std::signal(SIGINT, RequestShutdown);
  std::signal(SIGTERM, RequestShutdown);

  try {
    choreo::api::ServerOptions serverOptions;
    serverOptions.bindAddress = "127.0.0.1";
    serverOptions.port = arguments.port;
    serverOptions.allowedOrigins = std::move(arguments.allowedOrigins);
    choreo::api::ApiServer server{std::move(serverOptions)};
    if (!arguments.webRoot.empty()) {
      server.RegisterStaticDirectory(arguments.webRoot);
    }

    choreo::document::ApplicationApi::Options applicationOptions;
    applicationOptions.generatorExecutable = arguments.generator;
    applicationOptions.initialProjectPath = arguments.projectPath;
    choreo::document::ApplicationApi applicationApi{
        server, std::move(applicationOptions)};

    server.Start();
    std::cout << "{\"type\":\"ready\",\"url\":\"http://127.0.0.1:"
              << server.Port() << "\"}\n"
              << std::flush;
    while (g_shutdownRequested == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
  } catch (const std::exception& exception) {
    std::cerr << "choreo-server: " << exception.what() << '\n';
    return 1;
  }
  return 0;
}
