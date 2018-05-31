#include <string.h>
#include <jni.h>
#include <android/log.h>

#include <iostream>

#include "OsgMainApp.hpp"

OsgMainApp mainApp;

extern "C" {
    JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_init(JNIEnv * env, jobject obj, jint width, jint height);
    JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_step(JNIEnv * env, jobject obj);
    JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_mouseButtonPressEvent(JNIEnv * env, jobject obj, jfloat x, jfloat y, jint button);
    JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_mouseButtonReleaseEvent(JNIEnv * env, jobject obj, jfloat x, jfloat y, jint button);
    JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_mouseMoveEvent(JNIEnv * env, jobject obj, jfloat x, jfloat y);
    JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_keyboardDown(JNIEnv * env, jobject obj, jint key);
    JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_keyboardUp(JNIEnv * env, jobject obj, jint key);
    JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_loadObject(JNIEnv * env, jobject obj, jstring path);
};

JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_init(JNIEnv * env, jobject obj, jint width, jint height){
    mainApp.initOsgWindow(0,0,width,height);
}
JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_step(JNIEnv * env, jobject obj){
    mainApp.draw();
}
JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_mouseButtonPressEvent(JNIEnv * env, jobject obj, jfloat x, jfloat y, jint button){
    mainApp.mouseButtonPressEvent(x,y,button);
}
JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_mouseButtonReleaseEvent(JNIEnv * env, jobject obj, jfloat x, jfloat y, jint button){
    mainApp.mouseButtonReleaseEvent(x,y,button);
}
JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_mouseMoveEvent(JNIEnv * env, jobject obj, jfloat x, jfloat y){
    mainApp.mouseMoveEvent(x,y);
}
JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_keyboardDown(JNIEnv * env, jobject obj, jint key){
    mainApp.keyboardDown(key);
}
JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_keyboardUp(JNIEnv * env, jobject obj, jint key){
    mainApp.keyboardUp(key);
}

static std::string jstring2string(JNIEnv *env, jstring jStr) {
    if (!jStr)
        return "";

    const jclass stringClass = env->GetObjectClass(jStr);
    const jmethodID getBytes = env->GetMethodID(stringClass, "getBytes", "(Ljava/lang/String;)[B");
    const jbyteArray stringJbytes = (jbyteArray) env->CallObjectMethod(jStr, getBytes, env->NewStringUTF("UTF-8"));

    size_t length = (size_t) env->GetArrayLength(stringJbytes);
    jbyte* pBytes = env->GetByteArrayElements(stringJbytes, NULL);

    std::string ret = std::string((char *)pBytes, length);
    env->ReleaseByteArrayElements(stringJbytes, pBytes, JNI_ABORT);

    env->DeleteLocalRef(stringJbytes);
    env->DeleteLocalRef(stringClass);
    return ret;
}

JNIEXPORT void JNICALL Java_osg_AndroidExample_osgNativeLib_loadObject(JNIEnv * env, jobject obj, jstring path){
    mainApp.loadObject(jstring2string(env, path));
}
