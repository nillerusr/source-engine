//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_basefourwheelvehicle.h"
#include "tf_movedata.h"
#include "ObjectControlPanel.h"
#include <vgui_controls/Label.h>
#include <vgui_controls/Button.h>
#include "vehicle_mortar_shared.h"
#include "vgui_rotation_slider.h"
#include <vgui/ISurface.h>
#include "vgui_basepanel.h"
#include "ground_line.h"
#include "hud_minimap.h"
#include "vgui_bitmapimage.h"
#include "iusesmortarpanel.h"

// How long it waits after you've changed the mortar's yaw to draw using the server's value.
#define CLIENT_MORTAR_YAW_COUNTDOWN	0.5


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class C_VehicleMortar : public C_BaseTFFourWheelVehicle, public IUsesMortarPanel
{
	DECLARE_CLASS( C_VehicleMortar, C_BaseTFFourWheelVehicle );
public:
	DECLARE_CLIENTCLASS();

	C_VehicleMortar();

	virtual void ReceiveMessage( int classID,  bf_read &msg );

	// Fire off a mortar.
	void FireMortar();

	virtual void ClientThink();

// C_BaseEntity overrides.
public:
	virtual void GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS]);

// IUsesMortarPanel
public:
	// Get the data from this mortar needed by the panel
	virtual void	GetMortarData( float *flClientMortarYaw, bool *bAllowedToFire, float *flPower, float *flFiringPower, float *flFiringAccuracy, int *iFiringState );
	virtual void	SendYawCommand( void );
	virtual void	ForceClientYawCountdown( float flTime );
	virtual void	ClickFire( void );

public:
	// Mortar firing info.
	int m_iFiringState; // One of the MORTAR_ defines.
	bool m_bMortarReloading;
	float m_flPower;
	bool m_bAllowedToFire;
	
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
	C_VehicleMortar( const C_VehicleMortar & );		// not defined, not accessible

};


IMPLEMENT_CLIENTCLASS_DT(C_VehicleMortar, DT_VehicleMortar, CVehicleMortar)
	RecvPropFloat( RECVINFO( m_flMortarYaw ) ),
	RecvPropFloat( RECVINFO( m_flMortarPitch ) ),
	RecvPropBool( RECVINFO( m_bAllowedToFire ) )
END_RECV_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------

C_VehicleMortar::C_VehicleMortar()
{
	m_iFiringState = MORTAR_IDLE;
	m_bMortarReloading = false;
	m_flPower = 0;
	
	m_flMortarYaw = 0;
	m_flClientMortarYaw = 0;
	m_flMortarPitch = 0;
	m_flForceClientYawCountdown = 0;
	m_bAllowedToFire = true;

	SetNextClientThink( CLIENT_THINK_ALWAYS );
}


