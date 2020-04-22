//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client version of CObjectMortar
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "CommanderOverlay.h"
#include "c_baseobject.h"
#include "tf_shareddefs.h"
#include "c_basetfplayer.h"
#include "ObjectControlPanel.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include "vgui_rotation_slider.h"
#include <vgui/ISurface.h>
#include "vgui_basepanel.h"
#include "vgui_bitmapimage.h"
#include "iusesmortarpanel.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_ObjectMortar : public C_BaseObject, public IUsesMortarPanel
{
	DECLARE_CLASS( C_ObjectMortar, C_BaseObject );
public:
	DECLARE_CLIENTCLASS();
	DECLARE_ENTITY_PANEL();

	C_ObjectMortar();

	virtual void SetDormant( bool bDormant );
	virtual void Select( void );
	virtual void RecalculateIDString( void );
	
	void		 FireMortar( void );

// IUsesMortarPanel
public:
	// Get the data from this mortar needed by the panel
	virtual void	GetMortarData( float *flClientMortarYaw, bool *bAllowedToFire, float *flPower, float *flFiringPower, float *flFiringAccuracy, int *iFiringState );
	virtual void	SendYawCommand( void );
	virtual void	ForceClientYawCountdown( float flTime );
	virtual void	ClickFire( void );

public:
	int		m_iRoundType;
	int		m_iMortarRounds[ MA_LASTAMMOTYPE ];

	// Mortar firing info.
	int		m_iFiringState; // One of the MORTAR_ defines.
	bool	m_bMortarReloading;
	float	m_flPower;
	bool	m_bAllowedToFire;
	
	// Parameters for the next shot.
	float m_flFiringPower;
	float m_flFiringAccuracy;

	float m_flMortarYaw;	// What direction the mortar is aimed in.
	float m_flMortarPitch;
	
	// This is what is used on the client to draw the ground line and orient the mortar.
	// It is usually copied right over from m_flClientMortarYaw (which comes from the server),
	// but this is also used when rotating the mortar so you can see the line move smoothly.
	float m_flClientMortarYaw;

	// This is set to about 1/4 seconds when you rotate the mortar line so you use the client's
	// (smooth, non-lagged) yaw changes instead of the server's.
	float m_flForceClientYawCountdown;

private:
	C_ObjectMortar( const C_ObjectMortar & ); // not defined, not accessible
};

