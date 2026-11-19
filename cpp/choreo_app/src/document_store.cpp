#include "choreo/document/document_store.hpp"

#include <stdexcept>
#include <utility>

#include <wpi/util/json.hpp>

#include "choreo/document/evaluation.hpp"

namespace choreo::document {
namespace {

bool SameDocument(const Document& left, const Document& right) {
  if (wpi::util::json{left.project} != wpi::util::json{right.project} ||
      left.trajectories.size() != right.trajectories.size()) {
    return false;
  }
  for (const auto& [id, trajectory] : left.trajectories) {
    const auto* other = right.trajectories.find(id);
    if (!other || wpi::util::json{trajectory} != wpi::util::json{*other}) {
      return false;
    }
  }
  return true;
}

}

DocumentStore::DocumentStore(Document document, std::size_t historyLimit)
    : history_{evaluation::EvaluateDocument(document), historyLimit},
      historyLimit_{historyLimit} {}

std::shared_ptr<const Document> DocumentStore::Snapshot() const {
  return history_.Snapshot();
}
std::uint64_t DocumentStore::Revision() const {
  return revision_;
}
bool DocumentStore::CanUndo() const {
  return history_.CanUndo();
}
bool DocumentStore::CanRedo() const {
  return history_.CanRedo();
}

DocumentStore::ListenerId DocumentStore::Subscribe(Listener listener) {
  if (!listener) {
    throw std::invalid_argument("listener cannot be empty");
  }
  const auto id = nextListenerId_++;
  listeners_.emplace(id, std::move(listener));
  return id;
}

void DocumentStore::Unsubscribe(ListenerId id) {
  listeners_.erase(id);
}

std::vector<std::exception_ptr> DocumentStore::TakeNotificationErrors() {
  return std::exchange(notificationErrors_, {});
}

void DocumentStore::Notify() {
  pendingNotifications_.emplace_back(Snapshot(), revision_);
  if (dispatching_) {
    return;
  }
  dispatching_ = true;
  try {
    while (!pendingNotifications_.empty()) {
      auto notification = std::move(pendingNotifications_.front());
      pendingNotifications_.pop_front();
      std::vector<Listener> listeners;
      listeners.reserve(listeners_.size());
      for (const auto& [id, listener] : listeners_) {
        listeners.push_back(listener);
      }
      for (const auto& listener : listeners) {
        try {
          listener(notification.first, notification.second);
        } catch (...) {
          notificationErrors_.push_back(std::current_exception());
        }
      }
    }
    dispatching_ = false;
  } catch (...) {
    dispatching_ = false;
    throw;
  }
}

bool DocumentStore::IsProjectDirty() const {
  return !savedProject_ ||
         wpi::util::json::parse_or_throw(
             wpi::util::json{Snapshot()->project}.to_string()) !=
             wpi::util::json::parse_or_throw(*savedProject_);
}

bool DocumentStore::IsTrajectoryDirty(const std::string& id) const {
  const auto* trajectory = Snapshot()->trajectories.find(id);
  const auto saved = savedTrajectories_.find(id);
  if (!trajectory || saved == savedTrajectories_.end()) {
    return trajectory || saved != savedTrajectories_.end();
  }
  return wpi::util::json::parse_or_throw(
             wpi::util::json{*trajectory}.to_string()) !=
         wpi::util::json::parse_or_throw(saved->second);
}

void DocumentStore::Commit(Document document) {
  auto evaluated = evaluation::EvaluateDocument(document);
  if (SameDocument(*Snapshot(), evaluated)) {
    return;
  }
  history_.Commit(std::move(evaluated));
  ++revision_;
  Notify();
}

void DocumentStore::Commit(Document document, std::uint64_t expectedRevision) {
  if (expectedRevision != revision_) {
    throw std::invalid_argument("document revision is stale");
  }
  Commit(std::move(document));
}

void DocumentStore::CommitGeneratedTrajectory(Trajectory trajectory,
                                              std::uint64_t expectedRevision) {
  if (expectedRevision != revision_) {
    throw std::invalid_argument("document revision is stale");
  }
  const auto snapshot = Snapshot();
  const auto* original = snapshot->trajectories.find(trajectory.uuid);
  if (!original || !trajectory.config || !trajectory.snapshot ||
      !trajectory.output ||
      std::visit([](const auto& samples) { return !samples; },
                 trajectory.output->trajectory)) {
    throw std::invalid_argument("invalid generated trajectory");
  }
  if ((snapshot->project.type == DriveType::Swerve &&
       !std::holds_alternative<std::optional<wpi::math::HolonomicTrajectory>>(
           trajectory.output->trajectory)) ||
      (snapshot->project.type == DriveType::Differential &&
       !std::holds_alternative<std::optional<wpi::math::DifferentialTrajectory>>(
           trajectory.output->trajectory))) {
    throw std::invalid_argument("generated trajectory drive type does not match");
  }
  trajectory = wpi::util::json{trajectory}.get<Trajectory>();
  if (wpi::util::json{*trajectory.config} !=
          wpi::util::json{snapshot->project.config} ||
      wpi::util::json{*trajectory.snapshot} !=
          wpi::util::json{original->params} ||
      trajectory.events.size() != original->events.size()) {
    throw std::invalid_argument("generated trajectory does not match inputs");
  }
  auto editable = trajectory;
  editable.config = original->config;
  editable.snapshot = original->snapshot;
  editable.output = original->output;
  for (std::size_t index = 0; index < editable.events.size(); ++index) {
    editable.events[index].from.targetTime =
        original->events[index].from.targetTime;
  }
  if (wpi::util::json{editable} != wpi::util::json{*original}) {
    throw std::invalid_argument("generated trajectory changed editable inputs");
  }
  auto candidate = *snapshot;
    const auto trajectoryId = trajectory.uuid;
  candidate.trajectories =
      candidate.trajectories.set(trajectoryId, std::move(trajectory));
  Commit(std::move(candidate), expectedRevision);
}

void DocumentStore::RenameVariable(const std::string& id,
                                   const std::string& newName) {
  Commit(evaluation::RenameVariable(*Snapshot(), id, newName));
}

void DocumentStore::UpdateVariable(
    const std::string& id, const std::optional<std::string>& name,
    const std::optional<std::string>& expression) {
  auto candidate = *Snapshot();
  auto target = candidate.project.variables.find(id);
  if (target == candidate.project.variables.end()) {
    throw std::invalid_argument("unknown variable ID: " + id);
  }
  if (!name && !expression) {
    throw std::invalid_argument("variable patch must contain editable fields");
  }
  if (name && *name != target->second.name) {
    candidate = evaluation::RenameVariable(candidate, id, *name);
  }
  if (expression) {
    candidate.project.variables.at(id).expression = *expression;
  }
  Commit(std::move(candidate));
}

bool DocumentStore::Undo() {
  if (!history_.Undo()) {
    return false;
  }
  ++revision_;
  Notify();
  return true;
}

bool DocumentStore::Redo() {
  if (!history_.Redo()) {
    return false;
  }
  ++revision_;
  Notify();
  return true;
}

SerializedDocument DocumentStore::Serialize() const {
  const auto snapshot = Snapshot();
  SerializedDocument result{wpi::util::json{snapshot->project}.to_string(), {}};
  for (const auto& [id, trajectory] : snapshot->trajectories) {
    if (id != trajectory.uuid) {
      throw std::invalid_argument("trajectory key does not match UUID");
    }
    result.trajectories.emplace(id, wpi::util::json{trajectory}.to_string());
  }
  return result;
}

void DocumentStore::Reload(const SerializedDocument& serialized) {
  Document candidate;
  candidate.project =
      wpi::util::json::parse_or_throw(serialized.project).get<Project>();
  for (const auto& [id, content] : serialized.trajectories) {
    auto trajectory =
        wpi::util::json::parse_or_throw(content).get<Trajectory>();
    if (id != trajectory.uuid) {
      throw std::invalid_argument("trajectory key does not match UUID");
    }
    candidate.trajectories =
        candidate.trajectories.set(id, std::move(trajectory));
  }
  SnapshotHistory replacement{evaluation::EvaluateDocument(candidate),
                              historyLimit_};
  history_ = std::move(replacement);
  savedProject_ = serialized.project;
  savedTrajectories_ = serialized.trajectories;
  ++revision_;
  Notify();
}

}