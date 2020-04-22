//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SCENEMANAGER_TOOLS_H
#define SCENEMANAGER_TOOLS_H
#ifdef _WIN32
#pragma once
#endif

class mxWindow;
class ITreeItem;

#define ERROR_R 255
#define ERROR_G 102
#define ERROR_B	0

#define CONSOLE_R 82
#define CONSOLE_G 173
#define CONSOLE_B 216

class CSentence;
class KeyValues;
class CUtlBuffer;

bool SceneManager_LoadSentenceFromWavFile( char const *wavfile, CSentence& sentence );
bool SceneManager_SaveSentenceToWavFile( char const *wavfile, CSentence& sentence );

void SceneManager_AddWindowStyle( mxWindow *w, int addbits );
void SceneManager_MakeToolWindow( mxWindow *w, bool smallcaption );

char *va( PRINTF_FORMAT_STRING const char *fmt, ... );
void Con_Printf( PRINTF_FORMAT_STRING const char *fmt, ... );
void Con_Overprintf( PRINTF_FORMAT_STRING const char *fmt, ... );
void Con_ColorPrintf( int r, int g, int b, PRINTF_FORMAT_STRING const char *fmt, ... );

char *SceneManager_MakeWindowsSlashes( char *pname );
const char *SceneManager_GetGameDirectory( void );
bool SceneManager_FullpathFileExists( const char *filename );

int ConvertANSIToUnicode(const char *ansi, wchar_t *unicode, int unicodeBufferSize);
int ConvertUnicodeToANSI(const wchar_t *unicode, char *ansi, int ansiBufferSize);

extern class IFileSystem *filesystem;

extern char g_appTitle[];

void VSS_Checkout( char const *name, bool updatestaticons = true );
void VSS_Checkin( char const *name, bool updatestaticons = true );

void SceneManager_LoadWindowPositions( KeyValues *kv, mxWindow *wnd );
void SceneManager_SaveWindowPositions( CUtlBuffer& buf, int indent, mxWindow *wnd );

void MakeFileWriteable( const char *filename );

#endif // SCENEMANAGER_TOOLS_H
