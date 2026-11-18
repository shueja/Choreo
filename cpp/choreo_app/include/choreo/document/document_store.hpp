#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "choreo/document/history.hpp"

namespace choreo::document {

class SerializedDocument {
 public:
  std::string project;
  std::map<std::string, std::string> trajectories;
};

class DocumentFiles {
 public:
  std::filesystem::path project;
  std::map<std::string, std::filesystem::path> trajectories;
};

class SaveResult {
 public:
  std::optional<std::string> projectError;
  std::map<std::string, std::string> trajectoryErrors;

  bool Success() const { return !projectError && trajectoryErrors.empty(); }
};

class DocumentStore {
 public:
  using Listener =
      std::function<void(std::shared_ptr<const Document>, std::uint64_t)>;
  using ListenerId = std::uint64_t;

  explicit DocumentStore(Document document, std::size_t historyLimit = 100);

  std::shared_ptr<const Document> Snapshot() const;
  std::uint64_t Revision() const;
  bool CanUndo() const;
  bool CanRedo() const;
  bool IsProjectDirty() const;
  bool IsTrajectoryDirty(const std::string& id) const;
  ListenerId Subscribe(Listener listener);
  void Unsubscribe(ListenerId id);
  std::vector<std::exception_ptr> TakeNotificationErrors();
  void Commit(Document document);
  void Commit(Document document, std::uint64_t expectedRevision);
  void CommitGeneratedTrajectory(Trajectory trajectory,
                                 std::uint64_t expectedRevision);
  void RenameVariable(const std::string& id, const std::string& newName);
  bool Undo();
  bool Redo();
  SerializedDocument Serialize() const;
  void Reload(const SerializedDocument& serialized);
  SaveResult SaveFiles(const DocumentFiles& files);
  void ReloadFiles(const DocumentFiles& files);

 private:
  void Notify();

  SnapshotHistory history_;
  std::size_t historyLimit_;
  std::uint64_t revision_ = 0;
  std::optional<std::string> savedProject_;
  std::map<std::string, std::string> savedTrajectories_;
  std::map<ListenerId, Listener> listeners_;
  std::deque<std::pair<std::shared_ptr<const Document>, std::uint64_t>>
      pendingNotifications_;
  std::vector<std::exception_ptr> notificationErrors_;
  ListenerId nextListenerId_ = 0;
  bool dispatching_ = false;
};

}