//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Defines the application object.
//
//===========================================================================//

#ifndef HAMMER_H
#define HAMMER_H
#ifdef _WIN32
#pragma once
#endif


#ifndef __AFXWIN_H__
	#error include 'stdafx.h' before including this file for PCH
#endif

#include "resource.h"       // main symbols
#include "RunCommands.h"
#include "IHammer.h"
#include "tier1/utlmap.h"
#include "tier3/tier3dm.h"


//-----------------------------------------------------------------------------
// Forward declarations...
//-----------------------------------------------------------------------------
class CMapDoc;
class IStudioRender;
class IBaseFileSystem;
class IEngineAPI;
class IMDLCache;
class CGameConfig;


//
// Values for retrieving specific directories using GetDirectory.
//
enum DirIndex_t
{
	DIR_PROGRAM,			// The editor install directory.
	DIR_PREFABS,			// The directory for prefabs.
	DIR_GAME_EXE,			// The location of the game executable.
	DIR_MOD,				// The location of the mod currently being worked on.
	DIR_GAME,				// The location of the base game currently being worked on.
	DIR_MATERIALS,			// The location of the mod's materials.
	DIR_AUTOSAVE			// The location of autosave files.
};


// combines a list of commands & a name:
class CCommandSequence
{
	public:

		CCommandArray m_Commands;
		char m_szName[128];
};


class CHammerDocTemplate : public CMultiDocTemplate
{
public:
	CHammerDocTemplate( UINT nIDResource, CRuntimeClass* pDocClass, CRuntimeClass* pFrameClass, CRuntimeClass* pViewClass ) :
	  CMultiDocTemplate( nIDResource, pDocClass, pFrameClass, pViewClass )
	  {
	  }

	  virtual	CDocument	*OpenDocumentFile( LPCTSTR lpszPathName, BOOL bMakeVisible = TRUE );
	  virtual	void		CloseAllDocuments( BOOL bEndSession );
	  virtual	void		InitialUpdateFrame( CFrameWnd* pFrame, CDocument* pDoc, BOOL bMakeVisible = TRUE );
				void		UpdateInstanceMap( CMapDoc *pInstanceMapDoc );
};


void AppRegisterPostInitFn( void (*)() );
void AppRegisterMessageLoopFn( void (*)() );
void AppRegisterMessagePretranslateFn( void (*)( MSG * ) );
void AppRegisterPreShutdownFn( void (*)() );


class CHammer : public CWinApp, public CTier3AppSystem< IHammer >
{
	typedef CTier3AppSystem< IHammer > BaseClass;

public:
	CHammer(void);
	virtual ~CHammer(void);

	// Methods of IAppSystem
	virtual bool Connect( CreateInterfaceFn factory );
	virtual void Disconnect();
	virtual void *QueryInterface( const char *pInterfaceName );
	virtual InitReturnVal_t Init();
	virtual void Shutdown();

	// Methods of IHammer
	virtual bool HammerPreTranslateMessage( MSG * pMsg );
	virtual bool HammerIsIdleMessage( MSG * pMsg );
	virtual bool HammerOnIdle( long count );
	virtual void RunFrame();
	virtual int MainLoop();
	virtual const char *GetDefaultMod();
	virtual const char *GetDefaultGame();
	virtual RequestRetval_t RequestNewConfig();
	virtual const char *GetDefaultModFullPath();
	virtual bool InitSessionGameConfig(const char *szGame);

	virtual BOOL PreTranslateMessage(MSG *pMsg);

	// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CHammer)
	public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual CDocument* OpenDocumentFile(LPCTSTR lpszFileName);
	virtual BOOL OnIdle(LONG lCount);
	virtual int Run(void);
	//}}AFX_VIRTUAL

	void GetDirectory(DirIndex_t dir, char *p);
	void SetDirectory(DirIndex_t dir, const char *p);

	COLORREF GetProfileColor(const char *pszSection, const char *pszKey, int r, int g, int b);

	void OnActivateApp(bool bActive);
	bool IsActiveApp();

	void BeginImportWCSettings();
	void BeginImportVHESettings();
	void EndImportSettings();

	void BeginClosing();
	bool IsClosing();

	void Enable3DRender(bool bEnable);
	
	void ReleaseVideoMemory();
	void SuppressVideoAllocation( bool bSuppress );
	bool CanAllocateVideo() const;

	void Help(const char *pszTopic);

	CGameConfig *PromptForGameConfig();

	void OpenURL(const char *pszURL, HWND hwnd);
	void OpenURL(UINT nID, HWND hwnd);
	// list of "command arrays" for compiling files:
	CTypedPtrArray<CPtrArray,CCommandSequence*> m_CmdSequences;
	void SaveSequences();
	void LoadSequences();

	void Autosave();
	void LoadLastGoodSave();
	void ResetAutosaveTimer();
	bool VerifyAutosaveDirectory( char *szAutosaveDir = 0 ) const;	
	int GetNextAutosaveNumber( CUtlMap<FILETIME, WIN32_FIND_DATA, int> *pFileMap, DWORD *pdwTotalDirSize, const CString * ) const;

	// When in lighting preview, it will avoid rendering frames.
	// This forces it to render the next frame.
	void SetForceRenderNextFrame();
	bool GetForceRenderNextFrame();

	static void SetIsNewDocumentVisible( bool bIsVisible );
	static bool IsNewDocumentVisible( void );

	CHammerDocTemplate *pMapDocTemplate;
	CHammerDocTemplate *pManifestDocTemplate;

	//{{AFX_MSG(CHammer)
	afx_msg void OnAppAbout();
	afx_msg void OnFileOpen();
	afx_msg void OnFileNew();
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()

protected:

	// These execute inside a minidump handler.
	static int StaticHammerInternalInit( void *pParam );
	InitReturnVal_t HammerInternalInit();

	static int StaticInternalMainLoop( void *pParam );
	int InternalMainLoop();

	static bool	m_bIsNewDocumentVisible;

	// Check for 16-bit color or higher.
	bool Check16BitColor();

	void UpdateLighting(CMapDoc *pDoc);

	bool m_bClosing;					// The user has initiated app shutdown.
	bool m_bActiveApp;
	bool m_SuppressVideoAllocation;

	bool m_bForceRenderNextFrame;

	char m_szAppDir[MAX_PATH];
	char m_szAutosaveDir[MAX_PATH];
};


#define APP()		((CHammer *)AfxGetApp())


//-----------------------------------------------------------------------------
// Global interfaces...
//-----------------------------------------------------------------------------
extern IBaseFileSystem	*g_pFileSystem;
extern IEngineAPI	*g_pEngineAPI;
extern CreateInterfaceFn g_Factory;
bool IsRunningInEngine();

// event update system - lets you check for events such as gemoetry modification for updating stuff.
void SignalUpdate(int ev);									// EVTYPE_xx
int GetUpdateCounter(int ev);									// return timestamp
float GetUpdateTime(int ev);									// return floating point time event was signalled
void SignalGlobalUpdate(void);								// flag ALL events, such as on map load

#define EVTYPE_FACE_CHANGED 0
#define EVTYPE_LIGHTING_CHANGED 1
#define EVTYPE_BITMAP_RECEIVED_FROM_LPREVIEW 2

extern bool g_bHDR;											// should we act like we're in hdr mode?
extern int g_nBitmapGenerationCounter;


#endif // HAMMER_H
