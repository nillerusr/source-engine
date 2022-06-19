#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#ifdef ANDROID
#include <android/log.h>
#include <SDL_version.h>

#define TAG "SRCENG"
#define PRIO ANDROID_LOG_DEBUG
#define LogPrintf(...) do { __android_log_print(PRIO, TAG, __VA_ARGS__); printf( __VA_ARGS__); } while( 0 );

#else
#define LogPrintf(...) printf(__VA_ARGS__)
#endif

typedef void (*t_set_getprocaddress)(void *(*new_proc_address)(const char *));
t_set_getprocaddress gl4es_set_getprocaddress;

typedef void *(*t_eglGetProcAddress)( const char * );
t_eglGetProcAddress eglGetProcAddress;

void *GetProcAddress( const char *procname )
{
	return eglGetProcAddress(procname);
}

void InitGL4ES()
{
	void *lgl4es = dlopen("libgl4es.so", RTLD_LAZY);
	if( !lgl4es )
	{
		LogPrintf("Failed to dlopen library libgl4es.so: %s\n", dlerror());
	}

	void *lEGL = dlopen("libEGL.so", RTLD_LAZY);
	if( !lEGL )
	{
		LogPrintf("Failed to dlopen library libEGL.so: %s\n", dlerror());
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
	}
}

#ifdef ANDROID
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <SDL_hints.h>

#include "tier0/threadtools.h"

char *LauncherArgv[512];
char java_args[4096];
int iLastArgs = 0;

#define TAG "SRCENG"
#define PRIO ANDROID_LOG_DEBUG
#define LogPrintf(...) do { __android_log_print(PRIO, TAG, __VA_ARGS__); printf( __VA_ARGS__); } while( 0 );
#define DLLEXPORT extern "C" __attribute__((visibility("default")))

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

	D("-nouserclip");

	parseArgs(java_args);

	D("-fullscreen");
	D("-nosteam");
	D("-insecure");
}

DLLEXPORT int LauncherMainAndroid( int argc, char **argv )
{
	SDL_version ver;
	SDL_GetVersion( &ver );

	LogPrintf("SDL version: %d.%d.%d rev: %s\n", (int)ver.major, (int)ver.minor, (int)ver.patch, SDL_GetRevision());

	SetLauncherArgs();

	InitGL4ES();

	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
	DeclareCurrentThreadIsMainThread(); // Init thread propertly on Android

	return LauncherMain(iLastArgs, LauncherArgv);
}

#endif
