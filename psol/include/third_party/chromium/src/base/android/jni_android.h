// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_JNI_ANDROID_H_
#define BASE_ANDROID_JNI_ANDROID_H_

#include <jni.h>
#include <sys/types.h>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"

namespace base {
namespace android {

// Used to mark symbols to be exported in a shared library's symbol table.
#define JNI_EXPORT __attribute__ ((visibility("default")))

// Contains the registration method information for initializing JNI bindings.
struct RegistrationMethod {
  const char* name;
  bool (*func)(JNIEnv* env);
};

// Attach the current thread to the VM (if necessary) and return the JNIEnv*.
JNIEnv* AttachCurrentThread();

// Detach the current thread from VM if it is attached.
void DetachFromVM();

// Initializes the global JVM. It is not necessarily called before
// InitApplicationContext().
void InitVM(JavaVM* vm);

// Initializes the global application context object. The |context| can be any
// valid reference to the application context. Internally holds a global ref to
// the context. InitVM and InitApplicationContext maybe called in either order.
void InitApplicationContext(const JavaRef<jobject>& context);

// Gets a global ref to the application context set with
// InitApplicationContext(). Ownership is retained by the function - the caller
// must NOT release it.
const jobject GetApplicationContext();

// Finds the class named |class_name| and returns it.
// Use this method instead of invoking directly the JNI FindClass method (to
// prevent leaking local references).
// This method triggers a fatal assertion if the class could not be found.
// Use HasClass if you need to check whether the class exists.
ScopedJavaLocalRef<jclass> GetClass(JNIEnv* env, const char* class_name);

// Similar to the above, but the caller is responsible to manage the jclass
// lifetime.
jclass GetUnscopedClass(JNIEnv* env, const char* class_name) WARN_UNUSED_RESULT;

// Returns true iff the class |class_name| could be found.
bool HasClass(JNIEnv* env, const char* class_name);

// Returns the method ID for the method with the specified name and signature.
// This method triggers a fatal assertion if the method could not be found.
// Use HasMethod if you need to check whether a method exists.
jmethodID GetMethodID(JNIEnv* env,
                      const JavaRef<jclass>& clazz,
                      const char* method_name,
                      const char* jni_signature);

// Similar to GetMethodID, but takes a raw jclass.
jmethodID GetMethodID(JNIEnv* env,
                      jclass clazz,
                      const char* method_name,
                      const char* jni_signature);

// Unlike GetMethodID, returns NULL if the method could not be found.
jmethodID GetMethodIDOrNull(JNIEnv* env,
                            jclass clazz,
                            const char* method_name,
                            const char* jni_signature);

// Returns the method ID for the static method with the specified name and
// signature.
// This method triggers a fatal assertion if the method could not be found.
// Use HasMethod if you need to check whether a method exists.
jmethodID GetStaticMethodID(JNIEnv* env,
                            const JavaRef<jclass>& clazz,
                            const char* method_name,
                            const char* jni_signature);

// Similar to the GetStaticMethodID, but takes a raw jclass.
jmethodID GetStaticMethodID(JNIEnv* env,
                            jclass clazz,
                            const char* method_name,
                            const char* jni_signature);

// Unlike GetStaticMethodID, returns NULL if the method could not be found.
jmethodID GetStaticMethodIDOrNull(JNIEnv* env,
                                  jclass clazz,
                                  const char* method_name,
                                  const char* jni_signature);

// Returns true iff |clazz| has a method with the specified name and signature.
bool HasMethod(JNIEnv* env,
               const JavaRef<jclass>& clazz,
               const char* method_name,
               const char* jni_signature);

// Gets the method ID from the class name. Clears the pending Java exception
// and returns NULL if the method is not found. Caches results. Note that
// GetMethodID() below avoids a class lookup, but does not cache results.
// Strings passed to this function are held in the cache and MUST remain valid
// beyond the duration of all future calls to this function, across all
// threads. In practice, this means that the function should only be used with
// string constants.
jmethodID GetMethodIDFromClassName(JNIEnv* env,
                                   const char* class_name,
                                   const char* method,
                                   const char* jni_signature);

// Gets the field ID for a class field.
// This method triggers a fatal assertion if the field could not be found.
jfieldID GetFieldID(JNIEnv* env,
                    const JavaRef<jclass>& clazz,
                    const char* field_name,
                    const char* jni_signature);

// Returns true if |clazz| as a field with the given name and signature.
// TODO(jcivelli): Determine whether we explicitly have to pass the environment.
bool HasField(JNIEnv* env,
              const JavaRef<jclass>& clazz,
              const char* field_name,
              const char* jni_signature);

// Gets the field ID for a static class field.
// This method triggers a fatal assertion if the field could not be found.
jfieldID GetStaticFieldID(JNIEnv* env,
                          const JavaRef<jclass>& clazz,
                          const char* field_name,
                          const char* jni_signature);

// Returns true if an exception is pending in the provided JNIEnv*.
bool HasException(JNIEnv* env);

// If an exception is pending in the provided JNIEnv*, this function clears it
// and returns true.
bool ClearException(JNIEnv* env);

// This function will call CHECK() macro if there's any pending exception.
void CheckException(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_JNI_ANDROID_H_
