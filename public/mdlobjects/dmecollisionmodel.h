//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a collision model
//
//===========================================================================//

#ifndef DMECOLLISIONMODEL_H
#define DMECOLLISIONMODEL_H

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


//-----------------------------------------------------------------------------
// A class representing an attachment point
//-----------------------------------------------------------------------------
class CDmeCollisionModel : public CDmElement
{
	DEFINE_ELEMENT( CDmeCollisionModel, CDmElement );

public:
	CDmaVar< float >			m_flMass;
	CDmaVar< bool >				m_bAutomaticMassComputation;	
	CDmaVar< float >			m_flInertia;
	CDmaVar< float >			m_flDamping;
	CDmaVar< float >			m_flRotationalDamping;
	CDmaVar< float >			m_flDrag;
	CDmaVar< float >			m_flRollingDrag;
	CDmaVar< int >				m_nMaxConvexPieces;	
	CDmaVar< bool >				m_bRemove2D;	
	CDmaVar< bool >				m_bConcavePerJoint;	
	CDmaVar< float >			m_flWeldPositionTolerance;
	CDmaVar< float >			m_flWeldNormalTolerance;
	CDmaVar< bool >				m_bConcave;	
	CDmaVar< bool >				m_bForceMassCenter;	
	CDmaVar< Vector >			m_vecMassCenter;
	CDmaVar< bool >				m_bNoSelfCollisions;	
	CDmaVar< bool >				m_bAssumeWorldSpace;	
	CDmaString					m_SurfaceProperty;
};


#endif // DMECOLLISIONMODEL_H
