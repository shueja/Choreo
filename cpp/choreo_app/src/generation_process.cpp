#include "choreo/document/generation_process.hpp"

#include <csignal>
#include <fstream>
#include <iterator>
#include <random>
#include <stdexcept>
#include <system_error>
#include <vector>

#include <wpi/net/uv/Loop.hpp>
#include <wpi/net/uv/Process.hpp>
#include <wpi/util/json.hpp>

namespace choreo::document {
namespace {

std::string PathArgument(const std::filesystem::path& path) {
  const auto utf8 = path.u8string();
  return std::string(utf8.begin(), utf8.end());
}

void WriteInput(const std::filesystem::path& path, const std::string& content) {
  std::ofstream file{path, std::ios::binary};
  file.write(content.data(), static_cast<std::streamsize>(content.size()));
  file.close();
  if (!file) {
    throw std::runtime_error("cannot write generation input");
  }
}

}

struct GenerationProcess::Impl {
  std::filesystem::path directory;
  std::filesystem::path resultPath;
  std::string trajectoryId;
  const DocumentStore* owner = nullptr;
  std::uint64_t revision = 0;
  std::shared_ptr<wpi::net::uv::Loop> loop;
  std::shared_ptr<wpi::net::uv::Process> process;
  std::int64_t exitStatus = -1;
  int exitSignal = 0;
  bool exited = false;
  bool cancelled = false;
  bool committed = false;

  ~Impl() {
    if (process && !exited) {
      process->Kill(SIGKILL);
      loop->Run();
    }
    if (!directory.empty()) {
      std::error_code error;
      std::filesystem::remove_all(directory, error);
    }
  }
};

GenerationProcess::GenerationProcess(const std::filesystem::path& executable,
                                     const DocumentStore& store,
                                     const std::string& trajectoryId,
                                     const std::string& progressUrl)
    : impl_{std::make_unique<Impl>()} {
  const auto snapshot = store.Snapshot();
  const auto* trajectory = snapshot->trajectories.find(trajectoryId);
  if (!trajectory) {
    throw std::invalid_argument("unknown trajectory");
  }
  impl_->trajectoryId = trajectoryId;
  impl_->owner = &store;
  impl_->revision = store.Revision();
  std::random_device random;
  for (int attempt = 0; attempt < 100; ++attempt) {
    auto directory = std::filesystem::temp_directory_path() /
                     ("choreo-generation-" + std::to_string(random()) + "-" +
                      std::to_string(random()));
    std::error_code error;
    if (std::filesystem::create_directory(directory, error)) {
      impl_->directory = std::move(directory);
      break;
    }
    if (error) {
      throw std::system_error(error, "cannot create generation directory");
    }
  }
  if (impl_->directory.empty()) {
    throw std::runtime_error("cannot allocate generation directory");
  }
#ifndef _WIN32
  std::filesystem::permissions(impl_->directory,
                               std::filesystem::perms::owner_all,
                               std::filesystem::perm_options::replace);
#endif
  const auto projectPath = impl_->directory / "input.chor";
  const auto trajectoryPath = impl_->directory / "input.traj";
  impl_->resultPath = impl_->directory / "result.traj";
  WriteInput(projectPath, wpi::util::json{snapshot->project}.to_string());
  WriteInput(trajectoryPath, wpi::util::json{*trajectory}.to_string());

  impl_->loop = wpi::net::uv::Loop::Create();
  if (!impl_->loop) {
    throw std::runtime_error("cannot create generation process loop");
  }
  const auto executableArg = PathArgument(std::filesystem::absolute(executable));
  const auto projectArg = PathArgument(projectPath);
  const auto trajectoryArg = PathArgument(trajectoryPath);
  const auto resultArg = PathArgument(impl_->resultPath);
  using Process = wpi::net::uv::Process;
  std::vector<Process::Option> arguments{executableArg, "--chor", projectArg,
                                         "--trajectory", trajectoryArg,
                                         "--output", resultArg};
  if (!progressUrl.empty()) {
    arguments.emplace_back("--progress-url");
    arguments.emplace_back(progressUrl);
  }
  impl_->process = Process::SpawnArray(*impl_->loop, executableArg, arguments);
  if (!impl_->process) {
    throw std::runtime_error("cannot start generation process");
  }
  auto* state = impl_.get();
  impl_->process->exited.connect([state](std::int64_t status, int signal) {
    state->exitStatus = status;
    state->exitSignal = signal;
    state->exited = true;
    state->process->Close();
  });
}

GenerationProcess::~GenerationProcess() = default;

bool GenerationProcess::Poll() {
  impl_->loop->Run(wpi::net::uv::Loop::Mode::NO_WAIT);
  return impl_->exited;
}

void GenerationProcess::Wait() {
  impl_->loop->Run();
}

void GenerationProcess::Cancel() {
  if (!impl_->exited) {
    impl_->cancelled = true;
    impl_->process->Kill(SIGKILL);
  }
}

void GenerationProcess::CommitResult(DocumentStore& store) {
  if (&store != impl_->owner) {
    throw std::invalid_argument("generation result belongs to another store");
  }
  if (!impl_->exited || impl_->cancelled || impl_->exitStatus != 0 ||
      impl_->exitSignal != 0 || impl_->committed) {
    throw std::runtime_error("generation did not complete successfully");
  }
  constexpr std::uintmax_t maxResultBytes = 64 * 1024 * 1024;
  std::error_code error;
  const auto size = std::filesystem::file_size(impl_->resultPath, error);
  if (error || size > maxResultBytes) {
    throw std::runtime_error("missing or oversized generation result");
  }
  std::ifstream file{impl_->resultPath, std::ios::binary};
  std::string content{std::istreambuf_iterator<char>{file},
                      std::istreambuf_iterator<char>{}};
  if (!file || content.size() != size) {
    throw std::runtime_error("cannot read generation result");
  }
  auto trajectory =
      wpi::util::json::parse_or_throw(content).get<Trajectory>();
  if (trajectory.uuid != impl_->trajectoryId) {
    throw std::invalid_argument("generation result trajectory ID mismatch");
  }
  store.CommitGeneratedTrajectory(std::move(trajectory), impl_->revision);
  impl_->committed = true;
}

}