//====== Copyright © 1996-2008, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef MISSIONCHOOSER_TGAIMAGEPANEL_H
#define MISSIONCHOOSER_TGAIMAGEPANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/Panel.h"

//-----------------------------------------------------------------------------
// Purpose: Displays a tga image
//   Supports using DrawSetTextureRGBA when in-game.
//   Multiple instances of this panel using the same TGA name will share the same loaded image
//-----------------------------------------------------------------------------
class CMissionChooserTGAImagePanel : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CMissionChooserTGAImagePanel, vgui::Panel );

public:
	CMissionChooserTGAImagePanel( vgui::Panel *parent, const char *name );
	~CMissionChooserTGAImagePanel();

	static void ClearImageCache();

	void SetTGA( const char *filename, char const *pPathID = 0 );
	virtual void Paint();

private:

	// Index into a private cache where the texture data can be found
	int m_nLoadedTextureIndex;
	// Every time the cache is cleared, the cache's generation index is incremented.
	// This value is used to indicate whether this TGA needs recaching.
	int m_nCacheGenerationIndex;

	char m_szTGAName[256];
};

#endif // MISSIONCHOOSER_TGAIMAGEPANEL_H