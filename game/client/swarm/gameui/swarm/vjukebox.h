//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VJUKEBOX_H__
#define __VJUKEBOX_H__

#include "basemodui.h"
#include "VGenericPanelList.h"
#include "vgui_controls/CvarToggleCheckButton.h"
#include "gameui_util.h"

class CNB_Button;
class CNB_Header_Footer;

namespace BaseModUI {

class AddonGenericPanelList;


class JukeboxListItem : public vgui::EditablePanel, IGenericPanelListItem
{
	DECLARE_CLASS_SIMPLE( JukeboxListItem, vgui::EditablePanel );

public:
	JukeboxListItem(vgui::Panel *parent, const char *panelName);

	void SetTrackIndex( int nTrackIndex );
	int GetTrackIndex();

	// Inherited from IGenericPanelListItem
	virtual bool IsLabel() { return false; }
	void OnMousePressed( vgui::MouseCode code );
	virtual void OnMessage(const KeyValues *params, vgui::VPANEL ifromPanel);
	virtual void Paint();

protected:
	virtual void ApplySchemeSettings(vgui::IScheme *pScheme);

private:
	vgui::Label* m_LblName;
	vgui::IBorder* m_DefaultBorder;
	vgui::IBorder* m_FocusBorder;
	vgui::CheckButton* m_BtnEnabled;
	bool m_bCurrentlySelected;
	vgui::HFont	m_hTextFont;
	int m_nTrackIndex;

	CPanelAnimationVarAliasType( float, m_flDetailsExtraHeight, "DetailsExtraHeight", "0", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flDetailsRowHeight, "DetailsRowHeight", "0", "proportional_float" );
};

class VJukebox : public CBaseModFrame
{
	DECLARE_CLASS_SIMPLE( VJukebox, CBaseModFrame );

public:
	VJukebox(vgui::Panel *parent, const char *panelName);
	~VJukebox();
	void Activate();
	void PaintBackground( void );
	void OnCommand(const char *command);
	virtual void OnMessage(const KeyValues *params, vgui::VPANEL ifromPanel);

	virtual void OnThink();
	virtual void PerformLayout();

	void UpdateTrackList();
	MESSAGE_FUNC_PARAMS( OnMusicItemSelected, "MusicImportComplete", pInfo );

protected:
	void ApplySchemeSettings(vgui::IScheme *pScheme);
	bool LoadAddonListFile( KeyValues *&pAddons );
	bool LoadAddonInfoFile( KeyValues *&pAddonInfo, const char *pcAddonDir, bool bIsVPK );
	void UpdateButtonsForTrack( int nIndex );
	void GetAddonImage( const char *pcAddonDir, char *pcImagePath, int nImagePathSize, bool bIsVPK );
	void ExtractAddonMetadata( const char *pcAddonDir );

private:
	void UpdateFooter( void );

	//GenericPanelList* m_GplTrackList;
	vgui::ListPanel *m_GplTrackList;
	vgui::Label *m_LblName;
	vgui::Label *m_LblNoTracks;
	CNB_Header_Footer *m_pHeaderFooter;
	CNB_Button *m_pAddTrackButton;
	CNB_Button *m_pRemoveTrackButton;
};

};

#endif
