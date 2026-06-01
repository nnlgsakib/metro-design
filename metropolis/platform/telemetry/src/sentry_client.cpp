#include "telemetry.h"
#include <cstdio>

namespace met {

// Sentry integration is conditionally compiled when the Sentry native SDK
// is available.  In the stub path we log to stderr.
//
// To enable:
//   find_package(sentry-native REQUIRED)
//   target_link_libraries(met_platform PUBLIC sentry-native)
//   add_definitions(-DMET_HAS_SENTRY)
//
// The sentry-native SDK is at https://github.com/getsentry/sentry-native.

void AttachSentryNative(const char *dsn, const char *version) {
  std::fprintf(stderr, "[SENTRY] Would init with DSN=%s version=%s\n", dsn, version);
}

void SentryCapture(const char *message) {
  std::fprintf(stderr, "[SENTRY] Would capture: %s\n", message);
}

void SentryShutdown() {
  std::fprintf(stderr, "[SENTRY] Would shutdown\n");
}

}  // namespace met
