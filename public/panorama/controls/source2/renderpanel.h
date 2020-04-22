//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef PANORAMA_RENDERPANEL_H
#define PANORAMA_RENDERPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "panorama/controls/panel2d.h"
#include "materialsystem2/imaterialsystem2utils.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Render panel, to use for raw source2 render operations directly in render thread
//-----------------------------------------------------------------------------
class CRenderPanel : public CPanel2D
{
	DECLARE_PANEL2D( CRenderPanel, CPanel2D );

public:
	CRenderPanel( CPanel2D *parent, const char * pchPanelID );
	virtual ~CRenderPanel();

	void SetRenderThreadCallback( CRenderThreadCallback *pRenderCallback );

	// Override and make return true if you need to paint every single frame, and will not manually call SetRepaint when you want to repaint
	virtual bool BShouldAlwaysRepaint() { return true; }

protected:
	// Override of Panel2D paint
	virtual void Paint() OVERRIDE;

private:
	CRenderThreadCallback *m_pRenderCallback;
};

} // namespace panorama

#endif // PANORAMA_RENDERPANEL_H
