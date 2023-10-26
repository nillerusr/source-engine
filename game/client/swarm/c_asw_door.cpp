#include "cbase.h"
#include "c_asw_door.h"
#include <vgui/ISurface.h>
#include <vgui_controls/Panel.h>
#include "vgui/IInput.h"
#include "engine/ivdebugoverlay.h"
#include "asw_shareddefs.h"
#include "effect_dispatch_data.h"
#include "c_te_effect_dispatch.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

IMPLEMENT_CLIENTCLASS_DT( C_ASW_Door, DT_ASW_Door, CASW_Door )
	RecvPropFloat		(RECVINFO(m_flTotalSealTime)),
	RecvPropFloat		(RECVINFO(m_flCurrentSealTime)),
	RecvPropInt			(RECVINFO(m_iDoorStrength)),
	RecvPropInt			(RECVINFO(m_iDoorType)),	
	RecvPropInt			(RECVINFO(m_lifeState)),
	RecvPropInt			(RECVINFO(m_iHealth) ),
	RecvPropBool		(RECVINFO(m_bAutoOpen)),
	RecvPropBool		(RECVINFO(m_bBashable)),
	RecvPropBool		(RECVINFO(m_bShootable)),
	RecvPropBool		(RECVINFO(m_bCanCloseToWeld)),
	RecvPropBool		(RECVINFO(m_bRecommendedSeal)),
	RecvPropBool		(RECVINFO(m_bWasWeldedByMarine)),
	RecvPropFloat		(RECVINFO(m_fLastMomentFlipDamage)),
	RecvPropVector		(RECVINFO(m_vecClosedPosition)),
	RecvPropBool		(RECVINFO(m_bSkillMarineHelping)),	
END_RECV_TABLE()

bool C_ASW_Door::s_bLoadedSealedIconTexture = false;
bool C_ASW_Door::s_bLoadedFullySealedIconTexture = false;
int  C_ASW_Door::s_nSealedIconTextureID = -1;
int  C_ASW_Door::s_nFullySealedIconTextureID = -1;

// for mousing over door health
CUtlVector<C_ASW_Door*>	g_ClientDoorList;

C_ASW_Door::C_ASW_Door()
{
	m_fLastWeldedTime = 0.0f;

	Q_snprintf(m_szSealedIconTexture, sizeof(m_szSealedIconTexture), "vgui/swarm/UseIcons/UseIconDoorPartlySealed");

	g_ClientDoorList.AddToTail(this);
}

C_ASW_Door::~C_ASW_Door()
{
	g_ClientDoorList.FindAndRemove(this);
}

// returns how sealed this door is, from 0 to 1.0
float C_ASW_Door::GetSealAmount()
{
	if (m_flTotalSealTime <= 0)
		return 0;

	return (m_flCurrentSealTime/m_flTotalSealTime);
}

int C_ASW_Door::GetSealedIconTextureID()
{
	if (!s_bLoadedSealedIconTexture)
	{
		// load the portrait textures
		s_nSealedIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nSealedIconTextureID, m_szSealedIconTexture, true, false);
		s_bLoadedSealedIconTexture = true;
	}

	return s_nSealedIconTextureID;
}

int C_ASW_Door::GetFullySealedIconTextureID()
{
	if (!s_bLoadedFullySealedIconTexture)
	{
		s_nFullySealedIconTextureID = vgui::surface()->CreateNewTextureID();		
		vgui::surface()->DrawSetTextureFile( s_nFullySealedIconTextureID, "vgui/swarm/UseIcons/UseIconDoorFullySealed", true, false);
		s_bLoadedFullySealedIconTexture = true;
	}

	return s_nFullySealedIconTextureID;
}

const char* C_ASW_Door::GetSealedIconText()
{
	if ( gpGlobals->curtime > m_fLastWeldedTime + 1.0f )
	{
		if ( m_bWasWeldedByMarine )
		{
			if ( GetSealAmount() >= 1.0f )
			{
				return "#asw_door_fully_reinforced";
			}
			else if ( GetSealAmount() >= 0.5f )
			{
				return "#asw_door_reinforced";
			}
		}

		return "#asw_door_sealed";
	}

	if ( m_bUnwelding )
	{
		return "#asw_door_unsealing";
	}

	if ( GetSealAmount() >= 0.5f )
	{
		return "#asw_door_reinforcing";
	}

	return "#asw_door_sealing";
}


bool C_ASW_Door::IsOpen()
{
	Vector diff = GetAbsOrigin() - m_vecClosedPosition;
	float dist = diff.LengthSqr();
	return (dist > 2);	// 2 to allow for network rounding...
}

bool C_ASW_Door::IsMoving()
{
	Vector vel;
	EstimateAbsVelocity(vel);
	return vel.LengthSqr() > 0;
}

#define DOOR_CORNER_DISTANCE 62.0f
#define DOOR_HEIGHT 135.0f

Vector C_ASW_Door::GetWeldFacingPoint(C_BaseEntity* pOther)
{
	// work out which side of the door the marine is on
	Vector diff = pOther->GetAbsOrigin() - GetAbsOrigin();
	VectorNormalize(diff);
	QAngle angDoorFacing = GetAbsAngles();
	Vector vecDoorFacing = vec3_origin;
	AngleVectors(angDoorFacing, &vecDoorFacing);
	bool bFrontSide = (DotProduct(vecDoorFacing, diff) > 0);

	// depending on the side, get one of the corners
	angDoorFacing.y -= bFrontSide ? 81 : 102;
	AngleVectors(angDoorFacing, &vecDoorFacing);
	Vector result = GetAbsOrigin() + vecDoorFacing * DOOR_CORNER_DISTANCE;

	// correct by height
	result.z += DOOR_HEIGHT * (1.0f - GetSealAmount());

	return result;
}

