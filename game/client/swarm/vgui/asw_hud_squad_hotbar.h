#ifndef _INCLUDED_ASW_HUD_SQUAD_HOTBAR_H
#define _INCLUDED_ASW_HUD_SQUAD_HOTBAR_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/Panel.h>
#include "asw_hudelement.h"

#define ASW_MAX_ITEM_USE_ORDERS 6

class CASW_Hotbar_Entry;
class C_ASW_Marine;
namespace vgui
{
	class ImagePanel;
	class Label;
};

// shows weapons held by the current marine
class CASW_Hud_Squad_Hotbar : public CASW_HudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_Hud_Squad_Hotbar, vgui::Panel );

public:
	CASW_Hud_Squad_Hotbar( const char *pElementName );
	~CASW_Hud_Squad_Hotbar();

	void Init( void );
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme);
	virtual void PerformLayout();
	virtual bool ShouldDraw( void );
	virtual void OnThink();

	void UpdateList();
	void ActivateHotbarItem( int i );

	int GetHotBarSlot( const char *szWeaponName );

	//void StopItemFX( C_ASW_Marine *pMarine, float flDeleteTime = -1, bool bFailed = false );
	// Handler for our message
	//void MsgFunc_ASWOrderUseItemFX( bf_read &msg );
	//void MsgFunc_ASWOrderStopItemFX( bf_read &msg );

	int m_iNumMarines;

protected:
	CUtlVector<CASW_Hotbar_Entry*> m_pEntries;
	//CUtlReference<CNewParticleEffect> m_pOrderEffect[ASW_MAX_ITEM_USE_ORDERS];

	struct HotbarOrderEffects_t
	{
		int									iEffectID;  // this is the marine's entindex right now
		CUtlReference<CNewParticleEffect>	pEffect;
		float								flRemoveAtTime;

		HotbarOrderEffects_t()
		{
			iEffectID = -1;
			pEffect = NULL;
			flRemoveAtTime = -1;
		}
	};

	typedef CUtlFixedLinkedList<HotbarOrderEffects_t> HotbarOrderEffectsList_t;
	HotbarOrderEffectsList_t m_hHotbarOrderEffects;
};	


// individual entry in the list
class CASW_Hotbar_Entry : public vgui::Panel
{
public:
	DECLARE_CLASS_SIMPLE( CASW_Hotbar_Entry, vgui::Panel );

	CASW_Hotbar_Entry( vgui::Panel *pParent, const char *pElementName );
	virtual ~CASW_Hotbar_Entry();

	void SetDetails( C_ASW_Marine *pMarine, int iInventoryIndex );

	virtual void PerformLayout();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	void UpdateImage();
	void ClearImage();
	void ShowTooltip();
	void ActivateItem();

	// details for this entry
	CHandle<C_ASW_Marine> m_hMarine;
	int m_iInventoryIndex;
	int m_iHotKeyIndex;

	vgui::ImagePanel *m_pWeaponImage;
	vgui::Label *m_pMarineNameLabel;
	vgui::Label *m_pKeybindLabel;
	vgui::Label *m_pQuantityLabel;
	bool m_bMouseOver;
};

//-----------------------------------------------------------------------------
//	CASWSelectOverlayPulse
// 
//	creates a highlight pulse over the panel of an offhand item that has just been used
//
//-----------------------------------------------------------------------------
class CASWSelectOverlayPulse : public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASWSelectOverlayPulse, vgui::Label );
public:
	CASWSelectOverlayPulse( vgui::Panel* pParent );
	~CASWSelectOverlayPulse( void );

	void	Initialize();
	void	OnThink();


private:
	vgui::ImagePanel	*m_pOverlayLabel;
	vgui::Panel			*m_pParent;

	float				m_fStartTime;
	float				m_fScale;
	bool				m_bStarted;

};


#endif // _INCLUDED_ASW_HUD_SQUAD_HOTBAR_H