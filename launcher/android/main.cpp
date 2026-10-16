/*
Copyright (C) 2022 nillerusr

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <jni.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/system_properties.h>
#include <sys/sysinfo.h>
#include <SDL_hints.h>
#include "tier0/dbg.h"
#include "tier0/threadtools.h"

char *LauncherArgv[512];
char java_args[4096];
int iLastArgs = 0;

extern void InitCrashHandler();
DLL_EXPORT int LauncherMain( int argc, char **argv ); // from launcher.cpp

DLL_EXPORT int Java_com_valvesoftware_ValveActivity2_setenv(JNIEnv *jenv, jclass *jclass, jstring env, jstring value, jint over)
{
	const char *envStr = jenv->GetStringUTFChars(env, NULL);
	const char *valueStr = jenv->GetStringUTFChars(value, NULL);
	Msg( "Java_com_valvesoftware_ValveActivity2_setenv %s=%s\n", envStr, valueStr );
	int result = setenv( envStr, valueStr, over );
	jenv->ReleaseStringUTFChars(env, envStr);
	jenv->ReleaseStringUTFChars(value, valueStr);
	return result;
}

DLL_EXPORT void Java_com_valvesoftware_ValveActivity2_nativeOnActivityResult()
{
//	Msg( "Java_com_valvesoftware_ValveActivity_nativeOnActivityResult\n" );
}

DLL_EXPORT void Java_com_valvesoftware_ValveActivity2_setArgs(JNIEnv *env, jclass *clazz, jstring str)
{
	const char *strValue = env->GetStringUTFChars(str, NULL);
	strncpy( java_args, strValue, sizeof(java_args) - 1 );
	java_args[sizeof(java_args) - 1] = '\0';
	env->ReleaseStringUTFChars(str, strValue);
}

void SetLauncherArgs()
{
#define A(a,b) LauncherArgv[iLastArgs++] = (char*)a; \
	LauncherArgv[iLastArgs++] = (char*)b
#define D(a) LauncherArgv[iLastArgs++] = (char*)a

	static char binPath[2048];
	snprintf(binPath, sizeof binPath, "%s/hl2_linux", getenv("APP_DATA_PATH") );
	D(binPath);

	D("-nouserclip");

	char *pch;

	pch = strtok (java_args," ");
	while (pch != NULL)
	{
		LauncherArgv[iLastArgs++] = pch;
		pch = strtok (NULL, " ");
	}

	D("-fullscreen");
	D("-nosteam");
	D("-insecure");

#undef A
#undef D
}

float GetTotalMemory()
{
	struct sysinfo info;
	if( sysinfo(&info) != 0 )
		return 0.f;
	return (float)info.totalram * info.mem_unit / 1024.f / 1024.f;
}

void android_property_print(const char *name)
{
	char strValue[PROP_VALUE_MAX];
	memset(strValue, 0, sizeof(strValue));
	__system_property_get(name, strValue);
	Msg("prop %s=%s\n", name, strValue);
}

// Detect the system locale and set the LANG environment variable
// so the engine can pick the correct language automatically.
void DetectSystemLanguage()
{
	char locale[PROP_VALUE_MAX];
	memset(locale, 0, sizeof(locale));

	// Android 7.0+ (API 24) uses persist.sys.locale
	if( __system_property_get("persist.sys.locale", locale) <= 0 )
	{
		// Fallback for older devices
		__system_property_get("ro.product.locale", locale);
	}

	if( locale[0] )
	{
		Msg( "Detected system locale: %s\n", locale );
		// The engine's PchLanguageICUCodeToELanguage expects codes like "zh_CN", "zh_TW", "en_US"
		// Android uses "zh-CN", "zh-TW", "en-US" format, convert hyphen to underscore
		for( char *p = locale; *p; ++p )
		{
			if( *p == '-' )
				*p = '_';
		}
		setenv("LANG", locale, 1);
	}
}


DLL_EXPORT int LauncherMainAndroid( int argc, char **argv )
{
	InitCrashHandler();

	Msg("GetTotalMemory() = %.2f \n", GetTotalMemory());

	android_property_print("ro.build.version.sdk");
	android_property_print("ro.product.device");
	android_property_print("ro.product.manufacturer");
	android_property_print("ro.product.model");
	android_property_print("ro.product.name");

	// Detect system language so the engine uses the correct localization
	DetectSystemLanguage();

	SetLauncherArgs();

	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
	DeclareCurrentThreadIsMainThread(); // Init thread propertly on Android

	return LauncherMain(iLastArgs, LauncherArgv);
}
