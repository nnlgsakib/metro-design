#include "telemetry.h"
#include <cstdio>
#include <ctime>
#include <mutex>

namespace met {

struct TelemetryClient::Impl {
  std::string dsn;
  std::string version;
  bool initialized = false;
  std::mutex mtx;

  // In-memory counters for Prometheus-style exposition
  std::unordered_map<std::string, MetricSample> counters;
  std::unordered_map<std::string, MetricSample> histograms;

  void FlushLog(LogLevel level, std::string_view msg) {
    const char *level_str = "DEBUG";
    switch (level) {
      case LogLevel::kDebug:
        level_str = "DEBUG";
        break;
      case LogLevel::kInfo:
        level_str = "INFO";
        break;
      case LogLevel::kWarning:
        level_str = "WARN";
        break;
      case LogLevel::kError:
        level_str = "ERROR";
        break;
      case LogLevel::kFatal:
        level_str = "FATAL";
        break;
    }
    std::time_t now = std::time(nullptr);
    std::fprintf(stderr, "[%s] %s: %.*s\n", level_str, std::ctime(&now), static_cast<int>(msg.size()), msg.data());
  }
};

TelemetryClient::TelemetryClient() : impl_(std::make_unique<Impl>()) {}
TelemetryClient::~TelemetryClient() { Shutdown(); }

TelemetryClient &TelemetryClient::Instance() {
  static TelemetryClient instance;
  return instance;
}

void TelemetryClient::Init(std::string_view dsn, std::string_view version) {
  std::lock_guard<std::mutex> lock(impl_->mtx);
  impl_->dsn = dsn;
  impl_->version = version;
  impl_->initialized = true;
  impl_->FlushLog(LogLevel::kInfo, "Telemetry initialized");
}

void TelemetryClient::Shutdown() {
  std::lock_guard<std::mutex> lock(impl_->mtx);
  if (impl_->initialized) {
    impl_->FlushLog(LogLevel::kInfo, "Telemetry shutting down");
    impl_->initialized = false;
  }
}

void TelemetryClient::Log(LogLevel level, std::string_view message) {
  std::lock_guard<std::mutex> lock(impl_->mtx);
  impl_->FlushLog(level, message);
}

void TelemetryClient::LogInfo(std::string_view message) { Log(LogLevel::kInfo, message); }
void TelemetryClient::LogWarn(std::string_view message) { Log(LogLevel::kWarning, message); }
void TelemetryClient::LogError(std::string_view message) { Log(LogLevel::kError, message); }

void TelemetryClient::IncCounter(std::string_view name, double value,
                                 std::unordered_map<std::string, std::string> labels) {
  std::lock_guard<std::mutex> lock(impl_->mtx);
  std::string key(name);
  auto it = impl_->counters.find(key);
  if (it != impl_->counters.end()) {
    it->second.value += value;
  } else {
    impl_->counters[key] = MetricSample{std::string(name), value, std::move(labels)};
  }
}

void TelemetryClient::ObserveHistogram(std::string_view name, double value,
                                       std::unordered_map<std::string, std::string> labels) {
  std::lock_guard<std::mutex> lock(impl_->mtx);
  std::string key(name);
  impl_->histograms[key] = MetricSample{std::string(name), value, std::move(labels)};
}

void TelemetryClient::CaptureException(std::string_view message) {
  Log(LogLevel::kError, message);
}

void TelemetryClient::SetUser(std::string_view user_id, std::string_view email) {
  LogInfo(std::string("User: ") + std::string(user_id));
}

}  // namespace met
