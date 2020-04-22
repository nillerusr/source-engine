//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Dme version of a skeletal model (gets compiled into a MDL)
//
//=============================================================================
#include "movieobjects/dmemodel.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datacache/imdlcache.h"
#include "materialsystem/imaterialsystem.h"
#include "tier2/tier2.h"
#include "studio.h"
#include "materialsystem/imaterialsystemhardwareconfig.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeModel, CDmeModel );


//-----------------------------------------------------------------------------
// Stack of DmeModels currently being rendered. Used to set up render state
//-----------------------------------------------------------------------------
CUtlStack< CDmeModel * > CDmeModel::s_ModelStack;
static CUtlVector< matrix3x4_t > s_PoseToWorld;


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeModel::OnConstruction()
{
	m_JointTransforms.Init( this, "jointTransforms" );
	m_BaseStates.Init( this, "baseStates" );
}

void CDmeModel::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Add joint
//-----------------------------------------------------------------------------
int CDmeModel::AddJoint( CDmeDag *pJoint )
{
	int nIndex = GetJointTransformIndex( pJoint->GetTransform() );
	if ( nIndex >= 0 )
		return nIndex;

	return m_JointTransforms.AddToTail( pJoint->GetTransform() );
}


//-----------------------------------------------------------------------------
// Add joint
//-----------------------------------------------------------------------------
CDmeJoint *CDmeModel::AddJoint( const char *pJointName, CDmeDag *pParent )
{
	CDmeJoint *pJoint = CreateElement<CDmeJoint>( pJointName, GetFileId() );
	CDmeTransform *pTransform = pJoint->GetTransform();
	pTransform->SetName( pJointName );

	if ( !pParent )
	{
		pParent = this;
	}
	pParent->AddChild( pJoint );
	m_JointTransforms.AddToTail( pTransform );
	return pJoint;
}


//-----------------------------------------------------------------------------
// Returns the number of joint transforms we know about
//-----------------------------------------------------------------------------
int CDmeModel::GetJointTransformCount() const
{
	return m_JointTransforms.Count();
}


//-----------------------------------------------------------------------------
// Determines joint transform index	given a joint transform
//-----------------------------------------------------------------------------
int CDmeModel::GetJointTransformIndex( CDmeTransform *pTransform ) const
{
	int nCount = m_JointTransforms.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( pTransform == m_JointTransforms[i] )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Determines joint transform index	given a joint
//-----------------------------------------------------------------------------
int CDmeModel::GetJointTransformIndex( CDmeDag *pJoint ) const
{
	return GetJointTransformIndex( pJoint->GetTransform() );
}


//-----------------------------------------------------------------------------
// Determines joint transform index	given a joint name
//-----------------------------------------------------------------------------
CDmeTransform *CDmeModel::GetJointTransform( int nIndex )
{
	return m_JointTransforms[ nIndex ];
}

const CDmeTransform *CDmeModel::GetJointTransform( int nIndex ) const
{
	return m_JointTransforms[ nIndex ];
}


//-----------------------------------------------------------------------------
// Finds a base state by name, returns NULL if not found
//-----------------------------------------------------------------------------
CDmeTransformList *CDmeModel::FindBaseState( const char *pBaseStateName )
{
	int nCount = m_BaseStates.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( m_BaseStates[i]->GetName(), pBaseStateName ) )
			return m_BaseStates[i];
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Captures the current joint transforms into a base state
//-----------------------------------------------------------------------------
void CDmeModel::CaptureJointsToBaseState( const char *pBaseStateName )
{
	CDmeTransformList *pTransformList = FindBaseState( pBaseStateName );
	if ( !pTransformList )
	{
		pTransformList = CreateElement<CDmeTransformList>( pBaseStateName, GetFileId() );
		m_BaseStates.AddToTail( pTransformList );
	}

	// Make the transform list have the correct number of elements
	int nJointCount = m_JointTransforms.Count();
	int nCurrentCount = pTransformList->GetTransformCount();
	if ( nJointCount > nCurrentCount )
	{
		for ( int i = nCurrentCount; i < nJointCount; ++i )
		{
			CDmeTransform *pTransform = CreateElement<CDmeTransform>( m_JointTransforms[i]->GetName(), pTransformList->GetFileId() );
			pTransformList->m_Transforms.AddToTail( pTransform );
		}
	}
	else if ( nJointCount < nCurrentCount )
	{
		pTransformList->m_Transforms.RemoveMultiple( nJointCount, nCurrentCount - nJointCount );
	}

	// Copy the state over
	for ( int i = 0; i < nJointCount; ++i )
	{
		matrix3x4_t mat;
		m_JointTransforms[i]->GetTransform( mat );
		pTransformList->SetTransform( i, mat );
	}
}


//-----------------------------------------------------------------------------
// Loads up joint transforms for this model
//-----------------------------------------------------------------------------
void CDmeModel::LoadJointTransform( CDmeDag *pJoint, CDmeTransformList *pBindPose, const matrix3x4_t &parentToWorld, const matrix3x4_t &parentToBindPose, bool bSetHardwareState )
{
	CDmeTransform *pTransform = pJoint->GetTransform();

	// Determines joint transform index; no index, no traversing lower in the hierarchy
	int nJointIndex = GetJointTransformIndex( pTransform );
	if ( nJointIndex < 0 )
		return;

	// FIXME: Sucky search here necessary to find bone matrix index
	matrix3x4_t	jointToWorld, jointToParent;
	pTransform->GetTransform( jointToParent );
	ConcatTransforms( parentToWorld, jointToParent, jointToWorld );

	matrix3x4_t bindJointToParent, bindPoseToJoint, bindPoseToWorld, jointToBindPose;
	if ( pBindPose )
	{
		if ( nJointIndex >= pBindPose->GetTransformCount() )
		{
			Warning( "Model is in an invalid state! There are different numbers of bones in the bind pose and joint transform list!\n" );
			return;
		}
		pBindPose->GetTransform( nJointIndex )->GetTransform( bindJointToParent );
	}
	else
	{
		MatrixCopy( jointToParent, bindJointToParent );
	}
	ConcatTransforms( parentToBindPose, bindJointToParent, jointToBindPose );

	MatrixInvert( jointToBindPose, bindPoseToJoint );
	ConcatTransforms( jointToWorld, bindPoseToJoint, bindPoseToWorld );

	if ( bSetHardwareState )
	{
		CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
		pRenderContext->LoadBoneMatrix( nJointIndex, bindPoseToWorld );
	}
	MatrixCopy( bindPoseToWorld, s_PoseToWorld[ nJointIndex ] );

	int nChildCount = pJoint->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pChildJoint = pJoint->GetChild(i);
		if ( !pChildJoint )
			continue;

		LoadJointTransform( pChildJoint, pBindPose, jointToWorld, jointToBindPose, bSetHardwareState );
	}
}


