//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmetimeframe.h"
#include "tier0/dbg.h"
#include "datamodel/dmelementfactoryhelper.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Class factory 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeTimeFrame, CDmeTimeFrame );


//-----------------------------------------------------------------------------
// Constructor, destructor 
//-----------------------------------------------------------------------------
void CDmeTimeFrame::OnConstruction()
{
	m_Start   .InitAndSet( this, "startTime",    0, FATTRIB_HAS_CALLBACK );
	m_Duration.InitAndSet( this, "durationTime", 0, FATTRIB_HAS_CALLBACK );
	m_Offset  .InitAndSet( this, "offsetTime",   0 );
	m_Scale   .InitAndSet( this, "scale",    1.0f );
}

void CDmeTimeFrame::OnDestruction()
{
}


void CDmeTimeFrame::OnAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::OnAttributeChanged( pAttribute );

	// notify parent clip that the time has changed
	if ( pAttribute == m_Start.GetAttribute() || pAttribute == m_Duration.GetAttribute() )
	{
		InvokeOnAttributeChangedOnReferrers( GetHandle(), pAttribute );
	}
}

void CDmeTimeFrame::SetEndTime( DmeTime_t endTime, bool bChangeDuration )
{
	if ( bChangeDuration )
	{
		m_Duration = endTime.GetTenthsOfMS() - m_Start;
	}
	else
	{
		m_Start = endTime.GetTenthsOfMS() - m_Duration;
	}
}

void CDmeTimeFrame::SetTimeScale( float flScale, DmeTime_t scaleCenter, bool bChangeDuration )
{
#ifdef _DEBUG
	DmeTime_t preCenterTime = ToChildMediaTime( scaleCenter, false );
#endif

	float ratio = m_Scale / flScale;
	int t = scaleCenter.GetTenthsOfMS() - m_Start;

	if ( bChangeDuration )
	{
		int newDuration = int( m_Duration * ratio );

		if ( scaleCenter.GetTenthsOfMS() != m_Start )
		{
			int newStart = int( ( m_Start - scaleCenter.GetTenthsOfMS() ) * ratio + scaleCenter.GetTenthsOfMS() );
			SetStartTime( DmeTime_t( newStart ) );
		}

		int newStart = m_Start;
		int newOffset = int( ( t + m_Offset ) * ratio + newStart - scaleCenter.GetTenthsOfMS() );
		SetTimeOffset( DmeTime_t( newOffset ) );
		SetDuration( DmeTime_t( newDuration ) );
	}
	else
	{
		int newOffset = int( ( t + m_Offset ) * ratio - t );
		SetTimeOffset( DmeTime_t( newOffset ) );
	}

	SetTimeScale( flScale );

#ifdef _DEBUG
	DmeTime_t postCenterTime = ToChildMediaTime( scaleCenter, false );
	Assert( abs( preCenterTime - postCenterTime ) <= DMETIME_MINDELTA );
#endif
}
