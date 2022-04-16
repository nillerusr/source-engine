//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Rising liquid that acts as a one-way portal
//
// $NoKeywords: $
//===========================================================================//

#include "cbase.h"
#include "func_liquidportal.h"
#include "portal_player.h"
#include "isaverestore.h"
#include "saverestore_utlvector.h"

LINK_ENTITY_TO_CLASS( func_liquidportal, CFunc_LiquidPortal );

BEGIN_DATADESC( CFunc_LiquidPortal )
	DEFINE_FIELD( m_hLinkedPortal, FIELD_EHANDLE ),
	DEFINE_FIELD( m_bFillInProgress, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_fFillStartTime, FIELD_TIME ),
	DEFINE_FIELD( m_fFillEndTime, FIELD_TIME ),
	DEFINE_FIELD( m_matrixThisToLinked, FIELD_VMATRIX ),
	DEFINE_UTLVECTOR( m_hTeleportList, FIELD_EHANDLE ),
	DEFINE_UTLVECTOR( m_hLeftToTeleportThisFill, FIELD_EHANDLE ),

	DEFINE_KEYFIELD( m_strInitialLinkedPortal, FIELD_STRING, "InitialLinkedPortal" ),
	DEFINE_KEYFIELD( m_fFillTime, FIELD_FLOAT, "FillTime" ),

	// Inputs
	DEFINE_INPUTFUNC( FIELD_STRING, "SetLinkedLiquidPortal",	InputSetLinkedLiquidPortal ),
	DEFINE_INPUTFUNC( FIELD_FLOAT, "SetFillTime",		InputSetFillTime ),
	DEFINE_INPUTFUNC( FIELD_VOID, "StartFilling",		InputStartFilling ),

	DEFINE_INPUTFUNC( FIELD_VOID, "AddActivatorToTeleportList",		InputAddActivatorToTeleportList ),
	DEFINE_INPUTFUNC( FIELD_VOID, "RemoveActivatorFromTeleportList",		InputRemoveActivatorFromTeleportList ),

	DEFINE_FUNCTION( CBaseEntity::Think ),
END_DATADESC()


IMPLEMENT_SERVERCLASS_ST( CFunc_LiquidPortal, DT_Func_LiquidPortal )
	SendPropEHandle( SENDINFO(m_hLinkedPortal) ),
	SendPropFloat( SENDINFO(m_fFillStartTime) ),
	SendPropFloat( SENDINFO(m_fFillEndTime) ),
END_SEND_TABLE()


CFunc_LiquidPortal::CFunc_LiquidPortal( void )
: m_bFillInProgress( false )
{
	m_matrixThisToLinked.Identity(); //Zero space is a bad place. No heroes to face, but we need 1's in this case.

}

void CFunc_LiquidPortal::Spawn( void )
{
	BaseClass::Spawn();

	SetSolid( SOLID_VPHYSICS );
	SetSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );

	CBaseEntity *pBaseEnt = gEntList.FindEntityByName( NULL, STRING(m_strInitialLinkedPortal) );
	Assert( (pBaseEnt == NULL) || (dynamic_cast<CFunc_LiquidPortal *>(pBaseEnt) != NULL) );
	SetLinkedLiquidPortal( (CFunc_LiquidPortal *)pBaseEnt );
	SetThink( &CFunc_LiquidPortal::Think );
}

void CFunc_LiquidPortal::Activate( void )
{
	BaseClass::Activate();

	SetSolid( SOLID_VPHYSICS );
	SetSolidFlags( FSOLID_NOT_SOLID );
	SetMoveType( MOVETYPE_NONE );
	SetModel( STRING( GetModelName() ) );

	ComputeLinkMatrix(); //collision origin may have changed during activation

	SetThink( &CFunc_LiquidPortal::Think );

	for( int i = m_hLeftToTeleportThisFill.Count(); --i >= 0; )
	{
		CBaseEntity *pEnt = m_hLeftToTeleportThisFill[i].Get();

		if( pEnt && pEnt->IsPlayer() )
		{
			((CPortal_Player *)pEnt)->m_hSurroundingLiquidPortal = this;
		}
	}
}

int	CFunc_LiquidPortal::Save( ISave &save )
{
	if( !BaseClass::Save( save ) )
		return 0;

	save.StartBlock( "LiquidPortal" );

	short iTeleportListCount = m_hTeleportList.Count();
	save.WriteShort( &iTeleportListCount );

	if( iTeleportListCount != 0 )
		save.WriteEHandle( m_hTeleportList.Base(), iTeleportListCount );

	short iLeftToTeleportThisFillCount = m_hLeftToTeleportThisFill.Count();
	save.WriteShort( &iLeftToTeleportThisFillCount );

	if( iLeftToTeleportThisFillCount != 0 )
		save.WriteEHandle( m_hLeftToTeleportThisFill.Base(), iLeftToTeleportThisFillCount );

	save.EndBlock();
	
	return 1;
}

