//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#ifndef __VMULTIPLAYER_H__
#define __VMULTIPLAYER_H__


#include "basemodui.h"
#include "VFlyoutMenu.h"


#define MAX_SPRAYPAINT_LOGOS 17


struct SprayPaintLogo_t
{
	char m_szFilename[ MAX_PATH ];
	bool m_bCustom;
};

class CNB_Header_Footer;

namespace BaseModUI {

class DropDownMenu;
class SliderControl;
class BaseModHybridButton;

class Multiplayer : public CBaseModFrame, public FlyoutMenuListener
{
	DECLARE_CLASS_SIMPLE( Multiplayer, CBaseModFrame );

public:
	Multiplayer(vgui::Panel *parent, const char *panelName);
	~Multiplayer();

	//FloutMenuListener
	virtual void OnNotifyChildFocus( vgui::Panel* child );
	virtual void OnFlyoutMenuClose( vgui::Panel* flyTo );
	virtual void OnFlyoutMenuCancelled();

	virtual void PerformLayout();

	Panel* NavigateBack();

protected:
	virtual void Activate();
	virtual void OnThink();
	virtual void PaintBackground();
	virtual void ApplySchemeSettings( vgui::IScheme* pScheme );
	virtual void OnKeyCodePressed(vgui::KeyCode code);
	virtual void OnCommand( const char *command );

	void InitLogoList();

	MESSAGE_FUNC_CHARPTR( OnFileSelected, "FileSelected", fullpath );
	MESSAGE_FUNC( OnFileSelectionCancelled, "FileSelectionCancelled" );

private:
	void				UpdateFooter( bool bEnableCloud );

	int					m_nNumSpraypaintLogos;
	SprayPaintLogo_t	m_nSpraypaint[ MAX_SPRAYPAINT_LOGOS ];

	DropDownMenu		*m_drpAllowLanGames;
	DropDownMenu		*m_drpAllowCustomContent;
	DropDownMenu		*m_drpColorBlind;
	DropDownMenu		*m_drpGameInstructor;
	DropDownMenu		*m_drpAllowFreeLook;
	DropDownMenu		*m_drpSpraypaint;
	DropDownMenu		*m_drpGore;

	vgui::ImagePanel	*m_pSprayLogo;

	BaseModHybridButton	*m_btnBrowseSpraypaint;
	BaseModHybridButton	*m_btnCancel;

	vgui::FileOpenDialog *m_hImportSprayDialog;
	vgui::FileOpenDialog *m_hSelectSprayDialog;

	CNB_Header_Footer *m_pHeaderFooter;
};

};

#endif // __VMULTIPLAYER_H__