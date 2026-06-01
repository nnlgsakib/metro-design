#ifndef _ofxMemory_h_
#define _ofxMemory_h_

#include "ofxCore.h"

#define kOfxMemorySuite          "OfxMemorySuiteV1"
#define kOfxMemorySuiteVersion   1

typedef struct OfxMemorySuiteV1 {
  void *(*memoryAlloc)(void *handle, size_t bytes);
  void  (*memoryFree)(void *handle, void *ptr);
} OfxMemorySuiteV1;

#endif
