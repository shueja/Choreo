#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>

#include "choreo/api/server.hpp"

namespace choreo::document {

class ApplicationApi {
 public:
  using ProjectPathCompletion =
      std::function<void(std::optional<std::filesystem::path>)>;
  // Must return promptly; schedule UI work asynchronously and invoke the
  // completion when the user selects a path or cancels.
  using ProjectPathPrompt = std::function<void(ProjectPathCompletion)>;

  struct Options {
    std::filesystem::path generatorExecutable;
    std::filesystem::path initialProjectPath;
    ProjectPathPrompt promptForProjectPath;
    ProjectPathPrompt promptForSavePath;
  };

  ApplicationApi(choreo::api::ApiServer& server, Options options);
  ~ApplicationApi();

  ApplicationApi(const ApplicationApi&) = delete;
  ApplicationApi& operator=(const ApplicationApi&) = delete;
  ApplicationApi(ApplicationApi&&) = delete;
  ApplicationApi& operator=(ApplicationApi&&) = delete;

 private:
  struct Impl;
  std::shared_ptr<Impl> m_impl;
};

}  // namespace choreo::document
