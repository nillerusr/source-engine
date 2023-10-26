//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "c_basetempentity.h"
#include "networkstringtable_clientdll.h"
#include "effect_dispatch_data.h"
#include "c_te_effect_dispatch.h"
#include "tier1/keyvalues.h"
#include "toolframework_client.h"
#include "tier0/vprof.h"
#include "particles_new.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//-----------------------------------------------------------------------------
// CClientEffectRegistration registration
//-----------------------------------------------------------------------------

CClientEffectRegistration *CClientEffectRegistration::s_pHead = NULL;

CClientEffectRegistration::CClientEffectRegistration( const char *pEffectName, ClientEffectCallback fn )
{
	AssertMsg1( pEffectName[0] != '\"', "Error: Effect %s. "
		"Remove quotes around the effect name in DECLARE_CLIENT_EFFECT.\n", pEffectName );

	m_pEffectName = pEffectName;
	m_pFunction = fn;
	m_pNext = s_pHead;
	s_pHead = this;
}


//-----------------------------------------------------------------------------
// Purpose: EffectDispatch TE
//-----------------------------------------------------------------------------
class C_TEEffectDispatch : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEEffectDispatch, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEEffectDispatch( void );
	virtual			~C_TEEffectDispatch( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

public:
	CEffectData m_EffectData;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEEffectDispatch::C_TEEffectDispatch( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEEffectDispatch::~C_TEEffectDispatch( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void DispatchEffectToCallback( const char *pEffectName, const CEffectData &m_EffectData )
{
	// Built a faster lookup
	static CUtlStringMap< CClientEffectRegistration* > map;
	static bool bInitializedMap = false;
	if ( !bInitializedMap )
	{
		for ( CClientEffectRegistration *pReg = CClientEffectRegistration::s_pHead; pReg; pReg = pReg->m_pNext )
		{
			// If the name matches, call it
			if ( map.Defined( pReg->m_pEffectName ) )
			{
				Warning( "Encountered multiple different effects with the same name \"%s\"!\n", pReg->m_pEffectName );
				continue;
			}

			map[ pReg->m_pEffectName ] = pReg;
		}
		bInitializedMap = true;
	}

	// Look through all the registered callbacks
	UtlSymId_t nSym = map.Find( pEffectName );
	if ( nSym == UTL_INVAL_SYMBOL )
	{
		Warning("DispatchEffect: effect \"%s\" not found on client\n", pEffectName );
		return;
	}

	// NOTE: Here, we want to scope resource access to only be able to use
	// those resources specified as being dependencies of this effect
	g_pPrecacheSystem->LimitResourceAccess( DISPATCH_EFFECT, pEffectName );
	map[nSym]->m_pFunction( m_EffectData );

	// NOTE: Here, we no longer need to restrict resource access
	g_pPrecacheSystem->EndLimitedResourceAccess( );
}


//-----------------------------------------------------------------------------
// Record effects
//-----------------------------------------------------------------------------
static void RecordEffect( const char *pEffectName, const CEffectData &data )
{
	if ( !ToolsEnabled() )
		return;

	if ( clienttools->IsInRecordingMode() && ( (data.m_fFlags & EFFECTDATA_NO_RECORD) == 0 ) )
	{
		KeyValues *msg = new KeyValues( "TempEntity" );

		const char *pSurfacePropName = physprops->GetPropName( data.m_nSurfaceProp );

		char pName[1024];
		Q_snprintf( pName, sizeof(pName), "TE_DispatchEffect %s %s", pEffectName, pSurfacePropName );

 		msg->SetInt( "te", TE_DISPATCH_EFFECT );
 		msg->SetString( "name", pName );
		msg->SetFloat( "time", gpGlobals->curtime );
		msg->SetFloat( "originx", data.m_vOrigin.x );
		msg->SetFloat( "originy", data.m_vOrigin.y );
		msg->SetFloat( "originz", data.m_vOrigin.z );
		msg->SetFloat( "startx", data.m_vStart.x );
		msg->SetFloat( "starty", data.m_vStart.y );
		msg->SetFloat( "startz", data.m_vStart.z );
		msg->SetFloat( "normalx", data.m_vNormal.x );
		msg->SetFloat( "normaly", data.m_vNormal.y );
		msg->SetFloat( "normalz", data.m_vNormal.z );
		msg->SetFloat( "anglesx", data.m_vAngles.x );
		msg->SetFloat( "anglesy", data.m_vAngles.y );
		msg->SetFloat( "anglesz", data.m_vAngles.z );
		msg->SetInt( "flags", data.m_fFlags );
		msg->SetFloat( "scale", data.m_flScale );
		msg->SetFloat( "magnitude", data.m_flMagnitude );
		msg->SetFloat( "radius", data.m_flRadius );
		msg->SetString( "surfaceprop", pSurfacePropName );
		msg->SetInt( "color", data.m_nColor );
		msg->SetInt( "damagetype", data.m_nDamageType );
		msg->SetInt( "hitbox", data.m_nHitBox );
 		msg->SetString( "effectname", pEffectName );

		// FIXME: Need to write the attachment name here
 		msg->SetInt( "attachmentindex", data.m_nAttachmentIndex );

		// NOTE: Ptrs are our way of indicating it's an entindex
		msg->SetPtr( "entindex", (void*)data.entindex() );

		ToolFramework_PostToolMessage( HTOOLHANDLE_INVALID, msg );
		msg->deleteThis();
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEEffectDispatch::PostDataUpdate( DataUpdateType_t updateType )
{
	VPROF( "C_TEEffectDispatch::PostDataUpdate" );

	// Find the effect name.
	const char *pEffectName = g_StringTableEffectDispatch->GetString( m_EffectData.GetEffectNameIndex() );
	if ( pEffectName )
	{
		DispatchEffectToCallback( pEffectName, m_EffectData );
		RecordEffect( pEffectName, m_EffectData );
	}
}


IMPLEMENT_CLIENTCLASS_EVENT_DT( C_TEEffectDispatch, DT_TEEffectDispatch, CTEEffectDispatch )
	
	RecvPropDataTable( RECVINFO_DT( m_EffectData ), 0, &REFERENCE_RECV_TABLE( DT_EffectData ) )
			
END_RECV_TABLE()

//-----------------------------------------------------------------------------
// Client version of dispatch effect, for predicted weapons
//-----------------------------------------------------------------------------
void DispatchEffect( IRecipientFilter& filter, float delay, const char *pName, const CEffectData &data )
{
	if ( !te->SuppressTE( filter ) )
	{
		DispatchEffectToCallback( pName, data );
		RecordEffect( pName, data );
	}
}

void DispatchEffect( const char *pName, const CEffectData &data )
{
	CPASFilter filter( data.m_vOrigin );
	if ( !te->SuppressTE( filter ) )
	{
		DispatchEffectToCallback( pName, data );
		RecordEffect( pName, data );
	}
}


//-----------------------------------------------------------------------------
// Playback
//-----------------------------------------------------------------------------
void DispatchEffect( IRecipientFilter& filter, float delay, KeyValues *pKeyValues )
{
	CEffectData data;
	data.m_nMaterial = 0;
		  
	data.m_vOrigin.x = pKeyValues->GetFloat( "originx" );
	data.m_vOrigin.y = pKeyValues->GetFloat( "originy" );
	data.m_vOrigin.z = pKeyValues->GetFloat( "originz" );
	data.m_vStart.x = pKeyValues->GetFloat( "startx" );
	data.m_vStart.y = pKeyValues->GetFloat( "starty" );
	data.m_vStart.z = pKeyValues->GetFloat( "startz" );
	data.m_vNormal.x = pKeyValues->GetFloat( "normalx" );
	data.m_vNormal.y = pKeyValues->GetFloat( "normaly" );
	data.m_vNormal.z = pKeyValues->GetFloat( "normalz" );
	data.m_vAngles.x = pKeyValues->GetFloat( "anglesx" );
	data.m_vAngles.y = pKeyValues->GetFloat( "anglesy" );
	data.m_vAngles.z = pKeyValues->GetFloat( "anglesz" );
	data.m_fFlags = pKeyValues->GetInt( "flags" );
	data.m_flScale = pKeyValues->GetFloat( "scale" );
	data.m_flMagnitude = pKeyValues->GetFloat( "magnitude" );
	data.m_flRadius = pKeyValues->GetFloat( "radius" );
	const char *pSurfaceProp = pKeyValues->GetString( "surfaceprop" );
	data.m_nSurfaceProp = physprops->GetSurfaceIndex( pSurfaceProp );
	data.m_nDamageType = pKeyValues->GetInt( "damagetype" );
	data.m_nHitBox = pKeyValues->GetInt( "hitbox" );
	data.m_nColor = pKeyValues->GetInt( "color" );
	data.m_nAttachmentIndex = pKeyValues->GetInt( "attachmentindex" );

	// NOTE: Ptrs are our way of indicating it's an entindex
	ClientEntityHandle_t hWorld = ClientEntityList().EntIndexToHandle( 0 );
	data.m_hEntity = (int)pKeyValues->GetPtr( "entindex", (void*)hWorld.ToInt() );

	const char *pEffectName = pKeyValues->GetString( "effectname" );

	DispatchEffect( filter, 0.0f, pEffectName, data );
}


//-----------------------------------------------------------------------------
// Purpose: Displays an error effect in case of missing precache
//-----------------------------------------------------------------------------
void ErrorEffectCallback( const CEffectData &data )
{
	CSmartPtr<CNewParticleEffect> pEffect = CNewParticleEffect::Create( NULL, "error" );
	if ( pEffect->IsValid() )
	{
		pEffect->SetSortOrigin( data.m_vOrigin );
		pEffect->SetControlPoint( 0, data.m_vOrigin );
		pEffect->SetControlPoint( 1, data.m_vStart );
		Vector vecForward, vecRight, vecUp;
		AngleVectors( data.m_vAngles, &vecForward, &vecRight, &vecUp );
		pEffect->SetControlPointOrientation( 0, vecForward, vecRight, vecUp );
	}
}

DECLARE_CLIENT_EFFECT_BEGIN( Error, ErrorEffectCallback )
	PRECACHE( PARTICLE_SYSTEM, "error" )
DECLARE_CLIENT_EFFECT_END()
