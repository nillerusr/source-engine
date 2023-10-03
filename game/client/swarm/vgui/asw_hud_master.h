#ifndef _INCLUDED_ASW_HUD_MASTER_H
#define _INCLUDED_ASW_HUD_MASTER_H
#ifdef _WIN32
#pragma once
#endif

#include "asw_hudelement.h"
#include "asw_shareddefs.h"
#include <vgui_controls/Panel.h>
#include "asw_video.h"

class C_ASW_Marine;
class C_ASW_Marine_Resource;
class CASW_Marine_Profile;
class C_ASW_Player;
class C_ASW_Weapon;
class CASW_WeaponInfo;

struct HudSheetTexture_t
{
	float u;
	float v;
	float s;
	float t;
};

// ====================================
//  Macros for defining texture sheets
// ====================================

#define DECLARE_HUD_SHEET( name ) \
	int m_n##name##ID; \
	Vector2D m_vec##name##Size; \
	enum {

#define DECLARE_HUD_SHEET_UV( name ) \
	UV_##name

#define END_HUD_SHEET( name ) \
	NUM_##name##_UVS, \
	}; \
	HudSheetTexture_t m_##name##[ NUM_##name##_UVS ];


class HudSheet_t
{
public:
	HudSheet_t( int *pTextureID, HudSheetTexture_t *pTextureData, int nNumSubTextures, const char *pszTextureFile, Vector2D *pSize ) :
	  m_pSheetID( pTextureID ), m_pTextureData( pTextureData ), m_nNumSubTextures( nNumSubTextures ), m_pszTextureFile( pszTextureFile ),
	  m_pSize( pSize ) { }

	int *m_pSheetID;
	HudSheetTexture_t *m_pTextureData;
	int m_nNumSubTextures;
	const char *m_pszTextureFile;
	Vector2D *m_pSize;
};

