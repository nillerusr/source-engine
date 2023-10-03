//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VTRANSITIONSCREEN_H__
#define __VTRANSITIONSCREEN_H__

#include "basemodui.h"

namespace BaseModUI 
{

class CTransitionScreen : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( CTransitionScreen, CBaseModFrame );

public:
	CTransitionScreen( vgui::Panel *parent, const char *panelName );
	~CTransitionScreen();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void PaintBackground();

	// returns true when transition screen has finalized its presentation
	bool IsTransitionComplete();

private:
	int			m_iImageID;
	float		m_flTransitionStartTime;
	vgui::HFont	m_hFont;
	bool		m_bComplete;
};

};

#endif // __VTRANSITIONSCREEN_H__