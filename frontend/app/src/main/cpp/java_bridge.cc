#include "java_bridge.h"
#include <android/log.h>

JavaVM* JavaBridge::java_vm_ = nullptr;
jobject JavaBridge::class_loader_ = nullptr;
jmethodID JavaBridge::find_class_method_ = nullptr;

void JavaBridge::SetJavaVM(JavaVM* vm) {
    java_vm_ = vm;
}

void JavaBridge::SetClassLoader(jobject class_loader) {
    JNIEnv* env = GetEnv();
    if (!env) return;
    class_loader_ = env->NewGlobalRef(class_loader);

    jclass class_loader_class = env->GetObjectClass(class_loader_);
    find_class_method_ = env->GetMethodID(class_loader_class, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
}

jclass JavaBridge::FindClass(const char* class_name) {
    JNIEnv* env = GetEnv();
//    if (!env) {
//        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "FindClass 실패: env is null");
//        return nullptr;
//    }
//    if (!class_loader_) {
//        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "FindClass 실패: class_loader_ is null");
//        return nullptr;
//    }
//    if (!find_class_method_) {
//        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "FindClass 실패: find_class_method_ is null");
//        return nullptr;
//    }

    jstring str_class_name = env->NewStringUTF(class_name);
    jobject clazz = env->CallObjectMethod(class_loader_, find_class_method_, str_class_name);
    env->DeleteLocalRef(str_class_name);

//    if (!clazz) {
//        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "FindClass 실패: loadClass 호출 실패: %s", class_name);
//    } else {
//        __android_log_print(ANDROID_LOG_INFO, "JavaBridge", "FindClass 성공: %s", class_name);
//    }
    return static_cast<jclass>(clazz);
}

void JavaBridge::SpeakText(const char* text) {
    JNIEnv* env = GetEnv();
    if (!env) return;

    jclass clazz = FindClass("com/capstone/whereigo/TtsManager");
    if (!clazz) {
        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "❌ TtsManager 클래스 못 찾음");
        return;
    }

    // 1. INSTANCE 필드 가져오기
    jfieldID instanceField = env->GetStaticFieldID(clazz, "INSTANCE", "Lcom/capstone/whereigo/TtsManager;");
    if (!instanceField) {
        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "❌ TtsManager.INSTANCE 필드 못 찾음");
        return;
    }

    jobject instance = env->GetStaticObjectField(clazz, instanceField);
    if (!instance) {
        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "❌ TtsManager.INSTANCE 객체 못 찾음");
        return;
    }

    // 2. INSTANCE에서 speak 메서드 가져오기 (instance method)
    jmethodID speakMethod = env->GetMethodID(clazz, "speak", "(Ljava/lang/String;)V");
    if (!speakMethod) {
        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "❌ TtsManager.speak 메서드 못 찾음");
        return;
    }

    // 3. INSTANCE를 통해 메서드 호출
    jstring jText = env->NewStringUTF(text);
    env->CallVoidMethod(instance, speakMethod, jText);
    env->DeleteLocalRef(jText);
}

void JavaBridge::EnqueueAudio(const char* filename) {
    JNIEnv* env = GetEnv();
    if (!env) return;

    jclass clazz = FindClass("com/capstone/whereigo/AudioManager");
    if (!clazz) {
        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "❌ AudioManager 클래스 못 찾음");
        return;
    }

    jfieldID instanceField = env->GetStaticFieldID(clazz, "INSTANCE", "Lcom/capstone/whereigo/AudioManager;");
    if (!instanceField) {
        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "❌ AudioManager.INSTANCE 필드 못 찾음");
        return;
    }

    jobject instance = env->GetStaticObjectField(clazz, instanceField);
    if (!instance) {
        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "❌ AudioManager.INSTANCE 객체 못 찾음");
        return;
    }

    jmethodID enqueueMethod = env->GetMethodID(clazz, "enqueueAudio", "(Ljava/lang/String;)V");
    if (!enqueueMethod) {
        __android_log_print(ANDROID_LOG_ERROR, "JavaBridge", "❌ enqueueAudio 메서드 못 찾음");
        return;
    }

    jstring jFilename = env->NewStringUTF(filename);
    env->CallVoidMethod(instance, enqueueMethod, jFilename);
    env->DeleteLocalRef(jFilename);
}

void JavaBridge::UpdateYaw(float cameraYaw, float pathYaw) {
    JNIEnv* env = GetEnv();
    if (!env) return;

    jclass clazz = FindClass("com/capstone/whereigo/HelloArFragment");
    if (!clazz) return;

    jmethodID method = env->GetStaticMethodID(clazz, "updateYawFromNative", "(FF)V");
    if (!method) return;

    env->CallStaticVoidMethod(clazz, method, cameraYaw, pathYaw);
}

void JavaBridge::VibrateOnce() {
    JNIEnv* env = GetEnv();
    if (!env) return;

    jclass clazz = FindClass("com/capstone/whereigo/HelloArFragment");
    if (!clazz) return;

    jmethodID method = env->GetStaticMethodID(clazz, "vibrateOnce", "()V");
    if (!method) return;

    env->CallStaticVoidMethod(clazz, method);
}

JNIEnv* JavaBridge::GetEnv() {
    if (!java_vm_) return nullptr;
    JNIEnv* env = nullptr;
    if (java_vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
        return env;
    }
    return nullptr;
}
