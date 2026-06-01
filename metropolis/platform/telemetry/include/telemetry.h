#ifndef MET_TELEMETRY_H_
#define MET_TELEMETRY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace met {

enum class LogLevel { kDebug, kInfo, kWarning, kError, kFatal };

struct MetricSample {
  std::string name;
  double value;
  std::unordered_map<std::string, std::string> labels;
};

class TelemetryClient {
 public:
  TelemetryClient();
  ~TelemetryClient();

  TelemetryClient(const TelemetryClient &) = delete;
  TelemetryClient &operator=(const TelemetryClient &) = delete;
  TelemetryClient(TelemetryClient &&) = delete;
  TelemetryClient &operator=(TelemetryClient &&) = delete;

  static TelemetryClient &Instance();

  void Init(std::string_view dsn, std::string_view version);
  void Shutdown();

  void Log(LogLevel level, std::string_view message);
  void LogInfo(std::string_view message);
  void LogWarn(std::string_view message);
  void LogError(std::string_view message);

  void IncCounter(std::string_view name, double value = 1.0,
                  std::unordered_map<std::string, std::string> labels = {});
  void ObserveHistogram(std::string_view name, double value,
                        std::unordered_map<std::string, std::string> labels = {});

  void CaptureException(std::string_view message);
  void SetUser(std::string_view user_id, std::string_view email = "");

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace met

#endif  // MET_TELEMETRY_H_