Vector C_ASW_Door::GetSparkNormal( C_BaseEntity* pOther )
{
	// work out which side of the door the marine is on
	Vector diff = pOther->GetAbsOrigin() - GetAbsOrigin();
	VectorNormalize(diff);
	QAngle angDoorFacing = GetAbsAngles();
	Vector vecDoorFacing = vec3_origin;
	AngleVectors(angDoorFacing, &vecDoorFacing);
	bool bFrontSide = (DotProduct(vecDoorFacing, diff) > 0);

	if (bFrontSide)
		return vecDoorFacing;

	return -vecDoorFacing;
}

// checks the client door list for a door near this position
//  NOTE: currently only returns damaged doors
#define ASW_DOOR_NEAR_PADDING 20
C_ASW_Door* C_ASW_Door::GetDoorNear(Vector vecSrc)
{
	for (int i=0;i<g_ClientDoorList.Count();i++)
	{			
		C_ASW_Door *pEnt = g_ClientDoorList[i];
		int iDoorType;
		if (!pEnt || pEnt->GetHealth() <= 0 || pEnt->GetHealthFraction(iDoorType) >= 1.0f)
			continue;

		Vector mins, maxs;
		
		// get the size of the door
		pEnt->GetRenderBoundsWorldspace(mins,maxs);

		// pull out all 8 corners of this volume
		Vector worldPos[8];
		Vector screenPos[8];
		worldPos[0] = mins;
		worldPos[1] = mins; worldPos[1].x = maxs.x;
		worldPos[2] = mins; worldPos[2].y = maxs.y;
		worldPos[3] = mins; worldPos[3].z = maxs.z;
		worldPos[4] = mins;
		worldPos[5] = maxs; worldPos[5].x = mins.x;
		worldPos[6] = maxs; worldPos[6].y = mins.y;
		worldPos[7] = maxs; worldPos[7].z = mins.z;

		// convert them to screen space
		for (int k=0;k<8;k++)
		{
			debugoverlay->ScreenPosition( worldPos[k], screenPos[k] );
		}

		// find the rectangle bounding all screen space points
		Vector topLeft = screenPos[0];
		Vector bottomRight = screenPos[0];
		for (int k=0;k<8;k++)
		{
			topLeft.x = MIN(screenPos[k].x, topLeft.x);
			topLeft.y = MIN(screenPos[k].y, topLeft.y);
			bottomRight.x = MAX(screenPos[k].x, bottomRight.x);
			bottomRight.y = MAX(screenPos[k].y, bottomRight.y);
		}
		int BracketSize = 5;	// todo: set by screen res?

		// pad it a bit
		topLeft.x -= BracketSize * 2;
		topLeft.y -= BracketSize * 2;
		bottomRight.x += BracketSize * 2;
		bottomRight.y += BracketSize * 2;

		// check if the cursor is inside this
		int x, y;
		vgui::input()->GetCursorPos( x, y );

		if (x >= topLeft.x && x <= bottomRight.x &&
			y >= topLeft.y && y <= bottomRight.y)
		{
			return pEnt;
		}	
	}

	return NULL;
}

float C_ASW_Door::GetHealthFraction(int &iDoorType) const
{
	if ( m_iDoorStrength == 0 )	// indestructable
	{
		iDoorType = 2;
		return 1.0f;
	}
	if ( m_iDoorStrength == ASW_DOOR_NORMAL_HEALTH )		// normal
	{
		iDoorType = 0;
	}
	else
	{
		iDoorType = 1;		// reinforced
	}

	return ( static_cast< float >( MAX( 0, m_iHealth ) ) - MIN( 0.0f, m_fLastMomentFlipDamage ) ) / static_cast< float >( m_iDoorStrength );
}

void C_ASW_Door::OnDataChanged( DataUpdateType_t type )
{
	BaseClass::OnDataChanged( type );

	if ( !IsAlive() && VPhysicsGetObject())
	{
		VPhysicsDestroyObject();
	}

	if ( m_flOldSealTime != m_flCurrentSealTime )
	{
		m_fLastWeldedTime = gpGlobals->curtime;
		m_bUnwelding = ( m_flOldSealTime > m_flCurrentSealTime );
		m_flOldSealTime = m_flCurrentSealTime;
	}
}

void C_ASW_Door::ImpactTrace( trace_t *pTrace, int iDamageType, char *pCustomImpactName )
{
	Assert( pTrace->m_pEnt );

	CBaseEntity *pEntity = pTrace->m_pEnt;
 
	// Build the impact data
	CEffectData data;
	data.m_vOrigin = pTrace->endpos;
	data.m_vStart = pTrace->startpos;
	data.m_nSurfaceProp = pTrace->surface.surfaceProps;	
	if (!m_bShootable)
		data.m_nSurfaceProp = physprops->GetSurfaceIndex("metal");
	data.m_nDamageType = iDamageType;
	data.m_nHitBox = pTrace->hitbox;
#ifdef CLIENT_DLL
	data.m_hEntity = ClientEntityList().EntIndexToHandle( pEntity->entindex() );
#else
	data.m_nEntIndex = pEntity->entindex();
#endif

	// Send it on its way
	if ( !pCustomImpactName )
	{
		DispatchEffect( "Impact", data );
	}
	else
	{
		DispatchEffect( pCustomImpactName, data );
	}
}
