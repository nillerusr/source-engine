//========= Copyright Valve Corporation, All rights reserved. ============//
#include <dbghelp.h>
#include <dbgeng.h>

#include "mdmpRipper.h"
#include "vgui_controls/MessageMap.h"
#include "vgui_controls/MenuBar.h"
#include "vgui_controls/Menu.h"
#include "tier1/KeyValues.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Frame.h"
#include "CMiniDumpObject.h"
#include "CDbgOutput.h"

#ifndef MDMODULEPANEL_H
#define MDMODULEPANEL_H

#ifdef _WIN32
#pragma once
#endif

using namespace vgui;


class CMDModulePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CMDModulePanel, vgui::EditablePanel );
public:
	CMDModulePanel( vgui::Panel *pParent, const char *pName );
	~CMDModulePanel( );

	virtual void OnCommand( const char *pCommand );
	virtual void Close();
	void OnKeyCodeTyped( KeyCode code );
	virtual void Create( const char *filename );
	virtual void Create( CUtlVector<CMiniDumpObject *> *ptr );

	void UpdateKnownDB( char *type );
	void ModuleLookUp();

	static DWORD WINAPI StaticAnalyzeThread( LPVOID lParam );

private:
	MESSAGE_FUNC_PARAMS( OnCompare, "compare", data );
	MESSAGE_FUNC_INT_CHARPTR( OnDbgOutput, "DebugOutput", iMask, pszDebugText );

	void LoadKnownModules();

	void InitializeDebugEngine( void );
	void ReleaseDebugEngine( void );
	void AnalyzeDumpFile( const char *pszDumpFile );
	DWORD AnalyzeThread( void );
	
	vgui::ListPanel *m_pTokenList;
	vgui::RichText  *m_pAnalyzeText;
	CUtlVector<CMiniDumpObject *> m_MiniDumpList;
	CUtlVector<module> m_knownModuleList;

	HANDLE				m_hThread;
	IDebugClient		*m_pDbgClient;
	IDebugControl		*m_pDbgControl;
	IDebugSymbols2		*m_pDbgSymbols;
	CDbgOutput			m_cDbgOutput;
};


#endif // MDMODULEPANEL_H