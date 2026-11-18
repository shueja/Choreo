#pragma once

#include <filesystem>
#include <memory>
#include <string>

#include "choreo/document/document_store.hpp"

namespace choreo::document {

class GenerationProcess {
 public:
  GenerationProcess(const std::filesystem::path& executable,
                    const DocumentStore& store, const std::string& trajectoryId,
                    const std::string& progressUrl = {});
  ~GenerationProcess();
  GenerationProcess(const GenerationProcess&) = delete;
  GenerationProcess& operator=(const GenerationProcess&) = delete;

  bool Poll();
  void Wait();
  void Cancel();
  void CommitResult(DocumentStore& store);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}