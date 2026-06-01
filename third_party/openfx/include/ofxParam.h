#ifndef _ofxParam_h_
#define _ofxParam_h_

#include "ofxCore.h"

#define kOfxParamSuite           "OfxParamSuiteV1"
#define kOfxParamSuiteVersion    1

#define kOfxParamTypeInteger        "OfxParamTypeInteger"
#define kOfxParamTypeDouble         "OfxParamTypeDouble"
#define kOfxParamTypeBoolean        "OfxParamTypeBoolean"
#define kOfxParamTypeChoice         "OfxParamTypeChoice"
#define kOfxParamTypeRGBA           "OfxParamTypeRGBA"
#define kOfxParamTypeRGB            "OfxParamTypeRGB"
#define kOfxParamTypeDouble2D       "OfxParamTypeDouble2D"
#define kOfxParamTypeInteger2D      "OfxParamTypeInteger2D"
#define kOfxParamTypeDouble3D       "OfxParamTypeDouble3D"
#define kOfxParamTypeInteger3D      "OfxParamTypeInteger3D"
#define kOfxParamTypeGroup          "OfxParamTypeGroup"
#define kOfxParamTypePage           "OfxParamTypePage"
#define kOfxParamTypePushButton     "OfxParamTypePushButton"
#define kOfxParamTypeString         "OfxParamTypeString"
#define kOfxParamTypeCustom         "OfxParamTypeCustom"

#define kOfxParamPropType           "OfxParamPropType"
#define kOfxParamPropName           "OfxParamPropName"
#define kOfxParamPropLabel          "OfxParamPropLabel"
#define kOfxParamPropScriptName     "OfxParamPropScriptName"
#define kOfxParamPropHint           "OfxParamPropHint"
#define kOfxParamPropDoubleMin      "OfxParamPropDoubleMin"
#define kOfxParamPropDoubleMax      "OfxParamPropDoubleMax"
#define kOfxParamPropDoubleDefault  "OfxParamPropDoubleDefault"
#define kOfxParamPropIntMin         "OfxParamPropIntMin"
#define kOfxParamPropIntMax         "OfxParamPropIntMax"
#define kOfxParamPropIntDefault     "OfxParamPropIntDefault"
#define kOfxParamPropDisplayMin     "OfxParamPropDisplayMin"
#define kOfxParamPropDisplayMax     "OfxParamPropDisplayMax"
#define kOfxParamPropIncrement      "OfxParamPropIncrement"
#define kOfxParamPropDigits         "OfxParamPropDigits"
#define kOfxParamPropDoubleType     "OfxParamPropDoubleType"
#define kOfxParamPropChoiceOption   "OfxParamPropChoiceOption"
#define kOfxParamPropChoiceLabelOption "OfxParamPropChoiceLabelOption"
#define kOfxParamPropGroupOpen      "OfxParamPropGroupOpen"
#define kOfxParamPropPagePosition   "OfxParamPropPagePosition"
#define kOfxParamPropEvaluateOnChange "OfxParamPropEvaluateOnChange"
#define kOfxParamPropCacheInvalidate "OfxParamPropCacheInvalidate"

#define kOfxParamDoubleTypeNone     "OfxParamDoubleTypeNone"
#define kOfxParamDoubleTypeAngle    "OfxParamDoubleTypeAngle"
#define kOfxParamDoubleTypeScale    "OfxParamDoubleTypeScale"
#define kOfxParamDoubleTypeAbsolute "OfxParamDoubleTypeAbsolute"
#define kOfxParamDoubleTypeNormalisedX "OfxParamDoubleTypeNormalisedX"
#define kOfxParamDoubleTypeNormalisedY "OfxParamDoubleTypeNormalisedY"
#define kOfxParamDoubleTypeNormalisedXY "OfxParamDoubleTypeNormalisedXY"
#define kOfxParamDoubleTypeLatitude  "OfxParamDoubleTypeLatitude"
#define kOfxParamDoubleTypeLongitude "OfxParamDoubleTypeLongitude"

#define kOfxParamStringTypeSingleLine "OfxParamStringTypeSingleLine"
#define kOfxParamStringTypeMultiLine  "OfxParamStringTypeMultiLine"
#define kOfxParamStringTypeFilePath   "OfxParamStringTypeFilePath"
#define kOfxParamStringTypeDirectory  "OfxParamStringTypeDirectory"
#define kOfxParamStringTypeLabel      "OfxParamStringTypeLabel"

typedef struct OfxParamSuiteV1 {
  OfxStatus (*paramDefine)(OfxParamSetHandle paramSet, const char *paramType, const char *name, OfxPropertySetHandle *propertySet);
  OfxStatus (*paramGetHandle)(OfxParamSetHandle paramSet, const char *name, OfxParamSetHandle *param, OfxPropertySetHandle *propertySet);
  OfxStatus (*paramSetGetPropertySet)(OfxParamSetHandle paramSet, OfxPropertySetHandle *propHandle);
  OfxStatus (*paramGetPropertySet)(OfxParamSetHandle param, OfxPropertySetHandle *propHandle);
  OfxStatus (*paramGetValue)(OfxParamSetHandle param, int index, ...);
  OfxStatus (*paramGetValueAtTime)(OfxParamSetHandle param, double time, int index, ...);
  OfxStatus (*paramSetValue)(OfxParamSetHandle param, int index, ...);
} OfxParamSuiteV1;

#endif
