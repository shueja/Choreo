#include <algorithm>
#include <atomic>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <vector>

#include "choreo/document/document_store.hpp"

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace choreo::document {
namespace {

std::atomic<std::uint64_t> nextTemporary{0};

std::filesystem::path TemporaryPath(const std::filesystem::path& destination) {
  auto path = destination;
#if defined(_WIN32)
  const auto process = GetCurrentProcessId();
#else
  const auto process = getpid();
#endif
  path +=
      ".tmp-" + std::to_string(process) + "-" + std::to_string(nextTemporary++);
  return path;
}

bool SameAbsolutePath(const std::filesystem::path& left,
                      const std::filesystem::path& right) {
  const auto leftPath = std::filesystem::absolute(left).lexically_normal();
  const auto rightPath = std::filesystem::absolute(right).lexically_normal();
#if defined(_WIN32)
  return CompareStringOrdinal(leftPath.c_str(), -1, rightPath.c_str(), -1,
                              TRUE) == CSTR_EQUAL;
#else
  return leftPath == rightPath;
#endif
}

void ValidatePaths(const DocumentFiles& files) {
  std::vector<std::filesystem::path> paths{files.project};
  for (const auto& [id, path] : files.trajectories) {
    paths.push_back(path);
  }
  for (std::size_t index = 0; index < paths.size(); ++index) {
    if (paths[index].empty()) {
      throw std::invalid_argument("document path cannot be empty");
    }
    for (std::size_t previous = 0; previous < index; ++previous) {
      std::error_code error;
      if (SameAbsolutePath(paths[index], paths[previous]) ||
          (std::filesystem::equivalent(paths[index], paths[previous], error) &&
           !error)) {
        throw std::invalid_argument("document files cannot share a path");
      }
    }
  }
}

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream input{path, std::ios::binary};
  if (!input) {
    throw std::runtime_error("cannot open document file: " + path.string());
  }
  std::string result{std::istreambuf_iterator<char>{input},
                     std::istreambuf_iterator<char>{}};
  if (input.bad()) {
    throw std::runtime_error("cannot read document file: " + path.string());
  }
  return result;
}

void WriteAtomic(const std::filesystem::path& path,
                 const std::string& content) {
  std::filesystem::path temporary;
#if defined(_WIN32)
  HANDLE file = INVALID_HANDLE_VALUE;
  for (int attempt = 0; attempt < 100; ++attempt) {
    temporary = TemporaryPath(path);
    file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
      break;
    }
    if (GetLastError() != ERROR_FILE_EXISTS) {
      throw std::system_error(static_cast<int>(GetLastError()),
                              std::system_category(),
                              "cannot create temporary document file");
    }
  }
  if (file == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("cannot allocate temporary document file");
  }
  try {
    std::size_t offset = 0;
    while (offset < content.size()) {
      const auto remaining = content.size() - offset;
      const auto amount =
          static_cast<DWORD>(std::min<std::size_t>(remaining, MAXDWORD));
      DWORD written = 0;
      if (!WriteFile(file, content.data() + offset, amount, &written,
                     nullptr) ||
          written == 0) {
        throw std::system_error(static_cast<int>(GetLastError()),
                                std::system_category(),
                                "cannot write document file");
      }
      offset += written;
    }
    if (!FlushFileBuffers(file)) {
      throw std::system_error(static_cast<int>(GetLastError()),
                              std::system_category(),
                              "cannot flush document file");
    }
    if (!CloseHandle(file)) {
      file = INVALID_HANDLE_VALUE;
      throw std::system_error(static_cast<int>(GetLastError()),
                              std::system_category(),
                              "cannot close document file");
    }
    file = INVALID_HANDLE_VALUE;
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      throw std::system_error(static_cast<int>(GetLastError()),
                              std::system_category(),
                              "cannot replace document file");
    }
  } catch (...) {
    if (file != INVALID_HANDLE_VALUE) {
      CloseHandle(file);
    }
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw;
  }
#else
  int file = -1;
  for (int attempt = 0; attempt < 100; ++attempt) {
    temporary = TemporaryPath(path);
    file = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (file >= 0) {
      break;
    }
    if (errno != EEXIST) {
      throw std::system_error(errno, std::generic_category(),
                              "cannot create temporary document file");
    }
  }
  if (file < 0) {
    throw std::runtime_error("cannot allocate temporary document file");
  }
  try {
    std::size_t offset = 0;
    while (offset < content.size()) {
      const auto count =
          write(file, content.data() + offset, content.size() - offset);
      if (count < 0 && errno == EINTR) {
        continue;
      }
      if (count <= 0) {
        throw std::system_error(errno, std::generic_category(),
                                "cannot write document file");
      }
      offset += static_cast<std::size_t>(count);
    }
    if (fsync(file) != 0) {
      throw std::system_error(errno, std::generic_category(),
                              "cannot flush document file");
    }
    if (close(file) != 0) {
      file = -1;
      throw std::system_error(errno, std::generic_category(),
                              "cannot close document file");
    }
    file = -1;
    std::filesystem::rename(temporary, path);
  } catch (...) {
    if (file >= 0) {
      close(file);
    }
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw;
  }
#endif
}

}

SaveResult SaveSerializedFiles(const DocumentFiles& files,
                               const SerializedDocument& serialized) {
  ValidatePaths(files);
  if (serialized.trajectories.size() != files.trajectories.size()) {
    throw std::invalid_argument(
        "trajectory paths do not match document trajectories");
  }
  for (const auto& [id, content] : serialized.trajectories) {
    if (!files.trajectories.contains(id)) {
      throw std::invalid_argument("missing trajectory path: " + id);
    }
  }
  SaveResult result;
  try {
    WriteAtomic(files.project, serialized.project);
  } catch (const std::exception& error) {
    result.projectError = error.what();
  }
  for (const auto& [id, content] : serialized.trajectories) {
    try {
      WriteAtomic(files.trajectories.at(id), content);
    } catch (const std::exception& error) {
      result.trajectoryErrors.emplace(id, error.what());
    }
  }
  return result;
}

SaveResult DocumentStore::SaveFiles(const DocumentFiles& files) {
  auto serialized = Serialize();
  auto result = SaveSerializedFiles(files, serialized);
  MarkSaved(std::move(serialized), result);
  return result;
}

void DocumentStore::ReloadFiles(const DocumentFiles& files) {
  ValidatePaths(files);
  SerializedDocument serialized{ReadFile(files.project), {}};
  for (const auto& [id, path] : files.trajectories) {
    serialized.trajectories.emplace(id, ReadFile(path));
  }
  Reload(serialized);
}

}