IMPLEMENT_CLIENTCLASS_DT(C_ObjectMortar, DT_ObjectMortar, CObjectMortar)
	RecvPropInt( RECVINFO(m_iRoundType) ),
	RecvPropArray( RecvPropInt( RECVINFO(m_iMortarRounds[0]) ),	m_iMortarRounds )
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectMortar::C_ObjectMortar( void )
{
	memset( m_iMortarRounds, 0, sizeof( m_iMortarRounds ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectMortar::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );
	ENTITY_PANEL_ACTIVATE( "mortar", !bDormant );
}

//-----------------------------------------------------------------------------
// Purpose: Cycle ammo types on the mortar
//-----------------------------------------------------------------------------
void C_ObjectMortar::Select( void )
{
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( pPlayer == NULL )
		return;

	int iOldType = m_iRoundType++;

	// Cycle to the next ammo type
	while ( m_iRoundType != iOldType )
	{
		// Hit the end of the round types?
		if ( m_iRoundType == MA_LASTAMMOTYPE )
		{
			m_iRoundType = MA_SHELL;
			break;
		}

		// Does this round type need a technology?
		if ( MortarAmmoTechs[ m_iRoundType ] && MortarAmmoTechs[ m_iRoundType ][0] )
		{
			// Does the player have the technology?
			if ( pPlayer->HasNamedTechnology( MortarAmmoTechs[ m_iRoundType ] ) )
			{
				// Do we have ammo?
				if ( m_iMortarRounds[ m_iRoundType ] > 0 )
					break;
			}
		}

		// Go to the next round type
		m_iRoundType++;
	}

	engine->ClientCmd( VarArgs("mortarround %d", m_iRoundType) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectMortar::RecalculateIDString( void )
{
	// Only owners get full data
	if ( IsOwnedByLocalPlayer() )
	{
		if ( m_iRoundType >= 0 )
		{
			// -1 means we have infinite rounds of this type
			if ( m_iMortarRounds[ m_iRoundType ] == -1 )
			{
				Q_snprintf( m_szIDString, sizeof(m_szIDString), "%s  -  %s", GetTargetDescription(), MortarAmmoNames[ m_iRoundType ] );
			}
			else 
			{
				Q_snprintf( m_szIDString, sizeof(m_szIDString), "%s  -  %d %s", GetTargetDescription(), m_iMortarRounds[ m_iRoundType ], MortarAmmoNames[ m_iRoundType ] );
			}
			Q_strncat( m_szIDString, "\nUse it to change ammo types.", sizeof(m_szIDString), COPY_ALL_CHARACTERS );
		}
	}

	BaseClass::RecalculateIDString();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectMortar::ClickFire( void )
{
	switch( m_iFiringState )
	{
	case MORTAR_IDLE:
		m_iFiringState = MORTAR_CHARGING_POWER;
		break;
	
	case MORTAR_CHARGING_POWER:
		m_flFiringPower = m_flPower;
		m_iFiringState = MORTAR_CHARGING_ACCURACY;
		break;
	
	case MORTAR_CHARGING_ACCURACY:
		m_flFiringAccuracy = m_flPower;
		m_iFiringState = MORTAR_IDLE;

		FireMortar();
		break;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectMortar::GetMortarData( float *flClientMortarYaw, bool *bAllowedToFire, float *flPower, float *flFiringPower, float *flFiringAccuracy, int *iFiringState )
{
	*flClientMortarYaw = m_flClientMortarYaw;
	*bAllowedToFire = m_bAllowedToFire;
	*flPower = m_flPower;
	*flFiringPower = m_flFiringPower;
	*flFiringAccuracy = m_flFiringAccuracy;
	*iFiringState = m_iFiringState;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectMortar::SendYawCommand( void )
{
	char szbuf[48];
	Q_snprintf( szbuf, sizeof( szbuf ), "MortarYaw %0.2f\n", m_flClientMortarYaw );
	SendClientCommand( szbuf );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectMortar::ForceClientYawCountdown( float flTime )
{
	m_flForceClientYawCountdown = flTime;
}


//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CMortarControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CMortarControlPanel, CObjectControlPanel );
public:
	CMortarControlPanel( vgui::Panel *parent, const char *panelName );
	virtual ~CMortarControlPanel();
	
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnCommand( const char *command );
	C_ObjectMortar* GetMortar() const;

protected:
	virtual vgui::Panel* TickCurrentPanel();

private:
	CMortarMinimapPanel *m_pMinimapPanel;
};


DECLARE_VGUI_SCREEN_FACTORY( CMortarControlPanel, "mortar_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CMortarControlPanel::CMortarControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CMortarControlPanel" ) 
{
	m_pMinimapPanel = new CMortarMinimapPanel( this, "MinimapPanel" );
	m_pMinimapPanel->Init( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMortarControlPanel::~CMortarControlPanel()
{
	delete m_pMinimapPanel;
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CMortarControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	// Make sure all named panels are created up above because BaseClass::Init initializes them
	// all from their keyvalues.
	if ( !BaseClass::Init( pKeyValues, pInitData ) )
		return false;

	// Init subpanels.
	int x, y, w, h;
	GetBounds( x, y, w, h );

	m_pMinimapPanel->LevelInit( engine->GetLevelName() );
	m_pMinimapPanel->SetVisible( true );
	m_pMinimapPanel->InitMortarMinimap( GetMortar() );
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectMortar* CMortarControlPanel::GetMortar() const
{
	return dynamic_cast< C_ObjectMortar* >( GetOwningObject() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectMortar::FireMortar( void )
{
	char cmd[512];
	Q_snprintf( cmd, sizeof( cmd ), "FireMortar %.2f %.2f", m_flFiringPower, m_flFiringAccuracy );
	SendClientCommand( cmd );
}

//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
vgui::Panel* CMortarControlPanel::TickCurrentPanel()
{
	C_BaseObject *pObj = GetOwningObject();
	if (!pObj)
		return BaseClass::TickCurrentPanel();;

	ShowOwnerLabel( true );
	ShowHealthLabel( true );

	C_ObjectMortar *pMortar = GetMortar();
	if ( !pMortar )
		return BaseClass::TickCurrentPanel();;

	ShowOwnerLabel( false );
	ShowHealthLabel( false );

	m_pMinimapPanel->Repaint();

	float flAccuracySpeed = (1.0 / MORTAR_CHARGE_ACCURACY_RATE);

	// Handle power charging
	switch( pMortar->m_iFiringState )
	{
	case MORTAR_IDLE:
		pMortar->m_flPower = 0;
		break;

	case MORTAR_CHARGING_POWER:
		pMortar->m_flPower = MIN( pMortar->m_flPower + ( (1.0 / MORTAR_CHARGE_POWER_RATE) * gpGlobals->frametime), 1 );
		pMortar->m_flFiringPower = 0;
		pMortar->m_flFiringAccuracy = 0;
		if ( pMortar->m_flPower >= 1.0 )
		{
			// Hit Max, start going down
			pMortar->m_flFiringPower = pMortar->m_flPower;
			pMortar->m_iFiringState = MORTAR_CHARGING_ACCURACY;
		}
		break;

	case MORTAR_CHARGING_ACCURACY:
		// Calculate accuracy speed
		if ( pMortar->m_flFiringPower > 0.5 )
		{
			// Shots over halfway suffer an increased speed to the accuracy power, making accurate shots harder
			float flAdjustedPower = (pMortar->m_flFiringPower - 0.5) * 3.0;
			flAccuracySpeed += (pMortar->m_flFiringPower * flAdjustedPower);
		}

		pMortar->m_flPower = MAX( pMortar->m_flPower - ( flAccuracySpeed * gpGlobals->frametime), -0.25f);
		if ( pMortar->m_flPower <= -0.25 )
		{
			// Hit Min, fire mortar
			pMortar->m_flFiringAccuracy = pMortar->m_flPower;
			pMortar->m_iFiringState = MORTAR_IDLE;

			pMortar->FireMortar();
		}
		break;
	
	default:
		break;
	}

	return BaseClass::TickCurrentPanel();
}

//-----------------------------------------------------------------------------
// Button click handlers
//-----------------------------------------------------------------------------
void CMortarControlPanel::OnCommand( const char *command )
{
	C_ObjectMortar *pMortar = GetMortar();
	if ( !pMortar )
		return;

	if ( !Q_stricmp( command, "FireMortar" ) )
	{
		pMortar->ClickFire();
	}

	BaseClass::OnCommand(command);
}

