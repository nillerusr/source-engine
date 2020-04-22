//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef DODTEXTWINDOW_H
#define DODTEXTWINDOW_H
#ifdef _WIN32
#pragma once
#endif

#include "vguitextwindow.h"
#include "dodmenubackground.h"

#include <vgui_controls/Panel.h>

using namespace vgui;

//-----------------------------------------------------------------------------
// Purpose: displays the MOTD
//-----------------------------------------------------------------------------

class CDODTextWindow : public CTextWindow
{
private:
	DECLARE_CLASS_SIMPLE( CDODTextWindow, CTextWindow );

public:
	CDODTextWindow(IViewPort *pViewPort);
	virtual ~CDODTextWindow();

	virtual void Update();
	virtual void SetVisible(bool state);
	virtual void ShowPanel( bool bShow );
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual Panel *CreateControlByName( const char *controlName );

protected:
	ButtonCode_t m_iScoreBoardKey;

public:
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	CDODMenuBackground *m_pBackground;
};


#endif // DODTEXTWINDOW_H
