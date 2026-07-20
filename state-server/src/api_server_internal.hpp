// Copyright (c) Choreo contributors

#pragma once

#include <cctype>
#include <expected>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <wpi/util/json.hpp>

#include "choreo/rest_router/http_adapter.hpp"

namespace choreo::state_server::detail {

using choreo::rest_router::HeaderMap;
using choreo::rest_router::Request;
using choreo::rest_router::Response;
using choreo::rest_router::RouteParams;

enum class ApiError {
  PreconditionRequired,
  BadRoute,
  NotFound,
  InvalidJson,
  InvalidOrder,
  StaleRevision,
  OperationTerminal,
  NoUndoAvailable,
  NoRedoAvailable,
  InvalidHistoryScope,
  InvalidHistoryPatch,
  InvalidHistorySnapshot,
  InvalidName,
  InvalidMode,
  InvalidOperation,
  UuidConflict,
  NameConflict,
};

struct ApiErrorDescriptor {
  int status;
  std::string_view code;
  std::string_view message;
};

constexpr ApiErrorDescriptor DescribeApiError(ApiError error) {
  switch (error) {
    case ApiError::PreconditionRequired:
      return {428, "precondition_required",
              "Missing If-Match header for mutation request"};
    case ApiError::BadRoute:
      return {400, "bad_route", "Invalid route parameters"};
    case ApiError::NotFound:
      return {404, "not_found", "Resource not found"};
    case ApiError::InvalidJson:
      return {400, "invalid_json", "Invalid JSON payload"};
    case ApiError::InvalidOrder:
      return {400, "invalid_order", "Invalid order payload"};
    case ApiError::StaleRevision:
      return {409, "stale_revision",
              "If-Match did not match current resource revision"};
    case ApiError::OperationTerminal:
      return {409, "operation_terminal",
              "Operation already reached a terminal state"};
    case ApiError::NoUndoAvailable:
      return {409, "no_undo_available", "No undo entry available for this scope"};
    case ApiError::NoRedoAvailable:
      return {409, "no_redo_available", "No redo entry available for this scope"};
    case ApiError::InvalidHistoryScope:
      return {422, "invalid_history_scope", "Unknown history scope"};
    case ApiError::InvalidHistoryPatch:
      return {422, "invalid_history_patch", "Invalid history patch"};
    case ApiError::InvalidHistorySnapshot:
      return {422, "invalid_history_snapshot", "Invalid history snapshot"};
    case ApiError::InvalidName:
      return {400, "invalid_name", "Name must not be empty"};
    case ApiError::InvalidMode:
      return {400, "invalid_mode", "Invalid mode"};
    case ApiError::InvalidOperation:
      return {422, "invalid_operation", "Operation is not allowed"};
    case ApiError::UuidConflict:
      return {409, "uuid_conflict", "UUID already exists"};
    case ApiError::NameConflict:
      return {409, "name_conflict", "Name already exists"};
  }
  return {500, "internal_error", "Internal server error"};
}

inline std::string ToLower(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (char c : text) {
    out.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
  }
  return out;
}

inline std::optional<std::string> GetHeaderCaseInsensitive(
    const HeaderMap& headers, std::string_view key) {
  const auto key_lc = ToLower(key);
  for (const auto& [name, value] : headers) {
    if (ToLower(name) == key_lc) {
      return value;
    }
  }
  return std::nullopt;
}

inline std::string QuotedEtag(std::string_view token) {
  return std::format("\"{}\"", token);
}

inline Response PreconditionRequired() {
  const auto d = DescribeApiError(ApiError::PreconditionRequired);
  return choreo::rest_router::MakeJsonErrorResponse(d.status, d.code, d.message);
}

inline Response ErrorResponse(ApiError error,
                              std::string_view message_override = {}) {
  const auto d = DescribeApiError(error);
  return choreo::rest_router::MakeJsonErrorResponse(
      d.status, d.code,
      message_override.empty() ? d.message : message_override);
}

inline Response BadRoute(std::string_view message) {
  return ErrorResponse(ApiError::BadRoute, message);
}

inline Response NotFound(std::string_view message) {
  return ErrorResponse(ApiError::NotFound, message);
}

inline Response InvalidJson(std::string_view message) {
  return ErrorResponse(ApiError::InvalidJson, message);
}

inline Response InvalidOrder(std::string_view message) {
  return ErrorResponse(ApiError::InvalidOrder, message);
}

inline Response ConflictStale(std::string_view current_revision) {
  auto response = ErrorResponse(ApiError::StaleRevision);
  response.headers["ETag"] = QuotedEtag(current_revision);
  return response;
}

inline bool MatchesIfMatchHeader(const Request& request,
                                 std::string_view expected_token) {
  const auto if_match = GetHeaderCaseInsensitive(request.headers, "if-match");
  if (!if_match.has_value()) {
    return false;
  }

  const std::string expected_quoted = QuotedEtag(expected_token);
  return *if_match == expected_quoted || *if_match == expected_token ||
         *if_match == "*";
}

inline std::optional<Response> ValidateIfMatchPrecondition(
    const Request& request, std::string_view current_revision) {
  if (!GetHeaderCaseInsensitive(request.headers, "if-match")) {
    return PreconditionRequired();
  }
  if (!MatchesIfMatchHeader(request, current_revision)) {
    return ConflictStale(current_revision);
  }
  return std::nullopt;
}

inline Response JsonResponse(int status, const wpi::util::json& body) {
  Response response;
  response.status = status;
  response.content_type = "application/json";
  response.body = body.to_string();
  return response;
}

template <typename ExpectedT = wpi::util::json, typename Fn = std::function<Response(const ExpectedT&)>>
inline Response WithParsedJsonBody(const Request& request, Fn&& on_success) {
  try {
    auto body_json =
        wpi::util::json::parse_or_throw(std::string_view{request.body});
    if constexpr (std::is_same_v<std::decay_t<ExpectedT>, wpi::util::json>) {
      return on_success(body_json);
    } else {
      auto body = body_json.get<ExpectedT>();
      return on_success(body);
    }
  } catch (const std::exception& ex) {
    return InvalidJson(ex.what());
  }
}

template <typename T>
inline Response JsonModelResponse(int status, const T& value) {
  return JsonResponse(status, wpi::util::json(value));
}

template <typename T>
inline Response JsonModelResponseWithEtag(int status, std::string_view token,
                                          const T& value) {
  auto response = JsonModelResponse(status, value);
  response.headers["ETag"] = QuotedEtag(token);
  return response;
}

inline Response EmptyResponse(int status) {
  Response response;
  response.status = status;
  response.content_type = "application/json";
  response.body = "";
  return response;
}

template <typename T>
inline std::optional<size_t> FindByUuid(const std::vector<T>& items,
                                        std::string_view uuid) {
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i].uuid == uuid) {
      return i;
    }
  }
  return std::nullopt;
}