int	CFunc_LiquidPortal::Restore( IRestore &restore )
{
	m_hTeleportList.RemoveAll();
	m_hLeftToTeleportThisFill.RemoveAll();

	if( !BaseClass::Restore( restore ) )
		return 0;

	char szBlockName[SIZE_BLOCK_NAME_BUF];
	restore.StartBlock( szBlockName );

	if( !FStrEq( szBlockName, "LiquidPortal" ) ) //loading a save without liquid portal save data
		return 1;

	short iTeleportListCount;
	restore.ReadShort( &iTeleportListCount );

	if( iTeleportListCount != 0 )
	{
		m_hTeleportList.SetCount( iTeleportListCount );
		restore.ReadEHandle( m_hTeleportList.Base(), iTeleportListCount );
	}

	short iLeftToTeleportThisFillCount;
	restore.ReadShort( &iLeftToTeleportThisFillCount );

	if( iLeftToTeleportThisFillCount != 0 )
	{
		m_hLeftToTeleportThisFill.SetCount( iLeftToTeleportThisFillCount );
		restore.ReadEHandle( m_hLeftToTeleportThisFill.Base(), iLeftToTeleportThisFillCount );		
	}

	restore.EndBlock();

	return 1;	
}


void CFunc_LiquidPortal::InputSetLinkedLiquidPortal( inputdata_t &inputdata )
{
	CBaseEntity *pBaseEnt = gEntList.FindEntityByName( NULL, inputdata.value.String() );
	Assert( (pBaseEnt == NULL) || (dynamic_cast<CFunc_LiquidPortal *>(pBaseEnt) != NULL) );
	SetLinkedLiquidPortal( (CFunc_LiquidPortal *)pBaseEnt );
}

void CFunc_LiquidPortal::InputSetFillTime( inputdata_t &inputdata )
{
	m_fFillTime = inputdata.value.Float();
}

void CFunc_LiquidPortal::InputStartFilling( inputdata_t &inputdata )
{
	AssertMsg( m_fFillEndTime <= gpGlobals->curtime, "Fill already in progress." );
	m_fFillStartTime = gpGlobals->curtime;
	m_fFillEndTime = gpGlobals->curtime + m_fFillTime;
	m_bFillInProgress = true;

	//reset the teleport list for this fill
	m_hLeftToTeleportThisFill.RemoveAll();
	m_hLeftToTeleportThisFill.AddVectorToTail( m_hTeleportList );

	SetNextThink( gpGlobals->curtime + TICK_INTERVAL );
}

void CFunc_LiquidPortal::InputAddActivatorToTeleportList( inputdata_t &inputdata )
{
	if( inputdata.pActivator == NULL )
		return;

	for( int i = m_hTeleportList.Count(); --i >= 0; )
	{
		if( m_hTeleportList[i].Get() == inputdata.pActivator )
			return; //only have 1 reference of each entity
	}

	m_hTeleportList.AddToTail( inputdata.pActivator );
	if( m_bFillInProgress )
		m_hLeftToTeleportThisFill.AddToTail( inputdata.pActivator );
	
	if( inputdata.pActivator->IsPlayer() )
		((CPortal_Player *)inputdata.pActivator)->m_hSurroundingLiquidPortal = this;
}

void CFunc_LiquidPortal::InputRemoveActivatorFromTeleportList( inputdata_t &inputdata )
{
	if( inputdata.pActivator == NULL )
		return;

	for( int i = m_hTeleportList.Count(); --i >= 0; )
	{
		if( m_hTeleportList[i].Get() == inputdata.pActivator )
		{
			m_hTeleportList.FastRemove( i );

			if( inputdata.pActivator->IsPlayer() && (((CPortal_Player *)inputdata.pActivator)->m_hSurroundingLiquidPortal.Get() == this) )
				((CPortal_Player *)inputdata.pActivator)->m_hSurroundingLiquidPortal = NULL;
			
			if( m_bFillInProgress )
			{
				//remove from the list for this fill as well
				for( int j = m_hLeftToTeleportThisFill.Count(); --j >= 0; )
				{
					if( m_hLeftToTeleportThisFill[j].Get() == inputdata.pActivator )
					{
						m_hLeftToTeleportThisFill.FastRemove( j );
						break;
					}
				}
			}

			return;
		}
	}
}

