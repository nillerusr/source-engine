//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#if !defined( PLAYEROLVERLAYSELECTED_H )
#define PLAYEROLVERLAYSELECTED_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_basepanel.h"

class BitmapImage;
class KeyValues;
class CHudPlayerOverlay;
class CHudPlayerOverlaySelected : public CBasePanel
{
public:
	DECLARE_CLASS( CHudPlayerOverlaySelected, CBasePanel );

	CHudPlayerOverlaySelected( CHudPlayerOverlay *baseOverlay );
	virtual ~CHudPlayerOverlaySelected( void );

	bool Init( KeyValues* pInitData );

	void SetImages( BitmapImage *pImage[4] );

	virtual void Paint( void );

	virtual void OnCursorEntered();
	virtual void OnCursorExited();


private:
	BitmapImage *m_pImages[4];

	Color m_fgColor;
	Color m_bgColor;
	CHudPlayerOverlay *m_pBaseOverlay;
};

#endif // PLAYEROLVERLAYSELECTED_H