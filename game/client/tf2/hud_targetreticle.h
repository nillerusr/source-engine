//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Targeting reticle
//
// $NoKeywords: $
//=============================================================================//

#ifndef TARGETRETICLE_H
#define TARGETRETICLE_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include <vgui/Cursor.h>

class IMaterial;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CTargetReticle : public vgui::Panel
{
	typedef vgui::Panel BaseClass;
public:
	CTargetReticle( void );
	~CTargetReticle();

	void			Update();

	// vgui::Panel overrides.
	virtual void	Paint( void );

	void			Init( C_BaseEntity *pEntity, const char *sName );
	C_BaseEntity*	GetTarget( void );


protected:

	EHANDLE			m_hTargetEntity;
	vgui::Label		*m_pTargetLabel;
	
	int				m_iReticleId;
	int				m_iReticleLeftId;	// When it's hanging off the edge of the screen.
	int				m_iReticleRightId;

	int				m_iRenderTextureId;

	vgui::HCursor				m_CursorNone;
};

#endif // TARGETRETICLE_H
