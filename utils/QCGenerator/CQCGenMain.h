//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef MDRIPPERMAIN_H
#define MDRIPPERMAIN_H
#ifdef _WIN32
#pragma once
#endif

#include "matsys_controls/QCGenerator.h"
#include <vgui_controls/Frame.h>
#include <FileSystem.h>
#include "vgui/IScheme.h"

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: Main dialog for media browser
//-----------------------------------------------------------------------------
class CQCGenMain : public Frame
{
	DECLARE_CLASS_SIMPLE( CQCGenMain, Frame );

public:

    CQCGenerator *m_pQCGenerator;
	CQCGenMain(Panel *parent, const char *name, const char *pszMDParam, const char *pszCollisionParam );
	virtual ~CQCGenMain();
		
protected:
	
	virtual void OnClose();
	virtual void OnCommand( const char *command );

private:

	void		SetGlobalConfig( const char *modDir );

	vgui::ComboBox	*m_pConfigCombo;
	bool			m_bChanged;
	vgui::MenuBar	*m_pMenuBar;
	vgui::Panel		*m_pClientArea;
	
	MESSAGE_FUNC( OnRefresh, "refresh" );	
};


extern CQCGenMain *g_pCQCGenMain;


#endif // MDRIPPERMAIN_H
