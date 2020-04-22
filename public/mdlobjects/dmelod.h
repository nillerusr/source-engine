//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a lod
//
//===========================================================================//

#ifndef DMELOD_H
#define DMELOD_H

#ifdef _WIN32
#pragma once
#endif

#include "datamodel/dmelement.h"
#include "datamodel/dmattributevar.h"


//-----------------------------------------------------------------------------
// Forward Declarations
//-----------------------------------------------------------------------------
class CDmeModel;
class CDmeDag;
class CDmeCombinationOperator;

									 
//-----------------------------------------------------------------------------
// A class representing an attachment point
//-----------------------------------------------------------------------------
class CDmeLOD : public CDmElement
{
	DEFINE_ELEMENT( CDmeLOD, CDmElement );

public:
	// NOTE: It may be possible to eliminate the skeleton here
	// and assume the LOD always uses the root skeleton.
	CDmaElement< CDmeModel >	m_Model;
	CDmaElement< CDmeDag >		m_Skeleton;
	CDmaElement< CDmeCombinationOperator >	m_CombinationOperator;
	CDmaVar< float >			m_flSwitchMetric;
	CDmaVar< bool >				m_bNoFlex;	
	CDmaVar< bool >				m_bIsShadowLOD;	
};


#endif // DMELOD_H