void C_VehicleMortar::ReceiveMessage( int classID, bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		// message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_VehicleMortar::ClickFire()
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
void C_VehicleMortar::FireMortar()
{
	char cmd[512];
	Q_snprintf( cmd, sizeof( cmd ), "FireMortar %.2f %.2f", m_flFiringPower, m_flFiringAccuracy );
	SendClientCommand( cmd );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_VehicleMortar::SendYawCommand( void )
{
	char szbuf[48];
	Q_snprintf( szbuf, sizeof( szbuf ), "MortarYaw %0.2f\n", m_flClientMortarYaw );
	SendClientCommand( szbuf );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_VehicleMortar::ForceClientYawCountdown( float flTime )
{
	m_flForceClientYawCountdown = flTime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_VehicleMortar::GetMortarData( float *flClientMortarYaw, bool *bAllowedToFire, float *flPower, float *flFiringPower, float *flFiringAccuracy, int *iFiringState )
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
void C_VehicleMortar::ClientThink()
{
	m_flForceClientYawCountdown -= gpGlobals->frametime;
	if ( m_flForceClientYawCountdown <= 0 )
	{
		m_flClientMortarYaw = m_flMortarYaw;
	}
}


void C_VehicleMortar::GetBoneControllers( float controllers[MAXSTUDIOBONECTRLS])
{
	BaseClass::GetBoneControllers( controllers);

	controllers[0] = anglemod( m_flClientMortarYaw ) / 360.0;
	controllers[1] = anglemod( m_flMortarPitch ) / 360.0;
}


//-----------------------------------------------------------------------------
// CMortarMinimapPanel
//-----------------------------------------------------------------------------
CMortarMinimapPanel::CMortarMinimapPanel( vgui::Panel *pParent, const char *pElementName )
	: CMinimapPanel( pElementName )
{
	SetParent( pParent );

	m_bMouseDown = false;
	m_bFireButtonDown = false;
	m_LastX = m_LastY = -1;
	
	m_nTextureId = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_nTextureId, "hud/minimap/mortar_slider", true, false );

	m_nTextureId_CantFire = vgui::surface()->CreateNewTextureID();
	vgui::surface()->DrawSetTextureFile( m_nTextureId_CantFire, "hud/minimap/mortar_slider_cantfire", true, false );
}


CMortarMinimapPanel::~CMortarMinimapPanel()
{
}

void CMortarMinimapPanel::InitMortarMinimap( C_BaseEntity *pMortar )
{
	BaseClass::Init( NULL );

	m_hMortar = pMortar;
	
	m_MortarButtonUp.Init( GetVPanel(), "hud/minimap/icon_mortarbutton_up" );
	m_MortarButtonDown.Init( GetVPanel(), "hud/minimap/icon_mortarbutton_dn" );
	m_MortarButtonCantFire.Init( GetVPanel(), "hud/minimap/icon_mortarbutton_cantfire" );
	
	m_MortarDirectionImage.Init( GetVPanel(), "hud/minimap/icon_player_arrow" );
	m_MortarDirectionImage.SetColor( Color( 0, 255, 0, 255 ) );
}


C_BaseEntity *CMortarMinimapPanel::GetMortar() const
{
	return (C_BaseEntity*)m_hMortar;
}


void CMortarMinimapPanel::Paint()
{
	BaseClass::Paint();

	C_BaseEntity *pMortar = GetMortar();
	IUsesMortarPanel *pMortarInterface = dynamic_cast< IUsesMortarPanel* >( pMortar );
	if ( pMortar && pMortarInterface )
	{
		float flClientMortarYaw, flPower, flFiringPower, flFiringAccuracy;
		int iFiringState;
		bool bAllowedToFire; 
		pMortarInterface->GetMortarData( &flClientMortarYaw, &bAllowedToFire, &flPower, &flFiringPower, &flFiringAccuracy, &iFiringState );

		float yaw = flClientMortarYaw + 90;
	
		float x, y;
		if ( WorldToMinimap( MINIMAP_CLAMP, pMortar->GetAbsOrigin(), x, y ) )
		{
			int size = 20;
			
			BitmapImage *pImage = &m_MortarButtonCantFire;
			if ( bAllowedToFire )
			{				
				if ( m_bFireButtonDown )
					pImage = &m_MortarButtonDown;
				else
					pImage = &m_MortarButtonUp;
			}

			pImage->DoPaint( x-size/2, y-size/2, size, size, yaw );

			size = 40;
			m_MortarDirectionImage.DoPaint( x-size/2, y-size/2, size, size, pMortar->GetAbsAngles()[YAW] + 90 );

		
			// Draw the power bar.
			float flAngle = pMortar->GetAbsAngles()[YAW] + flClientMortarYaw;
			Vector vForward( -sin( DEG2RAD( flAngle ) ), cos( DEG2RAD( flAngle ) ), 0 );
			Vector vRight( vForward.y, -vForward.x, 0 );

			Vector vStartPoint = pMortar->GetAbsOrigin();
			Vector vEndPoint = vStartPoint + vForward * MORTAR_RANGE_MAX_INITIAL;
			Vector vInaccuracy = vEndPoint + vRight * (MORTAR_RANGE_MAX_INITIAL * MORTAR_INACCURACY_MAX_INITIAL);
			
			Vector2D vStart2D, vEnd2D, vInaccuracy2D;
			WorldToMinimap( MINIMAP_ALWAYS_ACCEPT, vStartPoint, vStart2D.x, vStart2D.y );
			WorldToMinimap( MINIMAP_ALWAYS_ACCEPT, vEndPoint, vEnd2D.x, vEnd2D.y );
			WorldToMinimap( MINIMAP_ALWAYS_ACCEPT, vInaccuracy, vInaccuracy2D.x, vInaccuracy2D.y );
			
			Vector2D vDir = vEnd2D - vStart2D;
			Vector2DNormalize( vDir );

			
			// These variables control the look.
			float flLength = (vEnd2D - vStart2D).Length();
			float flZeroT = 1.0f / 5;
			float flZero = flLength * flZeroT;
			
			float flFirePower = MAX( flPower, flFiringPower );

			float flStartFatness = 2;
			float flEndFatness = flStartFatness;
			
			float flScalePower = flFiringAccuracy;
			if ( iFiringState != MORTAR_IDLE )
				flScalePower = flPower;

			Vector2D vInaccuracyDir = vInaccuracy2D - vEnd2D;
			flEndFatness *= vInaccuracyDir.Length() * flScalePower * 0.4;
			flEndFatness = MAX( fabs( flEndFatness ), flStartFatness );
			
			Vector2D vPerp( vDir.y, -vDir.x );
			Vector2DNormalize( vPerp );
			
			Vector2D vStartPerp = vPerp * flStartFatness;
			Vector2D vEndPerp = vPerp * flEndFatness;

			
			// Draw the red-black power bars.
			vgui::ISurface *pSurface = vgui::surface();
			if ( bAllowedToFire )
				pSurface->DrawSetTexture( m_nTextureId );
			else
				pSurface->DrawSetTexture( m_nTextureId_CantFire );
			
			Vector2D vZeroPerp;
			Vector2DLerp( vStartPerp, vEndPerp, flZero / flLength, vZeroPerp );
						
			// Draw a black->red bar from zero to our current power.
			float flFirePowerDistance = RemapVal( flFirePower, 0, 1, flZero, flLength );
			Vector2D vPowerPerp;
			Vector2DLerp( vStartPerp, vEndPerp, RemapVal( flFirePowerDistance, flZero, flLength, flZeroT, 1 ), vPowerPerp );

			vgui::Vertex_t verts[4];
			verts[0].Init( vStart2D + vDir * flZero - vZeroPerp );
			verts[1].Init( verts[0].m_Position + vZeroPerp * 2 );

			verts[2].Init( vStart2D + vDir * flFirePowerDistance + vPowerPerp, Vector2D( flFirePower, 0 ) );
			verts[3] = verts[2];
			verts[3].m_Position -= vPowerPerp * 2;
			
			pSurface->DrawSetColor( 255, 255, 255, 255 );
			pSurface->DrawTexturedPolygon( 4, verts );


			// Draw the power slider.
			pSurface->DrawSetTexture( -1 );


			vgui::Vertex_t line[2];
			line[0].Init( vStart2D + vDir * flFirePowerDistance - vPowerPerp );
			line[1].Init( line[0].m_Position + vPowerPerp * 2 );
			pSurface->DrawTexturedLine( line[0], line[1] );



			// Draw a white outline.
			pSurface->DrawSetColor( 255, 255, 255, 255 );
			vgui::Vertex_t pts[4] =
			{
				vgui::Vertex_t( vStart2D - vStartPerp ),
				vgui::Vertex_t( vStart2D + vStartPerp ),
				vgui::Vertex_t( vEnd2D + vEndPerp ),
				vgui::Vertex_t( vEnd2D - vEndPerp )
			};
			pSurface->DrawTexturedPolyLine( pts, 4 );


			// Draw the zero line.
			line[0].Init( vStart2D + vDir * flZero - vZeroPerp );
			line[1].Init( line[0].m_Position + vZeroPerp * 2 );
			pSurface->DrawTexturedLine( line[0], line[1] );
		}
	}
}


void CMortarMinimapPanel::OnMousePressed( vgui::MouseCode code )
{
	BaseClass::OnMousePressed( code );

	if (code != vgui::MOUSE_LEFT)
		return;

	C_BaseEntity *pMortar = GetMortar();
	IUsesMortarPanel *pMortarInterface = dynamic_cast< IUsesMortarPanel* >( pMortar );
	if ( !pMortar || !pMortarInterface )
		return;

	float flClientMortarYaw, flPower, flFiringPower, flFiringAccuracy;
	int iFiringState;
	bool bAllowedToFire; 
	pMortarInterface->GetMortarData( &flClientMortarYaw, &bAllowedToFire, &flPower, &flFiringPower, &flFiringAccuracy, &iFiringState );
	
	// See if they clicked the "fire" button.
	float x, y;
	if ( WorldToMinimap( MINIMAP_ALWAYS_ACCEPT, pMortar->GetAbsOrigin(), x, y ) &&
		(Vector2D( x, y ) - Vector2D( m_LastX, m_LastY )).Length() <= 8 )
	{
		if ( bAllowedToFire )
		{
			// Treat it like they clicked the fire button.
			m_bFireButtonDown = true;
			pMortarInterface->ClickFire();
		}
	}
	else
	{
		pMortarInterface->ForceClientYawCountdown( 5000000 );
	}

	m_bMouseDown = true;
}


void CMortarMinimapPanel::OnCursorMoved( int x, int y )
{
	m_LastX = x;
	m_LastY = y;

	if ( !m_bMouseDown || m_bFireButtonDown )
		return;

	C_BaseEntity *pMortar = GetMortar();
	IUsesMortarPanel *pMortarInterface = dynamic_cast< IUsesMortarPanel* >( pMortar );
	if ( !pMortar || !pMortarInterface )
		return;

	float flClientMortarYaw, flPower, flFiringPower, flFiringAccuracy;
	int iFiringState;
	bool bAllowedToFire; 
	pMortarInterface->GetMortarData( &flClientMortarYaw, &bAllowedToFire, &flPower, &flFiringPower, &flFiringAccuracy, &iFiringState );

	float mortarX, mortarY;
	if ( WorldToMinimap( MINIMAP_ALWAYS_ACCEPT, pMortar->GetAbsOrigin(), mortarX, mortarY ) )
	{
		float flAngle = atan2( x - mortarX, mortarY - y );
		flClientMortarYaw = -anglemod( flAngle * 180.0f / M_PI + pMortar->GetAbsAngles()[YAW] );
	}
}


void CMortarMinimapPanel::OnMouseReleased( vgui::MouseCode code )
{
	BaseClass::OnMouseReleased( code );

	if ( code != vgui::MOUSE_LEFT )
		return;

	m_bMouseDown = false;
	
	if ( m_bFireButtonDown )
	{
		m_bFireButtonDown = false;
	}
	else
	{
		C_BaseEntity *pMortar = GetMortar();
		IUsesMortarPanel *pMortarInterface = dynamic_cast< IUsesMortarPanel* >( pMortar );
		if ( pMortar && pMortarInterface )
		{
			pMortarInterface->SendYawCommand();
			pMortarInterface->ForceClientYawCountdown( CLIENT_MORTAR_YAW_COUNTDOWN );
		}
	}
}


//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CVehicleMortarControlPanel : public CObjectControlPanel
{
	DECLARE_CLASS( CVehicleMortarControlPanel, CObjectControlPanel );

public:
	
	CVehicleMortarControlPanel( vgui::Panel *parent, const char *panelName );
	virtual ~CVehicleMortarControlPanel();
	
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnCommand( const char *command );
	C_VehicleMortar* GetMortar() const;


protected:

	virtual vgui::Panel* TickCurrentPanel();


private:

	void OnTickMainPanel();
	void OnTickDeployPanel( float flDeployTime );
	void OnTickGunnerPanel();

	void GetInRam( void );

	void StartDeploying();
	void StopDismantling();
	bool IsDeploying() const;
	bool IsUndeploying() const;
	bool IsDeployed() const;

private:
	vgui::Label		*m_pDriverLabel;
	vgui::Label		*m_pPassengerLabel;
	vgui::Button	*m_pOccupyButton;
	vgui::Button	*m_pCancelDeployButton;
	
	vgui::Label *m_pDeployMessageLabel; // says "Deployed in" or "Undeployed in"
	vgui::Label *m_pDeployTimeLabel; // says "N seconds"
	
	vgui::EditablePanel	*m_pDeployPanel;
	vgui::EditablePanel	*m_pGunnerPanel;

	CMortarMinimapPanel *m_pMinimapPanel;
	
	vgui::Label *m_pReloadingLabel;
};


DECLARE_VGUI_SCREEN_FACTORY( CVehicleMortarControlPanel, "vehicle_mortar_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CVehicleMortarControlPanel::CVehicleMortarControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CVehicleMortarControlPanel" ) 
{
	m_pDeployPanel = new CCommandChainingPanel( this, "DeployPanel" );
	m_pDeployPanel->SetZPos( -1 );

	m_pGunnerPanel = new CCommandChainingPanel( this, "GunnerPanel" );
	m_pGunnerPanel->SetZPos( -1 );

	m_pMinimapPanel = new CMortarMinimapPanel( m_pGunnerPanel, "MinimapPanel" );
	m_pMinimapPanel->SetZPos( 10 );
	m_pMinimapPanel->Init( NULL );
}


CVehicleMortarControlPanel::~CVehicleMortarControlPanel()
{
	delete m_pMinimapPanel;
}


//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CVehicleMortarControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	m_pDriverLabel = new vgui::Label( this, "DriverReadout", "" );
	m_pPassengerLabel = new vgui::Label( this, "PassengerReadout", "" );
	m_pOccupyButton = new vgui::Button( this, "OccupyButton", "Occupy" );
	
	m_pDeployMessageLabel = new vgui::Label( m_pDeployPanel, "DeployMessage", "" );
	m_pDeployTimeLabel = new vgui::Label( m_pDeployPanel, "DeployTime", "" );
	m_pCancelDeployButton = new vgui::Button( m_pDeployPanel, "CancelDeployButton", "" );

	m_pReloadingLabel = new vgui::Label( m_pGunnerPanel, "ReloadingLabel", "" );

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
	
	m_pDeployPanel->SetBounds( x, y, w, h );
	m_pDeployPanel->SetVisible( false );

	m_pGunnerPanel->SetBounds( x, y, w, h );
	m_pGunnerPanel->SetVisible( false );

	m_pReloadingLabel->SetVisible( false );
	return true;
}


//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------

vgui::Panel* CVehicleMortarControlPanel::TickCurrentPanel()
{
	C_VehicleMortar *pMortar = GetMortar();
	if ( !pMortar )
		return BaseClass::TickCurrentPanel();

	if ( IsUndeploying() )
	{
		OnTickDeployPanel( pMortar->GetDeployFinishTime() );
		return m_pDeployPanel;
	}
	else if ( IsDeploying() )
	{
		OnTickDeployPanel( pMortar->GetDeployFinishTime() );
		return m_pDeployPanel;
	}
	else if ( IsDeployed() )
	{
		OnTickGunnerPanel();
		return m_pGunnerPanel;
	}
	else
	{
		OnTickMainPanel();
		return BaseClass::TickCurrentPanel();
	}
}


C_VehicleMortar* CVehicleMortarControlPanel::GetMortar() const
{
	return dynamic_cast< C_VehicleMortar* >( GetOwningObject() );
}


void CVehicleMortarControlPanel::OnTickMainPanel()
{
	C_BaseObject *pObj = GetOwningObject();
	if (!pObj)
		return;

	ShowOwnerLabel( true );
	ShowHealthLabel( true );

	Assert( dynamic_cast<C_VehicleMortar*>(pObj) );
	C_VehicleMortar *pRam = static_cast<C_VehicleMortar*>(pObj);

	char buf[256];
	// Update the currently manned player label
	if ( pRam->GetDriverPlayer() )
	{
		Q_snprintf( buf, sizeof( buf ), "Driven by %s", pRam->GetDriverPlayer()->GetPlayerName() );
		m_pDriverLabel->SetText( buf );
		m_pDriverLabel->SetVisible( true );
	}
	else
	{
		m_pDriverLabel->SetVisible( false );
	}

	int nPassengerCount = pRam->GetPassengerCount();
	int nMaxPassengerCount = pRam->GetMaxPassengerCount();

	Q_snprintf( buf, sizeof( buf ), "Passengers %d/%d", nPassengerCount >= 1 ? nPassengerCount - 1 : 0, nMaxPassengerCount - 1 );
	m_pPassengerLabel->SetText( buf );

	// Update the get in button
	if ( pRam->IsPlayerInVehicle( C_BaseTFPlayer::GetLocalPlayer() ) ) 
	{
		m_pOccupyButton->SetEnabled( false );
		return;
	}

	if ( pRam->GetOwner() == C_BaseTFPlayer::GetLocalPlayer() )
	{
		if (nPassengerCount == nMaxPassengerCount)
		{
			// Owners can boot other players to get in
			C_BaseTFPlayer *pPlayer = static_cast<C_BaseTFPlayer*>(pRam->GetPassenger( VEHICLE_ROLE_DRIVER ));
			Q_snprintf( buf, sizeof( buf ), "Get In (Ejecting %s)", pPlayer->GetPlayerName() );
			m_pDriverLabel->SetText( buf );
			m_pOccupyButton->SetEnabled( true );
		}
		else
		{
			m_pOccupyButton->SetText( "Get In" );
			m_pOccupyButton->SetEnabled( true );
		}
	}
	else
	{
		m_pOccupyButton->SetText( "Get In" );
		m_pOccupyButton->SetEnabled( pRam->GetPassengerCount() < pRam->GetMaxPassengerCount() );
	}
}


void CVehicleMortarControlPanel::OnTickDeployPanel( float flDeployTime )
{
	ShowDismantleButton( false );

	// Update the countdown.
	int nSec = (int)(flDeployTime - gpGlobals->curtime + 0.5f);
	if (nSec < 0)
		nSec = 0;

	char buf[256];
	int nLen = Q_snprintf( buf, sizeof( buf ), "%d second", nSec );
	if (nSec != 1)
	{
		buf[nLen] = 's';
		++nLen;
		buf[nLen] = 0;
	}

	m_pDeployTimeLabel->SetText( buf );
}


void CVehicleMortarControlPanel::OnTickGunnerPanel()
{
	C_VehicleMortar *pMortar = GetMortar();
	if ( !pMortar )
		return;

	ShowOwnerLabel( false );
	ShowHealthLabel( false );

	m_pMinimapPanel->Repaint();

	// If it's reloading, tell the player
	m_pReloadingLabel->SetVisible( pMortar->m_bMortarReloading );
	if ( pMortar->m_bMortarReloading )
	{
		return;
	}

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
}


//-----------------------------------------------------------------------------
// Purpose: Handle clicking on the Occupy button
//-----------------------------------------------------------------------------
void CVehicleMortarControlPanel::GetInRam( void )
{
	SendToServerObject( "toggle_use" );
}

//-----------------------------------------------------------------------------
// Starts/stops deploying
//-----------------------------------------------------------------------------

bool CVehicleMortarControlPanel::IsDeploying() const
{
	C_VehicleMortar *pMortar = GetMortar();
	if ( !pMortar )
		return false;

	return !IsDeployed() && pMortar->GetDeployFinishTime() > 0.0f;
}

bool CVehicleMortarControlPanel::IsUndeploying() const
{
	C_VehicleMortar *pMortar = GetMortar();
	if ( !pMortar )
		return false;

	return IsDeployed() && pMortar->GetDeployFinishTime() > 0.0f;
}

bool CVehicleMortarControlPanel::IsDeployed() const
{
	C_VehicleMortar *pMortar = GetMortar();
	if ( pMortar )
		return pMortar->GetVehicleModeDeploy() == VEHICLE_MODE_DEPLOYED;
	else
		return false;
}

//-----------------------------------------------------------------------------
// Button click handlers
//-----------------------------------------------------------------------------
void CVehicleMortarControlPanel::OnCommand( const char *command )
{
	C_VehicleMortar *pMortar = GetMortar();
	if ( !pMortar )
		return;

	if (!Q_strnicmp(command, "Occupy", 7))
	{
		GetInRam();
		return;
	}
	else if ( !Q_stricmp( command, "Deploy" ) )
	{
		m_pDeployMessageLabel->SetText( "Deployed in" );
		m_pCancelDeployButton->SetVisible( true );

		SendToServerObject( "Deploy" );	// Tell the server.
	}
	else if ( !Q_stricmp( command, "CancelDeploy" ) )
	{
		SendToServerObject( command );
	}
	else if ( !Q_stricmp( command, "Undeploy" ) )
	{
		m_pDeployMessageLabel->SetText( "Undeployed in" );
		m_pCancelDeployButton->SetVisible( false );

		SendToServerObject( "Undeploy" );	// Tell the server.
	}
	else if ( !Q_stricmp( command, "FireMortar" ) )
	{
		pMortar->ClickFire();
	}

	BaseClass::OnCommand(command);
}

