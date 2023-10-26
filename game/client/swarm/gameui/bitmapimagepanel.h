//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef BITMAPIMAGEPANEL_H
#define BITMAPIMAGEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>

class CBitmapImagePanel : public vgui::Panel
{
public:
	CBitmapImagePanel( vgui::Panel *parent, char const *panelName, char const *filename = NULL );

	virtual void	PaintBackground();

	virtual void	setTexture( char const *filename );

	virtual void	forceReload( void );

private:
	typedef vgui::Panel BaseClass;

	void			forceUpload();

	bool			m_bUploaded;
	int				m_nTextureId;
	char			m_szTexture[ 128 ];
};

#endif // BITMAPIMAGEPANEL_H
