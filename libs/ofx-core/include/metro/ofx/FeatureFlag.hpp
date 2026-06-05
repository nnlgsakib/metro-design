// Copyright (c) 2026 Metro Design. All rights reserved.
#ifndef METRO_OFX_FEATUREFLAG_HPP
#define METRO_OFX_FEATUREFLAG_HPP

#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <shared_mutex>

namespace metro::ofx {

class RemoteOverrideProvider;

class FeatureFlag {
public:
    constexpr FeatureFlag() noexcept = default;

    FeatureFlag(bool enabled, int rolloutPct) noexcept
        : enabled_(enabled), rolloutPct_(rolloutPct) {}

    bool enabled() const noexcept { return enabled_; }
    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }

    int rolloutPercentage() const noexcept { return rolloutPct_; }
    void setRolloutPercentage(int pct) noexcept;

    explicit constexpr operator bool() const noexcept { return enabled_; }

    static bool checkRollout(int rolloutPct, std::string_view flagName);

private:
    bool enabled_{false};
    int rolloutPct_{100};
};

class RemoteOverrideProvider {
public:
    virtual ~RemoteOverrideProvider() = default;
    virtual std::unordered_map<std::string, bool> fetchOverrides() = 0;
    virtual bool connected() const noexcept = 0;
};

class FeatureFlagManager {
public:
    static FeatureFlagManager& instance();

    FeatureFlag& define(std::string_view name, bool defaultValue = false, int rolloutPct = 100);

    bool isEnabled(std::string_view name) const noexcept;

    void setEnabled(std::string_view name, bool enabled);

    void setRemoteOverride(std::unique_ptr<RemoteOverrideProvider> provider);

    bool loadConfigFile(std::string_view path);

    void refresh();

    void resetAll();

private:
    FeatureFlagManager() = default;

    struct FlagEntry {
        FeatureFlag flag;
        bool defaultValue{false};
        int defaultRolloutPct{100};
    };

    FeatureFlag& defineImpl(std::string name, bool defaultValue, int rolloutPct);

    static std::string envVarName(std::string_view name);

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, FlagEntry> flags_;
    std::unique_ptr<RemoteOverrideProvider> remoteProvider_;
    std::string rolloutSeed_;
};

} // namespace metro::ofx

#endif