#define ADD_HUD_SHEET( name, textureFile ) \
	m_HudSheets.AddToTail( HudSheet_t( &m_n##name##ID, &m_##name##[0], NUM_##name##_UVS, textureFile, &m_vec##name##Size ) );

#define HUD_UV_COORDS( sheet, full_texture_name ) \
	m_##sheet[ full_texture_name ].u, \
	m_##sheet[ full_texture_name ].v, \
	m_##sheet[ full_texture_name ].s, \
	m_##sheet[ full_texture_name ].t

#define MAX_SQUADMATE_HUD_POSITIONS 3

// ===========================================================================
//  This manually draws a number of HUD elements quickly using texture sheets
// ===========================================================================

class CASW_Hud_Master : public CASW_HudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CASW_Hud_Master, vgui::Panel );
public:
	CASW_Hud_Master( const char *pElementName );
	virtual ~CASW_Hud_Master();
	
	virtual void Init();
	virtual void VidInit();
	virtual bool ShouldDraw();

	void ActivateHotbarItem( int nSlot );

	int GetHotBarSlot( const char *szWeaponName );
	bool OwnsHotBarSlot( C_ASW_Player *pPlayer, int nSlot );

	void StopItemFX( C_ASW_Marine *pMarine, float flDeleteTime = -1, bool bFailed = false );
	// Handler for our message
	void MsgFunc_ASWOrderUseItemFX( bf_read &msg );
	void MsgFunc_ASWOrderStopItemFX( bf_read &msg );
	void MsgFunc_ASWInvalidDesination( bf_read &msg );

	int GetDeadIconTextureID();
	const HudSheetTexture_t* GetDeadIconUVs();
	
protected:
	virtual void ApplySchemeSettings( vgui::IScheme *scheme );
	virtual void Paint();
	virtual void OnThink();

	virtual bool LookupElementBounds( const char *elementName, int &x, int &y, int &wide, int &tall );


	void UpdatePortraitVideo();

	void PaintLocalMarinePortrait();
	void PaintSquadMemberStatus( int nPosition );
	void PaintLocalMarineInventory();
	void PaintSquadMatesInventory();
	void PaintFastReload();
	void PaintText();
	void PaintSquadMemberText( int nPosition );	

	int GetClassIcon( C_ASW_Marine_Resource *pMR );

	// textures

	DECLARE_HUD_SHEET( Sheet1 )
		DECLARE_HUD_SHEET_UV( hud_ammo_heal ),
		DECLARE_HUD_SHEET_UV( marine_health_circle_BG ),
		DECLARE_HUD_SHEET_UV( marine_health_circle_FG ),
		DECLARE_HUD_SHEET_UV( marine_health_circle_FG_subtract ),
		DECLARE_HUD_SHEET_UV( marine_health_circle_FG_infested ),
		DECLARE_HUD_SHEET_UV( marine_portrait_BG_cap ),
		DECLARE_HUD_SHEET_UV( marine_portrait_BG_circle ),
		DECLARE_HUD_SHEET_UV( marine_portrait_square ),

		DECLARE_HUD_SHEET_UV( NCOClassIcon ),
		DECLARE_HUD_SHEET_UV( SpecialWeaponsClassIcon ),
		DECLARE_HUD_SHEET_UV( MedicClassIcon ),
		DECLARE_HUD_SHEET_UV( TechClassIcon ),
		DECLARE_HUD_SHEET_UV( DeadIcon ),
		DECLARE_HUD_SHEET_UV( InfestedIcon ),

		DECLARE_HUD_SHEET_UV( team_health_bar_BG ),
		DECLARE_HUD_SHEET_UV( team_health_bar_FG ),
		DECLARE_HUD_SHEET_UV( team_health_bar_FG_subtract ),
		DECLARE_HUD_SHEET_UV( team_health_bar_FG_infested ),
		DECLARE_HUD_SHEET_UV( team_portrait_BG ),
	END_HUD_SHEET( Sheet1 );

	DECLARE_HUD_SHEET( Sheet_Stencil )
		DECLARE_HUD_SHEET_UV( hud_ammo_bullets ),
		DECLARE_HUD_SHEET_UV( hud_ammo_clip_empty ),
		DECLARE_HUD_SHEET_UV( hud_ammo_clip_full ),
		DECLARE_HUD_SHEET_UV( hud_ammo_grenade ),
		DECLARE_HUD_SHEET_UV( team_ammo_bar ),
	END_HUD_SHEET( Sheet_Stencil );

	// positioning

	CPanelAnimationVarAliasType( int, m_nMarinePortrait_x, "MarinePortrait_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_y, "MarinePortrait_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_face_x, "MarinePortrait_face_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_face_y, "MarinePortrait_face_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_face_radius, "MarinePortrait_face_radius", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_circle_bg_x, "MarinePortrait_circle_bg_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_circle_bg_y, "MarinePortrait_circle_bg_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_circle_bg_w, "MarinePortrait_circle_bg_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_circle_bg_t, "MarinePortrait_circle_bg_t", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_bar_bg_w, "MarinePortrait_bar_bg_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_class_icon_x, "MarinePortrait_class_icon_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_class_icon_y, "MarinePortrait_class_icon_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_class_icon_w, "MarinePortrait_class_icon_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_class_icon_t, "MarinePortrait_class_icon_t", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_weapon_name_x, "MarinePortrait_weapon_name_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_weapon_name_y, "MarinePortrait_weapon_name_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_low_ammo_x, "MarinePortrait_low_ammo_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_low_ammo_y, "MarinePortrait_low_ammo_y", "0", "proportional_ypos" );

	CPanelAnimationVarAliasType( int, m_nMarinePortrait_bullets_icon_x, "MarinePortrait_bullets_icon_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_bullets_icon_y, "MarinePortrait_bullets_icon_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_bullets_icon_w, "MarinePortrait_bullets_icon_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_bullets_icon_t, "MarinePortrait_bullets_icon_t", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_bullets_x, "MarinePortrait_bullets_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_bullets_y, "MarinePortrait_bullets_y", "0", "proportional_ypos" );

	CPanelAnimationVarAliasType( int, m_nMarinePortrait_grenades_icon_x, "MarinePortrait_grenades_icon_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_grenades_icon_y, "MarinePortrait_grenades_icon_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_grenades_icon_w, "MarinePortrait_grenades_icon_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_grenades_icon_t, "MarinePortrait_grenades_icon_t", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_grenades_x, "MarinePortrait_grenades_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_grenades_y, "MarinePortrait_grenades_y", "0", "proportional_ypos" );

	CPanelAnimationVarAliasType( int, m_nMarinePortrait_clips_x, "MarinePortrait_clips_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_clips_y, "MarinePortrait_clips_y", "0", "proportional_ypos" );	
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_clips_w, "MarinePortrait_clips_w", "0", "proportional_int" );	// width+height of one clip icon
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_clips_t, "MarinePortrait_clips_t", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nMarinePortrait_spacing, "MarinePortrait_clips_spacing", "0", "proportional_int" );

	CPanelAnimationVarAliasType( int, m_nPrimaryWeapon_x, "PrimaryWeapon_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nPrimaryWeapon_y, "PrimaryWeapon_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nSecondaryWeapon_x, "SecondaryWeapon_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nSecondaryWeapon_y, "SecondaryWeapon_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_x, "ExtraItem_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_y, "ExtraItem_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nWeapon_w, "Weapon_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nWeapon_t, "Weapon_t", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_w, "ExtraItem_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_t, "ExtraItem_t", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_hotkey_x, "ExtraItem_hotkey_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_hotkey_y, "ExtraItem_hotkey_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_quantity_x, "ExtraItem_quantity_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_quantity_y, "ExtraItem_quantity_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_battery_x, "ExtraItem_battery_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_battery_y, "ExtraItem_battery_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_battery_w, "ExtraItem_battery_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nExtraItem_battery_t, "ExtraItem_battery_t", "0", "proportional_int" );

	CPanelAnimationVarAliasType( int, m_nFastReload_x, "FastReload_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nFastReload_y, "FastReload_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nFastReload_w, "FastReload_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nFastReload_t, "FastReload_t", "0", "proportional_int" );

	CPanelAnimationVarAliasType( int, m_nSquadMates_x, "SquadMates_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nSquadMates_y, "SquadMates_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nSquadMates_spacing, "SquadMates_spacing", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_name_x, "SquadMate_name_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_name_y, "SquadMate_name_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_bg_x, "SquadMate_bg_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_bg_y, "SquadMate_bg_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_bg_w, "SquadMate_bg_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_bg_t, "SquadMate_bg_t", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_health_x, "SquadMate_health_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_health_y, "SquadMate_health_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_health_w, "SquadMate_health_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_health_t, "SquadMate_health_t", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_bullets_x, "SquadMate_bullets_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_bullets_y, "SquadMate_bullets_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_bullets_w, "SquadMate_bullets_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_bullets_t, "SquadMate_bullets_t", "0", "proportional_int" );	
	CPanelAnimationVarAliasType( int, m_nSquadMate_clips_x, "SquadMate_clips_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_clips_y, "SquadMate_clips_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_clips_spacing, "SquadMate_clips_spacing", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_clips_w, "SquadMate_clips_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_clips_t, "SquadMate_clips_t", "0", "proportional_int" );	

	CPanelAnimationVarAliasType( int, m_nSquadMate_class_icon_x, "SquadMate_class_icon_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_class_icon_y, "SquadMate_class_icon_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_class_icon_w, "SquadMate_class_icon_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_class_icon_t, "SquadMate_class_icon_t", "0", "proportional_int" );

	CPanelAnimationVarAliasType( int, m_nSquadMate_ExtraItem_x, "SquadMate_ExtraItem_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_ExtraItem_y, "SquadMate_ExtraItem_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_ExtraItem_w, "SquadMate_ExtraItem_w", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_ExtraItem_t, "SquadMate_ExtraItem_t", "0", "proportional_int" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_ExtraItem_hotkey_x, "SquadMate_ExtraItem_hotkey_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_ExtraItem_hotkey_y, "SquadMate_ExtraItem_hotkey_y", "0", "proportional_ypos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_ExtraItem_quantity_x, "SquadMate_ExtraItem_quantity_x", "0", "proportional_xpos" );
	CPanelAnimationVarAliasType( int, m_nSquadMate_ExtraItem_quantity_y, "SquadMate_ExtraItem_quantity_y", "0", "proportional_ypos" );

	CPanelAnimationVar( Color, m_SquadMate_bullets_bg_color, "SquadMate_bullets_bg_color", "0 0 0 0" );
	CPanelAnimationVar( Color, m_SquadMate_bullets_fg_color, "SquadMate_bullets_fg_color", "0 0 0 0" );
	CPanelAnimationVar( Color, m_SquadMate_clips_full_color, "SquadMate_clips_full_color", "0 0 0 0" );
	CPanelAnimationVar( Color, m_SquadMate_clips_empty_color, "SquadMate_clips_empty_color", "0 0 0 0" );
	CPanelAnimationVar( Color, m_SquadMate_name_color, "SquadMate_name_color", "0 0 0 0" );
	CPanelAnimationVar( Color, m_SquadMate_name_dead_color, "SquadMate_name_dead_color", "0 0 0 0" );
	CPanelAnimationVar( Color, m_SquadMate_ExtraItem_hotkey_color, "SquadMate_ExtraItem_hotkey_color", "0 0 0 0" );
	CPanelAnimationVar( Color, m_SquadMate_ExtraItem_quantity_color, "SquadMate_ExtraItem_quantity_color", "0 0 0 0" );

	// for testing
	CPanelAnimationVarAliasType( int, m_nWhiteTexture, "WhiteTexture", "vgui/white", "textureid" );

	int m_nMarinePortrait_circle_x;
	int m_nMarinePortrait_circle_x2;
	int m_nMarinePortrait_circle_y;
	int m_nMarinePortrait_circle_y2;

	int m_nMarinePortrait_bar_x;
	int m_nMarinePortrait_bar_x2;
	int m_nMarinePortrait_bar_y;
	int m_nMarinePortrait_bar_y2;

	CASW_Video m_LocalMarineVideo;
	int m_nLocalMarineVideoTextureID;

	// fonts

	CPanelAnimationVar( vgui::HFont, m_hDefaultSmallFont, "DefaultSmallFont", "DefaultSmall" );
	CPanelAnimationVar( vgui::HFont, m_hDefaultFont, "DefaultFont", "Default" );
	CPanelAnimationVar( vgui::HFont, m_hDefaultLargeFont, "DefaultLargeFont", "DefaultLarge" );
	
	CUtlVector<HudSheet_t> m_HudSheets;

	// data for the local marine
	C_ASW_Marine *m_pLocalMarine;
	CHandle<C_ASW_Marine> m_hLocalMarine;
	C_ASW_Marine_Resource *m_pLocalMarineResource;
	CASW_Marine_Profile *m_pLocalMarineProfile;
	C_ASW_Weapon *m_pLocalMarineActiveWeapon;
	int m_nLocalMarineClips;
	int m_nLocalMarineMaxClips;
	int m_nLocalMarineBullets;
	int m_nLocalMarineGrenades;
	int m_nLocalMarineHealth;
	int m_nLastLocalMarineHealth;

	// health bar
	float m_flLastHealthChangeTime;
	float m_flCurHealthPercent;
	float m_flPrevHealthPercent;
	bool m_bFading;
	Color m_colorMain;
	Color m_colorBG;
	Color m_colorDelay;
	float m_flMainProgress;
	float m_flDelayProgress;


	// data for squadmates

	struct SquadMateInfo_t
	{
		bool bPositionActive;
		ASW_Marine_Class MarineClass;
		float flHealthFraction;
		bool bInhabited;
		C_ASW_Weapon *pWeapon;
		int nClips;
		int nMaxClips;
		float flAmmoFraction;
		int nClassIcon;			// UV for his class icon
		wchar_t wszMarineName[ 32 ];
		const CASW_WeaponInfo *pExtraItemInfo;
		const CASW_WeaponInfo *pExtraItemInfoShift;
		int nExtraItemQuantity;
		int nEquipmentListIndex;
		CHandle<C_ASW_Marine> hMarine;
		int nExtraItemInventoryIndex;
		float flInfestation;
		int xpos;
	};
	SquadMateInfo_t m_SquadMateInfo[ MAX_SQUADMATE_HUD_POSITIONS ];

	// fast reload timings
	float m_flLastReloadProgress;
	float m_flLastNextAttack;
	float m_flLastFastReloadStart;
	float m_flLastFastReloadEnd;

	// hotbar order effects
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


#endif // _INCLUDED_ASW_HUD_MASTER_H