//-----------------------------------------------------------------------------
// Sets up the render state for the model
//-----------------------------------------------------------------------------
CDmeModel::SetupBoneRetval_t CDmeModel::SetupBoneMatrixState( const matrix3x4_t& shapeToWorld, bool bForceSoftwareSkin )
{
	int nJointCount = m_JointTransforms.Count();
	if ( nJointCount <= 0 )
		return NO_SKIN_DATA;

	int nBoneBatchCount = g_pMaterialSystemHardwareConfig->MaxVertexShaderBlendMatrices();
	bool bSetHardwareState = ( nJointCount <= nBoneBatchCount ) && !bForceSoftwareSkin;

	s_PoseToWorld.EnsureCount( nJointCount );

	// Finds a base state by name, returns NULL if not found
	CDmeTransformList *pBindPose = FindBaseState( "bind" );

	matrix3x4_t parentToBindPose;
	SetIdentityMatrix( parentToBindPose );

	int nChildCount = GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pChildJoint = GetChild(i);
		if ( !pChildJoint )
			continue;

		LoadJointTransform( pChildJoint, pBindPose, shapeToWorld, parentToBindPose, bSetHardwareState );
	}

	return bSetHardwareState ? BONES_SET_UP : TOO_MANY_BONES;
}

matrix3x4_t *CDmeModel::SetupModelRenderState( const matrix3x4_t& shapeToWorld, bool bHasSkinningData, bool bForceSoftwareSkin )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	if ( bHasSkinningData && ( s_ModelStack.Count() > 0 ) )
	{
		SetupBoneRetval_t retVal = s_ModelStack.Top()->SetupBoneMatrixState( shapeToWorld, bForceSoftwareSkin );
 		if ( retVal == TOO_MANY_BONES )
		{
			pRenderContext->MatrixMode( MATERIAL_MODEL );
			pRenderContext->LoadIdentity( );
			return s_PoseToWorld.Base();
		}
		if ( retVal != NO_SKIN_DATA )
			return NULL;
	}

	if ( bForceSoftwareSkin )
	{
		pRenderContext->MatrixMode( MATERIAL_MODEL );
		pRenderContext->LoadIdentity( );
		s_PoseToWorld.EnsureCount( 1 );
		MatrixCopy( shapeToWorld, s_PoseToWorld[0] );
		return s_PoseToWorld.Base();
	}

	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadMatrix( shapeToWorld );
	return NULL;
}

void CDmeModel::CleanupModelRenderState()
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	pRenderContext->MatrixMode( MATERIAL_MODEL );
	pRenderContext->LoadIdentity();
}


//-----------------------------------------------------------------------------
// Recursively render the Dag hierarchy
//-----------------------------------------------------------------------------
void CDmeModel::Draw( CDmeDrawSettings *pDrawSettings /* = NULL */ )
{
	s_ModelStack.Push( this );
	BaseClass::Draw( pDrawSettings );
	s_ModelStack.Pop( );
}


//-----------------------------------------------------------------------------
// Set if Z is the up axis of the model
//-----------------------------------------------------------------------------
void CDmeModel::ZUp( bool bZUp )
{
	SetValue( "upAxis", bZUp ? "Z" : "Y" );
}


//-----------------------------------------------------------------------------
// Returns true if the DmeModel is Z Up.
// NOTE: Since Y & Z are the only supported modes and Y is the default
//       because that's how DmeModel data was originally defined,
//       assume Y is up if the m_UpAxis attribute is not "Z"
//-----------------------------------------------------------------------------
bool CDmeModel::IsZUp() const
{
	const char *pszZUp = this->GetValueString( "upAxis" );

	return ( pszZUp && *pszZUp ) ? StringHasPrefix( pszZUp, "Z" ) : false;
}