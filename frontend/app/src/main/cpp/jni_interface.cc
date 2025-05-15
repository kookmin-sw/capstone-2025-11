/*
 * Copyright 2017 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>

#include "hello_ar_application.h"
#include "java_bridge.h"

#include <android/log.h>


#define JNI_METHOD(return_type, method_name) \
  JNIEXPORT return_type JNICALL              \
  Java_com_capstone_whereigo_JniInterface_##method_name

extern "C" {

namespace {
// maintain a reference to the JVM so we can use it later.
static JavaVM *g_vm = nullptr;

static jobject g_class_loader = nullptr;
static jmethodID g_load_class_method = nullptr;

static jobject g_mappingFragment = nullptr;
static jmethodID g_mapping_method = nullptr;

pair<float, float> data;

inline jlong jptr(hello_ar::HelloArApplication *native_hello_ar_application) {
  return reinterpret_cast<intptr_t>(native_hello_ar_application);
}

inline hello_ar::HelloArApplication *native(jlong ptr) {
  return reinterpret_cast<hello_ar::HelloArApplication *>(ptr);
}

}  // namespace

jint JNI_OnLoad(JavaVM *vm, void *) {
  g_vm = vm;
  JavaBridge::SetJavaVM(vm);
  return JNI_VERSION_1_6;
}

JNI_METHOD(jlong, createNativeApplication)
(JNIEnv *env, jclass, jobject j_asset_manager, jstring j_path) {
  AAssetManager *asset_manager = AAssetManager_fromJava(env, j_asset_manager);
  if (asset_manager == nullptr) {
    LOGE("❌ AssetManager_fromJava failed! AssetManager is null.");
  } else {
    LOGI("✅ AssetManager_fromJava success.");
  }

  const char* c_path = env->GetStringUTFChars(j_path, nullptr);  // Java String → C 문자열
  std::string path(c_path);  // C 문자열 → std::string
  return jptr(new hello_ar::HelloArApplication(asset_manager, path));
}

JNI_METHOD(jboolean, isDepthSupported)
(JNIEnv *, jclass, jlong native_application) {
  return native(native_application)->IsDepthSupported();
}

JNI_METHOD(void, onSettingsChange)
(JNIEnv *, jclass, jlong native_application,
 jboolean is_instant_placement_enabled) {
  native(native_application)->OnSettingsChange(is_instant_placement_enabled);
}

JNI_METHOD(void, destroyNativeApplication)
(JNIEnv *, jclass, jlong native_application) {
  delete native(native_application);
}

JNI_METHOD(void, onPause)
(JNIEnv *, jclass, jlong native_application) {
  native(native_application)->OnPause();
}

JNI_METHOD(void, onResume)
(JNIEnv *env, jclass, jlong native_application, jobject context,
 jobject activity) {
  native(native_application)->OnResume(env, context, activity);
}

JNI_METHOD(void, setClassLoader)
(JNIEnv *env, jclass, jobject classLoader) {
    g_class_loader = env->NewGlobalRef(classLoader);

    jclass classLoaderClass = env->FindClass("java/lang/ClassLoader");
    g_load_class_method = env->GetMethodID(classLoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    JavaBridge::SetClassLoader(classLoader);
}

JNI_METHOD(void, changeStatus)
(JNIEnv *, jclass, jlong native_application) {
    native(native_application)->ChangeStatus();
}

JNI_METHOD(void, onGlSurfaceCreated)
(JNIEnv *, jclass, jlong native_application) {
  native(native_application)->OnSurfaceCreated();
}

JNI_METHOD(void, onDisplayGeometryChanged)
(JNIEnv *, jclass, jlong native_application, int display_rotation, int width,
 int height) {
  native(native_application)
      ->OnDisplayGeometryChanged(display_rotation, width, height);
}

JNI_METHOD(void, onGlSurfaceDrawFrame)
(JNIEnv *, jclass, jlong native_application,
 jboolean depth_color_visualization_enabled, jboolean use_depth_for_occlusion) {
  native(native_application)
      ->OnDrawFrame(depth_color_visualization_enabled, use_depth_for_occlusion);
}

JNIEXPORT void JNICALL
Java_com_capstone_whereigo_MappingFragment_registerNativeSelf(JNIEnv* env, jobject thiz, jlong native_application) {
    if (g_mappingFragment == nullptr) {
        g_mappingFragment = env->NewGlobalRef(thiz); // 전역 참조로 보관
        jclass clazz = env->GetObjectClass(g_mappingFragment);
        g_mapping_method = env->GetMethodID(clazz, "updateKeyFrameListSize", "(I)V");

        native(native_application)->g_mappingFragment = g_mappingFragment;
        native(native_application)->method_id = g_mapping_method;
    }
}

JNI_METHOD(void, getPoseStamp)
(JNIEnv *, jclass, jlong native_application) {
    data = native(native_application)->pose_graph.getLastElementOfKeyFrameList();
}

JNI_METHOD(float, getX)
(JNIEnv *, jclass) {
    return data.first;
}

JNI_METHOD(float, getZ)
(JNIEnv *, jclass) {
    return data.second;
}

JNI_METHOD(void, onTouched)
(JNIEnv *, jclass, jlong native_application, jfloat x, jfloat y) {
//  native(native_application)->OnTouched(x, y);
}

JNI_METHOD(jboolean, hasDetectedPlanes)
(JNIEnv *, jclass, jlong native_application) {
  return static_cast<jboolean>(
      native(native_application)->HasDetectedPlanes() ? JNI_TRUE : JNI_FALSE);
}

JNI_METHOD(void, savePoseGraph)
(JNIEnv *, jclass, jlong native_application, jobjectArray labels) {
  native(native_application)->SavePoseGraph(labels);
}

JNIEnv *GetJniEnv() {
  JNIEnv *env;
  jint result = g_vm->AttachCurrentThread(&env, nullptr);
  return result == JNI_OK ? env : nullptr;
}

jclass FindClass(const char* classname) {
    JNIEnv* env = GetJniEnv();
    if (!g_class_loader || !g_load_class_method) {
        return env->FindClass(classname);
    }
    jstring str_class_name = env->NewStringUTF(classname);
    jobject clazz = env->CallObjectMethod(g_class_loader, g_load_class_method, str_class_name);
    env->DeleteLocalRef(str_class_name);
    return static_cast<jclass>(clazz);
}

}  // extern "C"
