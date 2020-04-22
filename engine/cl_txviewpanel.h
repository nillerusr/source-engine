//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#ifndef CL_TXVIEWPANEL_H
#define CL_TXVIEWPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Frame.h>


namespace vgui
{
	class Button;
	class CheckButton;
	class Label;
	class ProgressBar;
	class FileOpenDialog;
	class Slider;
	class ListViewPanel;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class TxViewPanel : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( TxViewPanel, vgui::Frame );

public:
	TxViewPanel( vgui::Panel *parent );
	~TxViewPanel();

	virtual void OnTick();

	// Command issued
	virtual void OnCommand( const char *command );
	virtual void OnMessage( const KeyValues *params,  vgui::VPANEL fromPanel );

	static	void	Install( vgui::Panel *parent );

protected:
	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );

protected:
	vgui::Button		*m_pRefresh;
	vgui::ListViewPanel	*m_pView;
};

extern TxViewPanel *g_pTxViewPanel;

#endif // #ifndef CL_TXVIEWPANEL_H
