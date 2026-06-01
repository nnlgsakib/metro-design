add_executable(telemetry_test
    telemetry_test.cpp
)

target_link_libraries(telemetry_test PRIVATE
    platform
)

add_test(NAME TelemetryTest COMMAND telemetry_test)
