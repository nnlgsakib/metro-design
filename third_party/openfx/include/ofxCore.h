#ifndef _ofxCore_h_
#define _ofxCore_h_

#include <stdint.h>

#define kOfxStatOK               0
#define kOfxStatFailed          (-1)
#define kOfxStatErrFatal        (-2)
#define kOfxStatErrUnknown      (-3)
#define kOfxStatErrMissing      (-4)
#define kOfxStatErrUnsupported  (-5)
#define kOfxStatErrExists       (-6)
#define kOfxStatErrFormat       (-7)
#define kOfxStatErrMemory       (-8)
#define kOfxStatErrBadHandle    (-9)
#define kOfxStatErrBadIndex     (-10)
#define kOfxStatErrValue        (-11)
#define kOfxStatReplyYes        1
#define kOfxStatReplyNo         0
#define kOfxStatReplyDefault    2

typedef int OfxStatus;

typedef struct OfxPropertySetStruct  *OfxPropertySetHandle;
typedef struct OfxImageEffectStruct  *OfxImageEffectHandle;
typedef struct OfxParamSetStruct     *OfxParamSetHandle;
typedef struct OfxHostStruct         *OfxHostHandle;

typedef struct OfxPointI {
  int x, y;
} OfxPointI;

typedef struct OfxDoublePoint {
  double x, y;
} OfxDoublePoint;

typedef struct OfxRangeI {
  int min, max;
} OfxRangeI;

typedef struct OfxDoubleRange {
  double min, max;
} OfxDoubleRange;

typedef struct OfxRGBAColourB {
  unsigned char r, g, b, a;
} OfxRGBAColourB;

typedef struct OfxRGBAColourF {
  float r, g, b, a;
} OfxRGBAColourF;

typedef struct OfxRGBAColourD {
  double r, g, b, a;
} OfxRGBAColourD;

typedef struct OfxRGBColourB {
  unsigned char r, g, b;
} OfxRGBColourB;

typedef struct OfxRGBColourF {
  float r, g, b;
} OfxRGBColourF;

typedef struct OfxRGBColourD {
  double r, g, b;
} OfxRGBColourD;

typedef struct OfxTime {
  double time;
} OfxTime;

typedef struct OfxKeyTime {
  double time;
} OfxKeyTime;

typedef struct OfxHost {
  OfxPropertySetHandle host;
  void               (*setHost)(struct OfxHost *host);
} OfxHost;

typedef OfxStatus (*OfxSetHostProc)(struct OfxHost *host);
typedef OfxStatus (*OfxMainEntryProc)(const char *action, void *handle, void *inArgs, void *outArgs);

typedef struct OfxPlugin {
  OfxPlugin        *next;
  const char       *identifier;
  const char       *pluginVersion;
  const char       *apiVersion;
  int               apiMinor;
  OfxSetHostProc    setHost;
  OfxMainEntryProc  mainEntry;
} OfxPlugin;

typedef struct OfxPluginSuiteV1 {
  int               (*getNumberOfPlugins)(void);
  OfxPlugin *       (*getPlugin)(int index);
} OfxPluginSuiteV1;

#define kOfxPluginSuite          "OfxPluginSuite"
#define kOfxPluginSuiteVersion   1

#endif
