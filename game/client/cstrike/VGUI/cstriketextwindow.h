//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CSTEXTWINDOW_H
#define CSTEXTWINDOW_H
#ifdef _WIN32
#pragma once
#endif

#include "vguitextwindow.h"

//-----------------------------------------------------------------------------
// Purpose: displays the MOTD
//-----------------------------------------------------------------------------

class CCSTextWindow : public CTextWindow
{
private:
	DECLARE_CLASS_SIMPLE( CCSTextWindow, CTextWindow );

public:
	CCSTextWindow(IViewPort *pViewPort);
	virtual ~CCSTextWindow();

	virtual void Update();
	virtual void SetVisible(bool state);
	virtual void ShowPanel( bool bShow );
	virtual void OnKeyCodePressed(vgui::KeyCode code);

protected:
	ButtonCode_t m_iScoreBoardKey;

	// Background panel -------------------------------------------------------

public:
	virtual void PaintBackground();
	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	bool m_backgroundLayoutFinished;

	// End background panel ---------------------------------------------------
};


#endif // CSTEXTWINDOW_H
