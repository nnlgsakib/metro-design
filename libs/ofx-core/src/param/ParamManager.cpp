// Copyright (c) 2026 Metro Design. All rights reserved.
#include "metro/ofx/param/ParamManager.hpp"
#include <cstring>

namespace metro::ofx::param {

const char *typeString(Type t)
{
    switch (t) {
        case Type::Integer:    return kOfxParamTypeInteger;
        case Type::Double:     return kOfxParamTypeDouble;
        case Type::Boolean:    return kOfxParamTypeBoolean;
        case Type::Choice:     return kOfxParamTypeChoice;
        case Type::RGBA:       return kOfxParamTypeRGBA;
        case Type::RGB:        return kOfxParamTypeRGB;
        case Type::Double2D:   return kOfxParamTypeDouble2D;
        case Type::Integer2D:  return kOfxParamTypeInteger2D;
        case Type::Double3D:   return kOfxParamTypeDouble3D;
        case Type::Integer3D:  return kOfxParamTypeInteger3D;
        case Type::Group:      return kOfxParamTypeGroup;
        case Type::Page:       return kOfxParamTypePage;
        case Type::PushButton: return kOfxParamTypePushButton;
        case Type::String:     return kOfxParamTypeString;
        case Type::Custom:     return kOfxParamTypeCustom;
    }
    return kOfxParamTypeDouble;
}

ParamManager::ParamManager(OfxImageEffectHandle effect)
    : effect_(effect)
{
}

OfxStatus ParamManager::defineParam(const ParamSpec &spec)
{
    if (!suite_) return kOfxStatErrBadHandle;

    const char *ofxType = typeString(spec.type);
    OfxPropertySetHandle paramProps;

    OfxStatus stat = suite_->paramDefine(paramSet_, ofxType, spec.name.c_str(), &paramProps);
    if (stat != kOfxStatOK) return stat;

    params_.push_back(spec.name);
    return kOfxStatOK;
}

OfxStatus ParamManager::defineParams(const std::vector<ParamSpec> &specs)
{
    for (const auto &spec : specs) {
        OfxStatus stat = defineParam(spec);
        if (stat != kOfxStatOK) return stat;
    }
    return kOfxStatOK;
}

} // namespace metro::ofx::param
