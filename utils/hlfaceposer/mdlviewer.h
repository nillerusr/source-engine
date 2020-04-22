//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MDLVIEWER_H
#define MDLVIEWER_H
#ifdef _WIN32
#pragma once
#endif

#include "sceneimage.h"

#define IDC_FILE_LOADMODEL			1001
#define IDC_FILE_LOADBACKGROUNDTEX	1002
#define IDC_FILE_LOADGROUNDTEX		1003
#define IDC_FILE_UNLOADGROUNDTEX	1004
#define IDC_FILE_RECENTFILES1		1008
#define IDC_FILE_RECENTFILES2		1009
#define IDC_FILE_RECENTFILES3		1010
#define IDC_FILE_RECENTFILES4		1011
#define IDC_FILE_RECENTFILES5		1012
#define IDC_FILE_RECENTFILES6		1013
#define IDC_FILE_RECENTFILES7		1014
#define IDC_FILE_RECENTFILES8		1015
#define IDC_FILE_EXIT				1016
#define IDC_FILE_REFRESH			1017
#define IDC_FILE_SAVESOUNDSCRIPTCHANGES 1018
#define IDC_FILE_REBUILDSCENESIMAGE	1019

#define IDC_EXPRESSIONS_SAVE		1020
#define IDC_EXPRESSIONS_LOAD		1021
#define IDC_EXPRESSIONS_SAVEAS		1022

#define IDC_EXPRESSIONS_EXPORT		1023

#define IDC_EXPRESSIONS_CLOSE		1024
#define IDC_EXPRESSIONS_CLOSEALL	1025

#define IDC_EXPRESSIONS_NEW			1026
#define IDC_EXPRESSIONS_REDOBITMAPS 1027


#define IDC_CHOREOSCENE_NEW			1030
#define IDC_CHOREOSCENE_LOAD		1031
#define IDC_CHOREOSCENE_SAVE		1032
#define IDC_CHOREOSCENE_SAVEAS		1033
#define IDC_CHOREOSCENE_CLOSE		1034
#define IDC_CHOREOSCENE_ADDACTOR	1035
#define IDC_FILE_LOADMODEL_STEAM	1036
#define IDC_CHOREOSCENE_LOADNEXT	1038

#define IDC_OPTIONS_COLORBACKGROUND	1101
#define IDC_OPTIONS_COLORGROUND		1102
#define IDC_OPTIONS_COLORLIGHT		1103
#define IDC_OPTIONS_CENTERVIEW		1104
#define IDC_OPTIONS_MAKESCREENSHOT	1105
#define IDC_OPTIONS_DUMP			1106
#define IDC_OPTIONS_CENTERONFACE	1107
#define IDC_OPTIONS_CLEARMODELSOUNDS 1108

#define IDC_OPTIONS_LANGUAGESTART	1150

#define IDC_WINDOW_FIRSTTOOL		1200
#define IDC_WINDOW_LASTTOOL			1231
#define IDC_WINDOW_TILE_HORIZ		1232
#define IDC_WINDOW_TILE_VERT		1233
#define IDC_WINDOW_CASCADE			1234
#define IDC_WINDOW_HIDEALL			1235
#define IDC_WINDOW_SHOWALL			1236
#define IDC_WINDOW_TILE				1237

#define IDC_WINDOW_TAB				1238
#define IDC_MODEL_TAB				1239
#define IDC_GRIDSETTINGS			1240

#define IDC_HELP_GOTOHOMEPAGE		1301
#define IDC_HELP_ABOUT				1302

class mxMenuBar;
class mxMenu;
class MatSysWindow;
class ControlPanel;
class FlexPanel;
class mxStatusWindow;
class CChoreoView;
class CMDLViewerWorkspace;
class CMDLViewerWindowTab;
class CMDLViewerModelTab;
class CMDLViewerGridSettings;

enum { Action, Size, Timer, Idle, Show, Hide,
		MouseUp, MouseDown, MouseMove, MouseDrag,
		KeyUp, KeyDown
	};

class MDLViewer : public mxWindow, public ISceneCompileStatus
{
	mxMenuBar *mb;
	mxMenu *menuFile;
	mxMenu *menuOptions;
	mxMenu *menuCloseCaptionLanguages;
	mxMenu *menuWindow;
	mxMenu *menuHelp;
	mxMenu *menuEdit;
	mxMenu *menuExpressions;
	mxMenu *menuChoreography;

	CMDLViewerWorkspace *workspace;
	CMDLViewerWindowTab *windowtab;
	CMDLViewerModelTab *modeltab;
	CMDLViewerGridSettings *gridsettings;

	void loadRecentFiles ();
	void saveRecentFiles ();
	void initRecentFiles ();

	int m_nCurrentFrame;

public:
	// CREATORS
	MDLViewer ();
	~MDLViewer ();

	virtual void		OnDelete();
	virtual bool		CanClose();

	virtual void UpdateStatus( char const *pchSceneName, bool bQuiet, int nIndex, int nCount );

	void OnFileLoaded( char const *pszFile );

	// MANIPULATORS
	virtual int handleEvent (mxEvent *event);
	void redraw ();
	virtual bool PaintBackground( void );

	void UpdateWindowMenu( void );
	void UpdateLanguageMenu( int currentLanguageId );

	void InitModelTab( void );
	void InitGridSettings( void );

	int GetActiveModelTab( void );
	void SetActiveModelTab( int modelindex );

	void Refresh( void );
	void LoadModelFile( const char *pszFile );
	int		GetCurrentHitboxSet(void);

	virtual bool Closing( void );

	void LoadWindowPositions( void );
	void SaveWindowPositions( void );

	void OnSaveSoundScriptChanges();
	void OnRebuildScenesImage();

	void OnCascade();
	void OnTile();
	void OnTileHorizontally();
	void OnTileVertically();

	void OnHideAll();
	void OnShowAll();

	void Think( float dt );

	int	 GetCurrentFrame( void );

	// ACCESSORS
	mxMenuBar *getMenuBar () const { return mb; }

	void LoadModel_Steam();

	void OnVCDSaved();

private:
	void DoTile( int x, int y );

	void LoadPosition( void );
	void SavePosition( void );

	bool AreSoundScriptsDirty();

	bool	m_bOldSoundScriptsDirty;
	bool	m_bVCDSaved;
};


const char *GetGameDirectory( );
void CreatePath( const char *pPath );
extern MDLViewer *g_MDLViewer;
extern char g_appTitle[];

#endif // MDLVIEWER_H