inline std::optional<std::reference_wrapper<const std::string>> FindRouteParam(
    const choreo::rest_router::RouteParams& params, std::string_view key) {
  const auto it = params.find(std::string(key));
  if (it == params.end()) {
    return std::nullopt;
  }
  return std::cref(it->second);
}

inline std::expected<std::reference_wrapper<const std::string>, Response>
RequireRouteParam(const RouteParams& params, std::string_view key,
                  std::string_view missing_message = "Missing route parameter") {
  const auto value = FindRouteParam(params, key);
  if (!value.has_value()) {
    return std::unexpected(BadRoute(missing_message));
  }
  return *value;
}

template <typename Map>
inline auto FindMappedValue(Map& map, const typename Map::key_type& key)
    -> std::optional<std::reference_wrapper<typename Map::mapped_type>> {
  const auto it = map.find(key);
  if (it == map.end()) {
    return std::nullopt;
  }
  return std::ref(it->second);
}

inline bool ParseInsertIndex(const wpi::util::json& body, size_t max_size,
                             size_t& out_index, std::string& error) {
  out_index = max_size;
  if (!body.contains("insertIndex") || body.at("insertIndex").is_null()) {
    return true;
  }

  if (!body.at("insertIndex").is_number()) {
    error = "insertIndex must be a non-negative integer";
    return false;
  }

  const double value = body.at("insertIndex").get_number();
  if (value < 0.0) {
    error = "insertIndex must be a non-negative integer";
    return false;
  }

  const auto as_size = static_cast<size_t>(value);
  if (static_cast<double>(as_size) != value) {
    error = "insertIndex must be a non-negative integer";
    return false;
  }

  if (as_size > max_size) {
    error = "insertIndex out of range";
    return false;
  }

  out_index = as_size;
  return true;
}

inline bool ParseOrderArray(const wpi::util::json& body,
                            std::vector<std::string>& out_order,
                            std::string& error) {
  if (!body.is_object() || !body.contains("order") ||
      !body.at("order").is_array()) {
    error = "Request body must include array field 'order'";
    return false;
  }

  out_order.clear();
  for (const auto& entry : body.at("order").get_array()) {
    if (!entry.is_string()) {
      error = "order entries must be UUID strings";
      return false;
    }
    out_order.emplace_back(entry.get_string());
  }
  return true;
}

inline std::optional<std::reference_wrapper<const wpi::util::json>>
RequireObjectField(const wpi::util::json& body, std::string_view field) {
  if (!body.is_object() || !body.contains(std::string(field)) ||
      !body.at(field).is_object()) {
    return std::nullopt;
  }
  return std::cref(body.at(field));
}

inline std::optional<std::reference_wrapper<const std::string>>
RequireStringField(const wpi::util::json& body, std::string_view field) {
  if (!body.is_object() || !body.contains(std::string(field)) ||
      !body.at(field).is_string()) {
    return std::nullopt;
  }
  return std::cref(body.at(field).get_string());
}

}  // namespace choreo::state_server::detail
