//========= Copyright © 1996-2002, Valve LLC, All rights reserved. ============
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef TF_STATSSUMMARY_H
#define TF_STATSSUMMARY_H
#ifdef _WIN32
#pragma once
#endif

#include "GameEventListener.h"

class LocationDetailsPanel;
namespace vgui
{
	class Label;
};

class LoadingMissionDetailsPanel: public vgui::EditablePanel
{
private:
	DECLARE_CLASS_SIMPLE( LoadingMissionDetailsPanel, vgui::EditablePanel );

public:
	LoadingMissionDetailsPanel( vgui::Panel *parent, const char *name );
	virtual ~LoadingMissionDetailsPanel();

	virtual void ApplySchemeSettings(vgui::IScheme *scheme);
	void SetGenerationOptions( KeyValues *pKeys );

	vgui::Label *m_pLocationNameLabel;
	vgui::Label *m_pLocationDescriptionLabel;

	KeyValues *m_pGenerationOptions;
};

class CASW_Loading_Panel : public vgui::EditablePanel, public CGameEventListener
{
private:
	DECLARE_CLASS_SIMPLE( CASW_Loading_Panel, vgui::EditablePanel );

public:
	CASW_Loading_Panel();	 
	CASW_Loading_Panel( vgui::Panel *parent );
	virtual ~CASW_Loading_Panel();	 

	void Init( void );

	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);
	virtual void PerformLayout();
	virtual void FireGameEvent( IGameEvent *event );

	void SetLoadingMapName( const char *szMapName );
	void SetGenerationOptions( KeyValues *pKeys );

private:
	void GetLoadingScreenSize(int &w, int &t, int &xoffset);

	vgui::ImagePanel* m_pBackdrop;
	vgui::Panel* m_pBlackBar[2];
	char m_szLoadingPic[256];
	LoadingMissionDetailsPanel *m_pDetailsPanel;
};


CASW_Loading_Panel *GASWLoadingPanel();
void DestroyASWLoadingPanel();

#endif // TF_STATSSUMMARY_H
