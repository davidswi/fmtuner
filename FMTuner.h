// File: FMTuner.h -- interface definition for Javascript FMTuner object
// Author: David Switzer
// Project: FmTuner, WebKit-based FM tuner UI
// (c) 2012, David Switzer

#ifndef FMTUNER_H
#define FMTUNER_H

#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>

// Initialize and Finalize
void FMTuner_initCB(JSContextRef ctx, JSObjectRef object);
void FMTuner_finalizeCB(JSObjectRef object);

// Properties
bool FMTuner_hasPropCB(JSContextRef ctx, JSObjectRef object, JSStringRef propName);
JSValueRef FMTuner_getPropCB(JSContextRef ctx, JSObjectRef object, JSStringRef propName, JSValueRef *exception);
bool FMTuner_setPropCB(JSContextRef ctx, JSObjectRef object, JSStringRef propName, JSValueRef value, JSValueRef *exception);

// Methods
JSValueRef FMTuner_callAsFnCB(JSContextRef ctx, JSObjectRef thisObject, size_t argCount, const JSValueRef arguments[], JSValueRef *exception);

// Constructor
JSObjectRef FMTuner_callAsCtorCB(JSContextRef ctx, JSObjectRef thisObject, size_t argCount, const JSValueRef arguments[], JSValueRef *exception);

// Instance
bool FMTuner_hasInstCB(JSContextRef ctx, JSObjectRef ctor, JSValueRef possibleInst, JSValueRef *exception);

// Class registration with WebKit
int FMTuner_addClass(JSGlobalContextRef ctx);


#endif FMTUNER_H


