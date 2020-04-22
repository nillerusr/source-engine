//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_HUD_MENU_SPY_BUILD_H
#define TF_HUD_MENU_SPY_BUILD_H
#ifdef _WIN32
#pragma once
#endif

#ifdef STAGING_ONLY
#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/Label.h>
#include "IconPanel.h"
#include "tf_controls.h"
#include "tf_hud_base_build_menu.h"

using namespace vgui;

#define ALL_BUILDINGS	-1
#define NUM_SPY_BUILDINGS 3

enum spybuildmenulayouts_t
{
	SPY_BUILDMENU_DEFAULT = 0,
};

struct SpyConstructBuilding_t
{
	SpyConstructBuilding_t() {}

	SpyConstructBuilding_t( bool bEnabled,
							 ObjectType_t type,
							 int iMode, 
							 const char *pszConstructAvailableObjectRes,
							 const char *pszConstructAlreadyBuiltObjectRes,
							 const char *pszConstructCantAffordObjectRes,
							 const char *pszConstructUnavailableObjectRes,
							 const char *pszDestroyActiveObjectRes,
							 const char *pszDestroyInactiveObjectRes,
							 const char *pszDestroyUnavailableObjectRes )
							 : m_bEnabled( bEnabled )
							 , m_iObjectType ( type )
							 , m_iMode( iMode )
							 , m_pszConstructAvailableObjectRes( pszConstructAvailableObjectRes )
							 , m_pszConstructAlreadyBuiltObjectRes( pszConstructAlreadyBuiltObjectRes )
							 , m_pszConstructCantAffordObjectRes( pszConstructCantAffordObjectRes )
							 , m_pszConstructUnavailableObjectRes( pszConstructUnavailableObjectRes )
							 , m_pszDestroyActiveObjectRes( pszDestroyActiveObjectRes )
							 , m_pszDestroyInactiveObjectRes( pszDestroyInactiveObjectRes )
							 , m_pszDestroyUnavailableObjectRes( pszDestroyUnavailableObjectRes )
	{}

	bool m_bEnabled;
	ObjectType_t m_iObjectType;
	int m_iMode;
	// Construction panels
	const char *m_pszConstructAvailableObjectRes;
	const char *m_pszConstructAlreadyBuiltObjectRes;
	const char *m_pszConstructCantAffordObjectRes;
	const char *m_pszConstructUnavailableObjectRes;
	// Destruction panels
	const char *m_pszDestroyActiveObjectRes;
	const char *m_pszDestroyInactiveObjectRes;
	const char *m_pszDestroyUnavailableObjectRes;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct SpyConstructBuildingReplacement_t
{
	SpyConstructBuildingReplacement_t(	ObjectType_t type,
								int iMode,
								const char *strConstructionAvailable,
								const char *strConstructionAlreadyBuilt,
								const char *strConstructionCantAfford,
								const char *strConstructionUnavailable,
								const char *strDestructionActive,
								const char *strDestructionInactive,
								const char *strDestructionUnavailable,
								int iReplacementSlots,
								int iDisableSlots
								)
								: m_building( true,
											  type,
											  iMode,
											  strConstructionAvailable,
											  strConstructionAlreadyBuilt,
											  strConstructionCantAfford,
											  strConstructionUnavailable,
											  strDestructionActive,
											  strDestructionInactive,
											  strDestructionUnavailable )

	{
		m_iReplacementSlots = iReplacementSlots;
		m_iDisableSlots = iDisableSlots;
	}

	SpyConstructBuilding_t m_building;
	int m_iReplacementSlots;
	int m_iDisableSlots;
};

extern const SpyConstructBuilding_t g_kSpyBuildings[];
extern const SpyConstructBuildingReplacement_t s_alternateSpyBuildings[];

class CHudMenuSpyBuild : public CHudBaseBuildMenu
{
	DECLARE_CLASS_SIMPLE( CHudMenuSpyBuild, CHudBaseBuildMenu );

public:
	CHudMenuSpyBuild( const char *pElementName );

	virtual void ApplySchemeSettings( IScheme *scheme ) OVERRIDE;
	virtual void SetVisible( bool state ) OVERRIDE;
	virtual void OnTick( void ) OVERRIDE;
	virtual bool ShouldDraw( void ) OVERRIDE;

	int	HudElementKeyInput( int down, ButtonCode_t keynum, const char *pszCurrentBinding );
	virtual int GetRenderGroupPriority() { return 50; }
	int	CalcCustomBuildMenuLayout( void ) const;

	static void ReplaceBuildings( SpyConstructBuilding_t (&targetBuildings)[ NUM_SPY_BUILDINGS ] );
	static void GetBuildingIDAndModeFromSlot( int iSlot, int &iBuilding, int &iMode, const SpyConstructBuilding_t (&buildings)[ NUM_SPY_BUILDINGS ] );

private:

	void SendBuildMessage( int iSlot );
	bool SendDestroyMessage( int iSlot );
	void SetSelectedItem( int iSlot );
	void UpdateHintLabels( void );	// show/hide the bright and dim build, destroy hint labels
	void InitBuildings();
	bool CanBuild( int iSlot );

	EditablePanel *m_pAvailableObjects[NUM_SPY_BUILDINGS];
	EditablePanel *m_pAlreadyBuiltObjects[NUM_SPY_BUILDINGS];
	EditablePanel *m_pCantAffordObjects[NUM_SPY_BUILDINGS];
	EditablePanel *m_pUnavailableObjects[NUM_SPY_BUILDINGS];

	// 360 layout only
	CIconPanel *m_pActiveSelection;
	CExLabel *m_pBuildLabelBright;
	CExLabel *m_pBuildLabelDim;
	CExLabel *m_pDestroyLabelBright;
	CExLabel *m_pDestroyLabelDim;
	int m_iSelectedItem;
	bool m_bInConsoleMode;

	spybuildmenulayouts_t m_iCurrentBuildMenuLayout;
	SpyConstructBuilding_t m_Buildings[NUM_SPY_BUILDINGS];
};

#endif // STAGING_ONLY

#endif	// TF_HUD_MENU_SPY_BUILD_H