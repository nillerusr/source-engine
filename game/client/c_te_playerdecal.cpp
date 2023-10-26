//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "c_basetempentity.h"
#include "iefx.h"
#include "fx.h"
#include "decals.h"
#include "materialsystem/IMaterialSystem.h"
#include "filesystem.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/itexture.h"
#include "materialsystem/imaterialvar.h"
#include "precache_register.h"
#include "tier0/vprof.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar cl_playerspraydisable( "cl_playerspraydisable", "0", FCVAR_CLIENTDLL | FCVAR_ARCHIVE, "Disable player sprays." );

#ifndef _XBOX
PRECACHE_REGISTER_BEGIN( GLOBAL, PrecachePlayerDecal )
PRECACHE( MATERIAL, "decals/playerlogo01" )
#if !defined(HL2_DLL)
PRECACHE( MATERIAL, "decals/playerlogo02" )
PRECACHE( MATERIAL, "decals/playerlogo03" )
PRECACHE( MATERIAL, "decals/playerlogo04" )
PRECACHE( MATERIAL, "decals/playerlogo05" )
PRECACHE( MATERIAL, "decals/playerlogo06" )
PRECACHE( MATERIAL, "decals/playerlogo07" )
PRECACHE( MATERIAL, "decals/playerlogo08" )
PRECACHE( MATERIAL, "decals/playerlogo09" )
PRECACHE( MATERIAL, "decals/playerlogo10" )
PRECACHE( MATERIAL, "decals/playerlogo11" )
PRECACHE( MATERIAL, "decals/playerlogo12" )
PRECACHE( MATERIAL, "decals/playerlogo13" )
PRECACHE( MATERIAL, "decals/playerlogo14" )
PRECACHE( MATERIAL, "decals/playerlogo15" )
PRECACHE( MATERIAL, "decals/playerlogo16" )
PRECACHE( MATERIAL, "decals/playerlogo17" )
PRECACHE( MATERIAL, "decals/playerlogo18" )
PRECACHE( MATERIAL, "decals/playerlogo19" )
PRECACHE( MATERIAL, "decals/playerlogo20" )
PRECACHE( MATERIAL, "decals/playerlogo21" )
PRECACHE( MATERIAL, "decals/playerlogo22" )
PRECACHE( MATERIAL, "decals/playerlogo23" )
PRECACHE( MATERIAL, "decals/playerlogo24" )
PRECACHE( MATERIAL, "decals/playerlogo25" )
PRECACHE( MATERIAL, "decals/playerlogo26" )
PRECACHE( MATERIAL, "decals/playerlogo27" )
PRECACHE( MATERIAL, "decals/playerlogo28" )
PRECACHE( MATERIAL, "decals/playerlogo29" )
PRECACHE( MATERIAL, "decals/playerlogo30" )
PRECACHE( MATERIAL, "decals/playerlogo31" )
PRECACHE( MATERIAL, "decals/playerlogo32" )
PRECACHE( MATERIAL, "decals/playerlogo33" )
PRECACHE( MATERIAL, "decals/playerlogo34" )
PRECACHE( MATERIAL, "decals/playerlogo35" )
PRECACHE( MATERIAL, "decals/playerlogo36" )
PRECACHE( MATERIAL, "decals/playerlogo37" )
PRECACHE( MATERIAL, "decals/playerlogo38" )
PRECACHE( MATERIAL, "decals/playerlogo39" )
PRECACHE( MATERIAL, "decals/playerlogo40" )
PRECACHE( MATERIAL, "decals/playerlogo41" )
PRECACHE( MATERIAL, "decals/playerlogo42" )
PRECACHE( MATERIAL, "decals/playerlogo43" )
PRECACHE( MATERIAL, "decals/playerlogo44" )
PRECACHE( MATERIAL, "decals/playerlogo45" )
PRECACHE( MATERIAL, "decals/playerlogo46" )
PRECACHE( MATERIAL, "decals/playerlogo47" )
PRECACHE( MATERIAL, "decals/playerlogo48" )
PRECACHE( MATERIAL, "decals/playerlogo49" )
PRECACHE( MATERIAL, "decals/playerlogo40" )
PRECACHE( MATERIAL, "decals/playerlogo41" )
PRECACHE( MATERIAL, "decals/playerlogo42" )
PRECACHE( MATERIAL, "decals/playerlogo43" )
PRECACHE( MATERIAL, "decals/playerlogo44" )
PRECACHE( MATERIAL, "decals/playerlogo45" )
PRECACHE( MATERIAL, "decals/playerlogo46" )
PRECACHE( MATERIAL, "decals/playerlogo47" )
PRECACHE( MATERIAL, "decals/playerlogo48" )
PRECACHE( MATERIAL, "decals/playerlogo49" )
PRECACHE( MATERIAL, "decals/playerlogo50" )
PRECACHE( MATERIAL, "decals/playerlogo51" )
PRECACHE( MATERIAL, "decals/playerlogo52" )
PRECACHE( MATERIAL, "decals/playerlogo53" )
PRECACHE( MATERIAL, "decals/playerlogo54" )
PRECACHE( MATERIAL, "decals/playerlogo55" )
PRECACHE( MATERIAL, "decals/playerlogo56" )
PRECACHE( MATERIAL, "decals/playerlogo57" )
PRECACHE( MATERIAL, "decals/playerlogo58" )
PRECACHE( MATERIAL, "decals/playerlogo59" )
PRECACHE( MATERIAL, "decals/playerlogo60" )
PRECACHE( MATERIAL, "decals/playerlogo61" )
PRECACHE( MATERIAL, "decals/playerlogo62" )
PRECACHE( MATERIAL, "decals/playerlogo63" )
PRECACHE( MATERIAL, "decals/playerlogo64" )
#endif
PRECACHE_REGISTER_END()
#endif

