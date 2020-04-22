//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef CLIENTMODE_TFBASE_H
#define CLIENTMODE_TFBASE_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"

class ConVar;
class CMinimapPanel;

namespace vgui
{
	typedef unsigned long HScheme;
}


// This class defines the base clientmode behavior in TF. Classes that derive from
// it and override functions should forward them to the base class.
class ClientModeTFBase : public ClientModeShared
{
	DECLARE_CLASS( ClientModeTFBase, ClientModeShared );
public:
					ClientModeTFBase( void );
	virtual			~ClientModeTFBase( void );

	virtual void	Init();
	virtual void	Shutdown();

	virtual void	Enable();
	
	virtual void	PreRender( CViewSetup *pSetup );
	virtual void	PostRender();
	virtual	void	Update();

	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );

	// Input
	virtual int		KeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );

	virtual CMinimapPanel	*GetMinimap( void );

	virtual vgui::Panel *GetMinimapParent( void ) = 0;

private:
	void			Initialize( void );

	bool			m_bInitialized;

	ConVar			*m_pCVDrawFullSkybox;

	float			m_flOldDrawFullSkybox;

	static CMinimapPanel	*m_pMinimap;
};


extern vgui::HScheme g_hVGuiObjectScheme;

#endif // CLIENTMODE_TFBASE_H
