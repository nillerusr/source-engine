//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tf_basehint.h"
#include "tf_hints.h"
#include "hintitemorderbase.h"
#include "iclientmode.h"
#include "clientmode_commander.h"
#include "hud_technologytreedoc.h"
#include "paneleffect.h"
#include "techtree.h"
#include "hintitemobjectbase.h"
#include "c_order.h"
#include "c_basetfplayer.h"
#include "weapon_selection.h"
#include <KeyValues.h>
#include "c_weapon_builder.h"
#include "VGuiMatSurface/IMatSystemSurface.h"
#include "c_tf_hints.h"
#include "c_hint_events.h"
#include "c_tf_hintmanager.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//	Class Hierarchy
//	CHintItemBase -- base class for hint items
//		CHintItemOrderBase -- base class for hints derived from orders ( know how to draw
//								a white line from the hint to the order panel )
//			CHintItemObjectBase -- base class for hints that care about another object ( stores the object type name )
//				CHintGotoObject -- Contains logic that relates to the other object
//				CHintWaitBuilding
//			CHintChangeToCommander -- first level hint, doesn't rely on object, but does rely on UI manipulation
//		CHintChooseAnyTechnology -- doesn't try to draw line to order since it's in tactical view?
//		


//-----------------------------------------------------------------------------
// Purpose: Change to commander view hint
//-----------------------------------------------------------------------------
class CHintChangeToCommander : public CHintItemOrderBase
{
	DECLARE_CLASS( CHintChangeToCommander, CHintItemOrderBase );
	
public:
	CHintChangeToCommander( vgui::Panel *parent, const char *panelName );
	virtual void Think( void );
};

DECLARE_HINTITEMFACTORY( CHintChangeToCommander )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//			*text - 
//			itemwidth - 
//-----------------------------------------------------------------------------
CHintChangeToCommander::CHintChangeToCommander( vgui::Panel *parent, const char *panelName )
: BaseClass( parent, panelName )
{
}

