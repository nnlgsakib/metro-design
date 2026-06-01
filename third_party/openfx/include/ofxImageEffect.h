#ifndef _ofxImageEffect_h_
#define _ofxImageEffect_h_

#include "ofxCore.h"

#define kOfxImageEffectSuite     "OfxImageEffectSuiteV1"
#define kOfxImageEffectSuiteVersion 1

#define kOfxImageEffectPluginApi  "OfxImageEffectPluginAPI"
#define kOfxImageEffectPluginApiVersion 1

#define kOfxActionLoad            "OfxActionLoad"
#define kOfxActionUnload          "OfxActionUnload"
#define kOfxActionDescribe        "OfxActionDescribe"
#define kOfxActionCreateInstance  "OfxActionCreateInstance"
#define kOfxActionDestroyInstance "OfxActionDestroyInstance"
#define kOfxActionBeginInstanceChanged   "OfxActionBeginInstanceChanged"
#define kOfxActionEndInstanceChanged     "OfxActionEndInstanceChanged"
#define kOfxActionInstanceChanged        "OfxActionInstanceChanged"
#define kOfxActionPurgeCaches     "OfxActionPurgeCaches"
#define kOfxActionSyncPrivateData "OfxActionSyncPrivateData"
#define kOfxActionBeginRender     "OfxActionBeginRender"
#define kOfxActionEndRender       "OfxActionEndRender"
#define kOfxActionRender          "OfxActionRender"
#define kOfxActionIsIdentity      "OfxActionIsIdentity"

#define kOfxImageEffectPropType                  "OfxImageEffectPropType"
#define kOfxImageEffectPropName                  "OfxImageEffectPropName"
#define kOfxImageEffectPropLabel                 "OfxImageEffectPropLabel"
#define kOfxImageEffectPropShortLabel            "OfxImageEffectPropShortLabel"
#define kOfxImageEffectPropLongLabel             "OfxImageEffectPropLongLabel"
#define kOfxImageEffectPropGrouping              "OfxImageEffectPropGrouping"
#define kOfxImageEffectPropDescription           "OfxImageEffectPropDescription"
#define kOfxImageEffectPropSupportedContexts     "OfxImageEffectPropSupportedContexts"
#define kOfxImageEffectPropSupportedPixelDepths  "OfxImageEffectPropSupportedPixelDepths"
#define kOfxImageEffectPropPluginHandle          "OfxImageEffectPropPluginHandle"
#define kOfxImageEffectPropHostFrameRate         "OfxImageEffectPropHostFrameRate"
#define kOfxImageEffectPropHostPixelScale        "OfxImageEffectPropHostPixelScale"
#define kOfxImageEffectPropHostBackgroundColour  "OfxImageEffectPropHostBackgroundColour"
#define kOfxImageEffectPropHostMips              "OfxImageEffectPropHostMips"

#define kOfxImageEffectContextGenerator       "OfxImageEffectContextGenerator"
#define kOfxImageEffectContextFilter          "OfxImageEffectContextFilter"
#define kOfxImageEffectContextTransition      "OfxImageEffectContextTransition"
#define kOfxImageEffectContextPaint           "OfxImageEffectContextPaint"
#define kOfxImageEffectContextGeneral         "OfxImageEffectContextGeneral"
#define kOfxImageEffectContextRetimer         "OfxImageEffectContextRetimer"
#define kOfxImageEffectContextReader          "OfxImageEffectContextReader"
#define kOfxImageEffectContextWriter          "OfxImageEffectContextWriter"
#define kOfxImageEffectContextColorDecision   "OfxImageEffectContextColorDecision"

#define kOfxImageEffectPropClipPreferences   "OfxImageEffectPropClipPreferences"
#define kOfxImageEffectPropInAnalysis         "OfxImageEffectPropInAnalysis"

#define kOfxImageEffectActionDescribeInPlace "OfxImageEffectActionDescribeInPlace"
#define kOfxImageEffectActionGetClipPreferences "OfxImageEffectActionGetClipPreferences"
#define kOfxImageEffectActionGetRegionOfDefinition "OfxImageEffectActionGetRegionOfDefinition"
#define kOfxImageEffectActionGetRegionsOfInterest "OfxImageEffectActionGetRegionsOfInterest"

typedef struct OfxImageEffectSuiteV1 {
  OfxStatus (*getPropertySet)(OfxImageEffectHandle imageEffect, OfxPropertySetHandle *propHandle);
  OfxStatus (*getParamSet)(OfxImageEffectHandle imageEffect, OfxParamSetHandle *paramSet);
  OfxStatus (*clipDefine)(OfxImageEffectHandle imageEffect, const char *clipName, OfxPropertySetHandle *propertySet);
  OfxStatus (*clipGetPropertySet)(OfxImageEffectHandle imageEffect, const char *clipName, OfxPropertySetHandle *propHandle);
  OfxStatus (*clipGetHandle)(OfxImageEffectHandle imageEffect, const char *clipName, OfxImageEffectHandle *clipHandle, OfxPropertySetHandle *propHandle);
  OfxStatus (*imageClipGetImage)(OfxImageEffectHandle clipHandle, double time, const char *format, OfxPropertySetHandle *imageHandle, OfxPropertySetHandle *dataHandle);
  OfxStatus (*imageClipReleaseImage)(OfxPropertySetHandle dataHandle);
  OfxStatus (*clipGetRegionOfDefinition)(OfxImageEffectHandle clipHandle, double time, OfxPropertySetHandle *props);
} OfxImageEffectSuiteV1;

#endif
