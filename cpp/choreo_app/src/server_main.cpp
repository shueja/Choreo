#include <array>
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

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

#include "choreo/document/application_api.hpp"

namespace {

volatile std::sig_atomic_t g_shutdownRequested = 0;

#ifndef _WIN32
bool OpenBrowser(std::string_view url) {
#ifdef __APPLE__
  constexpr std::string_view executable = "open";
#else
  constexpr std::string_view executable = "xdg-open";
#endif
  posix_spawn_file_actions_t actions;
  if (posix_spawn_file_actions_init(&actions) != 0) {
    return false;
  }
  const int redirectResult =
      posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO, "/dev/null",
                                       O_WRONLY, 0) |
      posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null",
                                       O_WRONLY, 0);
  if (redirectResult != 0) {
    posix_spawn_file_actions_destroy(&actions);
    return false;
  }
  std::string program{executable};
  std::string target{url};
  std::array<char*, 3> arguments{program.data(), target.data(), nullptr};
  pid_t process = 0;
  const int spawnResult = posix_spawnp(
      &process, program.c_str(), &actions, nullptr, arguments.data(), environ);
  posix_spawn_file_actions_destroy(&actions);
  if (spawnResult != 0) {
    return false;
  }
  std::thread([process] {
    while (waitpid(process, nullptr, 0) == -1 && errno == EINTR) {
    }
  }).detach();
  return true;
}
#else
bool OpenBrowser(std::string_view url) {
  const std::wstring wideUrl{url.begin(), url.end()};
  const auto result = ShellExecuteW(nullptr, L"open", wideUrl.c_str(), nullptr,
                                    nullptr, SW_SHOWNORMAL);
  return reinterpret_cast<std::intptr_t>(result) > 32;
}
#endif

#ifdef _WIN32
std::optional<std::filesystem::path> ShowProjectPathDialog(bool save) {
  std::array<wchar_t, 32768> selectedPath{};
  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.lpstrFilter = L"Choreo project (*.chor)\0*.chor\0All files (*.*)\0*.*\0";
  dialog.lpstrFile = selectedPath.data();
  dialog.nMaxFile = static_cast<DWORD>(selectedPath.size());
  dialog.lpstrDefExt = L"chor";
  dialog.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST |
                 (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
  const auto selected = save ? GetSaveFileNameW(&dialog)
                             : GetOpenFileNameW(&dialog);
  if (!selected) {
    return std::nullopt;
  }
  return std::filesystem::path{selectedPath.data()};
}

void ConfigureProjectPathPrompts(
    choreo::document::ApplicationApi::Options& options) {
  auto configure = [](auto& prompt, bool save) {
    prompt = [save](auto completion) {
      std::thread([save, completion = std::move(completion)]() mutable {
        std::optional<std::filesystem::path> path;
        try {
          path = ShowProjectPathDialog(save);
        } catch (...) {
        }
        completion(std::move(path));
      }).detach();
    };
  };
  configure(options.promptForProjectPath, false);
  configure(options.promptForSavePath, true);
}
#endif

void RequestShutdown(int) {
  g_shutdownRequested = 1;
}

struct Arguments {
  std::uint16_t port = 0;
  std::filesystem::path projectPath;
  std::filesystem::path webRoot;
  std::string generator;
  std::vector<std::string> allowedOrigins;
  bool openBrowser = true;
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
    if (argument == "--no-browser") {
      arguments.openBrowser = false;
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
            "[--web-root PATH] [--allowed-origin ORIGIN...] [--no-browser]\n"
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
  #ifdef _WIN32
    ConfigureProjectPathPrompts(applicationOptions);
  #endif
    choreo::document::ApplicationApi applicationApi{
        server, std::move(applicationOptions)};

    server.Start();
    const auto url =
        "http://127.0.0.1:" + std::to_string(server.Port());
    std::cout << "{\"type\":\"ready\",\"url\":\"" << url << "\"}\n"
              << std::flush;
    if (arguments.openBrowser && !OpenBrowser(url)) {
      std::cerr << "choreo-server: could not open browser for " << url << '\n';
    }
    while (g_shutdownRequested == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
  } catch (const std::exception& exception) {
    std::cerr << "choreo-server: " << exception.what() << '\n';
    return 1;
  }
  return 0;
}
