//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client's CObjectSentrygun
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "bone_setup.h"
#include "CommanderOverlay.h"
#include "c_baseobject.h"
#include "C_Obj_SentryGun.h"
#include "tf_shareddefs.h"
#include "c_basetfplayer.h"
#include "ObjectControlPanel.h"
#include <vgui_controls/Button.h>

inline float UTIL_AngleMod(float a)
{
	return anglemod(a);
}

//-----------------------------------------------------------------------------
// Purpose: Base Sentrygun
//-----------------------------------------------------------------------------
BEGIN_RECV_TABLE_NOBASE(C_ObjectSentrygun, DT_SentrygunTeamOnlyVars)
	RecvPropInt(RECVINFO( m_iAmmo )),
END_RECV_TABLE()

IMPLEMENT_CLIENTCLASS_DT(C_ObjectSentrygun, DT_ObjectSentrygun, CObjectSentrygun)
	RecvPropInt( RECVINFO( m_iBaseTurnRate ) ),
	RecvPropEHandle(RECVINFO(m_hEnemy)),
	RecvPropDataTable( "teamonly", 0, 0, &REFERENCE_RECV_TABLE( DT_SentrygunTeamOnlyVars )),
	RecvPropInt(RECVINFO(m_bTurtled)),
 	RecvPropInt( RECVINFO( m_nAnimationParity ) ),
	RecvPropInt( RECVINFO( m_nOrientationParity ) ),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_ObjectSentrygun::C_ObjectSentrygun()
{
	m_fBoneXRotator = 0;
	m_fBoneYRotator = 0;
	m_iAmmo = 0;
	m_bTurtled = false;
	m_flStartedTurtlingAt = 0;
	m_flStartedUnTurtlingAt = 0;

	SetViewOffset( Vector(0,0,22) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectSentrygun::SetDormant( bool bDormant )
{
	BaseClass::SetDormant( bDormant );
	ENTITY_PANEL_ACTIVATE( "sentrygun", !bDormant );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_ObjectSentrygun::DrawModel( int flags )
{
	float flRealOriginZ = GetLocalOrigin().z;

	// If we're turtling, slide the model into the ground
	if ( m_bTurtled )
	{
		// How far down are we?
		float flTime = MIN( gpGlobals->curtime - m_flStartedTurtlingAt, SENTRY_TURTLE_TIME );
		float flPercent = 1 - (SENTRY_TURTLE_TIME - flTime) / SENTRY_TURTLE_TIME;

		// FIXME: This is totally wrong!!!
		Vector vNewOrigin = GetLocalOrigin();
		vNewOrigin.z -= (CollisionProp()->OBBSize().z * flPercent);
		SetLocalOrigin( vNewOrigin );
		InvalidateBoneCache();
	}
	else if ( !m_bTurtled )
	{
		if ( m_flStartedUnTurtlingAt )
		{
			float flTime = MIN( gpGlobals->curtime - m_flStartedUnTurtlingAt, SENTRY_TURTLE_TIME );
			float flPercent = (SENTRY_TURTLE_TIME - flTime) / SENTRY_TURTLE_TIME;
			
		// FIXME: This is totally wrong!!!
			Vector vNewOrigin = GetLocalOrigin();
			vNewOrigin.z -= (CollisionProp()->OBBSize().z * flPercent);
			SetLocalOrigin( vNewOrigin );
			InvalidateBoneCache();

			// Fully unturtled?
			if ( flTime >= SENTRY_TURTLE_TIME )
			{
				m_flStartedUnTurtlingAt = 0;
			}
		}
	}

	int drawn = BaseClass::DrawModel( flags );
	SetLocalOriginDim( Z_INDEX, flRealOriginZ );

	return drawn;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectSentrygun::PreDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PreDataUpdate( updateType );

	m_bLastTurtled = m_bTurtled;
	m_nLastAnimationParity = m_nAnimationParity;
	m_angPrevLocalAngles = GetLocalAngles();
	m_nPrevOrientationParity = 	m_nOrientationParity;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectSentrygun::PostDataUpdate( DataUpdateType_t updateType )
{
	BaseClass::PostDataUpdate( updateType );

	if ( m_bLastTurtled != m_bTurtled )
	{
		if ( m_bTurtled )
		{
			m_flStartedTurtlingAt = gpGlobals->curtime;
			m_flStartedUnTurtlingAt = 0;
		}
		else
		{
			m_flStartedUnTurtlingAt = gpGlobals->curtime;
			m_flStartedTurtlingAt = 0;
		}
	}

	if ( m_nLastAnimationParity != m_nAnimationParity )
 	{
 		SetCycle( 0.0f );
 	}

	bool changed = false;
	QAngle angleDiff;
	angleDiff = ( GetAbsAngles() - m_angPrevLocalAngles );
	for (int i = 0;i < 3; i++ )
	{
		angleDiff[i] = UTIL_AngleMod( angleDiff[ i ] );
	}

	if ( angleDiff.Length() > 0.1f )
	{
		changed = true;
	}
	if ( updateType == DATA_UPDATE_CREATED || changed )
	{
		// Orient it
		m_vecCurAngles.y = UTIL_AngleMod( GetLocalAngles().y );
		RecomputeOrientation();
	}
	else if ( m_nPrevOrientationParity != m_nOrientationParity )
	{
		if ( changed )
		{
			RecomputeOrientation();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectSentrygun::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		// Start thinking (Baseclass stops it)
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectSentrygun::FinishedBuilding( void )
{
	BaseClass::FinishedBuilding();

	EmitSound( "ObjectSentrygun.Activate" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_ObjectSentrygun::GetBoneControllers(float controllers[MAXSTUDIOBONECTRLS])
{
	studiohdr_t *pModel = modelinfo->GetStudiomodel( GetModel() );

	// When yaw preview is on,
	if (!IsPreviewingYaw())
	{
		Studio_SetController(pModel, 0, m_fBoneXRotator, controllers[0]);
	}
	else
	{
		// Bone rotation == 0 here to make it exactly match the preview
		Studio_SetController(pModel, 0, 0, controllers[0]);
	}

	Studio_SetController(pModel, 1, m_fBoneYRotator, controllers[1]);
	Studio_SetController(pModel, 2, m_fBoneYRotator, controllers[2]);
	Studio_SetController(pModel, 3, m_fBoneYRotator, controllers[3]);
}

//-----------------------------------------------------------------------------
// Purpose: This is called to get the initial builder yaw...
//-----------------------------------------------------------------------------
float C_ObjectSentrygun::GetInitialBuilderYaw()
{
	// Take the current rotation into account
	return GetAbsAngles().y + m_fBoneXRotator;
}

//-----------------------------------------------------------------------------
// Called when a rotation happens
//-----------------------------------------------------------------------------
void C_ObjectSentrygun::RecomputeOrientation( )
{
	m_iRightBound = UTIL_AngleMod( m_vecCurAngles.y - 50);
	m_iLeftBound = UTIL_AngleMod( m_vecCurAngles.y + 50);
	if ( m_iRightBound > m_iLeftBound )
	{
		m_iRightBound = m_iLeftBound;
		m_iLeftBound = UTIL_AngleMod( m_vecCurAngles.y - 50);
	}

	// Start it rotating
	m_vecGoalAngles.y = m_iRightBound;;
	m_vecGoalAngles.x = m_vecCurAngles.x = 0;

	m_fBoneXRotator = 0.0f;
	m_fBoneYRotator = 0.0f;

	m_bTurningRight = true;
}


//-----------------------------------------------------------------------------
// Purpose: Handle movement of the turret
//-----------------------------------------------------------------------------
bool C_ObjectSentrygun::MoveTurret(void)
{
	bool bMoved = 0;

	float turnrate = (float)(m_iBaseTurnRate) * 10.0f;
	turnrate *= gpGlobals->frametime;

	// any x movement?
	if ( m_vecCurAngles.x != m_vecGoalAngles.x )
	{
		float flDir = m_vecGoalAngles.x > m_vecCurAngles.x ? 1 : -1 ;

		m_vecCurAngles.x += 0.1 * (turnrate * 5) * flDir;

		// if we started below the goal, and now we're past, peg to goal
		if (flDir == 1)
		{
			if (m_vecCurAngles.x > m_vecGoalAngles.x)
				m_vecCurAngles.x = m_vecGoalAngles.x;
		} 
		else
		{
			if (m_vecCurAngles.x < m_vecGoalAngles.x)
				m_vecCurAngles.x = m_vecGoalAngles.x;
		}

		m_fBoneYRotator = m_vecCurAngles.x;

		bMoved = 1;
	}

	if ( m_vecCurAngles.y != m_vecGoalAngles.y )
	{
		float flDir = m_vecGoalAngles.y > m_vecCurAngles.y ? 1 : -1 ;
		float flDist = fabs(m_vecGoalAngles.y - m_vecCurAngles.y);
		bool bReversed = false;
		
		if (flDist > 180)
		{
			flDist = 360 - flDist;
			flDir = -flDir;
			bReversed = true;
		}

		if (m_hEnemy == NULL )
		{
			if (flDist > 30)
			{
				if (m_fTurnRate < turnrate * 20)
				{
					m_fTurnRate += turnrate;
				}
			}
			else
			{
				// Slow down
				if ( m_fTurnRate > (turnrate * 5) )
					m_fTurnRate -= turnrate;
			}
		}
		else
		{
			// When tracking enemies, move faster and don't slow
			if (flDist > 30)
			{
				if (m_fTurnRate < turnrate * 30)
				{
					m_fTurnRate += turnrate * 3;
				}
			}
		}

		m_vecCurAngles.y += 0.1 * m_fTurnRate * flDir;

		// if we passed over the goal, peg right to it now
		if (flDir == -1)
		{
			if ( (bReversed == false && m_vecGoalAngles.y > m_vecCurAngles.y) || (bReversed == true && m_vecGoalAngles.y < m_vecCurAngles.y) )
				m_vecCurAngles.y = m_vecGoalAngles.y;
		} 
		else
		{
			if ( (bReversed == false && m_vecGoalAngles.y < m_vecCurAngles.y) || (bReversed == true && m_vecGoalAngles.y > m_vecCurAngles.y) )
				m_vecCurAngles.y = m_vecGoalAngles.y;
		}

		if (m_vecCurAngles.y < 0)
			m_vecCurAngles.y += 360;
		else if (m_vecCurAngles.y >= 360)
			m_vecCurAngles.y -= 360;

		if (flDist < (0.05 * turnrate))
			m_vecCurAngles.y = m_vecGoalAngles.y;

		m_fBoneXRotator = m_vecCurAngles.y - UTIL_AngleMod( GetAbsAngles().y );

		bMoved = 1;
	}

	if ( !bMoved || !m_fTurnRate )
		m_fTurnRate = turnrate;

	if ( bMoved )
	{
		NetworkStateChanged();
	}

	return bMoved;
}

void C_ObjectSentrygun::ClientThink( void )
{
	// Turtling sentryguns don't think
	if ( IsTurtled() )
		return;

	if ( IsPlacing() || IsBuilding() )
		return;


	if ( m_hEnemy != NULL )
	{
		// Figure out where we're firing at
		Vector vecMid = EyePosition();
		Vector vecFireTarget = m_hEnemy->WorldSpaceCenter(); //  + vecMid; // BodyTarget( vecMid );
		Vector vecDirToEnemy = vecFireTarget - vecMid;
		QAngle angToTarget;
		VectorAngles(vecDirToEnemy, angToTarget);
		
		angToTarget.y = UTIL_AngleMod( angToTarget.y );
		if (angToTarget.x < -180)
			angToTarget.x += 360;
		if (angToTarget.x > 180)
			angToTarget.x -= 360;

		// now all numbers should be in [1...360]
		// pin to turret limitations to [-50...50]
		if (angToTarget.x > 50)
			angToTarget.x = 50;
		else if (angToTarget.x < -50)
			angToTarget.x = -50;

		m_vecGoalAngles.y = angToTarget.y;
		m_vecGoalAngles.x = angToTarget.x;

		MoveTurret();
		return;
	}

	// Rotate
	if ( !MoveTurret() )
	{
		// Play a sound occasionally
		if ( random->RandomFloat(0, 1) < 0.02 )
		{
			EmitSound( "ObjectSentrygun.Idle" );
		}

		// Switch rotation direction
		if (m_bTurningRight)
		{
			m_bTurningRight = false;
			m_vecGoalAngles.y = m_iLeftBound;
		}
		else
		{
			m_bTurningRight = true;
			m_vecGoalAngles.y = m_iRightBound;
		}

		// Randomly look up and down a bit
		if ( random->RandomFloat(0, 1) < 0.3 )
		{
			m_vecGoalAngles.x = (int)random->RandomFloat(-10,10);
		}
	}
}


//-----------------------------------------------------------------------------
// Control screen 
//-----------------------------------------------------------------------------
class CSentrygunControlPanel : public CRotatingObjectControlPanel
{
	DECLARE_CLASS( CSentrygunControlPanel, CRotatingObjectControlPanel );

public:
	CSentrygunControlPanel( vgui::Panel *parent, const char *panelName );
	virtual bool Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData );
	virtual void OnTick();
	virtual void OnCommand( const char *command );

	void		 AddAmmo( void );

private:
	vgui::Label		*m_pAmmoLabel;
};


DECLARE_VGUI_SCREEN_FACTORY( CSentrygunControlPanel, "sentrygun_control_panel" );


//-----------------------------------------------------------------------------
// Constructor: 
//-----------------------------------------------------------------------------
CSentrygunControlPanel::CSentrygunControlPanel( vgui::Panel *parent, const char *panelName )
	: BaseClass( parent, "CSentrygunControlPanel" ) 
{
}

//-----------------------------------------------------------------------------
// Initialization 
//-----------------------------------------------------------------------------
bool CSentrygunControlPanel::Init( KeyValues* pKeyValues, VGuiScreenInitData_t* pInitData )
{
	m_pAmmoLabel = new vgui::Label( this, "AmmoReadout", "" );

	if (!BaseClass::Init(pKeyValues, pInitData))
		return false;

	return true;
}

//-----------------------------------------------------------------------------
// Frame-based update
//-----------------------------------------------------------------------------
void CSentrygunControlPanel::OnTick()
{
	BaseClass::OnTick();
	
	C_BaseObject *pObj = GetOwningObject();
	if (!pObj)
		return;

	Assert( dynamic_cast<C_ObjectSentrygun*>(pObj) );
	C_ObjectSentrygun *pSentrygun = static_cast<C_ObjectSentrygun*>(pObj);

	char buf[256];
	int iAmmo = pSentrygun->GetAmmoLeft();
	if (iAmmo > 0)
	{
		Q_snprintf( buf, sizeof( buf ), "%d rounds left", iAmmo );
	}
	else
	{
		Q_snprintf( buf, sizeof( buf ), "OUT OF AMMO" );
	}
	m_pAmmoLabel->SetText( buf );
}

//-----------------------------------------------------------------------------
// Purpose: Handle ammo input to the sentrygun
//-----------------------------------------------------------------------------
void CSentrygunControlPanel::AddAmmo( void )
{
	C_BaseObject *pObj = GetOwningObject();
	if (pObj)
	{
		pObj->SendClientCommand( "addammo" );
	}
}

//-----------------------------------------------------------------------------
// Button click handlers
//-----------------------------------------------------------------------------
void CSentrygunControlPanel::OnCommand( const char *command )
{
	if (!Q_strnicmp(command, "AddAmmo", 7))
	{
		AddAmmo();
		return;
	}

	BaseClass::OnCommand(command);
}



//======================================================================================================
// SENTRYGUN TYPES
//======================================================================================================
// Purpose: Plasma sentrygun
//-----------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT(C_ObjectSentrygunPlasma, DT_ObjectSentrygunPlasma, CObjectSentrygunPlasma)
END_RECV_TABLE()

C_ObjectSentrygunPlasma::C_ObjectSentrygunPlasma()
{
}

//-----------------------------------------------------------------------------
// Purpose: Rocket launcher sentrygun
//-----------------------------------------------------------------------------
IMPLEMENT_CLIENTCLASS_DT(C_ObjectSentrygunRocketlauncher, DT_ObjectSentrygunRocketlauncher, CObjectSentrygunRocketlauncher)
END_RECV_TABLE()

C_ObjectSentrygunRocketlauncher::C_ObjectSentrygunRocketlauncher()
{
}


