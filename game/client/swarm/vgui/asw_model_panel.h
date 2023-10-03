#ifndef ASW_MODEL_PANEL_H
#define ASW_MODEL_PANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "basemodel_panel.h"

class CStudioHdr;

//-----------------------------------------------------------------------------
// Base class for all Alien Swarm model panels
//-----------------------------------------------------------------------------
class CASW_Model_Panel : public CBaseModelPanel
{
	DECLARE_CLASS_SIMPLE( CASW_Model_Panel, CBaseModelPanel );
public:
	CASW_Model_Panel( vgui::Panel *parent, const char *name );
	virtual ~CASW_Model_Panel();

	virtual void Paint();
	virtual void SetupLights() { }

	// function to set up scene sets
	void SetupCustomLights( Color cAmbient, Color cKey, float fKeyBoost, Color cRim, float fRimBoost );

	void SetBodygroup( int iGroup, int iValue );
	int FindBodygroupByName( const char *name );
	int GetNumBodyGroups();
	CStudioHdr *GetModelPtr();

	// TODO: Add support for flex weights/rules to this panel

	bool m_bShouldPaint;

private:
	CStudioHdr* m_pStudioHdr;
};


#endif // ASW_MODEL_PANEL_H
