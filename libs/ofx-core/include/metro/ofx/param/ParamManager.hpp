// Copyright (c) 2026 Metro Design. All rights reserved.
#ifndef METRO_OFX_PARAM_PARAMMANAGER_HPP
#define METRO_OFX_PARAM_PARAMMANAGER_HPP

#include <string>
#include <vector>
#include <functional>
#include <ofxCore.h>
#include <ofxParam.h>
#include <ofxImageEffect.h>

namespace metro::ofx::param {

enum class Type {
    Integer,
    Double,
    Boolean,
    Choice,
    RGBA,
    RGB,
    Double2D,
    Integer2D,
    Double3D,
    Integer3D,
    Group,
    Page,
    PushButton,
    String,
    Custom
};

const char *typeString(Type t);

struct ParamSpec {
    Type        type{Type::Double};
    std::string name;
    std::string label;
    std::string scriptName;
    std::string hint;
    std::string groupName;

    double doubleMin{0.0}, doubleMax{1.0}, doubleDefault{0.5};
    int    intMin{0}, intMax{100}, intDefault{0};
    bool   booleanDefault{false};
    std::vector<std::string> choiceOptions;
    std::vector<std::string> choiceLabels;

    std::string stringDefault;
    std::string stringType;

    bool evaluateOnChange{true};
    bool canInvalidate{false};
};

class ParamManager {
public:
    explicit ParamManager(OfxImageEffectHandle effect);
    ~ParamManager() = default;

    OfxStatus defineParam(const ParamSpec &spec);
    OfxStatus defineParams(const std::vector<ParamSpec> &specs);

    template<typename T>
    OfxStatus getValue(const std::string &name, T &out) const;
    template<typename T>
    OfxStatus getValueAtTime(const std::string &name, double time, T &out) const;
    template<typename T>
    OfxStatus setValue(const std::string &name, const T &value);

    OfxParamSetHandle paramSet() const { return paramSet_; }
    OfxImageEffectHandle effect() const { return effect_; }
    int paramCount() const { return static_cast<int>(params_.size()); }

private:
    OfxImageEffectHandle effect_{nullptr};
    OfxParamSetHandle paramSet_{nullptr};
    const OfxParamSuiteV1 *suite_{nullptr};
    std::vector<std::string> params_;
};

} // namespace metro::ofx::param

#endif
