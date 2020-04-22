//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef VGUI_TEXTUREBUDGETPANEL_H
#define VGUI_TEXTUREBUDGETPANEL_H

#ifdef _WIN32
#pragma once
#endif

#include "vgui/vgui_basebudgetpanel.h"
#include "tier0/vprof.h"

#ifdef VPROF_ENABLED

class CTextureBudgetPanel : public CBaseBudgetPanel
{
public:
	typedef CBaseBudgetPanel BaseClass;

	CTextureBudgetPanel( vgui::Panel *pParent, const char *pElementName );
	~CTextureBudgetPanel();

	void OnCVarStateChanged();


// CBaseBudgetPanel overrides.
public:
	virtual void SetTimeLabelText();
	virtual void SetHistoryLabelText();
	virtual void ResetAll();


// VGUI overrides.
public:
	virtual void OnTick( void );
	virtual void Paint();
	virtual void PerformLayout();


// Internal.
private:
	
	void SnapshotTextureHistory();
	void SendConfigDataToBase();
	CounterGroup_t GetCurrentCounterGroup() const;
	CON_COMMAND_MEMBER_F( CTextureBudgetPanel, "showbudget_texture_global_dumpstats", DumpGlobalTextureStats, "Dump all items in +showbudget_texture_global in a text form", 0 );

private:

	vgui::Label *m_pModeLabel;	
	int m_LastCounterGroup;
	int m_MaxValue;
	int m_SumOfValues;
};

#endif // VPROF_ENABLED

#endif // VGUI_TEXTUREBUDGETPANEL_H
