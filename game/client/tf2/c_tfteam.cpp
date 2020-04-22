//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Client side C_TFTeam class
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_tfteam.h"
#include "c_basetfplayer.h"
#include "engine/IEngineSound.h"
#include "hud.h"
#include "recvproxy.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define ATTACK_NOTIFICATION_TIME 10.0f

#define MESSAGESTRINGID_RESOURCESHARVESTED	(IMessageChars::MESSAGESTRINGID_BASE+0)

//-----------------------------------------------------------------------------
// Purpose: RecvProxy that converts the Team's object UtlVector to entindexes
//-----------------------------------------------------------------------------
void RecvProxy_ObjectList( const CRecvProxyData *pData, void *pStruct, void *pOut )
{
	C_TFTeam *pTeam = (C_TFTeam*)pStruct;
	CBaseHandle *pHandle = (CBaseHandle*)(&(pTeam->m_aObjects[ pData->m_iElement ])); 
	RecvProxy_IntToEHandle( pData, pStruct, pHandle );
}


void RecvProxyArrayLength_TeamObjects( void *pStruct, int objectID, int currentArrayLength )
{
	C_TFTeam *pTeam = (C_TFTeam*)pStruct;
	
	if ( pTeam->m_aObjects.Count() != currentArrayLength )
	{
		pTeam->m_aObjects.SetSize( currentArrayLength );
	}
}


IMPLEMENT_CLIENTCLASS_DT(C_TFTeam, DT_TFTeam, CTFTeam)
	RecvPropFloat( RECVINFO(m_fResources) ), 
	RecvPropFloat( RECVINFO(m_fPotentialResources) ),
	RecvPropInt( RECVINFO(m_bHaveZone) ),
	
	RecvPropArray2( 
		RecvProxyArrayLength_TeamObjects,
		RecvPropInt( "object_array_element", 0, SIZEOF_IGNORE, 0, RecvProxy_ObjectList ), 
		MAX_OBJECTS_PER_TEAM, 
		0, 
		"object_array"
		),
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TFTeam::C_TFTeam()
{
	m_LastAttackNotificationTime = -10000;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TFTeam::~C_TFTeam()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TFTeam::NotifyBaseUnderAttack( const Vector &vecPosition, bool bPlaySound, bool bForce )
{
	float currentTime = gpGlobals->curtime;
	if ( bForce || (currentTime - m_LastAttackNotificationTime > ATTACK_NOTIFICATION_TIME) )
	{
		m_LastAttackNotificationTime = currentTime;

		if (GetLocalTeam() == this)
		{
			// Play a sound.
			if (bPlaySound)
			{
				CLocalPlayerFilter filter;
				C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "TFTeam.NotifyBaseUnderAttack" );
			}

			MinimapCreateTempTrace( "minimap_under_attack", MINIMAP_PERSONAL_ORDERS, vecPosition );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float C_TFTeam::GetTeamResources( void )
{
	return m_fResources;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float C_TFTeam::GetPotentialTeamResources( void )
{
	return m_fPotentialResources;
}


//-----------------------------------------------------------------------------
// Purpose: Return true if this team controls a zone of the specified resource type
//-----------------------------------------------------------------------------
bool C_TFTeam::GetHaveZone( void )
{
	return m_bHaveZone;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int C_TFTeam::GetNumObjects( int iObjectType )
{
	// Asking for a count of a specific object type?
	if ( iObjectType > 0 )
	{
		int iCount = 0;
		for ( int i = 0; i < GetNumObjects(); i++ )
		{
			CBaseObject *pObject = GetObject(i);
			if ( pObject && pObject->GetType() == iObjectType )
			{
				iCount++;
			}
		}
		return iCount;
	}

	return m_aObjects.Count();
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_BaseObject *C_TFTeam::GetObject( int iIndex )
{
	Assert( iIndex >= 0 && iIndex < m_aObjects.Size() );
	return static_cast<C_BaseObject*>((C_BaseEntity*)m_aObjects[iIndex]);
}

bool C_TFTeam::IsObjectValid( int iIndex )
{
	Assert( iIndex >= 0 && iIndex < m_aObjects.Size() );
	return ((C_BaseEntity const*)m_aObjects[iIndex] != NULL);
}


//-----------------------------------------------------------------------------
// Purpose: Receive a spawn message from the server
//-----------------------------------------------------------------------------
void C_TFTeam::ReceiveMessage( int classID,  bf_read &msg )
{
	if ( classID != GetClientClass()->m_ClassID )
	{
		// message is for subclass
		BaseClass::ReceiveMessage( classID, msg );
		return;
	}

	int iAmount = msg.ReadLong();

	int iYPos;

	// Use appropriate string based on client's team
	char sResourceString[256];
	if ( GetLocalTeam() == this )
	{
		itoa(iAmount, sResourceString, 10 );
		Q_strncat( sResourceString, " RESOURCES HARVESTED!", sizeof(sResourceString), COPY_ALL_CHARACTERS );
		iYPos = ScreenHeight() / 4;
	}
	else
	{
		char sAmount[256];
		itoa(iAmount, sAmount, 10 );
		Q_strncpy( sResourceString, "ENEMY TEAM HAS HARVESTED ", sizeof(sResourceString) );
		Q_strncat( sResourceString, sAmount, sizeof(sResourceString), COPY_ALL_CHARACTERS );
		Q_strncat( sResourceString, " RESOURCES!", sizeof(sResourceString), COPY_ALL_CHARACTERS );
		iYPos = (ScreenHeight() / 4) + 32;
	}

	// Clear out any old strings with this ID.
	messagechars->RemoveStringsByID( MESSAGESTRINGID_RESOURCESHARVESTED );

	// Print the string
	int width, height;
	messagechars->GetStringLength( g_hFontTrebuchet24, &width, &height, sResourceString );
	messagechars->DrawStringForTime( 
		5.0, 
		g_hFontTrebuchet24, 
		(ScreenWidth() - width) / 2, 
		iYPos, 
		192, 
		192, 
		192, 
		255, 
		sResourceString,
		MESSAGESTRINGID_RESOURCESHARVESTED );
}

