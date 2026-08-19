#include "chuckstation2.hpp"

#ifndef __ANDROID__
#error "This file should only be compiled for Android targets"
#endif

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>
#include <pthread.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <errno.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

#define LOG_TAG "ChuckStation2"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace chuckstation2::platform {

static JavaVM* g_jvm = nullptr;
static jobject g_activity = nullptr;
static jobject g_class_loader = nullptr;
static jmethodID g_find_class_method = nullptr;
static AAssetManager* g_asset_manager = nullptr;
static ANativeWindow* g_native_window = nullptr;
static std::mutex g_window_mutex;
static std::atomic<bool> g_surface_ready{false};
static pthread_t g_main_thread_id;

// Get Android API level at runtime
static int get_api_level() {
    char value[92] = {0};
    int len = __system_property_get("ro.build.version.sdk", value);
    if (len > 0) {
        return atoi(value);
    }
    return 0;
}

// Get device model name
static std::string get_device_model() {
    char value[92] = {0};
    __system_property_get("ro.product.model", value);
    return std::string(value);
}

// Get Android version string
static std::string get_android_version() {
    char value[92] = {0};
    __system_property_get("ro.build.version.release", value);
    return std::string(value);
}

// Check if the device supports Vulkan
static bool supports_vulkan() {
    // Vulkan is available on Android 7.0+ (API 24)
    // with proper driver support
    return get_api_level() >= 24;
}

// Get internal storage path for emulator data
static std::string get_internal_storage_path() {
    if (!g_jvm || !g_activity) return "/data/data/com.chuckstation.ChuckStation2";

    JNIEnv* env = nullptr;
    bool attached = false;

    if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            attached = true;
        } else {
            return "/data/data/com.chuckstation.ChuckStation2";
        }
    }

    jclass activity_class = env->GetObjectClass(g_activity);
    jmethodID get_files_dir = env->GetMethodID(activity_class, "getFilesDir", "()Ljava/io/File;");
    jobject files_dir = env->CallObjectMethod(g_activity, get_files_dir);
    jclass file_class = env->GetObjectClass(files_dir);
    jmethodID get_path = env->GetMethodID(file_class, "getAbsolutePath", "()Ljava/lang/String;");
    jstring path_str = (jstring)env->CallObjectMethod(files_dir, get_path);
    const char* path = env->GetStringUTFChars(path_str, nullptr);
    std::string result(path);
    env->ReleaseStringUTFChars(path_str, path);

    if (attached) {
        g_jvm->DetachCurrentThread();
    }

    return result;
}

// Check if running on the main thread
static bool is_main_thread() {
    return pthread_equal(pthread_self(), g_main_thread_id) != 0;
}

// Set up HiDPI / display density scaling for Android
static float get_display_density() {
    if (!g_jvm || !g_activity) return 1.0f;

    JNIEnv* env = nullptr;
    bool attached = false;

    if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        if (g_jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
            attached = true;
        } else {
            return 1.0f;
        }
    }

    jclass activity_class = env->GetObjectClass(g_activity);
    jmethodID get_resources = env->GetMethodID(activity_class, "getResources", "()Landroid/content/res/Resources;");
    jobject resources = env->CallObjectMethod(g_activity, get_resources);
    jclass resources_class = env->GetObjectClass(resources);
    jmethodID get_display_metrics = env->GetMethodID(resources_class, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");
    jobject metrics = env->CallObjectMethod(resources, get_display_metrics);
    jclass metrics_class = env->GetObjectClass(metrics);
    jfieldID density_field = env->GetFieldID(metrics_class, "density", "F");
    float density = env->GetFloatField(metrics, density_field);

    if (attached) {
        g_jvm->DetachCurrentThread();
    }

    return density;
}

bool init(chuckstation2::instance* iris) {
    g_main_thread_id = pthread_self();

    int api_level = get_api_level();
    std::string device = get_device_model();
    std::string android_ver = get_android_version();

    LOGI("ChuckStation2 initializing on Android %s (API %d), Device: %s",
         android_ver.c_str(), api_level, device.c_str());

    if (!supports_vulkan()) {
        LOGE("ChuckStation2 requires Vulkan support (Android 7.0+ / API 24+)");
        return false;
    }

    // Configure SDL hints for Android
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

    // Apply Android-specific settings
    apply_settings(iris);

    LOGI("ChuckStation2 Android platform initialized successfully");
    return true;
}

bool apply_settings(chuckstation2::instance* iris) {
    // Android-specific window adjustments
    if (iris->window) {
        // Enable fullscreen immersive mode on Android
        SDL_SetWindowFullscreen(iris->window, true);

        // Keep screen on while emulator is running
        SDL_SetHint("SDL_ANDROID_KEEP_SCREEN_ON", "1");
    }

    return true;
}