void CFunc_LiquidPortal::SetLinkedLiquidPortal( CFunc_LiquidPortal *pLinked )
{
	CFunc_LiquidPortal *pCurrentLinkedPortal = m_hLinkedPortal.Get();
	if( pCurrentLinkedPortal == pLinked )
		return;

	if( pCurrentLinkedPortal != NULL )
	{
		m_hLinkedPortal = NULL;
		pCurrentLinkedPortal->SetLinkedLiquidPortal( NULL );
	}

	m_hLinkedPortal = pLinked;
	if( pLinked != NULL )
		pLinked->SetLinkedLiquidPortal( this );

	ComputeLinkMatrix();
}

void CFunc_LiquidPortal::ComputeLinkMatrix( void )
{
	CFunc_LiquidPortal *pLinkedPortal = m_hLinkedPortal.Get();
	if( pLinkedPortal )
	{
		VMatrix matLocalToWorld, matLocalToWorldInv, matRemoteToWorld;

		matLocalToWorld = EntityToWorldTransform();
		matRemoteToWorld = pLinkedPortal->EntityToWorldTransform();
		
		MatrixInverseTR( matLocalToWorld, matLocalToWorldInv );
		m_matrixThisToLinked = matRemoteToWorld * matLocalToWorldInv;

		MatrixInverseTR( m_matrixThisToLinked, pLinkedPortal->m_matrixThisToLinked );
	}
	else
	{
		m_matrixThisToLinked.Identity();
	}
}

void CFunc_LiquidPortal::TeleportImmersedEntity( CBaseEntity *pEntity )
{
	if( pEntity == NULL )
		return;

	if( pEntity->IsPlayer() )
	{
		CPortal_Player *pEntityAsPlayer = (CPortal_Player *)pEntity;

		Vector vNewOrigin = m_matrixThisToLinked * pEntity->GetAbsOrigin();
		QAngle qNewAngles = TransformAnglesToWorldSpace( pEntityAsPlayer->EyeAngles(), m_matrixThisToLinked.As3x4() );
		Vector vNewVelocity = m_matrixThisToLinked.ApplyRotation( pEntity->GetAbsVelocity() );

		pEntity->Teleport( &vNewOrigin, &qNewAngles, &vNewVelocity );

		pEntityAsPlayer->m_hSurroundingLiquidPortal = m_hLinkedPortal;
	}
	else
	{
		Vector vNewOrigin = m_matrixThisToLinked * pEntity->GetAbsOrigin();
		QAngle qNewAngles = TransformAnglesToWorldSpace( pEntity->GetAbsAngles(), m_matrixThisToLinked.As3x4() );
		Vector vNewVelocity = m_matrixThisToLinked.ApplyRotation( pEntity->GetAbsVelocity() );

		pEntity->Teleport( &vNewOrigin, &qNewAngles, &vNewVelocity );
	}
}

void CFunc_LiquidPortal::Think( void )
{
	if( m_bFillInProgress )
	{
		if( gpGlobals->curtime < m_fFillEndTime )
		{
			float fInterp = ((gpGlobals->curtime - m_fFillStartTime) / (m_fFillEndTime - m_fFillStartTime));
			Vector vMins, vMaxs;
			GetCollideable()->WorldSpaceSurroundingBounds( &vMins, &vMaxs );
			vMaxs.z = vMins.z + ((vMaxs.z - vMins.z) * fInterp);

			for( int i = m_hLeftToTeleportThisFill.Count(); --i >= 0; )
			{
				CBaseEntity *pEntity = m_hLeftToTeleportThisFill[i].Get();
				if( pEntity == NULL )
					continue;

				Vector vEntMins, vEntMaxs;
				pEntity->GetCollideable()->WorldSpaceSurroundingBounds( &vEntMins, &vEntMaxs );

				if( vEntMaxs.z <= vMaxs.z )
				{
					TeleportImmersedEntity( pEntity );
					m_hLeftToTeleportThisFill.FastRemove( i );
				}
			}

			SetNextThink( gpGlobals->curtime + TICK_INTERVAL );
		}
		else
		{
			//teleport everything that's left in the list
			for( int i = m_hLeftToTeleportThisFill.Count(); --i >= 0; )
			{
				TeleportImmersedEntity( m_hLeftToTeleportThisFill[i].Get() );
			}

			m_hLeftToTeleportThisFill.RemoveAll();
			m_bFillInProgress = false;
		}
	}		
}




