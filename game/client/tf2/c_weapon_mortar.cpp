//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's CWeaponMortar class
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "hud.h"
#include "hudelement.h"
#include "in_buttons.h"
#include "ground_line.h"
#include "clientmode_tfnormal.h"
#include "vgui_basepanel.h"
#include "c_tf_basecombatweapon.h"
#include "engine/IEngineSound.h"
#include "iinput.h"
#include "imessagechars.h"
#include "c_weapon__stubs.h"

//=============================================================================
// Purpose: Hud Element for Mortar firing
//=============================================================================
class CHudMortar : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudMortar, vgui::Panel );

public:
	DECLARE_MULTIPLY_INHERITED();

	CHudMortar( const char *pElementName );
	virtual void Paint( void );

	float	m_flPower;
	float	m_flFiringPower;
	float	m_flFiringAccuracy;
	float	m_flReset;
};

DECLARE_HUDELEMENT( CHudMortar );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudMortar::CHudMortar( const char *pElementName ) :
	CHudElement( pElementName ), vgui::Panel( NULL, pElementName )
{
	m_flPower = 0;
	m_flFiringPower = 0;
	m_flFiringAccuracy = 0;
	m_flReset = 0;
	SetPaintBackgroundEnabled( false );
	SetAutoDelete( false );
	SetName( "mortar" );

	SetHiddenBits( HIDEHUD_PLAYERDEAD );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMortar::Paint()
{
	// Clear out markers
	if ( m_flReset && m_flReset < gpGlobals->curtime )
	{
		m_flFiringPower = 0;
		m_flFiringAccuracy = 0;
		m_flReset = 0;
	}

	int w, h;
	GetSize( w, h );

	// Use an eighth of the height for each side
	int iOffset = h / 8;
	int iBarHeight = h - (2 * iOffset);
	int iZero = (w / 5);
	int iMarker = iZero + (m_flPower * (w - iZero));
	int iPower = iZero + (m_flFiringPower * (w - iZero));
	int iAccuracy = iZero + (m_flFiringAccuracy * (w - iZero));

	// Shade the bar
	int alpha = 205;
	int colorAmt = 255;
	int nSegs = 20;

	int i;
	for(i=0; i < nSegs; i++)
	{
		int left = iZero + (i*w) / nSegs;
		int right = iZero + ((i+1)*w) / nSegs;

		// Don't draw past the marker
		if ( m_flFiringPower && right > iPower )
			right = iPower;
		else if ( !m_flFiringPower && right > iMarker )
			right = iMarker;

		vgui::surface()->DrawSetColor( ((i + 5) * colorAmt) / nSegs, 0, 0, alpha );
		vgui::surface()->DrawFilledRect( left, iOffset, right, iBarHeight);
	}

	// Shade back from zero
	nSegs = 10;
	for(i=0; i < nSegs; i++)
	{
		int left = (i*iZero) / nSegs;
		int right = ((i+1)*iZero) / nSegs;

		if ( m_flFiringAccuracy && left < iAccuracy )
			left = iAccuracy;
		else if ( !m_flFiringAccuracy && left < iMarker )
			left = iMarker;

		vgui::surface()->DrawSetColor( ((nSegs - i) * colorAmt) / nSegs, 0, 0, alpha );
		vgui::surface()->DrawFilledRect(left, iOffset, right, iBarHeight);
	}

	// Draw the zero marker
	vgui::surface()->DrawSetColor(255,255,255,255);
	vgui::surface()->DrawFilledRect(iZero-2, iOffset, iZero+2, iBarHeight);
	
	// Draw the marker
	vgui::surface()->DrawSetColor(255,255,255,255);
	vgui::surface()->DrawFilledRect(iMarker-1, 0, iMarker+1, h);

	// Draw the power mark if we've set one
	if ( m_flFiringPower )
	{
		vgui::surface()->DrawSetColor(255,255,255,255);
		vgui::surface()->DrawFilledRect(iPower-1, iOffset, iPower+1, iBarHeight);
	}

	// Draw the accuracy mark if we've set one
	if ( m_flFiringAccuracy )
	{
		vgui::surface()->DrawSetColor(255,255,255,255);
		vgui::surface()->DrawFilledRect(iAccuracy-1, iOffset, iAccuracy+1, iBarHeight);
	}

	// Draw box
	vgui::surface()->DrawSetColor(255,255,255,255);
	vgui::surface()->DrawOutlinedRect(0, iOffset, w, iBarHeight);
}

static ConVar g_CVMortarGroundLineUpdateInterval( "mortar_groundlineupdateinterval", "0.1", 0, "Number of seconds, mininum, between ground line position updates." );


//=============================================================================
// Purpose: Client version of CWeaponMortar
//=============================================================================
class C_WeaponMortar : public C_BaseTFCombatWeapon
{
public:
	DECLARE_CLASS( C_WeaponMortar, C_BaseTFCombatWeapon );
	DECLARE_CLIENTCLASS();
	DECLARE_PREDICTABLE();

	C_WeaponMortar();
	~C_WeaponMortar();

	void FireMortar( void );

	virtual void PreDataUpdate( DataUpdateType_t updateType );
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual void HandleInput( void );
	virtual void Redraw();
	virtual void OverrideMouseInput( float *x, float *y );	

	// Deploy / Holster
	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

	// Mortar Data
	bool	m_bCarried;
	bool	m_bMortarReloading;
	Vector	m_vecMortarOrigin;
	Vector	m_vecMortarAngles;
	float	m_flPrevMortarServerYaw;
	float	m_flMortarYaw;
	float	m_flPrevMortarYaw;

	// Input Handling
	float	m_flNextClick;
	float	m_flAccuracySpeed;
	int		m_iFiringState;
	bool	m_bRotating;

	CGroundLine *m_pGroundLine;
	CGroundLine *m_pDarkLine;
	CHudMortar	*m_pPowerBar;
	IMaterial	*m_pRotateIcon;

	vgui::HFont		m_hFontText;

private:
	C_WeaponMortar( const C_WeaponMortar & );

	float			m_flLastGroundlineUpdateTime;

};

STUB_WEAPON_CLASS_IMPLEMENT( weapon_mortar, C_WeaponMortar );

IMPLEMENT_CLIENTCLASS_DT(C_WeaponMortar, DT_WeaponMortar, CWeaponMortar)
	RecvPropInt( RECVINFO(m_bCarried) ),
	RecvPropInt( RECVINFO(m_bMortarReloading) ),
	RecvPropVector( RECVINFO(m_vecMortarOrigin) ),
	RecvPropVector( RECVINFO(m_vecMortarAngles) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_WeaponMortar::C_WeaponMortar()
{
	m_bCarried = true;
	m_bMortarReloading = false;
	m_iFiringState = MORTAR_IDLE;
	m_flNextClick = 0;
	m_flAccuracySpeed = 0;
	m_bRotating = false;
	m_pGroundLine = NULL;
	m_pDarkLine = NULL;
	m_flMortarYaw = 0;
	m_flPrevMortarYaw = -1;

	m_pRotateIcon = materials->FindMaterial( "Hud/mortar/mortar_rotate", TEXTURE_GROUP_VGUI );
	m_pRotateIcon->IncrementReferenceCount();

	m_pPowerBar = GET_HUDELEMENT( CHudMortar );

	m_flLastGroundlineUpdateTime = 0.0f;

	m_hFontText = g_hFontTrebuchet24;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_WeaponMortar::~C_WeaponMortar()
{
	m_pPowerBar->SetParent( (vgui::Panel *)NULL );

	if ( m_pRotateIcon != NULL )
	{
		m_pRotateIcon->DecrementReferenceCount();
		m_pRotateIcon = NULL;
	}

	if ( m_pGroundLine )
	{
		delete m_pGroundLine;
	}
	if ( m_pDarkLine )
	{
		delete m_pDarkLine;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_WeaponMortar::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate(updateType);

	m_flPrevMortarServerYaw = m_vecMortarAngles.y;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_WeaponMortar::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged(updateType);

	// If the mortar's being carried by some other player, don't make a ground line
	if ( !IsCarriedByLocalPlayer() )
		return;

	// Draw power chart if the mortar is deployed
	if ( !m_bCarried && !m_bMortarReloading )
	{
		vgui::Panel *pParent = GetClientModeNormal()->GetViewport();
		int parentWidth, parentHeight;
		pParent->GetSize(parentWidth, parentHeight);

		int iWidth = 256;
		int iHeight = 40;
		int iX = (parentWidth - iWidth) / 2;
		int iY = (parentHeight - 200);

		// Only show the power bar if the mortar's the active weapon
		if ( IsActiveByLocalPlayer() )
		{
			m_pPowerBar->SetBounds( iX, iY, iWidth, iHeight );
			m_pPowerBar->SetParent(pParent);
		}
		else
		{
			m_pPowerBar->SetParent( (vgui::Panel *)NULL );
		}

		if ( !m_bRotating && m_flPrevMortarServerYaw != m_vecMortarAngles.y )
		{
			m_flMortarYaw = m_vecMortarAngles.y;
		}

		// Create the Ground lines
		if ( !m_pGroundLine )
		{
			m_pGroundLine = new CGroundLine();
			m_pGroundLine->Init( "player/support/mortarline" );
		}
		if ( !m_pDarkLine )
		{
			m_pDarkLine = new CGroundLine();
			m_pDarkLine->Init( "player/support/mortarline" );
		}
	}
	else
	{
		m_pPowerBar->SetParent( (vgui::Panel *)NULL );

		if ( m_pGroundLine )
		{
			delete m_pGroundLine;
			m_pGroundLine = NULL;
		}
		if ( m_pDarkLine )
		{
			delete m_pDarkLine;
			m_pDarkLine = NULL;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_WeaponMortar::HandleInput( void )
{
	// If it's being carried, ignore input
	if ( m_bCarried )
		return;
	// If the player's dead, ignore input
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( pPlayer == NULL )
		return;
	if ( pPlayer->GetHealth() < 1 )
		return;

	// Ignore input if it's reloading
	if ( m_bMortarReloading )
		return;

	// Secondary fire rotates the mortar
	if ( gHUD.m_iKeyBits & IN_ATTACK2 )
	{
		if ( pPlayer->HasPowerup(POWERUP_EMP) )
		{
			CLocalPlayerFilter filter;
			EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "WeaponMortar.EMPed" );
			return;
		}
		m_bRotating = true;

		gHUD.m_iKeyBits &= ~IN_ATTACK2;

		// Prevent firing while rotating
		m_pPowerBar->SetParent( (vgui::Panel *)NULL );
		return;
	}
	else
	{
		if ( m_bRotating )
		{
			// Bring up the firing bar again
			vgui::Panel *pParent = GetClientModeNormal()->GetViewport();
			m_pPowerBar->SetParent(pParent);
			m_bRotating = false;
		}
	}

	// Primary fire launches mortars
	if (gHUD.m_iKeyBits & IN_ATTACK)
	{
		if ( pPlayer->HasPowerup(POWERUP_EMP) )
		{
			CLocalPlayerFilter filter;
			EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "WeaponMortar.EMPed" );
			return;
		}

		if ( m_flNextClick <= gpGlobals->curtime )
		{
			// Play click animation
			// SendWeaponAnim( ACT_SLAM_DETONATOR_DETONATE );

			// Switch states
			switch( m_iFiringState )
			{
			case MORTAR_IDLE:
				m_iFiringState = MORTAR_CHARGING_POWER;
				break;
			case MORTAR_CHARGING_POWER:
				m_pPowerBar->m_flFiringPower = m_pPowerBar->m_flPower;
				m_iFiringState = MORTAR_CHARGING_ACCURACY;
				break;
			case MORTAR_CHARGING_ACCURACY:
				m_pPowerBar->m_flFiringAccuracy = m_pPowerBar->m_flPower;
				m_iFiringState = MORTAR_IDLE;

				FireMortar();
				break;
			default:
				break;
			}

			input->ClearInputButton( IN_ATTACK );
			gHUD.m_iKeyBits &= ~IN_ATTACK;

			m_flNextClick = gpGlobals->curtime + 0.05;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_WeaponMortar::Redraw()
{
	BaseClass::Redraw();

	// If the player's dead, abort
	C_BaseTFPlayer *pPlayer = C_BaseTFPlayer::GetLocalPlayer();
	if ( pPlayer == NULL )
		return;
	if ( pPlayer->GetHealth() < 1 )
	{
		m_iFiringState = MORTAR_IDLE;
		m_bCarried = true;
		return;
	}

	// If it's reloading, tell the player
	if ( m_bMortarReloading )
	{
		int width, height;
		messagechars->GetStringLength( m_hFontText, &width, &height, "Mortar is reloading..." );
		messagechars->DrawString( m_hFontText, (ScreenWidth() - width) / 2, YRES(350), 192, 192, 192, 255, "Mortar is reloading...", IMessageChars::MESSAGESTRINGID_NONE );
		return;
	}

	// Handle power charging
	switch( m_iFiringState )
	{
	case MORTAR_IDLE:
		m_pPowerBar->m_flPower = 0;
		break;
	case MORTAR_CHARGING_POWER:
		m_pPowerBar->m_flPower = MIN( m_pPowerBar->m_flPower + ( (1.0 / MORTAR_CHARGE_POWER_RATE) * gpGlobals->curtimeDelta), 1.0f);
		m_pPowerBar->m_flFiringPower = 0;
		m_pPowerBar->m_flFiringAccuracy = 0;
		if ( m_pPowerBar->m_flPower >= 1.0 )
		{
			// Hit Max, start going down
			m_pPowerBar->m_flFiringPower = m_pPowerBar->m_flPower;
			m_iFiringState = MORTAR_CHARGING_ACCURACY;
			m_flNextClick = gpGlobals->curtime + 0.25;
		}
		break;
	case MORTAR_CHARGING_ACCURACY:
		// Calculate accuracy speed
		m_flAccuracySpeed = (1.0 / MORTAR_CHARGE_ACCURACY_RATE);
		if ( m_pPowerBar->m_flFiringPower > 0.5 )
		{
			// Shots over halfway suffer an increased speed to the accuracy power, making accurate shots harder
			float flAdjustedPower = (m_pPowerBar->m_flFiringPower - 0.5) * 3.0;
			m_flAccuracySpeed += (m_pPowerBar->m_flFiringPower * flAdjustedPower);
		}

		m_pPowerBar->m_flPower = MAX( m_pPowerBar->m_flPower - ( m_flAccuracySpeed * gpGlobals->curtimeDelta), -0.25f);
		if ( m_pPowerBar->m_flPower <= -0.25 )
		{
			// Hit Min, fire mortar
			m_pPowerBar->m_flFiringAccuracy = m_pPowerBar->m_flPower;
			m_iFiringState = MORTAR_IDLE;

			FireMortar();
			m_flNextClick = gpGlobals->curtime + 0.25;
		}
		break;
	default:
		break;
	}

	// Draw the rotate icon if the player's rotating the mortar
	if ( m_bRotating )
	{
		vgui::Panel *pParent = GetClientModeNormal()->GetViewport();
		int parentWidth, parentHeight;
		pParent->GetSize(parentWidth, parentHeight);
		int iWidth = 64;
		int iHeight = 64;
		int iX = (parentWidth - iWidth) / 2;
		int iY = (parentHeight - 216);

		IMesh* pMesh = materials->GetDynamicMesh( true, NULL, NULL, m_pRotateIcon );

		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,0 );
		meshBuilder.Position3f( iX,iY,0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,0 );
		meshBuilder.Position3f( iX+iWidth, iY, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,1,1 );
		meshBuilder.Position3f( iX+iWidth, iY+iHeight, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Color3f( 1.0, 1.0, 1.0 );
		meshBuilder.TexCoord2f( 0,0,1 );
		meshBuilder.Position3f( iX, iY+iHeight, 0 );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}

	// Update the ground line if it's moved
	if ( !m_bCarried && (m_flPrevMortarYaw != m_flMortarYaw ) && 
		gpGlobals->curtime > m_flLastGroundlineUpdateTime + g_CVMortarGroundLineUpdateInterval.GetFloat() )
	{
		// Create the Ground line start & end points
		Vector vecForward;
		AngleVectors( QAngle( 0, m_flMortarYaw, 0 ), &vecForward );
		Vector vecStart = m_vecMortarOrigin + (vecForward * MORTAR_RANGE_MIN);

		float flRange = MORTAR_RANGE_MAX_INITIAL;
		if ( pPlayer->HasNamedTechnology( "mortar_range" ) )
			flRange = MORTAR_RANGE_MAX_UPGRADED;
		Vector vecEnd = m_vecMortarOrigin + (vecForward * flRange);

		m_pDarkLine->SetParameters( m_vecMortarOrigin, vecStart, Vector( 0.1,0.1,0.1 ), Vector( 0.1,0.1,0.1 ), 0.5, 22 );
		m_pGroundLine->SetParameters( vecStart, vecEnd, Vector(0,1,0), Vector(1,0,0), 0.5, 22 );

		m_flPrevMortarYaw = m_flMortarYaw;

		m_flLastGroundlineUpdateTime = gpGlobals->curtime;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Capture mouse input for mortar rotation
//-----------------------------------------------------------------------------
void C_WeaponMortar::OverrideMouseInput( float *x, float *y )
{
	if ( !m_bRotating )
		return;

	float flX = ( *x ) * 0.05f;

	m_flMortarYaw = anglemod(m_flMortarYaw - flX);

	*x = 0.0f;
	*y = 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: Weapon's been deployed
//-----------------------------------------------------------------------------
bool C_WeaponMortar::Deploy( void )
{
	if ( m_pDarkLine )
	{
		m_pDarkLine->SetVisible( true );
	}
	if ( m_pGroundLine )
	{
		m_pGroundLine->SetVisible( true );
	}

	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: Weapon's been holstered
//-----------------------------------------------------------------------------
bool C_WeaponMortar::Holster( C_BaseCombatWeapon *pSwitchingTo )
{
	if ( m_pDarkLine )
	{
		m_pDarkLine->SetVisible( false );
	}
	if ( m_pGroundLine )
	{
		m_pGroundLine->SetVisible( false );
	}

	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_WeaponMortar::FireMortar( void )
{
	// Clamp inaccuracy
	float flTempAcc = m_pPowerBar->m_flFiringAccuracy;
	if ( flTempAcc > 0.25 )
		flTempAcc = 0.25;
	else if ( flTempAcc < -0.25 )
		flTempAcc = -0.25;

	// HACKHACK: This is an amazingly bad way to do it. Replace this when the
	// client DLL can insert commands into the usercmds
	char szbuf[48];
	Q_snprintf( szbuf, sizeof( szbuf ), "mortar %0.2f %0.2f %0.2f\n", m_pPowerBar->m_flFiringPower, flTempAcc, m_flMortarYaw );
	engine->ClientCmd(szbuf);

	// Tell the power bar to reset soon
	m_pPowerBar->m_flReset = gpGlobals->curtime + 1.0;
}