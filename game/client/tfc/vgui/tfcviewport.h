//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TFCVIEWPORT_H
#define TFCVIEWPORT_H


#include "tfc_shareddefs.h"
#include "baseviewport.h"


using namespace vgui;

namespace vgui 
{
	class Panel;
}

class TFCViewport : public CBaseViewport
{

private:
	DECLARE_CLASS_SIMPLE( TFCViewport, CBaseViewport );

public:

	IViewPortPanel* CreatePanelByName(const char *szPanelName);
	void CreateDefaultPanels( void );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
		
	int GetDeathMessageStartHeight( void );
};


#endif // TFCViewport_H
