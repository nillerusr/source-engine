//========= Copyright 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef VGUI_BITMAPIMAGE_H
#define VGUI_BITMAPIMAGE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Image.h>

namespace vgui
{
	class Panel;
}

class KeyValues;

//-----------------------------------------------------------------------------
// Purpose: Bitmap image
//-----------------------------------------------------------------------------
class BitmapImage : public vgui::Image
{
public:
	BitmapImage();
	BitmapImage( vgui::VPANEL pPanelSize, const char *pFileName );
	bool Init( vgui::VPANEL pParent, const char *pFileName );
	bool Init( vgui::VPANEL pParent, KeyValues* pInitData );

	/* FIXME: Bleah!!! Don't want two different KeyValues
	bool Init( vgui::VPANEL pParent, KeyValues* pInitData ); */

	void DoPaint( vgui::VPANEL panel, float yaw = 0, float flAlphaModulate = 1.0f );
	void DoPaint( int x, int y, int w, int h, float yaw = 0, float flAlphaModulate = 1.0f );
	void Paint( );
	void SetColor( const Color& clr );
	Color GetColor( );
	void GetColor( int& r,int& g,int& b,int& a );
	void GetSize( int& wide, int& tall );
	void SetPos( int x, int y );
	void SetRenderSize( int x, int y );

	void SetImageFile( const char *newImage );

	// Pass NULL in to use the size set in SetSize
	// otherwise it'll use the size of the panel
	void UsePanelRenderSize( vgui::VPANEL pPanel );
	vgui::VPANEL GetRenderSizePanel( void );

	void SetViewport( bool use, float left, float top, float right, float bottom );

private:
	int				m_nTextureId;
	Color		m_clr;
	int				m_pos[2];
	int				m_Size[2];
	vgui::VPANEL	m_pPanelSize;

	bool			m_bUseViewport;
	float			m_rgViewport[ 4 ];
};


//-----------------------------------------------------------------------------
// Helper method to initialize a bitmap image from KeyValues data..
// KeyValues contains the bitmap data. pSectionName, if it exists,
// indicates which subsection of pInitData should be looked at to get at the
// image data. The parent argument specifies which panel to use as parent,
// and the final argument is the bitmap image to initialize.
// The function returns true if it succeeded.
//
// NOTE: This function looks for the key values 'material' and 'color'
// and uses them to set up the material + modulation color of the image
//-----------------------------------------------------------------------------
bool InitializeImage( KeyValues *pInitData, const char* pSectionName, vgui::Panel *pParent, BitmapImage* pBitmapImage );

/* FIXME: How sad. We need to make KeyValues + vgui::KeyValues be the same. Bleah
bool InitializeImage( KeyValues *pInitData, const char* pSectionName, vgui::Panel *pParent, BitmapImage* pBitmapImage ); */


#endif // VGUI_BITMAPIMAGE_H
