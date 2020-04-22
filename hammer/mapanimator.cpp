//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "GlobalFunctions.h"
#include "fgdlib/HelperInfo.h"
#include "MapAnimator.h"
#include "MapDoc.h"
#include "MapEntity.h"
#include "MapWorld.h"
#include "KeyFrame/KeyFrame.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS( CMapAnimator );


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapKeyFrame from a set
//			of string parameters from the FGD file.
// Input  : *pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapAnimator::CreateMapAnimator(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	return(new CMapAnimator);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapAnimator::CMapAnimator()
{
	m_CoordFrame.Identity();
	m_bCurrentlyAnimating = false;

	m_pCurrentKeyFrame = this;

	m_iPositionInterpolator = m_iRotationInterpolator = m_iTimeModifier = 0;
	m_nKeysChanged = 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapAnimator::~CMapAnimator()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass *
//-----------------------------------------------------------------------------
CMapClass *CMapAnimator::Copy(bool bUpdateDependencies)
{
	CMapAnimator *pNew = new CMapAnimator;
	pNew->CopyFrom(this, bUpdateDependencies);
	return pNew;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pObj - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapAnimator::CopyFrom(CMapClass *pObj, bool bUpdateDependencies)
{
	CMapKeyFrame::CopyFrom(pObj, bUpdateDependencies);
	CMapAnimator *pFrom = dynamic_cast<CMapAnimator*>( pObj );
	Assert( pFrom != NULL );

	memcpy( m_CoordFrame.Base(), pFrom->m_CoordFrame.Base(), sizeof(m_CoordFrame) );
	m_bCurrentlyAnimating = false;
	m_pCurrentKeyFrame = NULL;	// keyframe it's currently at

	m_iTimeModifier = pFrom->m_iTimeModifier;
	m_iPositionInterpolator = pFrom->m_iPositionInterpolator;
	m_iRotationInterpolator = pFrom->m_iRotationInterpolator;

	return this;
}

//-----------------------------------------------------------------------------
// Purpose: Returns a coordinate frame to render in, if the entity is animating
// Input  : matrix - 
// Output : returns true if a new matrix is returned, false if it is invalid
//-----------------------------------------------------------------------------
bool CMapAnimator::GetTransformMatrix( VMatrix& matrix )
{
	// are we currently animating?
	if ( m_bCurrentlyAnimating )
	{
		matrix = m_CoordFrame;
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Purpose: Notifies that the entity this is attached to has had a key change
// Input  : key - 
//			value - 
//-----------------------------------------------------------------------------
void CMapAnimator::OnParentKeyChanged( const char* key, const char* value )
{
	if ( !stricmp(key, "TimeModifier") )
	{
		m_iTimeModifier = atoi( value );
	}
	else if ( !stricmp(key, "PositionInterpolator") )
	{
		m_iPositionInterpolator = atoi( value );

		// HACK: Force everything in the path to update. Better to follow our path and update only it.
		UpdateAllDependencies(this);
	}
	else if ( !stricmp(key, "RotationInterpolator") )
	{
		m_iRotationInterpolator = atoi( value );
	}

	m_nKeysChanged++;

	CMapKeyFrame::OnParentKeyChanged( key, value );
}



//-----------------------------------------------------------------------------
// Purpose: Gets the current and previous keyframes for a given time.
// Input  : time - time into sequence
//			pKeyFrame - receives current keyframe pointer
//			pPrevKeyFrame - receives previous keyframe pointer
// Output : time remaining after the thing has reached this key
//-----------------------------------------------------------------------------
float CMapAnimator::GetKeyFramesAtTime( float time, CMapKeyFrame *&pKeyFrame, CMapKeyFrame *&pPrevKeyFrame )
{
	pKeyFrame = this; 
	pPrevKeyFrame = this;

	float outTime = time;

	while ( pKeyFrame )
	{
		if ( pKeyFrame->MoveTime() > outTime )
		{
			break;
		}

		// make sure this anim has enough time
		if ( pKeyFrame->MoveTime() < 0.01f )
		{
			outTime = 0.0f;
			break;
		}

		outTime -= pKeyFrame->MoveTime();
		
		pPrevKeyFrame = pKeyFrame;
		pKeyFrame = pKeyFrame->NextKeyFrame();
	}

	return outTime;
}


//-----------------------------------------------------------------------------
// Purpose: creates a new keyframe at the specified time
// Input  : time - 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
CMapEntity *CMapAnimator::CreateNewKeyFrame( float time )
{
	// work out where we are in the animation
	CMapKeyFrame *key;
	CMapKeyFrame *pPrevKey;
	float partialTime = GetKeyFramesAtTime( time, key, pPrevKey );

	CMapEntity *pCurrentEnt = dynamic_cast<CMapEntity*>( key->m_pParent );

	// check to see if we're direction on a key frame
	Vector posOffset( 0, 0, 0 );
	if ( partialTime == 0 )
	{
		// create this new key frame slightly after the current one, and offset
		posOffset[0] = 64;
	}

	// get our orientation and position at this time
	Vector vOrigin;
	QAngle angles;
	Quaternion qAngles;
	GetAnimationAtTime( key, pPrevKey, partialTime, vOrigin, qAngles, m_iPositionInterpolator, m_iRotationInterpolator );
	QuaternionAngles( qAngles, angles );

	// create the new map entity
	CMapEntity *pNewEntity = new CMapEntity;

	Vector newPos;
	VectorAdd( vOrigin, posOffset, newPos );
	pNewEntity->SetPlaceholder( TRUE );
	pNewEntity->SetOrigin( newPos );
	pNewEntity->SetClass( "keyframe_track" );

	char buf[128];
	sprintf( buf, "%f %f %f", angles[0], angles[1], angles[2] );
	pNewEntity->SetKeyValue( "angles", buf );

	// link it into the keyframe list

	// take over this existing next keyframe pointer
	const char *nextKeyName = pCurrentEnt->GetKeyValue( "NextKey" );
	if ( nextKeyName )
	{
		pNewEntity->SetKeyValue( "NextKey", nextKeyName );
	}
		
	// create a new unique name for this ent
	char newName[128];
	const char *oldName = pCurrentEnt->GetKeyValue( "targetname" );
	if ( !oldName || oldName[0] == 0 )
		oldName = "keyframe";

	CMapWorld *pWorld = GetWorldObject( this );
	if ( pWorld )
	{
		pWorld->GenerateNewTargetname( oldName, newName, sizeof( newName ), true, NULL );
		pNewEntity->SetKeyValue( "targetname", newName );

		// point the current entity at the newly created one
		pCurrentEnt->SetKeyValue( "NextKey", newName );

		// copy any relevant values
		const char *keyValue = pCurrentEnt->GetKeyValue( "parentname" );
		if ( keyValue )
			pNewEntity->SetKeyValue( "parentname", keyValue );

		keyValue = pCurrentEnt->GetKeyValue( "MoveSpeed" );
		if ( keyValue )
			pNewEntity->SetKeyValue( "MoveSpeed", keyValue );
	}
	
	return(pNewEntity);
}


//-----------------------------------------------------------------------------
// Purpose: stops CMapKeyframe from doing it's auto-connect behavior when cloning
// Input  : pClone - 
//-----------------------------------------------------------------------------
void CMapAnimator::OnClone( CMapClass *pClone, CMapWorld *pWorld, const CMapObjectList &OriginalList, CMapObjectList &NewList )
{
	CMapClass::OnClone( pClone, pWorld, OriginalList, NewList );
}

//-----------------------------------------------------------------------------
// Purpose: calculates the position of an animating object at a given time in
//			it's animation sequence
// Input  : animTime - 
//			*newOrigin - 
//			*newAngles - 
//-----------------------------------------------------------------------------
void CMapAnimator::GetAnimationAtTime( float animTime, Vector& newOrigin, Quaternion &newAngles )
{
	// setup the animation, given the time

	// get our new position & orientation
	float newTime, totalAnimTime = GetRemainingTime();
	animTime /= totalAnimTime;

	// don't use time modifier until we work out what we're going to do with it
	//Motion_CalculateModifiedTime( animTime, m_iTimeModifier, &newTime );
	newTime = animTime;

	// find out where we are in the keyframe sequence based on the time
	CMapKeyFrame *pPrevKeyFrame;
	float posTime = GetKeyFramesAtTime( newTime * totalAnimTime, m_pCurrentKeyFrame, pPrevKeyFrame );

	// find the position from that keyframe
	GetAnimationAtTime( m_pCurrentKeyFrame, pPrevKeyFrame, posTime, newOrigin, newAngles, m_iPositionInterpolator, m_iRotationInterpolator );
}


//-----------------------------------------------------------------------------
// Purpose: calculates the position of the animating object between two keyframes
// Input  : currentKey - 
//			pPrevKey - 
//			partialTime - 
//			newOrigin - 
//			newAngles - 
//			posInterpolator - 
//			rotInterpolator - 
//-----------------------------------------------------------------------------
void CMapAnimator::GetAnimationAtTime( CMapKeyFrame *currentKey, CMapKeyFrame *pPrevKey, float partialTime, Vector& newOrigin, Quaternion &newAngles, int posInterpolator, int rotInterpolator )
{
	// calculate the proportion of time to be spent on this keyframe
	float animTime;
	if ( currentKey->MoveTime() < 0.01 )
	{
		animTime = 1.0f;
	}
	else
	{
		animTime = partialTime / currentKey->MoveTime();
	}

	Assert( animTime >= 0.0f && animTime <= 1.0f );

	IPositionInterpolator *pInterp = currentKey->SetupPositionInterpolator( posInterpolator );

	// setup interpolation keyframes
	Vector keyOrigin;
	Quaternion keyAngles;
	pPrevKey->GetOrigin( keyOrigin );
	pPrevKey->GetQuatAngles( keyAngles );
	pInterp->SetKeyPosition( -1, keyOrigin );
	Motion_SetKeyAngles  ( -1, keyAngles );

	currentKey->GetOrigin( keyOrigin );
	currentKey->GetQuatAngles( keyAngles );
	pInterp->SetKeyPosition( 0, keyOrigin );
	Motion_SetKeyAngles  ( 0, keyAngles );

	currentKey->NextKeyFrame()->GetOrigin( keyOrigin );
	currentKey->NextKeyFrame()->GetQuatAngles( keyAngles );
	pInterp->SetKeyPosition( 1, keyOrigin );
	Motion_SetKeyAngles  ( 1, keyAngles );

	currentKey->NextKeyFrame()->NextKeyFrame()->GetOrigin( keyOrigin );
	currentKey->NextKeyFrame()->NextKeyFrame()->GetQuatAngles( keyAngles );
	pInterp->SetKeyPosition( 2, keyOrigin );
	Motion_SetKeyAngles  ( 2, keyAngles );

	// get our new interpolated position
	// HACK HACK - Hey Brian, look here!!!!
	Vector hackOrigin;
	pInterp->InterpolatePosition( animTime, hackOrigin );

	newOrigin[0] = hackOrigin[0];
	newOrigin[1] = hackOrigin[1];
	newOrigin[2] = hackOrigin[2];
	Motion_InterpolateRotation( animTime, rotInterpolator, newAngles );
}


//-----------------------------------------------------------------------------
// Purpose: Builds the animation transformation matrix for the entity if it's animating
//-----------------------------------------------------------------------------
void CMapAnimator::UpdateAnimation( float animTime )
{
	// only animate if the doc is animating and we're selected
	if ( !CMapDoc::GetActiveMapDoc()->IsAnimating() || !m_pParent->IsSelected() )
	{
		// we're not animating
		m_bCurrentlyAnimating = false;
		return;
	}

	m_bCurrentlyAnimating = true;

	Vector newOrigin;
	Quaternion newAngles;
	GetAnimationAtTime( animTime, newOrigin, newAngles );

	VMatrix mat, tmpMat;
	Vector ourOrigin;
	GetOrigin( ourOrigin );

	// build us a matrix
	// T(newOrigin)R(angle)T(-ourOrigin)
	m_CoordFrame.Identity() ;

	// transform back to the origin
	for ( int i = 0; i < 3; i++ )
	{
		m_CoordFrame[i][3] = -ourOrigin[i];
	}
	
	// Apply interpolated Rotation
	mat.Identity();
	QuaternionMatrix( newAngles, const_cast< matrix3x4_t & > ( mat.As3x4() ) );
	m_CoordFrame = m_CoordFrame * mat;
	
	// transform back to our new position
	mat.Identity();
	for ( int i = 0; i < 3; i++ )
	{
		mat[i][3] = newOrigin[i];
	}

	m_CoordFrame = m_CoordFrame * mat;
	
}


//-----------------------------------------------------------------------------
// Purpose: Rebuilds the line path between keyframe entities
//			samples the interpolator function to get an approximation of the curve
//-----------------------------------------------------------------------------
void CMapAnimator::RebuildPath( void )
{
	CMapWorld *pWorld = GetWorldObject( this );
	if ( !pWorld )
	{
		// Sometimes the object isn't linked back into the world yet when RebuildPath() 
		// is called... we will be get called again when needed, but may cause an incorrect
		// (linear-only) path to get drawn temporarily.
		return;
	}

	//
	// Build the path forward from the head. Keep a list of nodes we've visited to
	// use in detecting circularities.
	//
	CMapObjectList VisitedList;
	CMapKeyFrame *pCurKey = this;
	while ( pCurKey != NULL )
	{
		VisitedList.AddToTail( pCurKey );

		//
		// Attach ourselves as this keyframe's animator.
		//
		pCurKey->SetAnimator( this );

		//
		// Get the entity parent of this keyframe so we can query keyvalues.
		//
		CMapEntity *pCurEnt = dynamic_cast<CMapEntity *>( pCurKey->GetParent() );
		if ( !pCurEnt )
		{
			return;
		}

		//
		// Find the next keyframe in the path.
		//
		CMapEntity *pNextEnt = pWorld->FindEntityByName( pCurEnt->GetKeyValue( "NextKey" ) );
		CMapKeyFrame *pNextKey = NULL;

		if ( pNextEnt )
		{
			pNextKey = pNextEnt->GetChildOfType( ( CMapKeyFrame * )NULL );
			pCurKey->SetNextKeyFrame(pNextKey);
		}
		else
		{
			pCurKey->SetNextKeyFrame( NULL );
		}

		pCurKey = pNextKey;

		//
		// If we detect a circularity, stop.
		//
		if ( VisitedList.Find( pCurKey ) != -1 )
		{
			break;
		}
	}

	//
	// Now traverse the path again building the spline points, once again checking
	// the visited list for circularities.
	//
	VisitedList.RemoveAll();
	pCurKey = this;
	CMapKeyFrame *pPrevKey = this;
	while ( pCurKey != NULL )
	{
		VisitedList.AddToTail( pCurKey );

		pCurKey->BuildPathSegment(pPrevKey);

		pPrevKey = pCurKey;
		pCurKey = pCurKey->m_pNextKeyFrame;

		//
		// If we detect a circularity, stop.
		//
		if ( VisitedList.Find( pCurKey ) != -1 )
		{
			break;
		}
	}
}