//-----------------------------------------------------------------------------
// Purpose: Set completed flag if we've made it to commander mode
// Output : 	 virtual void
//-----------------------------------------------------------------------------
void CHintChangeToCommander::Think( void )
{
	BaseClass::Think();

	if ( g_pClientMode == ClientModeCommander() )
	{
		m_bCompleted = true;
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHintGotoObject : public CHintItemObjectBase
{
	DECLARE_CLASS( CHintGotoObject, CHintItemObjectBase );
	
public:
	CHintGotoObject( vgui::Panel *parent, const char *panelName );
	virtual void		Think( void );
	
private:
	enum
	{
		MAX_OBJECT_TYPE = 128,
	};
	
	EFFECT_HANDLE		m_ArrowEffect;
	
	float				m_flNextDistanceCheck;
	IClientMode			*m_pPreviousMode;
};

DECLARE_HINTITEMFACTORY( CHintGotoObject )

#define ZONE_DISTANCE_CHECK_INTERVAL 0.5f

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//-----------------------------------------------------------------------------
CHintGotoObject::CHintGotoObject( vgui::Panel *parent, const char *panelName ) 
	: BaseClass( parent, panelName )
{
	m_ArrowEffect = CreateArrowEffect( this, parent, NULL );
	
	m_flNextDistanceCheck = 0.0f;
	
	m_pPreviousMode = NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintGotoObject::Think( void )
{
	BaseClass::Think();
	
	ClientModeTFBase *basemode = ( ClientModeTFBase * )g_pClientMode;
	CMinimapPanel *minimap = basemode->GetMinimap();
	
	CPanelEffect *e = g_pTF2RootPanel->FindEffect( m_ArrowEffect );
	if ( e && minimap )
	{
		e->SetPanelOther( minimap );
	}
	
	// Check right away if we switch modes
	if ( g_pClientMode != m_pPreviousMode )
	{
		m_flNextDistanceCheck = 0.0f;
		m_pPreviousMode = g_pClientMode;
	}
	
	if ( gpGlobals->curtime < m_flNextDistanceCheck )
	{
		return;
	}
	
	m_flNextDistanceCheck = gpGlobals->curtime + ZONE_DISTANCE_CHECK_INTERVAL;
	
	// The order contains the resource zone target
	C_TFBaseHint *hint = static_cast< C_TFBaseHint * >( GetParent() );
	if ( hint )
	{
		C_Order *order = dynamic_cast< C_Order * >( ClientEntityList().GetEnt( hint->GetEntity() ) );
		if ( order )
		{
			C_BaseEntity *pTarget = ClientEntityList().GetEnt( order->GetTarget() );
			if ( IsObjectOfType( pTarget ) )
			{
				Vector zonecenter = pTarget->WorldSpaceCenter( );
				
				if ( e && minimap )
				{
					float mapx, mapy;
					
					// Convert target center to map position
					CMinimapPanel::MinimapPanel()->WorldToMinimap( MINIMAP_CLIP, zonecenter, mapx, mapy );
					
					e->SetUsingOffset( true, (int)mapx, (int)mapy );
				}
				
				Vector delta;
				
				C_BaseTFPlayer *local = C_BaseTFPlayer::GetLocalPlayer();
				if ( local )
				{
					delta = local->GetAbsOrigin() - zonecenter;
					if ( delta.Length() < 256.0f )
					{
						m_bCompleted = true;
					}
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHintDeployWeapon : public CHintItemOrderBase
{
	DECLARE_CLASS( CHintDeployWeapon, CHintItemOrderBase );
	
public:
	CHintDeployWeapon( vgui::Panel *parent, const char *panelName );
	
	virtual void Think( void );

	virtual void		SetWeaponType( const char *type );
	virtual char const	*GetWeaponType( void );

	virtual char const	*GetKeyName( void );

	virtual void		SetPrintName( const char *name );
	virtual char const	*GetPrintName( void );

	virtual void		ParseItem( KeyValues *pKeyValues );

	virtual bool		CheckKeyAndValue( const char *instring, int* keylength, const char **ppOutstring );

private:
	enum
	{
		MAX_WEAPON_TYPE = 128,
		MAX_WEAPON_NAME = 128,
	};

	char		m_szWeaponType[ MAX_WEAPON_TYPE ];
	char		m_szPrintName[ MAX_WEAPON_NAME ];
};

DECLARE_HINTITEMFACTORY( CHintDeployWeapon )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//-----------------------------------------------------------------------------
CHintDeployWeapon::CHintDeployWeapon( vgui::Panel *parent, const char *panelName ) 
: BaseClass( parent, panelName )
{
	SetWeaponType( "" );
	SetPrintName( "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *instring - 
//			keylength - 
//			**ppOutstring - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHintDeployWeapon::CheckKeyAndValue( const char *instring, int* keylength, const char **ppOutstring )
{
	if ( !Q_strnicmp( instring, "keyname", strlen( "keyname" ) ) )
	{
		*keylength = strlen( "keyname" );
		*ppOutstring = GetKeyName();
		return true;
	}
	else if ( !Q_strnicmp( instring, "printname", strlen( "printname" ) ) )
	{
		*keylength = strlen( "printname" );
		*ppOutstring = GetPrintName();
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *type - 
//-----------------------------------------------------------------------------
void CHintDeployWeapon::SetWeaponType( const char *type )
{
	Q_strncpy( m_szWeaponType, type, MAX_WEAPON_TYPE );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CHintDeployWeapon::GetWeaponType( void )
{
	return m_szWeaponType;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CHintDeployWeapon::SetPrintName( const char *name )
{
	Q_strncpy( m_szPrintName, name, MAX_WEAPON_NAME );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CHintDeployWeapon::GetPrintName( void )
{
	return m_szPrintName;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CHintDeployWeapon::GetKeyName( void )
{
	static char keyname[ 128 ];

	keyname[ 0 ] = 0;

	CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
	if ( pHudSelection )
	{
		C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
		if ( player )
		{
			for ( int slot = 0; slot < MAX_WEAPON_SLOTS; slot++ )
			{
				C_BaseCombatWeapon *weapon = pHudSelection->GetFirstPos( slot );
				if ( !weapon )
					continue;
				
				if ( !stricmp( weapon->GetName(), GetWeaponType() ) )
				{
					Q_snprintf( keyname, sizeof( keyname ), GetKeyNameForBinding( VarArgs( "slot%i", slot + 1 ) ) );
					break;
				}
			}
		}
	}

	return keyname;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pKeyValues - 
//-----------------------------------------------------------------------------
void CHintDeployWeapon::ParseItem( KeyValues *pKeyValues )
{
	BaseClass::ParseItem( pKeyValues );

	const char *type = pKeyValues->GetString( "weapon", "" );
	if ( type )
	{
		SetWeaponType( type );
	}

	const char *printname = pKeyValues->GetString( "printname", "" );
	if ( printname )
	{
		SetPrintName( printname );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintDeployWeapon::Think( void )
{
	BaseClass::Think();
	
	C_BaseTFPlayer *player = C_BaseTFPlayer::GetLocalPlayer();
	if ( !player )
		return;
	
	// Get the weapon selection Hud Element
	CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
	// Make sure it's not still active
	if ( pHudSelection->IsActive() )
		return;
	
	C_BaseCombatWeapon *weapon = GetActiveWeapon();
	if ( !weapon )
		return;
	
	if ( !stricmp( weapon->GetClientClass()->m_pNetworkName, "CWeaponBuilder" ) )
	{
		m_bCompleted = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHintStartPlacing : public CHintItemOrderBase
{
	DECLARE_CLASS( CHintStartPlacing, CHintItemOrderBase );
	
public:
	CHintStartPlacing( vgui::Panel *parent, const char *panelName );
	
	virtual void Think( void );
private:
};

DECLARE_HINTITEMFACTORY( CHintStartPlacing )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//-----------------------------------------------------------------------------
CHintStartPlacing::CHintStartPlacing( vgui::Panel *parent, const char *panelName ) 
: BaseClass( parent, panelName )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintStartPlacing::Think( void )
{
	BaseClass::Think();
	
	C_WeaponBuilder *builder = dynamic_cast< C_WeaponBuilder * >( GetActiveWeapon() );
	if ( builder && builder->IsPlacingObject() )
	{
		m_bCompleted = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHintStartBuilding : public CHintItemOrderBase
{
	DECLARE_CLASS( CHintStartBuilding, CHintItemOrderBase );
	
public:
	CHintStartBuilding( vgui::Panel *parent, const char *panelName );
	
	virtual void Think( void );
private:
};

DECLARE_HINTITEMFACTORY( CHintStartBuilding )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//-----------------------------------------------------------------------------
CHintStartBuilding::CHintStartBuilding( vgui::Panel *parent, const char *panelName ) 
	: BaseClass( parent, panelName )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintStartBuilding::Think( void )
{
	BaseClass::Think();
	
	C_WeaponBuilder *builder = dynamic_cast< C_WeaponBuilder * >( GetActiveWeapon() );
	if ( builder && builder->IsBuildingObject() )
	{
		m_bCompleted = true;
	}
}

#define CHECK_FOR_BUILDING_INTERVAL	1.0f

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHintWaitBuilding : public CHintItemObjectBase
{
	DECLARE_CLASS( CHintWaitBuilding, CHintItemObjectBase );
	
public:
	CHintWaitBuilding( vgui::Panel *parent, const char *panelName );
	
	virtual void Think( void );
private:
	
	float			m_flNextCheck;
};


DECLARE_HINTITEMFACTORY( CHintWaitBuilding )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//-----------------------------------------------------------------------------
CHintWaitBuilding::CHintWaitBuilding( vgui::Panel *parent, const char *panelName ) 
	: BaseClass( parent, panelName )
{
	m_flNextCheck = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintWaitBuilding::Think( void )
{
	BaseClass::Think();
	
	if ( !GetActive() )
		return;
	
	C_BaseTFPlayer *player = C_BaseTFPlayer::GetLocalPlayer();
	if ( !player )
		return;
	
	if ( gpGlobals->curtime < m_flNextCheck )
		return;
	
	m_flNextCheck = gpGlobals->curtime + CHECK_FOR_BUILDING_INTERVAL;
	
	// Find resource zone
	ClientEntityHandle_t e = ClientEntityList().FirstHandle();
	for ( ;  e != ClientEntityList().InvalidHandle(); e = ClientEntityList().NextHandle( e ) )
	{
		C_BaseEntity *ent = C_BaseEntity::Instance( e );
		if ( !ent )
			continue;
		
		if ( IsObjectOfType( ent ) )
		{
			C_BaseObject *obj = static_cast< C_BaseObject * >( ent );
			
			Assert( obj );
			
			if ( obj->GetTeamNumber() == player->GetTeamNumber() &&	obj->IsOwnedByLocalPlayer() )
			{
				m_bCompleted = true;
				break;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHintBuilderSelection : public CHintItemOrderBase
{
	DECLARE_CLASS( CHintBuilderSelection, CHintItemOrderBase );
	
public:
	CHintBuilderSelection( vgui::Panel *parent, const char *panelName );
	
	virtual void Think( void );
	virtual void		SetSelection( const char *type );
	virtual char const	*GetSelection( void );

	virtual void		ParseItem( KeyValues *pKeyValues );

	virtual bool		CheckKeyAndValue( const char *instring, int* keylength, const char **ppOutstring );

private:
	enum
	{
		MAX_SELECTION_NAME = 128,
	};

	char		m_szSelection[ MAX_SELECTION_NAME ];
};

DECLARE_HINTITEMFACTORY( CHintBuilderSelection )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//-----------------------------------------------------------------------------
CHintBuilderSelection::CHintBuilderSelection( vgui::Panel *parent, const char *panelName ) 
	: BaseClass( parent, panelName )
{
	SetSelection( "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *instring - 
//			keylength - 
//			**ppOutstring - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHintBuilderSelection::CheckKeyAndValue( const char *instring, int* keylength, const char **ppOutstring )
{
	if ( !Q_strnicmp( instring, "selection", strlen( "selection" ) ) )
	{
		*keylength = strlen( "selection" );
		*ppOutstring = GetSelection();
		return true;
	}

	return BaseClass::CheckKeyAndValue( instring, keylength, ppOutstring );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *selection - 
//-----------------------------------------------------------------------------
void CHintBuilderSelection::SetSelection( const char *selection )
{
	Q_strncpy( m_szSelection, selection, MAX_SELECTION_NAME );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CHintBuilderSelection::GetSelection( void )
{
	return m_szSelection;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pKeyValues - 
//-----------------------------------------------------------------------------
void CHintBuilderSelection::ParseItem( KeyValues *pKeyValues )
{
	BaseClass::ParseItem( pKeyValues );

	const char *selection = pKeyValues->GetString( "selection", "" );
	if ( selection )
	{
		SetSelection( selection );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintBuilderSelection::Think( void )
{
	BaseClass::Think();
	
	C_BaseTFPlayer *player = C_BaseTFPlayer::GetLocalPlayer();
	if ( !player )
		return;
	
	C_WeaponBuilder *builder = dynamic_cast< C_WeaponBuilder * >( GetActiveWeapon() );
	if ( !builder )
		return;

	const char *selection = builder->GetCurrentSelectionObjectName();
	if ( !selection )
		return;

	if ( !stricmp( selection, GetSelection() ) )
	{
		m_bCompleted = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CHintBuilderStartAction : public CHintItemOrderBase
{
	DECLARE_CLASS( CHintBuilderStartAction, CHintItemOrderBase );
	
public:
	CHintBuilderStartAction( vgui::Panel *parent, const char *panelName );
	
	virtual void Think( void );
	virtual void		SetAction( const char *type );
	virtual char const	*GetAction( void );

	virtual void		ParseItem( KeyValues *pKeyValues );

	virtual bool		CheckKeyAndValue( const char *instring, int* keylength, const char **ppOutstring );

private:
	enum
	{
		MAX_ACTION_NAME = 128,
	};

	char		m_szAction[ MAX_ACTION_NAME ];
};

DECLARE_HINTITEMFACTORY( CHintBuilderStartAction )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//-----------------------------------------------------------------------------
CHintBuilderStartAction::CHintBuilderStartAction( vgui::Panel *parent, const char *panelName ) 
	: BaseClass( parent, panelName )
{
	SetAction( "" );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *instring - 
//			keylength - 
//			**ppOutstring - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHintBuilderStartAction::CheckKeyAndValue( const char *instring, int* keylength, const char **ppOutstring )
{
	if ( !Q_strnicmp( instring, "action", strlen( "action" ) ) )
	{
		*keylength = strlen( "action" );
		*ppOutstring = GetAction();
		return true;
	}

	return BaseClass::CheckKeyAndValue( instring, keylength, ppOutstring );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *action - 
//-----------------------------------------------------------------------------
void CHintBuilderStartAction::SetAction( const char *action )
{
	Q_strncpy( m_szAction, action, MAX_ACTION_NAME );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CHintBuilderStartAction::GetAction( void )
{
	return m_szAction;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pKeyValues - 
//-----------------------------------------------------------------------------
void CHintBuilderStartAction::ParseItem( KeyValues *pKeyValues )
{
	BaseClass::ParseItem( pKeyValues );

	const char *action = pKeyValues->GetString( "action", "" );
	if ( action )
	{
		SetAction( action );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintBuilderStartAction::Think( void )
{
	BaseClass::Think();
	
	C_BaseTFPlayer *player = C_BaseTFPlayer::GetLocalPlayer();
	if ( !player )
		return;
	
	C_WeaponBuilder *builder = dynamic_cast< C_WeaponBuilder * >( GetActiveWeapon() );
	if ( !builder )
		return;
}

//-----------------------------------------------------------------------------
// Purpose: A fake hud element used to force the weapon hud element to draw when it's
//  not actually active
//-----------------------------------------------------------------------------
class CHudWeaponFlashHelper : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudWeaponFlashHelper, vgui::Panel );
public:
	CHudWeaponFlashHelper( const char *name );

	virtual void Init( void );
	virtual bool ShouldDraw( void );
	virtual void Paint();
	virtual void	ApplySchemeSettings( vgui::IScheme *scheme );

	// Associate a weapon
	void SetFlashWeapon( C_BaseCombatWeapon *weapon );

	// Start/stop flashing
	void StartFlashing( void );
	void StopFlashing( void );

	// Get position of weapon icon
	void GetWeaponIconBounds( C_BaseCombatWeapon *weapon, int& x, int& y, int& w, int& h );

	// Is player using the regular weapon selection UI?
	bool IsWeaponSelectionActive( void );

private:

	// Currently flashing
	bool					m_bFlashing;

	// The weapon to highlight
	EHANDLE					m_hWeapon;

	// The actual weapon selection hud element
	CBaseHudWeaponSelection		*m_pWeaponSelection;
};

DECLARE_HUDELEMENT( CHudWeaponFlashHelper )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
CHudWeaponFlashHelper::CHudWeaponFlashHelper( const char *name ) 
	: CHudElement( name ), BaseClass( NULL, "HudWeaponFlashHelper" )
{
	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	m_bFlashing = false;
	m_pWeaponSelection = NULL;

	SetHiddenBits( HIDEHUD_MISCSTATUS | HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponFlashHelper::Init( void )
{
	CHudElement::Init();

	m_pWeaponSelection = GetHudWeaponSelection();
	Assert( m_pWeaponSelection );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudWeaponFlashHelper::ShouldDraw( void )
{
	return ( CHudElement::ShouldDraw() && m_bFlashing && m_pWeaponSelection );
}

void CHudWeaponFlashHelper::ApplySchemeSettings( vgui::IScheme *scheme )
{
	BaseClass::ApplySchemeSettings( scheme );

	SetPaintBackgroundEnabled( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponFlashHelper::Paint()
{
	// Stop immediately if user starts to choose weapons
	if ( m_pWeaponSelection->IsActive() )
	{
		StopFlashing();
		return;
	}

	if ( g_pClientMode == ClientModeCommander() )
		return;

	C_BaseCombatWeapon *w = ( C_BaseCombatWeapon * )( (C_BaseEntity *)m_hWeapon );
	if ( !w )
		return;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
		return;
	
	// Redo drawing of Weapon Menu
	m_pWeaponSelection->DrawWList( pPlayer, w, true, EFFECT_R, EFFECT_G, EFFECT_B, EFFECT_A  );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *weapon - 
//-----------------------------------------------------------------------------
void CHudWeaponFlashHelper::SetFlashWeapon( C_BaseCombatWeapon *weapon )
{
	m_hWeapon = weapon;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponFlashHelper::StartFlashing( void )
{
	m_bFlashing = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudWeaponFlashHelper::StopFlashing( void )
{
	m_bFlashing = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *weapon - 
//			x - 
//			y - 
//			w - 
//			h - 
//-----------------------------------------------------------------------------
void CHudWeaponFlashHelper::GetWeaponIconBounds( C_BaseCombatWeapon *weapon, int& x, int& y,int& w, int& h )
{
	x = y = w = h = 0;

	if ( !m_pWeaponSelection || !weapon )
		return;

	C_BasePlayer *pPlayer = C_BasePlayer::GetLocalPlayer();
	if ( !pPlayer )
	{
		return;
	}

	wrect_t outrect;
	if ( !m_pWeaponSelection->ComputeRect( pPlayer, weapon, &outrect ) )
		return;

	x = outrect.left;
	y = outrect.top;
	w = outrect.right - outrect.left;
	h = outrect.bottom - outrect.top;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHudWeaponFlashHelper::IsWeaponSelectionActive( void )
{
	if ( m_pWeaponSelection && m_pWeaponSelection->IsActive() )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Change to commander view hint
//-----------------------------------------------------------------------------
class CHintHudWeaponFlash : public CHintItemBase
{
	DECLARE_CLASS( CHintHudWeaponFlash, CHintItemBase );
	
public:
	CHintHudWeaponFlash( vgui::Panel *parent, const char *panelName );
	~CHintHudWeaponFlash( void );

	virtual void	SetKeyValue( const char *key, const char *value );

	void			SetWeaponName( const char *name );
	char const		*GetWeaponName( void );

	virtual bool	CheckKeyAndValue( const char *instring, int* keylength, const char **ppOutstring );

	virtual void	SetActive( bool bActive );

	virtual void	Think( void );

private:
	C_BaseCombatWeapon *GetWeaponOfType( const char *type );

	enum
	{
		MAX_WEAPON_NAME = 128,
	};

	bool	m_bWeaponSet;

	EHANDLE m_hWeapon; 

	char	m_szWeaponName[ MAX_WEAPON_NAME ];

	CHudWeaponFlashHelper		*m_pWeaponFlashHelper;

	EFFECT_HANDLE		m_hLineEffect;
};

DECLARE_HINTITEMFACTORY( CHintHudWeaponFlash )

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*panelName - 
//			*text - 
//			itemwidth - 
//-----------------------------------------------------------------------------
CHintHudWeaponFlash::CHintHudWeaponFlash( vgui::Panel *parent, const char *panelName )
: BaseClass( parent, panelName )
{
	m_bWeaponSet = false;
	SetWeaponName( "" );
	m_pWeaponFlashHelper = NULL;
	m_hLineEffect = EFFECT_INVALID_HANDLE;

	CreateFlashEffect( this, parent );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHintHudWeaponFlash::~CHintHudWeaponFlash( void )
{
	if ( m_pWeaponFlashHelper )
	{
		m_pWeaponFlashHelper->StopFlashing();
		m_pWeaponFlashHelper->SetFlashWeapon( NULL );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *instring - 
//			keylength - 
//			**ppOutstring - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHintHudWeaponFlash::CheckKeyAndValue( const char *instring, int* keylength, const char **ppOutstring )
{
	if ( !Q_strnicmp( instring, "weapon", strlen( "weapon" ) ) )
	{
		*keylength = strlen( "weapon" );
		*ppOutstring = GetWeaponName();
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
C_BaseCombatWeapon *CHintHudWeaponFlash::GetWeaponOfType( const char *type )
{
	CBaseHudWeaponSelection *pHudSelection = GetHudWeaponSelection();
	if ( !pHudSelection )
		return NULL;

	C_BasePlayer *player = C_BasePlayer::GetLocalPlayer();
	if ( player )
	{
		for ( int slot = 0; slot < MAX_WEAPON_SLOTS; slot++ )
		{
			for ( int iPos = 0; iPos < MAX_WEAPON_POSITIONS; iPos++ )
			{
				C_BaseCombatWeapon *weapon = pHudSelection->GetWeaponInSlot( slot, iPos );
				if ( !weapon )
					continue;
			
				if ( !stricmp( weapon->GetName(), type ) )
				{
					return weapon;
				}
			}
		}
	}

	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *key - 
//			*value - 
//-----------------------------------------------------------------------------
void CHintHudWeaponFlash::SetKeyValue( const char *key, const char *value )
{
	BaseClass::SetKeyValue( key, value );

	if ( !stricmp( key, "weapon" ) )
	{
		SetWeaponName( value );

		ComputeTitle();
	}
	else if ( !stricmp( key, "weapontype" ) )
	{
		// Find the weapon itself
		C_BaseCombatWeapon *w = GetWeaponOfType( value );
		if ( w )
		{
			m_hWeapon = w;

			// Create open up hud effect, etc.
			m_pWeaponFlashHelper = GET_HUDELEMENT( CHudWeaponFlashHelper );
			if ( m_pWeaponFlashHelper )
			{
				m_pWeaponFlashHelper->SetFlashWeapon( w );

				int x, y, wide, tall;

				m_pWeaponFlashHelper->GetWeaponIconBounds( w, x, y, wide, tall );

				C_TFBaseHint *hint = static_cast< C_TFBaseHint * >( GetParent() );

				m_hLineEffect = CreateAxialLineEffectToRect( this, hint, x, y, wide, tall );

				hint->SetDesiredPosition( x, y + tall + 50 );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHintHudWeaponFlash::Think( void )
{
	BaseClass::Think();

	if ( !m_pWeaponFlashHelper )
		return;

	if ( m_pWeaponFlashHelper->IsWeaponSelectionActive() )
	{
		m_bCompleted = true;
	}

	bool incommander = ( g_pClientMode == ClientModeCommander() );

	CPanelEffect *effect = g_pTF2RootPanel->FindEffect( m_hLineEffect );
	if ( effect )
	{
		effect->SetVisible( !incommander );

		C_BaseCombatWeapon *w = static_cast< C_BaseCombatWeapon * >( ( C_BaseEntity * )m_hWeapon );

		// Update target rectangle
		if ( w )
		{
			int x, y, wide, tall;

			m_pWeaponFlashHelper->GetWeaponIconBounds( w, x, y, wide, tall );

			effect->SetTargetRect( x, y, wide, tall );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bActive - 
//-----------------------------------------------------------------------------
void CHintHudWeaponFlash::SetActive( bool bActive )
{
	BaseClass::SetActive( bActive );

	if ( !m_pWeaponFlashHelper )
		return;

	if ( bActive )
	{
		m_pWeaponFlashHelper->StartFlashing();
	}
	else
	{
		m_pWeaponFlashHelper->StopFlashing();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//-----------------------------------------------------------------------------
void CHintHudWeaponFlash::SetWeaponName( const char *name )
{
	Q_strncpy( m_szWeaponName, name, MAX_WEAPON_NAME );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : char const
//-----------------------------------------------------------------------------
const char *CHintHudWeaponFlash::GetWeaponName( void )
{
	return m_szWeaponName;
}



//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct FUNCTIONLIST_t
{
	const char *name;
	HINTCOMPLETIONFUNCTION pfn;
};

static FUNCTIONLIST_t g_CompletionFunctions[]=
{
	{ NULL, NULL },
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
// Output : HINTCOMPLETIONFUNCTION
//-----------------------------------------------------------------------------
HINTCOMPLETIONFUNCTION LookupCompletionFunction( const char *name )
{
	int i = 0;
	while ( 1 )
	{
		FUNCTIONLIST_t *f = &g_CompletionFunctions[ i ];
		if ( !f->name )
			break;
		
		if ( !stricmp( f->name, name ) )
		{
			return f->pfn;
		}
		i++;
	}
	
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct HINTITEM_t
{
	const char *name;
	CHintItemBase	*( *pfn )( vgui::Panel *parent, const char *name );
};

static HINTITEM_t g_HintItems[]=
{
	{ "CHintChangeToCommander", GET_HINTITEMFACTORY_NAME( CHintChangeToCommander ) },
	{ "CHintGotoObject", GET_HINTITEMFACTORY_NAME( CHintGotoObject ) },
	{ "CHintDeployWeapon", GET_HINTITEMFACTORY_NAME( CHintDeployWeapon ) },
	{ "CHintStartPlacing", GET_HINTITEMFACTORY_NAME( CHintStartPlacing ) },
	{ "CHintStartBuilding", GET_HINTITEMFACTORY_NAME( CHintStartBuilding ) },
	{ "CHintWaitBuilding", GET_HINTITEMFACTORY_NAME( CHintWaitBuilding ) },
	{ "CHintBuilderSelection", GET_HINTITEMFACTORY_NAME( CHintBuilderSelection ) },
	{ "CHintBuilderStartAction", GET_HINTITEMFACTORY_NAME( CHintBuilderStartAction ) },
	{ "CHintHudWeaponFlash", GET_HINTITEMFACTORY_NAME( CHintHudWeaponFlash ) },
	{ NULL, NULL },
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *parent - 
//			*name - 
// Output : CHintItemBase
//-----------------------------------------------------------------------------
CHintItemBase *CreateHintItem( vgui::Panel *parent, const char *name )
{
	int i = 0;
	while ( 1 )
	{
		HINTITEM_t *hi = &g_HintItems[ i ];
		if ( !hi->name )
			break;
		
		if ( !stricmp( hi->name, name ) )
		{
			if ( hi->pfn )
			{
				return (*hi->pfn)( parent, name );
			}
			else
			{
				Assert( !"Missing function pointer in CreateHintItem table!" );
				return NULL;
			}
		}
		i++;
	}
	
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

CHintData g_HintDatas[] =
{
	// Vote
	{ "TF_HINT_VOTEFORTECHNOLOGY", TF_HINT_VOTEFORTECHNOLOGY, 0, NULL, -1 },

	// Build
	{ "TF_HINT_BUILDRESOURCEPUMP", TF_HINT_BUILDRESOURCEPUMP, 0, HintEventFn_BuildObject, OBJ_RESOURCEPUMP },
	{ "TF_HINT_BUILDSENTRYGUN_PLASMA", TF_HINT_BUILDSENTRYGUN_PLASMA, 0, HintEventFn_BuildObject, OBJ_SENTRYGUN_PLASMA },

	// Object interaction
	{ "TF_HINT_REPAIROBJECT", TF_HINT_REPAIROBJECT, 0, NULL, -1 },

	// Technology discovery
	{ "TF_HINT_NEWTECHNOLOGY", TF_HINT_NEWTECHNOLOGY, 0, NULL, -1 },
	{ "TF_HINT_WEAPONRECEIVED", TF_HINT_WEAPONRECEIVED, 0, NULL, -1 },
	
	// Sentinal
	{ NULL, 0, 0, NULL, -1 },
};


int GetNumHintDatas()
{
	return ARRAYSIZE( g_HintDatas );
}


CHintData* GetHintData( int i )
{
	return &g_HintDatas[i];
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : id - 
// Output : char const
//-----------------------------------------------------------------------------
const char *LookupHintName( int id )
{
	int i = 0;
	while ( 1 )
	{
		CHintData *h = &g_HintDatas[ i ];
		if ( !h->name )
			break;
		
		if ( h->id == id )
		{
			return h->name;
		}
		
		i++;
	}
	
	return NULL;
}

DECLARE_HINTFACTORY( C_TFBaseHint )

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
typedef struct 
{
	const char *name;
	C_TFBaseHint	*( *pfn )( int id, int entity );
}
HINT_t;

static HINT_t g_Hints[]=
{
	{ "C_TFBaseHint", GET_HINTFACTORY_NAME( C_TFBaseHint ) },
	{ NULL, NULL },
};

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *name - 
//			id - 
//			entity - 
// Output : C_TFBaseHint
//-----------------------------------------------------------------------------
C_TFBaseHint *FactoryCreateHint( const char *name, int id, int entity )
{
	int i = 0;
	while ( 1 )
	{
		HINT_t *hi = &g_Hints[ i ];
		if ( !hi->name )
			break;
		
		if ( !stricmp( hi->name, name ) )
		{
			if ( hi->pfn )
			{
				return (*hi->pfn)( id, entity );
			}
			else
			{
				Assert( !"Missing function pointer in FactoryCreateHint table!" );
				return NULL;
			}
		}
		i++;
	}
	
	return NULL;
}

//-----------------------------------------------------------------------------
// Purpose: Generic factory for hints
// Input  : id - 
//			entity - 
// Output : C_TFBaseHint
//-----------------------------------------------------------------------------
C_TFBaseHint *C_TFBaseHint::CreateHint( int id, const char *subsection, int entity )
{
	C_TFBaseHint *hint = NULL;
	const char *hintname = LookupHintName( id );
	if ( !hintname )
		return NULL;
	
	// See if we should see this hint any more
	KeyValues *pkvStats = GetHintDisplayStats();
	if ( pkvStats )
	{
		KeyValues *pkvStatSection = pkvStats->FindKey( hintname, true );
		if ( pkvStatSection )
		{
			if ( subsection && subsection[0] )
			{
				pkvStatSection = pkvStatSection->FindKey( subsection, true );
			}
		}

		if ( !pkvStatSection )
		{
			Assert( !"C_TFBaseHint::CreateHint:  Problem creating hint subsection" );
			return NULL;
		}

		int times_shown = pkvStatSection->GetInt( "times_shown", 0 );
		pkvStatSection->SetString( "times_shown", VarArgs( "%i", times_shown ) );

		int times_max = pkvStatSection->GetInt( "times_max", 3 );
		pkvStatSection->SetString( "times_max", VarArgs( "%i", times_max ) );

		if ( times_shown >= times_max )
			return NULL;

		// Remember that we've seen it again
		times_shown++;
		pkvStatSection->SetString( "times_shown", VarArgs( "%i", times_shown ) );
	}

	// Ask Hint manager API for key values
	KeyValues *pkvHintSystem = GetHintKeyValues();
	if ( pkvHintSystem )
	{
		// 
		// Parse the list of hints looking for name
		KeyValues *pkvHint = pkvHintSystem->FindKey( hintname );
		if ( pkvHint )
		{
			// Use classname string to construct hint
			const char *defaultclass = "C_TFBaseHint";
			const char *classname = pkvHint->GetString( "classname" );
			if ( !classname || !classname[ 0 ] || !stricmp( classname, "default" ) )
			{
				classname = defaultclass;
			}
			
			hint = FactoryCreateHint( classname, id, entity );
			if ( hint )
			{
				hint->ParseFromData( pkvHint );
			}
		}	
	}
	
	return hint;
}
