//========= Copyright © 1996-2008, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#include "cbase.h"

#if !defined( NO_ENTITY_PREDICTION )

#include "IGameSystem.h"
#include <typeinfo.h>
#include "cdll_int.h"

#include <memory.h>
#include <stdarg.h>
#include "tier0/dbg.h"
#include "tier1/strtools.h"
#include "predictioncopy.h"
#include "engine/ivmodelinfo.h"
#include "tier1/fmtstr.h"
#include "utlvector.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CValueChangeTracker::CValueChangeTracker() :
	m_bActive( false ),
	m_bTracking( false ),
	m_pTrackField( NULL )
{
	Q_memset( m_OrigValueBuf, 0, sizeof( m_OrigValueBuf ) );
}

C_BaseEntity *CValueChangeTracker::GetEntity()
{
	return m_hEntityToTrack.Get();
}

void CValueChangeTracker::GetValue( char *buf, size_t bufsize )
{
	buf[ 0 ] = 0;

	Assert( IsActive() );

	if ( !m_hEntityToTrack.Get() )
		return;

	if ( !m_pTrackField )
		return;

	void const *pInputData = ( const byte * )m_hEntityToTrack.Get() + m_pTrackField->flatOffset[ TD_OFFSET_NORMAL ];
	if ( !pInputData )
		return;

	int fieldType = m_pTrackField->fieldType;

	switch( fieldType )
	{
	default:
	case FIELD_EMBEDDED:
	case FIELD_MODELNAME:
	case FIELD_SOUNDNAME:
	case FIELD_CUSTOM:
	case FIELD_CLASSPTR:
	case FIELD_EDICT:
	case FIELD_POSITION_VECTOR:
	case FIELD_VOID:
	case FIELD_FUNCTION:
		{
			Assert( 0 );
		}
		break;
	case FIELD_FLOAT:
	case FIELD_TIME:
		Q_snprintf( buf, bufsize, "%f", *(float const *)pInputData );
		break;
	case FIELD_STRING:
		Q_snprintf( buf, bufsize, "%s", (char const*)pInputData );
		break;
	case FIELD_VECTOR:
		{
			const Vector *pVec = (const Vector *)pInputData;
			Q_snprintf( buf, bufsize, "%f %f %f", pVec->x, pVec->y, pVec->z );
		}
		break;
	case FIELD_QUATERNION:
		{
			const Quaternion *p = ( const Quaternion * )pInputData;
			Q_snprintf( buf, bufsize, "%f %f %f %f", p->x, p->y, p->z, p->w );
		}
		break;

	case FIELD_COLOR32:
		{
			const Color *color = ( const Color * )pInputData;
			Q_snprintf( buf, bufsize, "%d %d %d %d", color->r(), color->g(), color->b(), color->a() );
		}
		break;

	case FIELD_BOOLEAN:
		Q_snprintf( buf, bufsize, "%s", (*(const bool *)pInputData) ? "true" : "false" );
		break;
	case FIELD_INTEGER:
	case FIELD_TICK:
	case FIELD_MODELINDEX:
		Q_snprintf( buf, bufsize, "%i", *(const int*)pInputData );
		break;

	case FIELD_SHORT:
		Q_snprintf( buf, bufsize, "%i", (int)*(const short*)pInputData );
		break;

	case FIELD_CHARACTER:
		Q_snprintf( buf, bufsize, "%c", *(const char *)pInputData );
		break;

	case FIELD_EHANDLE:
		Q_snprintf( buf, bufsize, "eh 0x%p", (void const *)((const EHANDLE *)pInputData)->Get() );
		break;
	}
}

void CValueChangeTracker::StartTrack( char const *pchContext )
{
	if ( !IsActive() )
		return;

	m_strContext = pchContext;

	// Grab current value into scratch buffer
	GetValue( m_OrigValueBuf, sizeof( m_OrigValueBuf ) );

	m_bTracking = true;
}

void CValueChangeTracker::EndTrack()
{
	if ( !IsActive() )
		return;

	if ( !m_bTracking )
		return;
	m_bTracking = false;

	char final[ eChangeTrackerBufSize ];
	GetValue( final, sizeof( final ) );

	CUtlString *history = &m_History[ m_History.AddToTail() ];
	if ( Q_stricmp( final, m_OrigValueBuf ) )
	{
		history->Set( CFmtStr( "+++ %-20.20s:  %s (was %s)", m_strContext.String(), final, m_OrigValueBuf ) );
	}
	else
	{
		history->Set( CFmtStr( "    %-20.20s:  %s", m_strContext.String(), final ) );
	}

	Msg( ":%s\n", history->String() );
}

void CValueChangeTracker::ClearTracking()
{
	m_bActive = false;
	m_bTracking = false;
	m_hEntityToTrack = NULL;
	m_strFieldName = "";
	m_History.RemoveAll();
	m_pTrackField = NULL;
}

void CValueChangeTracker::SetupTracking( C_BaseEntity *ent, char const *pchFieldName )
{
	ClearTracking();

	// Find the field
	datamap_t *dmap = ent->GetPredDescMap();
	if ( !dmap )
	{
		Msg( "No prediction datamap_t for entity %d/%s\n", ent->index, ent->GetClassname() );
		return;
	}

	CPredictionCopy::PrepareDataMap( dmap );

	m_pTrackField = CPredictionCopy::FindFlatFieldByName( pchFieldName, dmap );
	if ( !m_pTrackField )
	{
		Msg( "No field '%s' in datamap_t for entity %d/%s\n", pchFieldName, ent->index, ent->GetClassname() );
		return;
	}

	m_hEntityToTrack = ent;
	m_strFieldName = pchFieldName;
	m_bActive = true;
}

void CValueChangeTracker::Reset()
{
	m_History.RemoveAll();
}

bool CValueChangeTracker::IsActive() const
{
	return m_bActive;
}

void CValueChangeTracker::Spew()
{
	if ( IsActive() )
	{
		for ( int i = 0 ; i < m_History.Count(); ++i )
		{
			Msg( "%s\n", m_History[ i ].String() );
		}
	}

	Reset();
}

static CValueChangeTracker g_ChangeTracker;
CValueChangeTracker *g_pChangeTracker = &g_ChangeTracker;

CON_COMMAND_F( cl_pred_track, "<entindex> <fieldname>:  Track changes to entity index entindex, for field fieldname.", 0 )
{
	g_pChangeTracker->ClearTracking();

	if ( args.ArgC() != 3 )
	{
		Msg( "cl_pred_track <entindex> <fieldname>\n" );
		return;
	}

	int iEntIndex = Q_atoi( args[1] );

	C_BaseEntity *ent = cl_entitylist->GetBaseEntity( iEntIndex );
	if ( !ent )
	{
		Msg( "cl_pred_track:  Unknown ent index %d\n", iEntIndex );
		return;
   	}

	g_pChangeTracker->SetupTracking( ent, args[2] );
}

#endif // NO_ENTITY_PREDICTION