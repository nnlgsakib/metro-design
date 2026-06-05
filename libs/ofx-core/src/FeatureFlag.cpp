// Copyright (c) 2026 Metro Design. All rights reserved.
#include "metro/ofx/FeatureFlag.hpp"

#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <fstream>
#include <sstream>
#include <functional>
#include <mutex>
#include <array>
#include <utility>

namespace metro::ofx {

// ---------------------------------------------------------------------------
// FeatureFlag
// ---------------------------------------------------------------------------

void FeatureFlag::setRolloutPercentage(int pct) noexcept
{
    rolloutPct_ = (pct < 0) ? 0 : (pct > 100) ? 100 : pct;
}

bool FeatureFlag::checkRollout(int rolloutPct, std::string_view flagName)
{
    if (rolloutPct >= 100) return true;
    if (rolloutPct <= 0) return false;

    std::hash<std::string_view> hasher;
    size_t h = hasher(flagName);
    return (h % 100) < static_cast<size_t>(rolloutPct);
}

// ---------------------------------------------------------------------------
// FeatureFlagManager
// ---------------------------------------------------------------------------

FeatureFlagManager& FeatureFlagManager::instance()
{
    static FeatureFlagManager mgr;
    return mgr;
}

FeatureFlag& FeatureFlagManager::define(std::string_view name, bool defaultValue, int rolloutPct)
{
    return defineImpl(std::string(name), defaultValue, rolloutPct);
}

FeatureFlag& FeatureFlagManager::defineImpl(std::string name, bool defaultValue, int rolloutPct)
{
    auto it = flags_.find(name);
    if (it != flags_.end()) return it->second.flag;

    if (rolloutPct < 0) rolloutPct = 0;
    if (rolloutPct > 100) rolloutPct = 100;

    FlagEntry entry;
    entry.defaultValue = defaultValue;
    entry.defaultRolloutPct = rolloutPct;
    entry.flag.setEnabled(defaultValue);
    entry.flag.setRolloutPercentage(rolloutPct);

    auto result = flags_.emplace(std::move(name), std::move(entry));
    return result.first->second.flag;
}

bool FeatureFlagManager::isEnabled(std::string_view name) const noexcept
{
    std::shared_lock lock(mutex_);
    auto it = flags_.find(std::string(name));
    if (it == flags_.end()) return false;
    const auto& entry = it->second;
    if (!entry.flag.enabled()) return false;
    return FeatureFlag::checkRollout(entry.flag.rolloutPercentage(), name);
}

void FeatureFlagManager::setEnabled(std::string_view name, bool enabled)
{
    std::unique_lock lock(mutex_);
    auto it = flags_.find(std::string(name));
    if (it != flags_.end()) {
        it->second.flag.setEnabled(enabled);
    }
}

void FeatureFlagManager::setRemoteOverride(std::unique_ptr<RemoteOverrideProvider> provider)
{
    std::unique_lock lock(mutex_);
    remoteProvider_ = std::move(provider);
}

void FeatureFlagManager::resetAll()
{
    std::unique_lock lock(mutex_);
    for (auto& [name, entry] : flags_) {
        entry.flag.setEnabled(entry.defaultValue);
        entry.flag.setRolloutPercentage(entry.defaultRolloutPct);
    }
}

std::string FeatureFlagManager::envVarName(std::string_view name)
{
    std::string result = "METRO_FF_";
    for (char c : name) {
        result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    return result;
}

void FeatureFlagManager::refresh()
{
    std::unique_lock lock(mutex_);

    for (auto& [name, entry] : flags_) {
        entry.flag.setEnabled(entry.defaultValue);
        entry.flag.setRolloutPercentage(entry.defaultRolloutPct);
    }

    for (auto& [name, entry] : flags_) {
        std::string envName = envVarName(name);
        const char* envVal = std::getenv(envName.c_str());
        if (envVal) {
            std::string val(envVal);
            for (auto& c : val) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (val == "1" || val == "true" || val == "yes" || val == "on") {
                entry.flag.setEnabled(true);
            } else if (val == "0" || val == "false" || val == "no" || val == "off") {
                entry.flag.setEnabled(false);
            }
        }
    }

    if (remoteProvider_ && remoteProvider_->connected()) {
        try {
            auto overrides = remoteProvider_->fetchOverrides();
            for (const auto& [name, enabled] : overrides) {
                auto it = flags_.find(name);
                if (it != flags_.end()) {
                    it->second.flag.setEnabled(enabled);
                }
            }
        } catch (...) {
            std::fprintf(stderr, "[FeatureFlags] Remote override fetch failed (degraded)\n");
        }
    }
}

// ---------------------------------------------------------------------------
// Minimal TOML parser for [flags] section
// ---------------------------------------------------------------------------

static std::string trim(std::string_view s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return std::string(s);
}

static bool parseBool(std::string_view s)
{
    std::string lower;
    lower.reserve(s.size());
    for (char c : s) lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return lower == "true" || lower == "yes" || lower == "on" || lower == "1";
}

bool FeatureFlagManager::loadConfigFile(std::string_view path)
{
    std::ifstream file(std::string(path).c_str());
    if (!file.is_open()) {
        std::fprintf(stderr, "[FeatureFlags] Could not open config: %s\n", std::string(path).c_str());
        return false;
    }

    bool inFlagsSection = false;
    std::string line;

    while (std::getline(file, line)) {
        std::string_view sv(line);

        auto commentPos = sv.find('#');
        if (commentPos != std::string_view::npos) sv = sv.substr(0, commentPos);

        sv = trim(sv);
        if (sv.empty()) continue;

        if (sv.front() == '[' && sv.back() == ']') {
            std::string section = trim(sv.substr(1, sv.size() - 2));
            inFlagsSection = (section == "flags");
            continue;
        }

        if (!inFlagsSection) continue;

        auto eqPos = sv.find('=');
        if (eqPos == std::string_view::npos) continue;

        std::string key = trim(sv.substr(0, eqPos));
        std::string val = trim(sv.substr(eqPos + 1));

        if (key.empty()) continue;

        bool boolVal = parseBool(val);

        std::unique_lock lock(mutex_);
        auto it = flags_.find(key);
        if (it != flags_.end()) {
            it->second.flag.setEnabled(boolVal);
        }
    }

    return true;
}

} // namespace metro::ofx