void destroy(chuckstation2::instance* iris) {
    std::lock_guard<std::mutex> lock(g_window_mutex);

    if (g_native_window) {
        ANativeWindow_release(g_native_window);
        g_native_window = nullptr;
    }

    g_surface_ready = false;

    if (g_activity) {
        JNIEnv* env = nullptr;
        if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
            env->DeleteGlobalRef(g_activity);
        }
        g_activity = nullptr;
    }

    if (g_class_loader) {
        JNIEnv* env = nullptr;
        if (g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
            env->DeleteGlobalRef(g_class_loader);
        }
        g_class_loader = nullptr;
    }

    LOGI("ChuckStation2 Android platform destroyed");
}

// ---- JNI exports called from Java/Kotlin side ----

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    LOGI("ChuckStation2 native library loaded");
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved) {
    g_jvm = nullptr;
    LOGI("ChuckStation2 native library unloaded");
}

JNIEXPORT void JNICALL
Java_com_chuckstation_ChuckStation2_ChuckStation2Activity_nativeInit(JNIEnv* env, jobject thiz) {
    // Store global references for later use
    g_activity = env->NewGlobalRef(thiz);

    // Get the class loader for resource access
    jclass activity_class = env->GetObjectClass(thiz);
    jmethodID get_class_loader = env->GetMethodID(activity_class, "getClassLoader", "()Ljava/lang/ClassLoader;");
    jobject class_loader = env->CallObjectMethod(thiz, get_class_loader);
    g_class_loader = env->NewGlobalRef(class_loader);
    g_find_class_method = env->GetMethodID(
        env->GetObjectClass(class_loader), "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");

    // Get the AssetManager for accessing app assets
    jmethodID get_assets = env->GetMethodID(activity_class, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject asset_manager = env->CallObjectMethod(thiz, get_assets);
    g_asset_manager = AAssetManager_fromJava(env, asset_manager);

    LOGI("ChuckStation2 native init complete");
}

JNIEXPORT void JNICALL
Java_com_chuckstation_ChuckStation2_ChuckStation2Activity_nativeSurfaceCreated(JNIEnv* env, jobject thiz, jobject surface) {
    std::lock_guard<std::mutex> lock(g_window_mutex);

    g_native_window = ANativeWindow_fromSurface(env, surface);
    g_surface_ready = true;

    LOGI("ChuckStation2: Native surface created (%dx%d)",
         ANativeWindow_getWidth(g_native_window),
         ANativeWindow_getHeight(g_native_window));
}

JNIEXPORT void JNICALL
Java_com_chuckstation_ChuckStation2_ChuckStation2Activity_nativeSurfaceChanged(
    JNIEnv* env, jobject thiz, jobject surface, jint width, jint height) {
    std::lock_guard<std::mutex> lock(g_window_mutex);

    if (g_native_window) {
        ANativeWindow_release(g_native_window);
    }

    g_native_window = ANativeWindow_fromSurface(env, surface);

    LOGI("ChuckStation2: Surface changed to %dx%d", width, height);
}

JNIEXPORT void JNICALL
Java_com_chuckstation_ChuckStation2_ChuckStation2Activity_nativeSurfaceDestroyed(JNIEnv* env, jobject thiz) {
    std::lock_guard<std::mutex> lock(g_window_mutex);

    g_surface_ready = false;

    if (g_native_window) {
        ANativeWindow_release(g_native_window);
        g_native_window = nullptr;
    }

    LOGI("ChuckStation2: Native surface destroyed");
}

JNIEXPORT void JNICALL
Java_com_chuckstation_ChuckStation2_ChuckStation2Activity_nativePause(JNIEnv* env, jobject thiz) {
    // Called when the Activity goes to background
    LOGI("ChuckStation2: Activity paused");
}

JNIEXPORT void JNICALL
Java_com_chuckstation_ChuckStation2_ChuckStation2Activity_nativeResume(JNIEnv* env, jobject thiz) {
    // Called when the Activity comes to foreground
    LOGI("ChuckStation2: Activity resumed");
}

JNIEXPORT void JNICALL
Java_com_chuckstation_ChuckStation2_ChuckStation2Activity_nativeDestroy(JNIEnv* env, jobject thiz) {
    LOGI("ChuckStation2: Activity destroyed");
    g_surface_ready = false;
}

JNIEXPORT jstring JNICALL
Java_com_chuckstation_ChuckStation2_ChuckStation2Activity_nativeGetVersion(JNIEnv* env, jobject thiz) {
    return env->NewStringUTF(STR(_CS2_VERSION));
}

JNIEXPORT jstring JNICALL
Java_com_chuckstation_ChuckStation2_ChuckStation2Activity_nativeGetDeviceInfo(JNIEnv* env, jobject thiz) {
    char buf[256];
    snprintf(buf, sizeof(buf),
             "Android %s (API %d) | Device: %s | Vulkan: %s",
             get_android_version().c_str(),
             get_api_level(),
             get_device_model().c_str(),
             supports_vulkan() ? "Yes" : "No");
    return env->NewStringUTF(buf);
}

}  // extern "C"

}  // namespace chuckstation2::platform