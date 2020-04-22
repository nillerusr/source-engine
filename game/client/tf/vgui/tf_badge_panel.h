//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#ifndef TF_BADGE_PANEL_H
#define TF_BADGE_PANEL_H
#ifdef _WIN32
#pragma once
#endif

#include "vgui_controls/EditablePanel.h"
#include "tf_match_description.h"

class CTFBadgePanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTFBadgePanel, vgui::EditablePanel );
public:
	CTFBadgePanel( vgui::Panel *pParent, const char *pName );

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );

	void SetupBadge( const IProgressionDesc* pProgress, const LevelInfo_t& levelInfo );
	void SetupBadge( const IProgressionDesc* pProgress, const CSteamID& steamID );

private:
	class CModelImagePanel *m_pBadgePanel;
	uint32 m_nPrevLevel;
};


#endif // TF_BADGE_PANEL_H
