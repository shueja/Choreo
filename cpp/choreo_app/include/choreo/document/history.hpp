#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>

#include "choreo/document/store.hpp"

namespace choreo::document {

class SnapshotHistory {
 public:
  explicit SnapshotHistory(Document document, std::size_t limit = 100)
      : current_{std::make_shared<const Document>(std::move(document))},
        limit_{limit} {}

  std::shared_ptr<const Document> Snapshot() const { return current_; }
  std::uint64_t Revision() const { return revision_; }
  bool CanUndo() const { return !undo_.empty(); }
  bool CanRedo() const { return !redo_.empty(); }

  void Commit(Document document) {
    auto next = std::make_shared<const Document>(std::move(document));
    if (limit_ != 0) {
      undo_.push_back(current_);
      if (undo_.size() > limit_) {
        undo_.pop_front();
      }
    }
    redo_.clear();
    current_ = std::move(next);
    ++revision_;
  }

  bool Undo() {
    if (undo_.empty()) {
      return false;
    }
    redo_.push_back(current_);
    current_ = std::move(undo_.back());
    undo_.pop_back();
    ++revision_;
    return true;
  }

  bool Redo() {
    if (redo_.empty()) {
      return false;
    }
    undo_.push_back(current_);
    current_ = std::move(redo_.back());
    redo_.pop_back();
    ++revision_;
    return true;
  }

 private:
  std::shared_ptr<const Document> current_;
  std::deque<std::shared_ptr<const Document>> undo_;
  std::deque<std::shared_ptr<const Document>> redo_;
  std::size_t limit_;
  std::uint64_t revision_ = 0;
};

}