#include "telemetry.h"
#include <cstdio>

namespace met {

// Prometheus metrics exposition endpoint.
//
// When the Prometheus client library (prometheus-cpp) is linked, this
// registers counters and histograms with the global registry and exposes
// them via HTTP on /metrics.
//
// To enable:
//   find_package(prometheus-cpp REQUIRED)
//   target_link_libraries(met_platform PUBLIC prometheus-cpp-pull)
//   add_definitions(-DMET_HAS_PROMETHEUS)

void ExposeMetricsStub(const char *listen_addr) {
  std::fprintf(stderr, "[METRICS] Would expose on %s\n", listen_addr);
}

void EmitCounterStub(const char *name, double value) {
  std::fprintf(stderr, "[METRICS] counter %s += %f\n", name, value);
}

void EmitHistogramStub(const char *name, double value) {
  std::fprintf(stderr, "[METRICS] histogram %s = %f\n", name, value);
}

}  // namespace met