//-----------------------------------------------------------------------------
// Purpose: Player Decal TE
//-----------------------------------------------------------------------------
class C_TEPlayerDecal : public C_BaseTempEntity
{
public:
	DECLARE_CLASS( C_TEPlayerDecal, C_BaseTempEntity );
	DECLARE_CLIENTCLASS();

					C_TEPlayerDecal( void );
	virtual			~C_TEPlayerDecal( void );

	virtual void	PostDataUpdate( DataUpdateType_t updateType );

	virtual void	Precache( void );

public:
	int				m_nPlayer;
	Vector			m_vecOrigin;
	int				m_nEntity;
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEPlayerDecal::C_TEPlayerDecal( void )
{
	m_nPlayer = 0;
	m_vecOrigin.Init();
	m_nEntity = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
C_TEPlayerDecal::~C_TEPlayerDecal( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void C_TEPlayerDecal::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : filter - 
//			delay - 
//			pos - 
//			player - 
//			entity - 
//-----------------------------------------------------------------------------
void TE_PlayerDecal( IRecipientFilter& filter, float delay,
	const Vector* pos, int player, int entity  )
{
	if ( cl_playerspraydisable.GetBool() )
		return;

	// No valid target?
	C_BaseEntity *ent = cl_entitylist->GetEnt( entity );
	if ( !ent )
		return;

	// Find player logo for shooter
	player_info_t info;
	engine->GetPlayerInfo( player, &info );

	// Doesn't have a logo
	 if ( !info.customFiles[0] )	
		return;

	IMaterial *logo = materials->FindMaterial( VarArgs("decals/playerlogo%2.2d", player), TEXTURE_GROUP_DECAL );
	if ( IsErrorMaterial( logo ) )
		return;

	char logohex[ 16 ];
	Q_binarytohex( (byte *)&info.customFiles[0], sizeof( info.customFiles[0] ), logohex, sizeof( logohex ) );

	// See if logo has been downloaded.
	char texname[ 512 ];
	Q_snprintf( texname, sizeof( texname ), "temp/%s", logohex );
	char fulltexname[ 512 ];
	Q_snprintf( fulltexname, sizeof( fulltexname ), "materials/temp/%s.vtf", logohex );

	if ( !filesystem->FileExists( fulltexname ) )
	{
		char custname[ 512 ];
		Q_snprintf( custname, sizeof( custname ), "downloads/%s.dat", logohex );
		// it may have been downloaded but not copied under materials folder
		if ( !filesystem->FileExists( custname ) )
			return; // not downloaded yet

		// copy from download folder to materials/temp folder
		// this is done since material system can access only materials/*.vtf files

		if ( !engine->CopyFile( custname, fulltexname) )
			return;
	}

	ITexture *texture = materials->FindTexture( texname, TEXTURE_GROUP_DECAL );
	if ( IsErrorTexture( texture ) ) 
	{
		return; // not found 
	}

	// Update the texture used by the material if need be.
	bool bFound = false;
	IMaterialVar *pMatVar = logo->FindVar( "$basetexture", &bFound );
	if ( bFound && pMatVar )
	{
		if ( pMatVar->GetTextureValue() != texture )
		{
			pMatVar->SetTextureValue( texture );
			logo->RefreshPreservingMaterialVars();
		}
	}

	color32 rgbaColor = { 255, 255, 255, 255 };
	effects->PlayerDecalShoot( 
		logo, 
		(void *)player,
		entity, 
		ent->GetModel(), 
		ent->GetAbsOrigin(), 
		ent->GetAbsAngles(), 
		*pos, 
		0, 
		0,
		rgbaColor );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bool - 
//-----------------------------------------------------------------------------
void C_TEPlayerDecal::PostDataUpdate( DataUpdateType_t updateType )
{
#ifndef _XBOX
	VPROF( "C_TEPlayerDecal::PostDataUpdate" );

	// Decals disabled?
	if ( !r_decals.GetBool() )
		return;

	CLocalPlayerFilter filter;
	TE_PlayerDecal(  filter, 0.0f, &m_vecOrigin, m_nPlayer, m_nEntity );
#endif
}

IMPLEMENT_CLIENTCLASS_EVENT_DT(C_TEPlayerDecal, DT_TEPlayerDecal, CTEPlayerDecal)
	RecvPropVector( RECVINFO(m_vecOrigin)),
	RecvPropInt( RECVINFO(m_nEntity)),
	RecvPropInt( RECVINFO(m_nPlayer)),
END_RECV_TABLE()
