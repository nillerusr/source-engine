#ifdef ANDROID

#include <android/log.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dlfcn.h>
#include <SDL_hints.h>

#include "tier0/threadtools.h"

char *LauncherArgv[512];
char java_args[4096];
int iLastArgs = 0;

#define TAG "SRCENG"
#define PRIO ANDROID_LOG_DEBUG
#define LogPrintf(...) do { __android_log_print(PRIO, TAG, __VA_ARGS__); printf( __VA_ARGS__); } while( 0 );
#define DLLEXPORT extern "C" __attribute__((visibility("default")))

DLLEXPORT void Java_com_valvesoftware_ValveActivity2_setDataDirectoryPath(JNIEnv *env, jclass *clazz, jstring path)
{
	setenv( "APP_DATA_PATH", env->GetStringUTFChars(path, NULL), 1);
	LogPrintf( "Java_com_valvesoftware_ValveActivity2_setDataDirectoryPath: %s", getenv("APP_DATA_PATH") );
}

DLLEXPORT void Java_com_valvesoftware_ValveActivity2_setGameDirectoryPath(JNIEnv *env, jclass *clazz, jstring path)
{
	LogPrintf( "Java_com_valvesoftware_ValveActivity2_setGameDirectoryPath" );
	setenv( "VALVE_GAME_PATH", env->GetStringUTFChars(path, NULL), 1 );
}

DLLEXPORT int Java_com_valvesoftware_ValveActivity2_setenv(JNIEnv *jenv, jclass *jclass, jstring env, jstring value, jint over)
{
	LogPrintf( "Java_com_valvesoftware_ValveActivity2_setenv %s=%s", jenv->GetStringUTFChars(env, NULL), jenv->GetStringUTFChars(value, NULL) );
	return setenv( jenv->GetStringUTFChars(env, NULL), jenv->GetStringUTFChars(value, NULL), over );
}

DLLEXPORT void Java_com_valvesoftware_ValveActivity2_nativeOnActivityResult()
{
	LogPrintf( "Java_com_valvesoftware_ValveActivity_nativeOnActivityResult" );
}

void parseArgs( char *args )
{
	char *pch;
	pch = strtok (args," ");
	while (pch != NULL)
	{
		LauncherArgv[iLastArgs++] = pch;
		pch = strtok (NULL, " ");
	}
}

DLLEXPORT void Java_com_valvesoftware_ValveActivity2_setArgs(JNIEnv *env, jclass *clazz, jstring str)
{
	strncpy( java_args, env->GetStringUTFChars(str, NULL), sizeof java_args );
}

DLLEXPORT int LauncherMain( int argc, char **argv );

#define A(a,b) LauncherArgv[iLastArgs++] = (char*)a; \
	LauncherArgv[iLastArgs++] = (char*)b

#define D(a) LauncherArgv[iLastArgs++] = (char*)a

void SetLauncherArgs()
{
	static char binPath[2048];
	snprintf(binPath, sizeof binPath, "%s/hl2_linux", getenv("APP_DATA_PATH") );
	LogPrintf(binPath);
	D(binPath);

	parseArgs(java_args);

	A("-game", "hl2");
	D("-window");
	D("-nosteam");
	D("-insecure");
}

static void *lgles;

typedef void (*t_set_getprocaddress)(void *(*new_proc_address)(const char *));
t_set_getprocaddress gl4es_set_getprocaddress;

typedef void *(*t_eglGetProcAddress)( const char * );
t_eglGetProcAddress eglGetProcAddress;

void *GetProcAddress( const char *procname )
{
	void *result = dlsym(lgles, procname);
	if( result )
		return result;
	else if( eglGetProcAddress )
		return eglGetProcAddress(procname);
}

DLLEXPORT int LauncherMainAndroid( int argc, char **argv )
{

	SetLauncherArgs();

	void *lgl4es = dlopen("libgl4es.so", 0);
	if( !lgl4es )
	{
		LogPrintf("Failed to dlopen library libgl4es.so: %s\n", dlerror());
		return 1;
	}

	void *lEGL = dlopen("libEGL.so", 0);
	if( !lEGL )
	{
		LogPrintf("Failed to dlopen library libEGL.so: %s\n", dlerror());
		return 1;
	}

	lgles = dlopen("libGLESv2.so", 0);
	if( !lgles )
	{
		LogPrintf("Failed to dlopen library libGLESv2.so: %s\n", dlerror());
		return 1;
	}

	gl4es_set_getprocaddress = (t_set_getprocaddress)dlsym(lgl4es, "set_getprocaddress");
	eglGetProcAddress = (t_eglGetProcAddress)dlsym(lEGL, "eglGetProcAddress");

	if( gl4es_set_getprocaddress )
	{
		gl4es_set_getprocaddress( &GetProcAddress );
	}
	else
	{
		LogPrintf("Failed to call set_getprocaddress\n");
		return 1;
	}

	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
	DeclareCurrentThreadIsMainThread(); // Init thread propertly on Android

	return LauncherMain(iLastArgs, LauncherArgv);
}

#endif
