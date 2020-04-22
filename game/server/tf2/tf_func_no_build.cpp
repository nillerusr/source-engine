//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"
#include "tf_func_no_build.h"
#include "tf_team.h"
#include "NDebugOverlay.h"

//-----------------------------------------------------------------------------
// Purpose: Defines an area from which resources can be collected
//-----------------------------------------------------------------------------
class CFuncNoBuild : public CBaseEntity
{
	DECLARE_CLASS( CFuncNoBuild, CBaseEntity );
public:
	CFuncNoBuild();

	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Activate( void );

	// Inputs
	void	InputSetActive( inputdata_t &inputdata );
	void	InputSetInactive( inputdata_t &inputdata );
	void	InputToggleActive( inputdata_t &inputdata );

	void	SetActive( bool bActive );
	bool	GetActive() const;

	bool	IsEmpty( void );
	bool	PointIsWithin( const Vector &vecPoint );

	bool	PreventsBuildOf( int iObjectType );

	// need to transmit to players who are in commander mode
	int		UpdateTransmitState() { return SetTransmitState( FL_EDICT_FULLCHECK ); }
	int 	ShouldTransmit( const CCheckTransmitInfo *pInfo );

private:
	bool	m_bActive;
	bool	m_bOnlyPreventTowers;
};

LINK_ENTITY_TO_CLASS( func_no_build, CFuncNoBuild);

BEGIN_DATADESC( CFuncNoBuild )

	// keys 
	DEFINE_KEYFIELD_NOT_SAVED( m_bOnlyPreventTowers, FIELD_BOOLEAN, "OnlyPreventTowers" ),

	// inputs
	DEFINE_INPUTFUNC( FIELD_VOID, "SetActive", InputSetActive ),
	DEFINE_INPUTFUNC( FIELD_VOID, "SetInactive", InputSetInactive ),
	DEFINE_INPUTFUNC( FIELD_VOID, "ToggleActive", InputToggleActive ),

END_DATADESC()


PRECACHE_REGISTER( func_no_build );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CFuncNoBuild::CFuncNoBuild()
{
}

//-----------------------------------------------------------------------------
// Purpose: Initializes the resource zone
//-----------------------------------------------------------------------------
void CFuncNoBuild::Spawn( void )
{
	SetSolid( SOLID_BSP );
	AddSolidFlags( FSOLID_TRIGGER );
	SetMoveType( MOVETYPE_NONE );
	AddEffects( EF_NODRAW );
	SetModel( STRING( GetModelName() ) );
	m_bActive = true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncNoBuild::Precache( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: See if we've got a gather point specified 
//-----------------------------------------------------------------------------
void CFuncNoBuild::Activate( void )
{
	BaseClass::Activate();
	SetActive( true );
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncNoBuild::InputSetActive( inputdata_t &inputdata )
{
	SetActive( true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncNoBuild::InputSetInactive( inputdata_t &inputdata )
{
	SetActive( false );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncNoBuild::InputToggleActive( inputdata_t &inputdata )
{
	if ( m_bActive )
	{
		SetActive( false );
	}
	else
	{
		SetActive( true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CFuncNoBuild::SetActive( bool bActive )
{
	m_bActive = bActive;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CFuncNoBuild::GetActive() const
{
	return m_bActive;
}


//-----------------------------------------------------------------------------
// Purpose: Return true if the specified point is within this zone
//-----------------------------------------------------------------------------
bool CFuncNoBuild::PointIsWithin( const Vector &vecPoint )
{
	return CollisionProp()->IsPointInBounds( vecPoint );
}

//-----------------------------------------------------------------------------
// Purpose: Return true if this zone prevents building of the specified type
//-----------------------------------------------------------------------------
bool CFuncNoBuild::PreventsBuildOf( int iObjectType )
{
	if ( m_bOnlyPreventTowers )
		return ( iObjectType == OBJ_TOWER );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Transmit this to all players who are in commander mode
//-----------------------------------------------------------------------------
int CFuncNoBuild::ShouldTransmit( const CCheckTransmitInfo *pInfo )
{
	// Team rules may tell us that we should
	CBaseEntity* pRecipientEntity = CBaseEntity::Instance( pInfo->m_pClientEnt );
	Assert( pRecipientEntity->IsPlayer() );
	
	CBasePlayer *pPlayer = (CBasePlayer*)pRecipientEntity;
	if ( pPlayer->GetTeam() )
	{
		if (pPlayer->GetTeam()->ShouldTransmitToPlayer( pPlayer, this ))
			return FL_EDICT_ALWAYS;
	}

	return FL_EDICT_DONTSEND;
}


//-----------------------------------------------------------------------------
// Purpose: Does a nobuild zone prevent us from building?
//-----------------------------------------------------------------------------
bool PointInNoBuild( CBaseObject *pObject, const Vector &vecBuildOrigin )
{
	// Find out whether we're in a resource zone or not
	CBaseEntity *pEntity = NULL;
	while ((pEntity = gEntList.FindEntityByClassname( pEntity, "func_no_build" )) != NULL)
	{
		CFuncNoBuild *pNoBuild = (CFuncNoBuild *)pEntity;

		// Are we within this no build?
		if ( pNoBuild->GetActive() && pNoBuild->PointIsWithin( vecBuildOrigin ) )
		{
			if ( pNoBuild->PreventsBuildOf( pObject->GetType() ) )
				return true;	// prevent building.
		}
	}

	return false; // Building should be ok.
}

//-----------------------------------------------------------------------------
// Purpose: Return true if a nobuild zone prevents building this object at the specified origin
//-----------------------------------------------------------------------------
bool NoBuildPreventsBuild( CBaseObject *pObject, const Vector &vecBuildOrigin )
{
	// Find out whether we're in a no build zone or not
	if ( PointInNoBuild( pObject, vecBuildOrigin ) )
	{
		return true;
	}

	return false;
}
