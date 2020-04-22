//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef EDGEPROPERTIES_H
#define EDGEPROPERTIES_H
#ifdef _WIN32
#pragma once
#endif

class CCurveData;
class ChoreoScene;
class CChoreoEvent;
class CFlexAnimationTrack;

#include "basedialogparams.h"

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
struct CEdgePropertiesParams : public CBaseDialogParams
{
	// GlobalEvent descriptive name
	char			m_szName[ 256 ];

	void			SetFromFlexTrack( CFlexAnimationTrack *track );
	void			ApplyToTrack( CFlexAnimationTrack *track );

	void			SetFromCurve( CCurveData *ramp );
	void			ApplyToCurve( CCurveData *ramp );

	bool			m_bActive[ 2 ];
	int				m_InterpolatorType[ 2 ];
	float			m_flValue[ 2 ];
};

int EdgeProperties( CEdgePropertiesParams *params );

#endif // EDGEPROPERTIES_H
