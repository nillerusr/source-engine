//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//

#if !defined( PLAYEROVERLAYCLASS_H )
#define PLAYEROVERLAYCLASS_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_basepanel.h"

class BitmapImage;
class KeyValues;
class CHudPlayerOverlay;

class CHudPlayerOverlayClass : public CBasePanel
{
public:
	DECLARE_CLASS( CHudPlayerOverlayClass, CBasePanel );

	CHudPlayerOverlayClass( CHudPlayerOverlay *baseOverlay );
	virtual ~CHudPlayerOverlayClass( void );

	bool Init( KeyValues* pKeyValues );

	void SetColor( int r, int g, int b, int a );
	void SetImage( BitmapImage *pImage );
	void SetTeamAndClass( int team, int playerclass );

	// Keeps the image size correct
	virtual void SetSize( int w, int h );

	virtual void Paint( void );

	virtual void OnCursorEntered();
	virtual void OnCursorExited();

private:
	class CMapClassColors
	{
	public:
		CMapClassColors() : m_pClassImage(0) {}

		BitmapImage	*m_pClassImage;
		Color		m_clrClass;
	};

	// Initialize class info only once
	static bool InitClassInfo( KeyValues* pKeyValues );
	static bool ParseTeamClassInfo( KeyValues *pClassIcons, const char *classname, CMapClassColors *pClassColors );
	static bool ParseTeamClassIcons( CMapClassColors *pT, KeyValues *pTeam );

	static CMapClassColors** s_ppClassInfo;

	int		m_r, m_g, m_b, m_a;
	BitmapImage *m_pImage;

	Color m_fgColor;
	Color m_bgColor;

	CHudPlayerOverlay *m_pBaseOverlay;

	friend class CCleanupPlayerOverlayClass;
};

#endif // PLAYEROVERLAYCLASS_H