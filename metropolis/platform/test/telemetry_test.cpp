#include "telemetry/include/telemetry.h"
#include <cassert>
#include <cstdio>

int main() {
  auto &tel = met::TelemetryClient::Instance();

  tel.Init("https://dummy@sentry.io/1", "1.0.0");

  tel.LogInfo("test info message");
  tel.LogWarn("test warning");
  tel.LogError("test error");

  tel.IncCounter("plugin.load", 1.0);
  tel.IncCounter("plugin.load", 1.0);
  tel.IncCounter("effect.apply", 1.0);

  tel.ObserveHistogram("render.time_ms", 42.0);
  tel.CaptureException("test exception");

  tel.SetUser("test-user-1");

  tel.Shutdown();

  std::fprintf(stderr, "[PASS] TelemetryTest: all operations completed\n");
  return 0;
}
