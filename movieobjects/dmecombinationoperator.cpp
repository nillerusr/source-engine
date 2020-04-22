//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmecombinationoperator.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmattribute.h"
#include "movieobjects/dmechannel.h"
#include "movieobjects/dmemodel.h"
#include "movieobjects/dmeshape.h"
#include "movieobjects/dmedag.h"
#include "movieobjects/dmeclip.h"
#include "movieobjects/dmelog.h"
#include "movieobjects/dmevertexdata.h"
#include "movieobjects/dmemesh.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeCombinationInputControl, CDmeCombinationInputControl );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeCombinationInputControl::OnConstruction()
{
	m_RawControlNames.Init( this, "rawControlNames" );
	m_bIsStereo.Init( this, "stereo" );
	m_bIsEyelid.InitAndSet( this, "eyelid", false );
	m_WrinkleScales.Init( this, "wrinkleScales" );
}

void CDmeCombinationInputControl::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Backward compat
//-----------------------------------------------------------------------------
void CDmeCombinationInputControl::OnElementUnserialized()
{
	BaseClass::OnElementUnserialized();

	int nWrinkleCount = m_WrinkleScales.Count();
	int nControlCount = m_RawControlNames.Count();
	if ( nWrinkleCount < nControlCount )
	{
		for ( int i = nWrinkleCount; i < nControlCount; ++i )
		{
			m_WrinkleScales.AddToTail( 0.0f );
		}
	}
	else if ( nWrinkleCount > nControlCount )
	{
		m_WrinkleScales.RemoveMultiple( nControlCount, nWrinkleCount - nControlCount );
	}
}


//-----------------------------------------------------------------------------
// Adds a control, returns the control index
//-----------------------------------------------------------------------------
bool CDmeCombinationInputControl::AddRawControl( const char *pRawControlName )
{
	Assert( !strchr( pRawControlName, '_' ) && !strchr( pRawControlName, ' ' ) );
	int nCount = m_RawControlNames.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pRawControlName, m_RawControlNames[i] ) )
			return false;
	}

	m_RawControlNames.AddToTail( pRawControlName );
	m_WrinkleScales.AddToTail( 0.0f );
	return true;
}


//-----------------------------------------------------------------------------
// Finds a raw control by name
//-----------------------------------------------------------------------------
int CDmeCombinationInputControl::FindRawControl( const char *pRawControlName )
{
	Assert( !strchr( pRawControlName, '_' ) && !strchr( pRawControlName, ' ' ) );
	int nCount = m_RawControlNames.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pRawControlName, m_RawControlNames[i] ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Removes controls
//-----------------------------------------------------------------------------
bool CDmeCombinationInputControl::RemoveRawControl( const char *pRawControlName )
{
	int i = FindRawControl( pRawControlName );
	if ( i >= 0 )
	{
		m_RawControlNames.FastRemove( i );
		m_WrinkleScales.FastRemove( i );
		return true;
	}

	return false;
}

void CDmeCombinationInputControl::RemoveAllRawControls()
{
	m_RawControlNames.RemoveAll();
	m_WrinkleScales.RemoveAll( );
}


//-----------------------------------------------------------------------------
// Iterates remapped controls
//-----------------------------------------------------------------------------
int CDmeCombinationInputControl::RawControlCount() const
{
	return m_RawControlNames.Count();
}

const char *CDmeCombinationInputControl::RawControlName( int nIndex ) const
{
	return m_RawControlNames[ nIndex ];
}


//-----------------------------------------------------------------------------
// Is this control a stereo control?
//-----------------------------------------------------------------------------
bool CDmeCombinationInputControl::IsStereo() const
{
	return m_bIsStereo;
}

void CDmeCombinationInputControl::SetStereo( bool bStereo )
{
	m_bIsStereo = bStereo;
}


//-----------------------------------------------------------------------------
// Is this control an eyelid control?
//-----------------------------------------------------------------------------
bool CDmeCombinationInputControl::IsEyelid() const
{
	return m_bIsEyelid;
}

void CDmeCombinationInputControl::SetEyelid( bool bEyelid )
{
	m_bIsEyelid = bEyelid;
}


//-----------------------------------------------------------------------------
// Reordering controls
//-----------------------------------------------------------------------------
void CDmeCombinationInputControl::MoveRawControlUp( const char *pRawControlName )
{
	int nIndex = FindRawControl( pRawControlName );
	if ( nIndex > 0 )
	{
		m_RawControlNames.Swap( nIndex, nIndex - 1 );
		m_WrinkleScales.Swap( nIndex, nIndex - 1 );
	}
}

void CDmeCombinationInputControl::MoveRawControlDown( const char *pRawControlName )
{
	int nIndex = FindRawControl( pRawControlName );
	int nLastIndex = m_RawControlNames.Count() - 1;
	if ( nIndex >= 0 && nIndex < nLastIndex )
	{
		m_RawControlNames.Swap( nIndex, nIndex + 1 );
		m_WrinkleScales.Swap( nIndex, nIndex + 1 );
	}
}


//-----------------------------------------------------------------------------
// Returns the wrinkle scale for a particular control
//-----------------------------------------------------------------------------
float CDmeCombinationInputControl::WrinkleScale( const char *pRawControlName )
{
	int nIndex = FindRawControl( pRawControlName );
	return WrinkleScale( nIndex );
}

float CDmeCombinationInputControl::WrinkleScale( int nIndex )
{
	if ( nIndex < 0 || ( nIndex >= m_WrinkleScales.Count() ) )
		return 0.0f;
	return m_WrinkleScales[ nIndex ];
}

void CDmeCombinationInputControl::SetWrinkleScale( const char *pRawControlName, float flWrinkleScale )
{
	int nIndex = FindRawControl( pRawControlName );
	if ( nIndex < 0 || ( nIndex >= m_WrinkleScales.Count() ) )
		return;
	m_WrinkleScales.Set( nIndex, flWrinkleScale );
}


//-----------------------------------------------------------------------------
// The default value of an input control is the value the UI has by default
//-----------------------------------------------------------------------------
float CDmeCombinationInputControl::GetDefaultValue() const
{
	return RawControlCount() == 2 ? 0.5f : 0.0f;
}


//-----------------------------------------------------------------------------
// The base value of an input control will set the data to the base state
// i.e. The state upon which the deltas are relative.  Normally this is the
//      same as GetDefaultValue() except for EyeLid controls
//-----------------------------------------------------------------------------
float CDmeCombinationInputControl::GetBaseValue() const
{
	if ( IsEyelid() )
		return 0.0f;

	return GetDefaultValue();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CDmeCombinationInputControl::GetEyesUpDownFlexName() const
{
	const CDmAttribute *pEyesUpDownFlexAttr = GetAttribute( "eyesUpDownFlex", AT_STRING );

	if ( pEyesUpDownFlexAttr )
		return pEyesUpDownFlexAttr->GetValueString();

	return NULL;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeCombinationDominationRule, CDmeCombinationDominationRule );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRule::OnConstruction()
{
	m_Dominators.Init( this, "dominators", FATTRIB_HAS_CALLBACK | FATTRIB_HAS_ARRAY_CALLBACK );
	m_Suppressed.Init( this, "suppressed", FATTRIB_HAS_CALLBACK | FATTRIB_HAS_ARRAY_CALLBACK );
}

void CDmeCombinationDominationRule::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Notify parent that one of our attributes has changed
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRule::OnAttributeChanged( CDmAttribute *pAttribute )
{
	BaseClass::OnAttributeChanged( pAttribute );

	if ( pAttribute == m_Dominators.GetAttribute() || pAttribute == m_Suppressed.GetAttribute() )
	{
		InvokeOnAttributeChangedOnReferrers( GetHandle(), pAttribute );
	}
}


//-----------------------------------------------------------------------------
// Do we have this string already?
//-----------------------------------------------------------------------------
bool CDmeCombinationDominationRule::HasString( const char *pString, const CDmaStringArray& attr )
{
	int nCount = attr.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pString, attr[i] ) )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Adds a dominating control
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRule::AddDominator( const char *pDominatorControl )
{
	if ( HasString( pDominatorControl, m_Dominators ) )
	{
		Warning( "Domination rule already contains dominating control %s\n", pDominatorControl );
		return;
	}
	if ( HasString( pDominatorControl, m_Suppressed ) )
	{
		Warning( "Attemped to add a control as both dominator + suppressed %s\n", pDominatorControl );
		return;
	}
	m_Dominators.AddToTail( pDominatorControl );
}


//-----------------------------------------------------------------------------
// Add a suppressed control
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRule::AddSuppressed( const char *pSuppressedControl )
{
	if ( HasString( pSuppressedControl, m_Suppressed ) )
	{
		Warning( "Domination rule already contains suppressed control %s\n", pSuppressedControl );
		return;
	}
	if ( HasString( pSuppressedControl, m_Dominators ) )
	{
		Warning( "Attemped to add a control as both dominator + suppressed %s\n", pSuppressedControl );
		return;
	}
	m_Suppressed.AddToTail( pSuppressedControl );
}


//-----------------------------------------------------------------------------
// Remove all dominatior + suppressed controls
//-----------------------------------------------------------------------------
void CDmeCombinationDominationRule::RemoveAllDominators()
{
	m_Dominators.RemoveAll();
}

void CDmeCombinationDominationRule::RemoveAllSuppressed()
{
	m_Suppressed.RemoveAll();
}


//-----------------------------------------------------------------------------
// Iteration
//-----------------------------------------------------------------------------
int CDmeCombinationDominationRule::DominatorCount() const
{
	return m_Dominators.Count();
}

const char *CDmeCombinationDominationRule::GetDominator( int i ) const
{
	return m_Dominators[i];
}

int CDmeCombinationDominationRule::SuppressedCount() const
{
	return m_Suppressed.Count();
}

const char *CDmeCombinationDominationRule::GetSuppressed( int i ) const
{
	return m_Suppressed[i];
}


//-----------------------------------------------------------------------------
// Search
//-----------------------------------------------------------------------------
bool CDmeCombinationDominationRule::HasDominatorControl( const char *pDominatorControl ) const
{
	int nCount = DominatorCount();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( GetDominator(i), pDominatorControl ) )
			return true;
	}
	return false;
}

bool CDmeCombinationDominationRule::HasSuppressedControl( const char *pSuppressedControl ) const
{
	int nCount = SuppressedCount();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( GetSuppressed(i), pSuppressedControl ) )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeCombinationOperator, CDmeCombinationOperator );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::OnConstruction()
{
	m_InputControls.Init( this, "controls" );
	m_ControlValues[COMBO_CONTROL_NORMAL].Init( this, "controlValues" );
	m_ControlValues[COMBO_CONTROL_LAGGED].Init( this, "controlValuesLagged" );
	m_bSpecifyingLaggedData.Init( this, "usesLaggedValues" );

	m_Dominators.Init( this, "dominators", FATTRIB_HAS_CALLBACK | FATTRIB_HAS_ARRAY_CALLBACK );

	m_Targets.Init( this, "targets" );
	m_flLastLaggedComputationTime = FLT_MIN;
}

void CDmeCombinationOperator::OnDestruction()
{
	m_RawControlInfo.RemoveAll();
	m_CombinationInfo.RemoveAll();
	m_DominatorInfo.RemoveAll();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::OnAttributeChanged( CDmAttribute *pAttribute )
{
	m_Dominators.GetAttribute()->AddFlag( FATTRIB_DIRTY );
}


//-----------------------------------------------------------------------------
// Finds the index of the input control with the specified name (and creates one if necessary) 
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::OnElementUnserialized()
{
	BaseClass::OnElementUnserialized();

	// Mark all of the input as not being in their default state since we read it from a file
	int nCount = m_InputControls.Count();
	m_IsDefaultValue.SetCount( nCount );
	for ( int i = 0; i < nCount; ++i ) 
	{
		m_IsDefaultValue[i] = false;
	}
}


//-----------------------------------------------------------------------------
// Finds the index of the input control with the specified name (and creates one if necessary) 
//-----------------------------------------------------------------------------
ControlIndex_t CDmeCombinationOperator::FindOrCreateControl( const char *pControlName, bool bStereo, bool bAutoAddRawControl )
{
	Assert( !strchr( pControlName, '_' ) && !strchr( pControlName, ' ' ) );
	int nCount = m_InputControls.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !m_InputControls[i] )
			continue;

		if ( !Q_stricmp( pControlName, m_InputControls[i]->GetName() ) )
			return i;
	}

	// NOTE: the y coordinate of the control value is -1 if it's not a stereo control
	CDmeCombinationInputControl *pInputControl = CreateElement< CDmeCombinationInputControl >( pControlName, GetFileId() );
	pInputControl->SetStereo( bStereo );
	int nIndex = m_InputControls.AddToTail( pInputControl );
	m_ControlValues[COMBO_CONTROL_NORMAL].AddToTail( Vector( 0.0f, 0.5f, 0.5f ) );
	m_ControlValues[COMBO_CONTROL_LAGGED].AddToTail( Vector( 0.0f, 0.5f, 0.5f ) );
	m_IsDefaultValue.AddToTail( true );
	Assert( m_InputControls.Count() == m_ControlValues[COMBO_CONTROL_NORMAL].Count() );
	Assert( m_InputControls.Count() == m_ControlValues[COMBO_CONTROL_LAGGED].Count() );
	Assert( m_InputControls.Count() == m_IsDefaultValue.Count() );

	if ( bAutoAddRawControl )
	{
		AddRawControl( nIndex, pControlName );
	}

	return nIndex;
}


//-----------------------------------------------------------------------------
// Finds the index of the input control with the specified name
//-----------------------------------------------------------------------------
ControlIndex_t CDmeCombinationOperator::FindControlIndex( const char *pControlName )
{
	int nCount = m_InputControls.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !m_InputControls[i] )
			continue;

		if ( !Q_stricmp( pControlName, m_InputControls[i]->GetName() ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Removes a control
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::RemoveControl( const char *pControlName )
{
	ControlIndex_t nIndex = FindControlIndex( pControlName );
	if ( nIndex >= 0 )
	{
		DestroyElement( m_InputControls[nIndex] );
		m_InputControls.FastRemove( nIndex );
		m_ControlValues[COMBO_CONTROL_NORMAL].FastRemove( nIndex );
		m_ControlValues[COMBO_CONTROL_LAGGED].FastRemove( nIndex );
		m_IsDefaultValue.FastRemove( nIndex );

		Assert( m_InputControls.Count() == m_ControlValues[COMBO_CONTROL_NORMAL].Count() );
		Assert( m_InputControls.Count() == m_ControlValues[COMBO_CONTROL_LAGGED].Count() );
		Assert( m_InputControls.Count() == m_IsDefaultValue.Count() );

		RebuildRawControlList();
	}
}

void CDmeCombinationOperator::RemoveAllControls()
{
	int nCount = m_InputControls.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeCombinationInputControl *pInputControl = m_InputControls[i];
		m_InputControls.Set( i, NULL );
		DestroyElement( pInputControl );
	}
	m_InputControls.RemoveAll();
	m_ControlValues[COMBO_CONTROL_NORMAL].RemoveAll( );
	m_ControlValues[COMBO_CONTROL_LAGGED].RemoveAll( );
	m_IsDefaultValue.RemoveAll( );

	RebuildRawControlList();
}


//-----------------------------------------------------------------------------
// Changes a control's name
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::SetControlName( ControlIndex_t nControl, const char *pControlName )
{
	ControlIndex_t nFoundIndex = FindControlIndex( pControlName );
	if ( nFoundIndex >= 0 && nFoundIndex != nControl )
	{
		Warning( "A control with name \"%s\" already exists!\n", pControlName );
		return;
	}
	m_InputControls[nControl]->SetName( pControlName );
}


//-----------------------------------------------------------------------------
// Updates the default value associated with a control
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::UpdateDefaultValue( ControlIndex_t nControlIndex )
{
	if ( m_IsDefaultValue[ nControlIndex ] )
	{
		float flDefaultValue = GetRawControlCount( nControlIndex ) == 2 ? 0.5f : 0.0f;
		const Vector& vec = m_ControlValues[COMBO_CONTROL_NORMAL][nControlIndex];
		m_ControlValues[COMBO_CONTROL_NORMAL].Set( nControlIndex, Vector( flDefaultValue, vec.y, vec.z ) );
		const Vector& vec2 = m_ControlValues[COMBO_CONTROL_LAGGED][nControlIndex];
		m_ControlValues[COMBO_CONTROL_LAGGED].Set( nControlIndex, Vector( flDefaultValue, vec2.y, vec2.z ) );
	}
}


//-----------------------------------------------------------------------------
// Adds an output control to a input control
// If force is true, removes the raw control from any existing controls
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::AddRawControl( ControlIndex_t nControl, const char *pRawControlName )
{
	if ( FindRawControlIndex( pRawControlName, true ) >= 0 )
	{
		Warning( "Attempted to add the same remapped control \"%s\" twice!\n", pRawControlName );
		return;
	}

	CDmeCombinationInputControl *pInputControl = m_InputControls[nControl];
	Assert( pInputControl );
	if ( pInputControl->AddRawControl( pRawControlName ) )
	{
		UpdateDefaultValue( nControl );
		RebuildRawControlList();
	}
}


//-----------------------------------------------------------------------------
// Removes an output control from an input control
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::RemoveRawControl( ControlIndex_t nControl, const char *pRawControlName )
{
	CDmeCombinationInputControl *pInputControl = m_InputControls[nControl];
	Assert( pInputControl );
	if ( pInputControl->RemoveRawControl( pRawControlName ) )
	{
		UpdateDefaultValue( nControl );
		RebuildRawControlList();
	}
}

void CDmeCombinationOperator::RemoveAllRawControls( ControlIndex_t nControl )
{
	CDmeCombinationInputControl *pInputControl = m_InputControls[nControl];
	Assert( pInputControl );
	pInputControl->RemoveAllRawControls( );

	int nCount = m_InputControls.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		UpdateDefaultValue( i );
	}

	RebuildRawControlList();
}


//-----------------------------------------------------------------------------
// Iterates remapped controls
//-----------------------------------------------------------------------------
int CDmeCombinationOperator::GetRawControlCount( ControlIndex_t nControl ) const
{
	CDmeCombinationInputControl *pInputControl = m_InputControls[nControl];
	Assert( pInputControl );
	return pInputControl->RawControlCount();
}

const char *CDmeCombinationOperator::GetRawControlName( ControlIndex_t nControl, int nIndex ) const
{
	CDmeCombinationInputControl *pInputControl = m_InputControls[nControl];
	Assert( pInputControl );
	return pInputControl->RawControlName( nIndex );
}

float CDmeCombinationOperator::GetRawControlWrinkleScale( ControlIndex_t nControl, int nIndex ) const
{
	CDmeCombinationInputControl *pInputControl = m_InputControls[nControl];
	Assert( pInputControl );
	return pInputControl->WrinkleScale( nIndex );
}

float CDmeCombinationOperator::GetRawControlWrinkleScale( ControlIndex_t nControl, const char *pRawControlName ) const
{
	CDmeCombinationInputControl *pInputControl = m_InputControls[nControl];
	Assert( pInputControl );
	return pInputControl->WrinkleScale( pRawControlName );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float CDmeCombinationOperator::GetControlDefaultValue( ControlIndex_t nControl ) const
{
	CDmeCombinationInputControl *pInputControl = m_InputControls[nControl];
	Assert( pInputControl );
	return pInputControl->GetDefaultValue();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
float CDmeCombinationOperator::GetControlBaseValue( ControlIndex_t nControl ) const
{
	CDmeCombinationInputControl *pInputControl = m_InputControls[nControl];
	Assert( pInputControl );
	return pInputControl->GetBaseValue();
}


//-----------------------------------------------------------------------------
// Iterates a global list of output controls
//-----------------------------------------------------------------------------
int CDmeCombinationOperator::GetRawControlCount( ) const
{
	return m_RawControlInfo.Count();
}

const char *CDmeCombinationOperator::GetRawControlName( int nIndex ) const
{
	return m_RawControlInfo[nIndex].m_Name;
}

float CDmeCombinationOperator::GetRawControlWrinkleScale( int nIndex ) const
{
	return m_RawControlInfo[nIndex].m_flWrinkleScale;
}


//-----------------------------------------------------------------------------
// Sets the wrinkle scale for a particular raw control
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::SetWrinkleScale( ControlIndex_t nControl, const char *pRawControlName, float flWrinkleScale )
{
	CDmeCombinationInputControl *pInputControl = m_InputControls[nControl];
	Assert( pInputControl );
	pInputControl->SetWrinkleScale( pRawControlName, flWrinkleScale );
	RebuildRawControlList();
}


//-----------------------------------------------------------------------------
// Reordering controls
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::MoveControlUp( const char *pControlName )
{

	int nIndex = FindControlIndex( pControlName );
	if ( nIndex > 0 )
	{
		m_InputControls.Swap( nIndex, nIndex - 1 );
		for ( int i = 0; i < COMBO_CONTROL_TYPE_COUNT; ++i )
		{
			m_ControlValues[ i ].Swap( nIndex, nIndex - 1 );
		}
		RebuildRawControlList();
	}
}


//-----------------------------------------------------------------------------
// Reordering controls
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::MoveControlDown( const char *pControlName )
{
	int nIndex = FindControlIndex( pControlName );
	int nLastIndex = m_InputControls.Count() - 1;
	if ( nIndex >= 0 && nIndex < nLastIndex )
	{
		m_InputControls.Swap( nIndex, nIndex + 1 );
		for ( int i = 0; i < COMBO_CONTROL_TYPE_COUNT; ++i )
		{
			m_ControlValues[ i ].Swap( nIndex, nIndex + 1 );
		}
		RebuildRawControlList();
	}
}


//-----------------------------------------------------------------------------
// Reordering controls
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::MoveControlBefore( const char *pDragControlName, const char *pDropControlName )
{
	int pDragIndex( FindControlIndex( pDragControlName ) );
	int pDropIndex( FindControlIndex( pDropControlName ) );

	// Have to copy because InsertAfter may reallocate memory before doing the copy and therefore might be referencing garabage
	CDmeCombinationInputControl *inputControlCopy( m_InputControls[ pDragIndex ] );
	m_InputControls.InsertBefore( pDropIndex, inputControlCopy );
	if ( pDragIndex <= pDropIndex )
	{
		m_InputControls.Remove( pDragIndex );
		for ( int i = 0; i < COMBO_CONTROL_TYPE_COUNT; ++i )
		{
			const Vector controlValueCopy( m_ControlValues[ i ][ pDragIndex ] );
			m_ControlValues[ i ].InsertBefore( pDropIndex, controlValueCopy );
			m_ControlValues[ i ].Remove( pDragIndex );
		}
	}
	else
	{
		m_InputControls.Remove( pDragIndex + 1 );
		for ( int i = 0; i < COMBO_CONTROL_TYPE_COUNT; ++i )
		{
			const Vector controlValueCopy( m_ControlValues[ i ][ pDragIndex ] );
			m_ControlValues[ i ].InsertBefore( pDropIndex, controlValueCopy );
			m_ControlValues[ i ].Remove( pDragIndex + 1 );
		}
	}
}


//-----------------------------------------------------------------------------
// Reordering controls
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::MoveControlAfter( const char *pDragControlName, const char *pDropControlName )
{
	int nDragIndex = FindControlIndex( pDragControlName );
	int nDropIndex = FindControlIndex( pDropControlName );

	// Have to copy because InsertAfter may reallocate memory before doing the copy and therefore might be referencing garabage
	CDmeCombinationInputControl *inputControlCopy( m_InputControls[ nDragIndex ] );
	m_InputControls.InsertBefore( nDropIndex + 1, inputControlCopy );
	if ( nDragIndex < nDropIndex )
	{
		m_InputControls.Remove( nDragIndex );
		for ( int i = 0; i < COMBO_CONTROL_TYPE_COUNT; ++i )
		{
			const Vector controlValueCopy( m_ControlValues[ i ][ nDragIndex ] );
			m_ControlValues[ i ].InsertBefore( nDropIndex + 1, controlValueCopy );
			m_ControlValues[ i ].Remove( nDragIndex );
		}
	}
	else
	{
		m_InputControls.Remove( nDragIndex + 1 );
		for ( int i = 0; i < COMBO_CONTROL_TYPE_COUNT; ++i )
		{
			const Vector controlValueCopy( m_ControlValues[ i ][ nDragIndex ] );
			m_ControlValues[ i ].InsertBefore( nDropIndex + 1, controlValueCopy );
			m_ControlValues[ i ].Remove( nDragIndex + 1 );
		}
	}
}


//-----------------------------------------------------------------------------
// Reordering controls
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::MoveRawControlUp( ControlIndex_t nControlIndex, const char *pRawControlName )
{
	m_InputControls[nControlIndex]->MoveRawControlUp( pRawControlName );
}


//-----------------------------------------------------------------------------
// Reordering controls
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::MoveRawControlDown( ControlIndex_t nControlIndex, const char *pRawControlName )
{
	m_InputControls[nControlIndex]->MoveRawControlDown( pRawControlName );
}


//-----------------------------------------------------------------------------
// Returns true if a control is a stereo control
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::SetStereoControl( ControlIndex_t nControlIndex, bool bIsStereo )
{
	m_InputControls[nControlIndex]->SetStereo( bIsStereo );
}

bool CDmeCombinationOperator::IsStereoControl( ControlIndex_t nControlIndex ) const
{
	return m_InputControls[nControlIndex]->IsStereo();
}

bool CDmeCombinationOperator::IsStereoRawControl( int nIndex ) const
{
	return IsStereoControl( m_RawControlInfo[nIndex].m_InputControl );
}

void CDmeCombinationOperator::SetEyelidControl( ControlIndex_t nControlIndex, bool bIsEyelid )
{
	m_InputControls[nControlIndex]->SetEyelid( bIsEyelid );
}

bool CDmeCombinationOperator::IsEyelidControl( ControlIndex_t nControlIndex ) const
{
	return m_InputControls[nControlIndex]->IsEyelid();
}

bool CDmeCombinationOperator::IsEyelidRawControl( int nIndex ) const
{
	return IsEyelidControl( m_RawControlInfo[nIndex].m_InputControl );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CDmeCombinationOperator::GetEyesUpDownFlexName( ControlIndex_t nControlIndex ) const
{
	return m_InputControls[ nControlIndex ]->GetEyesUpDownFlexName();
}


//-----------------------------------------------------------------------------
// Sets the value of a control
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::SetControlValue( ControlIndex_t nControlIndex, float flValue, CombinationControlType_t type )
{
	m_IsDefaultValue[ nControlIndex ] = false;
	float flMultiLevel = m_ControlValues[type][nControlIndex].z;
	m_ControlValues[type].Set( nControlIndex, Vector( flValue, flValue, flMultiLevel ) );
}

void CDmeCombinationOperator::SetControlValue( ControlIndex_t nControlIndex, float flLevel, float flBalance, CombinationControlType_t type )
{
	Assert( IsStereoControl( nControlIndex ) );
	m_IsDefaultValue[ nControlIndex ] = false;
	float flMultiLevel = m_ControlValues[type][nControlIndex].z;
	m_ControlValues[type].Set( nControlIndex, Vector( flLevel, flBalance, flMultiLevel ) );
}

void CDmeCombinationOperator::SetControlValue( ControlIndex_t nControlIndex, const Vector2D& vec, CombinationControlType_t type )
{
	Assert( IsStereoControl( nControlIndex ) );
	m_IsDefaultValue[ nControlIndex ] = false;
	float flMultiLevel = m_ControlValues[type][nControlIndex].z;
	m_ControlValues[type].Set( nControlIndex, Vector( vec.x, vec.y, flMultiLevel ) );
}


//-----------------------------------------------------------------------------
// Sets the value of a control
//-----------------------------------------------------------------------------
float CDmeCombinationOperator::GetControlValue( ControlIndex_t nControlIndex, CombinationControlType_t type )	const
{
	Assert( !IsStereoControl( nControlIndex ) );
	return m_ControlValues[type].Get( nControlIndex ).x;
}


//-----------------------------------------------------------------------------
// Sets the value of a control
//-----------------------------------------------------------------------------
const Vector2D& CDmeCombinationOperator::GetStereoControlValue( ControlIndex_t nControlIndex, CombinationControlType_t type ) const
{
	return m_ControlValues[type].Get( nControlIndex ).AsVector2D();
}


//-----------------------------------------------------------------------------
// Sets the level of a control (only used by controls w/ 3 or more remappings)
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::SetMultiControlLevel( ControlIndex_t nControlIndex, float flMultiLevel, CombinationControlType_t type )
{
	m_IsDefaultValue[ nControlIndex ] = false;
	const Vector2D &value = m_ControlValues[type][nControlIndex].AsVector2D();
	m_ControlValues[type].Set( nControlIndex, Vector( value.x, value.y, flMultiLevel ) );
}

float CDmeCombinationOperator::GetMultiControlLevel( ControlIndex_t nControlIndex, CombinationControlType_t type ) const
{
	Assert( IsMultiControl( nControlIndex ) );
	return m_ControlValues[type][nControlIndex].z;
}


//-----------------------------------------------------------------------------
// Returns true if a control is a multi control (a control w/ 3 or more remappings)
//-----------------------------------------------------------------------------
bool CDmeCombinationOperator::IsMultiControl( ControlIndex_t nControlIndex ) const
{
	return m_InputControls[nControlIndex]->RawControlCount() >= 3 || IsEyelidControl( nControlIndex );
}


//-----------------------------------------------------------------------------
// Iterates controls
//-----------------------------------------------------------------------------
int CDmeCombinationOperator::GetControlCount() const
{
	return m_InputControls.Count();
}

const char *CDmeCombinationOperator::GetControlName( ControlIndex_t i ) const
{
	return m_InputControls[i]->GetName();
}


//-----------------------------------------------------------------------------
// Do we have a raw control?
//-----------------------------------------------------------------------------
bool CDmeCombinationOperator::HasRawControl( const char *pRawControlName ) const
{
	return FindRawControlIndex( pRawControlName ) >= 0;
}


//-----------------------------------------------------------------------------
// Finds the index of the remapped control with the specified name
//-----------------------------------------------------------------------------
CDmeCombinationOperator::RawControlIndex_t CDmeCombinationOperator::FindRawControlIndex( const char *pControlName, bool bIgnoreDefaultControls ) const
{
	int nRawControlCount = m_RawControlInfo.Count();
	for ( int i = 0; i < nRawControlCount; ++i )
	{
		if ( bIgnoreDefaultControls && m_RawControlInfo[i].m_bIsDefaultControl )
			continue;

		if ( !Q_stricmp( pControlName, m_RawControlInfo[i].m_Name ) )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Is a particular remapped control stereo?
//-----------------------------------------------------------------------------
bool CDmeCombinationOperator::IsRawControlStereo( const char *pRawControlName )
{
	RawControlIndex_t nIndex = FindRawControlIndex( pRawControlName );
	if ( nIndex < 0 )
		return false;

	CDmeCombinationInputControl *pInputControl = m_InputControls[ m_RawControlInfo[nIndex].m_InputControl ];
	return pInputControl->IsStereo();
}


//-----------------------------------------------------------------------------
// Would a particular delta state attached to this combination operator end up stereo?
//-----------------------------------------------------------------------------
bool CDmeCombinationOperator::IsDeltaStateStereo( const char *pDeltaStateName )
{
	int *pTemp = (int*)_alloca( m_RawControlInfo.Count() * sizeof(int) );
	int nCount = ParseDeltaName( pDeltaStateName, pTemp );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeCombinationInputControl *pInputControl = m_InputControls[ m_RawControlInfo[ pTemp[i] ].m_InputControl ];
		if ( pInputControl->IsStereo() )
			return true;
	}
	return false;
}


//-----------------------------------------------------------------------------
// Does one of the targets we refer to contain a particular delta state?
//-----------------------------------------------------------------------------
bool CDmeCombinationOperator::DoesTargetContainDeltaState( const char *pSearchName )
{
	int nTargetCount = m_Targets.Count();
	for ( int i = 0; i < nTargetCount; ++i )
	{
		const CDmrElementArray<> deltaArray( m_Targets[i], "deltaStates" );
		if ( !deltaArray.IsValid() )
			continue;

		int nDeltaCount = deltaArray.Count();
		for ( int j = 0; j < nDeltaCount; ++j )
		{
			if ( !deltaArray[j] )
				continue;

			char pBuf[512];
			Q_strncpy( pBuf, deltaArray[j]->GetName(), sizeof(pBuf) );

			char *pEnd;
			for ( char *pName = pBuf; *pName; pName = pEnd )
			{
				pEnd = strchr( pName, '_' );
				if ( !pEnd )
				{
					pEnd = pName + Q_strlen( pName );
				}
				else
				{
					// Null-terminate
					*pEnd = 0;
					++pEnd;
				}

				if ( !Q_stricmp( pSearchName, pName ) )
					return true;
			}
		}
	}
	return false;
}


//-----------------------------------------------------------------------------
// Computes list of all remapped controls
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::RebuildRawControlList()
{
	m_RawControlInfo.RemoveAll();

	int nControlCount = m_InputControls.Count();
	for ( int i = 0; i < nControlCount; ++i )
	{
		CDmeCombinationInputControl *pInputControl = m_InputControls[i];
		Assert( pInputControl );
		int nRemapCount = pInputControl->RawControlCount();

		const bool bIsEyelid = pInputControl->IsEyelid();

		float flStep = ( nRemapCount > 2 ) ? 1.0f / ( nRemapCount - 1 ) : 0.0f;
		for ( int j = 0; j < nRemapCount; ++j )
		{
			int k = m_RawControlInfo.AddToTail( );
			RawControlInfo_t &info = m_RawControlInfo[k];
			
			info.m_Name = pInputControl->RawControlName( j );
			info.m_InputControl = i;
			info.m_bIsDefaultControl = false;
			info.m_flWrinkleScale = pInputControl->WrinkleScale( j );
			info.m_bLowerEyelid = false;

			if ( bIsEyelid )
			{
				info.m_FilterRamp.Init( 0.0f, 1.0f, 10.0f, 11.0f );
				// TODO: Right now it's implicit that the lower eyelid is first...
				if ( j == 0 )
				{
					// Close Lower Lid
					info.m_bLowerEyelid = true;
				}
				continue;
			}

			switch( nRemapCount )
			{
			case 1:
				info.m_FilterRamp.Init( 0.0f, 1.0f, 10.0f, 11.0f );
				break;

			case 2:
				if ( j == 0 )
				{
					info.m_FilterRamp.Init( -11.0f, -10.0f, 0.0f, 0.5f );
				}
				else
				{
					info.m_FilterRamp.Init( 0.5f, 1.0f, 10.0f, 11.0f );
				}
				break;

			default:
				{
					if ( j == 0 )
					{
						info.m_FilterRamp.Init( -11.0f, -10.0f,	0, flStep );
					}
					else if ( j == nRemapCount-1 )
					{
						info.m_FilterRamp.Init( 1.0f - flStep, 1.0f, 10.0f, 11.0f );
					}
					else
					{
						float flPeak = j * flStep;
						info.m_FilterRamp.Init( flPeak - flStep, flPeak, flPeak, flPeak + flStep );
					}
				}
				break;
			}
		}
	}

	RebuildDominatorInfo();
}


//-----------------------------------------------------------------------------
// Computes lists of dominators and suppressors
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::RebuildDominatorInfo()
{
	m_DominatorInfo.RemoveAll();
	int nCount = m_Dominators.Count();
	int *pDominators = (int*)_alloca( m_RawControlInfo.Count() * sizeof(int) );
	int *pSuppressed = (int*)_alloca( m_RawControlInfo.Count() * sizeof(int) );
	for ( int i = 0; i < nCount; ++i )
	{
		CDmeCombinationDominationRule *pRule = m_Dominators[i];
		bool bRuleOk = true;

		int nDominatorCount = pRule->DominatorCount();
		int nSuppressedCount = pRule->SuppressedCount();
		if ( ( nDominatorCount == 0 ) || ( nSuppressedCount == 0 ) )
			continue;

		for ( int j = 0; j < nDominatorCount; ++j )
		{
			int nControlIndex = FindRawControlIndex( pRule->GetDominator(j) );
			if ( nControlIndex < 0 )
			{
				bRuleOk = false;
				break;
			}
			pDominators[j] = nControlIndex;
		}

		for ( int j = 0; j < nSuppressedCount; ++j )
		{
			int nControlIndex = FindRawControlIndex( pRule->GetSuppressed(j) );
			if ( nControlIndex < 0 )
			{
				bRuleOk = false;
				break;
			}
			pSuppressed[j] = nControlIndex;
		}

		if ( !bRuleOk )
			continue;

		int k = m_DominatorInfo.AddToTail();
		m_DominatorInfo[k].m_DominantIndices.AddMultipleToTail( nDominatorCount, pDominators );
		m_DominatorInfo[k].m_SuppressedIndices.AddMultipleToTail( nSuppressedCount, pSuppressed );
	}
}


//-----------------------------------------------------------------------------
// Adds a dominator. Dominators are specified using raw control names
//-----------------------------------------------------------------------------
CDmeCombinationDominationRule *CDmeCombinationOperator::AddDominationRule( )
{
	CDmeCombinationDominationRule *pDominationRule = CreateElement< CDmeCombinationDominationRule >( "rule", GetFileId() );
	m_Dominators.AddToTail( pDominationRule );
	return pDominationRule;
}


//-----------------------------------------------------------------------------
// Adds a dominator. Dominators are specified using raw control names
//-----------------------------------------------------------------------------
CDmeCombinationDominationRule *CDmeCombinationOperator::AddDominationRule( CDmeCombinationDominationRule *pSrcRule )
{
	CDmeCombinationDominationRule *pDestRule = pSrcRule->Copy( );
	pDestRule->SetFileId( GetFileId(), TD_DEEP );
	m_Dominators.AddToTail( pDestRule );
	return pDestRule;
}


//-----------------------------------------------------------------------------
// Adds a dominator. Dominators are specified using raw control names
//-----------------------------------------------------------------------------
CDmeCombinationDominationRule *CDmeCombinationOperator::AddDominationRule( int nDominatorCount, const char **ppDominatorControlNames, int nSuppressedCount, const char **ppSuppressedControlNames )
{
	CDmeCombinationDominationRule *pDominationRule = AddDominationRule();
	for ( int i = 0; i < nDominatorCount; ++i )
	{
		pDominationRule->AddDominator( ppDominatorControlNames[i] );
	}
	for ( int i = 0; i < nSuppressedCount; ++i )
	{
		pDominationRule->AddSuppressed( ppSuppressedControlNames[i] );
	}
	return pDominationRule;
}


//-----------------------------------------------------------------------------
// Adds a dominator. Dominators are specified using raw control names
//-----------------------------------------------------------------------------
CDmeCombinationDominationRule *CDmeCombinationOperator::AddDominationRule( const CUtlVector< const char * > dominators, const CUtlVector< const char * > suppressed )
{
	return AddDominationRule( dominators.Count(), (const char **)dominators.Base(), suppressed.Count(), (const char **)suppressed.Base() );
}


//-----------------------------------------------------------------------------
// Removes a domination rule
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::RemoveDominationRule( int nIndex )
{
	CDmeCombinationDominationRule *pRule = m_Dominators[nIndex];
	m_Dominators.Remove( nIndex );
	DestroyElement( pRule );
}

void CDmeCombinationOperator::RemoveDominationRule( CDmeCombinationDominationRule *pRule )
{
	int nCount = m_Dominators.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_Dominators[i] == pRule )
		{
			RemoveDominationRule( i );
			break;
		}
	}
}

void CDmeCombinationOperator::RemoveAllDominationRules()
{
	int nCount = m_Dominators.Count();
	for ( int i = nCount; --i >= 0; )
	{
		RemoveDominationRule( i );
	}
}


//-----------------------------------------------------------------------------
// Iteration
//-----------------------------------------------------------------------------
int CDmeCombinationOperator::DominationRuleCount() const
{
	return m_Dominators.Count();
}

CDmeCombinationDominationRule *CDmeCombinationOperator::GetDominationRule( int i )
{
	return m_Dominators[i];
}


//-----------------------------------------------------------------------------
// Finds a domination rule
//-----------------------------------------------------------------------------
int CDmeCombinationOperator::FindDominationRule( CDmeCombinationDominationRule *pRule )
{
	int nCount = m_Dominators.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( m_Dominators[i] == pRule )
			return i;
	}
	return -1;
}


//-----------------------------------------------------------------------------
// Rule reordering
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::MoveDominationRuleUp( CDmeCombinationDominationRule* pRule )
{
	int nIndex = FindDominationRule( pRule );
	if ( nIndex > 0 )
	{
		m_Dominators.Swap( nIndex, nIndex - 1 );
		return;
	}
}

void CDmeCombinationOperator::MoveDominationRuleDown( CDmeCombinationDominationRule* pRule )
{
	int nIndex = FindDominationRule( pRule );
	if ( nIndex >= 0 && nIndex < m_Dominators.Count() - 1 )
	{
		m_Dominators.Swap( nIndex, nIndex + 1 );
		return;
	}
}


//-----------------------------------------------------------------------------
// Attaches a channel to an input
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::AttachChannelToControlValue( ControlIndex_t nControlIndex, CombinationControlType_t type, CDmeChannel *pChannel )
{
	pChannel->SetOutput( m_ControlValues[type].GetAttribute(), nControlIndex );
}


//-----------------------------------------------------------------------------
// Determines the weighting of input controls based on the deltaState name
//-----------------------------------------------------------------------------
int CDmeCombinationOperator::FindDeltaStateIndex( CDmAttribute *pDeltaArray, const char *pDeltaStateName )
{
	const CDmrElementArray<> deltas( pDeltaArray );
	int nDeltaArrayCount = deltas.Count();
	for ( int i = 0; i < nDeltaArrayCount; ++i )
	{
		CDmElement *pDeltaElement = deltas[i];
		if ( pDeltaElement && !Q_stricmp( pDeltaElement->GetName(), pDeltaStateName ) )
			return i;
	}

	return -1;
}


//-----------------------------------------------------------------------------
// Determines which combination to use based on the deltaState name
//-----------------------------------------------------------------------------
int CDmeCombinationOperator::ParseDeltaName(const char *pDeltaStateName, int *pControlIndices )
{
	char pBuf[512];
	Q_strncpy( pBuf, pDeltaStateName, sizeof(pBuf) );

	int nComboCount = 0;
	char *pEnd;
	for ( char *pName = pBuf; *pName; pName = pEnd )
	{
		pEnd = strchr( pName, '_' );
		if ( !pEnd )
		{
			pEnd = pName + Q_strlen( pName );
		}
		else
		{
			// Null-terminate
			*pEnd = 0;
			++pEnd;
		}

		int nControlIndex = FindRawControlIndex( pName );
		if ( nControlIndex < 0 )
			return 0;

		pControlIndices[ nComboCount++ ] = nControlIndex;
	}

	return nComboCount;
}


//-----------------------------------------------------------------------------
// Finds dominators
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::FindDominators( CombinationOperation_t& op )
{
	// Dominators are sets of inputs, which, when set to 1, will 
	// supress another set of inputs. Only combinations which contain *all*
	// dominators will suppress any combinatation that contains *all* suppressors
	int nDominatorCount = m_DominatorInfo.Count();
	for ( int i = 0; i < nDominatorCount; ++i )
	{
		DominatorInfo_t &info = m_DominatorInfo[i];

		// Look for suppressor indices in the control indices list
		int nCount = info.m_SuppressedIndices.Count();
		int j;
		for ( j = 0; j < nCount; ++j )
		{
			if ( op.m_ControlIndices.Find( info.m_SuppressedIndices[j] ) < 0 )
				break;
		}
		if ( j != nCount )
			continue;

		op.m_DominatorIndices.AddToTail( i );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::ComputeCombinationInfo( int nIndex )
{
	CleanUpCombinationInfo( nIndex );

	int nCurrentCount = m_CombinationInfo.Count();
	while ( nIndex >= nCurrentCount )
	{
		int j = m_CombinationInfo.AddToTail();
		CombinationInfo_t &info = m_CombinationInfo[j];
		for ( int k = 0; k < COMBO_CONTROL_TYPE_COUNT; ++k )
		{
			info.m_hDestAttribute[k] = DMATTRIBUTE_HANDLE_INVALID;
		}
		++nCurrentCount;
	}

	CombinationInfo_t &info = m_CombinationInfo[nIndex];

	CDmElement *pSource = m_Targets[ nIndex ];
	if ( !pSource )
		return;

	CDmrElementArray<> deltas( pSource, "deltaStates" );
	if ( !deltas.IsValid() )
		return;

	CDmrArray<Vector2D> weights( pSource, "deltaStateWeights" );
	if ( !weights.IsValid() )
		return;

	CDmrArray<Vector2D> weightsLagged( pSource, "deltaStateWeightsLagged" );

	// This is an error state
	if ( deltas.Count() > weights.Count() )
		return;

	// This is an error state
	if ( weightsLagged.IsValid() && deltas.Count() > weightsLagged.Count() )
		return;

	info.m_hDestAttribute[COMBO_CONTROL_NORMAL] = weights.GetAttribute()->GetHandle();
	info.m_hDestAttribute[COMBO_CONTROL_LAGGED] = weightsLagged.IsValid() ? weightsLagged.GetAttribute()->GetHandle() : DMATTRIBUTE_HANDLE_INVALID;

	int *pTemp = (int*)_alloca( m_RawControlInfo.Count() * sizeof(int) );
	int nDeltaCount = deltas.Count();
	for ( int i = 0; i < nDeltaCount; ++i )
	{
		CDmElement *pDeltaElement = deltas[i];
		if ( !pDeltaElement )
			continue;

		int nControlCount = ParseDeltaName( pDeltaElement->GetName(), pTemp );
		if ( nControlCount == 0 )
			continue;

		int j = info.m_Outputs.AddToTail();
		CombinationOperation_t &op = info.m_Outputs[j];
		op.m_nDeltaStateIndex = i;
		op.m_ControlIndices.AddMultipleToTail( nControlCount, pTemp );

		// Find dominators
		FindDominators( op );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::ComputeCombinationInfo()
{
	int nTargetCount = m_Targets.Count();
	for ( int i = 0; i < nTargetCount; ++i )
	{
		ComputeCombinationInfo( i );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::CleanUpCombinationInfo( int nIndex )
{
	if ( nIndex >= m_CombinationInfo.Count() )
		return;

	CombinationInfo_t &info = m_CombinationInfo[ nIndex ];
	for ( int i = 0; i < COMBO_CONTROL_TYPE_COUNT; ++i )
	{
		info.m_hDestAttribute[i] = DMATTRIBUTE_HANDLE_INVALID;
	}
	info.m_Outputs.RemoveAll();
}


void CDmeCombinationOperator::CleanUpCombinationInfo( )
{
	int nCount = m_CombinationInfo.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CleanUpCombinationInfo( i );
	}
	m_CombinationInfo.RemoveAll();
}


//-----------------------------------------------------------------------------
// helpers for finding corrector names
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::Resolve()
{
	BaseClass::Resolve();

	if ( m_InputControls.IsDirty() || m_Dominators.IsDirty() )
	{
		RebuildRawControlList();
		ComputeCombinationInfo();
	}
}


//-----------------------------------------------------------------------------
// Adds a target for the combination operator
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::AddTarget( CDmElement *pElement )
{
	if ( !pElement )
		return;

	int i = m_Targets.AddToTail( pElement );
	ComputeCombinationInfo( i );
}


//-----------------------------------------------------------------------------
// Sets targets
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::AddTarget( CDmeDag *pDag )
{
	if ( !pDag )
		return;

	CDmeShape *pShape = pDag->GetShape();
	AddTarget( pShape );
	
	int nChildCount = pDag->GetChildCount();
	for ( int i = 0; i < nChildCount; ++i )
	{
		CDmeDag *pChild = pDag->GetChild(i);

		// Do not traverse into models
		if ( CastElement< CDmeModel >( pChild ) )
			continue;

		AddTarget( pChild );
	}
}

 
//-----------------------------------------------------------------------------
// Remaps non-stereo -> stereo, stereo ->left/right, also adds multilevel + filter
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::ComputeInternalControlValue( RawControlIndex_t nRawControlIndex, CombinationControlType_t type, Vector2D &value )
{
	const RawControlInfo_t &info = m_RawControlInfo[nRawControlIndex];
	const Vector &vecControlValue = m_ControlValues[ type ][ info.m_InputControl ];

	const bool bMultiControl = IsMultiControl( info.m_InputControl );
	float flValue = bMultiControl ? vecControlValue.z : vecControlValue.x;

	// Apply multicontrol remapping
	if ( flValue <= info.m_FilterRamp.x || flValue >= info.m_FilterRamp.w )
	{
		flValue = 0.0f;
	}
	else if ( flValue < info.m_FilterRamp.y )
	{
		flValue = RemapVal( flValue, info.m_FilterRamp.x, info.m_FilterRamp.y, 0.0f, 1.0f );
	}
	else if ( flValue > info.m_FilterRamp.z )
	{
		flValue = RemapVal( flValue, info.m_FilterRamp.z, info.m_FilterRamp.w, 1.0f, 0.0f );
	}
	else
	{
		flValue = 1.0f;
	}

	if ( IsEyelidControl( info.m_InputControl ) )
	{
		if ( info.m_bLowerEyelid )
		{
			flValue = ( 1.0f - flValue ) * vecControlValue.x;
		}
		else
		{
			flValue *= vecControlValue.x;
		}
	}
	else if ( bMultiControl )
	{
		flValue *= vecControlValue.x;
	}

	value.x = value.y = flValue;
	if ( IsStereoControl( info.m_InputControl ) )
	{
		if ( vecControlValue.y < 0.5f )
		{
			float flRightAmount = RemapVal( vecControlValue.y, 0.0f, 0.5f, 0.0f, 1.0f );
			value.y *= flRightAmount;
		}
		else
		{
			float flLeftAmount = RemapVal( vecControlValue.y, 0.5f, 1.0f, 1.0f, 0.0f );
			value.x *= flLeftAmount;
		}
	}
}


//-----------------------------------------------------------------------------
// Computes lagged input values from non-lagged input
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::ComputeLaggedInputValues()
{
	if ( !m_bSpecifyingLaggedData )
		return;

	float t = Plat_FloatTime();
	if ( m_flLastLaggedComputationTime == FLT_MIN )
	{
		m_flLastLaggedComputationTime = t;
	}

	float dt = t - m_flLastLaggedComputationTime;
	m_flLastLaggedComputationTime = t;
	float flFactor = ExponentialDecay( 0.8, 0.033, dt );

	int nCount = m_ControlValues[COMBO_CONTROL_NORMAL].Count();
	for ( int i = 0; i < nCount; ++i )
	{
		Vector vecLerp;
		VectorLerp( m_ControlValues[COMBO_CONTROL_NORMAL][i], m_ControlValues[COMBO_CONTROL_LAGGED][i], flFactor, vecLerp );
		m_ControlValues[COMBO_CONTROL_LAGGED].Set( i, vecLerp );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::Operate()
{
	ComputeLaggedInputValues();

	int nCount = m_CombinationInfo.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CombinationInfo_t &info = m_CombinationInfo[i];

		for ( CombinationControlType_t type = COMBO_CONTROL_FIRST; type < COMBO_CONTROL_TYPE_COUNT; type = (CombinationControlType_t)(type+1) )
		{
			if ( ( info.m_hDestAttribute[type] == DMATTRIBUTE_HANDLE_INVALID ) || !g_pDataModel->IsAttributeHandleValid( info.m_hDestAttribute[type] ) )
				continue;

			CDmAttribute* pAttribute = g_pDataModel->GetAttribute( info.m_hDestAttribute[type] );
			if ( !pAttribute || pAttribute->GetType() != AT_VECTOR2_ARRAY )
				continue;

			CDmrArray< Vector2D > vec2D( pAttribute );

			CombinationControlType_t useType = m_bSpecifyingLaggedData ? type : COMBO_CONTROL_NORMAL;
			int nOutputCount = info.m_Outputs.Count();
			for ( int j = 0; j < nOutputCount; ++j )
			{
				CombinationOperation_t &op = info.m_Outputs[j];

				// Compute the core combination
				Vector2D vecValue( 1.0f, 1.0f );
				int nCombinationCount = op.m_ControlIndices.Count();
				for ( int k = 0; k < nCombinationCount; ++k )
				{
					Vector2D v;
					ComputeInternalControlValue( op.m_ControlIndices[k], useType, v );
					vecValue *= v;
				}


				// Compute the dominators
				int nDominatorCount = op.m_DominatorIndices.Count();
				for ( int k = 0; k < nDominatorCount; ++k )
				{
					const CUtlVector< int > &dominantIndices = m_DominatorInfo[ op.m_DominatorIndices[k] ].m_DominantIndices;
					int nDominantCount = dominantIndices.Count();

					Vector2D suppressor( -1.0f, -1.0f );
					for ( int l = 0; l < nDominantCount; ++l )
					{
						Vector2D v;
						ComputeInternalControlValue( dominantIndices[l], useType, v );
						suppressor *= v;
					}
					suppressor.x += 1.0f; suppressor.y += 1.0f;

					vecValue *= suppressor;
				}

				vec2D.Set( op.m_nDeltaStateIndex, vecValue );
			}
		}
	}
}

void CDmeCombinationOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	for ( int i = 0; i < COMBO_CONTROL_TYPE_COUNT; ++i )
	{
		attrs.AddToTail( m_ControlValues[i].GetAttribute() );
	}
}

void CDmeCombinationOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	int nCount = m_CombinationInfo.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CombinationInfo_t &info = m_CombinationInfo[i];
		for ( int j = 0; j < COMBO_CONTROL_TYPE_COUNT; ++j )
		{
			if ( ( info.m_hDestAttribute[j] == DMATTRIBUTE_HANDLE_INVALID ) || !g_pDataModel->IsAttributeHandleValid( info.m_hDestAttribute[j] ) )
				continue;

			CDmAttribute* pAttribute = g_pDataModel->GetAttribute( info.m_hDestAttribute[j] );
			if ( !pAttribute || pAttribute->GetType() != AT_VECTOR2_ARRAY )
				continue;

			attrs.AddToTail( pAttribute );
		}
	}
}


//-----------------------------------------------------------------------------
// Used by studiomdl to discover the various combination rules
//-----------------------------------------------------------------------------
int CDmeCombinationOperator::GetOperationTargetCount() const
{
	return m_Targets.Count();
}

CDmElement *CDmeCombinationOperator::GetOperationTarget( int nTargetIndex )
{
	return m_Targets[ nTargetIndex ];
}

int CDmeCombinationOperator::GetOperationCount( int nTargetIndex ) const
{
	return m_CombinationInfo[ nTargetIndex ].m_Outputs.Count();
}

CDmElement *CDmeCombinationOperator::GetOperationDeltaState( int nTargetIndex, int nOpIndex )
{
	CDmElement *pElement = GetOperationTarget( nTargetIndex );
	const CDmrElementArray<> deltaArray( pElement, "deltaStates" );
	if ( !deltaArray.IsValid() )
		return NULL;

	int nDeltaStateIndex = m_CombinationInfo[ nTargetIndex ].m_Outputs[ nOpIndex ].m_nDeltaStateIndex;
	return deltaArray[ nDeltaStateIndex ];
}

const CUtlVector< int > &CDmeCombinationOperator::GetOperationControls( int nTargetIndex, int nOpIndex ) const
{
	return m_CombinationInfo[ nTargetIndex ].m_Outputs[ nOpIndex ].m_ControlIndices;
}

int CDmeCombinationOperator::GetOperationDominatorCount( int nTargetIndex, int nOpIndex ) const
{
	return m_CombinationInfo[ nTargetIndex ].m_Outputs[ nOpIndex ].m_DominatorIndices.Count();
}

const CUtlVector< int > &CDmeCombinationOperator::GetOperationDominator( int nTargetIndex, int nOpIndex, int nDominatorIndex ) const
{
	int nIndex = m_CombinationInfo[ nTargetIndex ].m_Outputs[ nOpIndex ].m_DominatorIndices[ nDominatorIndex ];
	return m_DominatorInfo[ nIndex ].m_DominantIndices;
}

void CDmeCombinationOperator::CopyControls( CDmeCombinationOperator *pSrc )
{
	// Clean Me Out!
	RemoveAllControls();

	// Copy The Source Controls
	for ( int i = 0; i < pSrc->m_InputControls.Count(); ++i )
	{
		CDmeCombinationInputControl *pSrcInput = pSrc->m_InputControls[ i ];
		if ( pSrcInput )
		{
			CDmeCombinationInputControl *pDstInput = pSrcInput->Copy( );
			pDstInput->SetFileId( GetFileId(), TD_DEEP );
			m_InputControls.AddToTail( pDstInput );
		}
	}

	// Copy The Control Values
	m_ControlValues[ COMBO_CONTROL_NORMAL ].CopyArray(
		pSrc->m_ControlValues[ COMBO_CONTROL_NORMAL ].Base(),
		pSrc->m_ControlValues[ COMBO_CONTROL_NORMAL ].Count() );

	m_ControlValues[ COMBO_CONTROL_LAGGED ].CopyArray(
		pSrc->m_ControlValues[ COMBO_CONTROL_LAGGED ].Base(),
		pSrc->m_ControlValues[ COMBO_CONTROL_LAGGED ].Count() );

	RebuildRawControlList();

	m_Dominators.Purge();

	// Copy The Source Controls
	for ( int i = 0; i < pSrc->m_Dominators.Count(); ++i )
	{
		CDmeCombinationDominationRule *pSrcDom = pSrc->m_Dominators[ i ];
		if ( pSrcDom )
		{
			CDmeCombinationDominationRule *pDstDom = pSrcDom->Copy( );
			pDstDom->SetFileId( GetFileId(), TD_DEEP );
			m_Dominators.AddToTail( pDstDom );
		}
	}

	RebuildRawControlList();
}


//-----------------------------------------------------------------------------
// Generates wrinkle deltas for the uncorrected controls
// NOTE: This is only being used because we have no authoring path for wrinkle data yet
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::GenerateWrinkleDeltas( CDmeShape *pShape, bool bOverwrite )
{
	CDmeMesh* pMesh = CastElement< CDmeMesh >( pShape );
	CDmeVertexData *pBindState = pMesh ? pMesh->FindBaseState( "bind" ) : NULL;
	if ( pBindState )
	{
		int nRawControlCount = m_RawControlInfo.Count();
		for ( int i = 0; i < nRawControlCount; ++i )
		{
			CDmeVertexDeltaData* pDelta = pMesh->FindDeltaState( m_RawControlInfo[i].m_Name );
			if ( pDelta )
			{
				pDelta->GenerateWrinkleDelta( pBindState, m_RawControlInfo[i].m_flWrinkleScale, bOverwrite );
			}
		}
	}
}

void CDmeCombinationOperator::GenerateWrinkleDeltas( bool bOverwrite /* = true */ )
{
	int nTargetCount = m_Targets.Count();
	for ( int i = 0; i < nTargetCount; ++i )
	{
		GenerateWrinkleDeltas( CastElement< CDmeShape >( m_Targets[i] ), bOverwrite );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::RemoveAllTargets()
{
	m_Targets.RemoveAll();

	CleanUpCombinationInfo();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::SetToDefault()
{
	const int nControlsCount = m_InputControls.Count();
	for ( int i = 0; i < nControlsCount; ++i )
	{
		m_IsDefaultValue[ i ] = true;
		UpdateDefaultValue( i );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::SetToBase()
{
	const int nControlsCount = m_InputControls.Count();
	for ( int i = 0; i < nControlsCount; ++i )
	{
		const float flBaseValue = GetControlBaseValue( i );
		for ( int j = 0; j < COMBO_CONTROL_TYPE_COUNT; ++j )
		{
			const Vector &v = m_ControlValues[ j ][ i ];
			m_ControlValues[ j ].Set( i, Vector( flBaseValue, v.y, v.z ) );
		}
	}
}


//-----------------------------------------------------------------------------
// * Remove all controls which refer to raw controls which refer to delta
//   states which do not exist in any target
// * Remove all dominators which refer to states which no longer exist
//-----------------------------------------------------------------------------
void CDmeCombinationOperator::Purge()
{
	bool bDelete = true;

	const int nTargetCount = m_Targets.Count();
	for ( int i = 0; i < nTargetCount; ++i )
	{
		CDmElement *pSource = m_Targets[ i ];
		if ( !pSource )
			continue;

		CDmrElementArray<> deltas( pSource, "deltaStates" );
		if ( !deltas.IsValid() )
			continue;

		const int nDeltaCount = deltas.Count();

		do
		{
			bDelete = false;

			const int nControlCount = GetControlCount();
			for ( int j = 0; j < nControlCount; ++j )
			{
				bDelete = true;
				const int nRawControlCount = GetRawControlCount( j );
				for ( int k = 0; bDelete && k < nRawControlCount; ++k )
				{
					for ( int l = 0; l < nDeltaCount; ++l )
					{
						if ( !Q_strcmp( GetRawControlName( j, k ), deltas[ l ]->GetName() ) )
						{
							bDelete = false;
							break;
						}
					}
				}

				if ( bDelete )
				{
					RemoveControl( GetControlName( j ) );
					break;
				}
			}
		} while ( bDelete );
	}

	do 
	{
		bDelete = false;

		const int nDomCount = DominationRuleCount();
		for ( int i = 0; i < nDomCount; ++i )
		{
			bDelete = true;
			CDmeCombinationDominationRule *pDom = GetDominationRule( i );

			const int nDCount = pDom->DominatorCount();
			for ( int j = 0; j < nDCount; ++j )
			{
				if ( HasRawControl( pDom->GetDominator( j ) ) )
				{
					bDelete = false;
					break;
				}
			}

			if ( !bDelete )
			{
				const int nSCount = pDom->SuppressedCount();
				for ( int j = 0; j < nSCount; ++j )
				{
					if ( HasRawControl( pDom->GetSuppressed( j ) ) )
					{
						bDelete = false;
						break;
					}
				}
			}

			if ( bDelete )
			{
				RemoveDominationRule( i );
				break;
			}
		}
	} while ( bDelete );
}


//-----------------------------------------------------------------------------
// Creates lagged log data from an input log
//-----------------------------------------------------------------------------
static void CreateLaggedLog( CDmeVector2Log *pLog, CDmeVector2Log *pLaggedLog, int nSamplesPerSec )
{
}


//-----------------------------------------------------------------------------
// Helper method to create a lagged version of channel data from source data
//-----------------------------------------------------------------------------
void CreateLaggedVertexAnimation( CDmeChannelsClip *pClip, int nSamplesPerSec )
{
	if ( !pClip )
		return;

	int nChannelCount = pClip->m_Channels.Count();
	for ( int i = 0; i < nChannelCount; ++i )
	{
		CDmeChannel *pChannel = pClip->m_Channels[i];
		CDmElement *pDest = pChannel->GetToElement();
		CDmAttribute *pDestAttr = pChannel->GetToAttribute();
		int nArrayIndex = pChannel->GetToArrayIndex();

		if ( pChannel->GetLog()->GetDataType() != AT_VECTOR2 )
			continue;

		if ( Q_stricmp( pDestAttr->GetName(), "controlValues" ) )
			continue;

		if ( !pDest->GetValue<bool>( "usesLaggedValues" ) )
			continue;

		CDmAttribute *pLaggedAttr = pDest->GetAttribute( "controlValuesLagged" );
		if ( !pLaggedAttr || pLaggedAttr->GetType() != AT_VECTOR2_ARRAY )
			continue;

		int nLen = Q_strlen( pChannel->GetName() );
		char *pNewChannelName = (char*)_alloca( nLen + 10 );
		memcpy( pNewChannelName, pChannel->GetName(), nLen+1 );
		Q_strncpy( &pNewChannelName[nLen], "_lagged", 10 );
		CDmeChannel *pNewChannel = CreateElement< CDmeChannel >( pNewChannelName, pClip->GetFileId() );
		pNewChannel->SetOutput( pLaggedAttr, nArrayIndex );
		CDmeVector2Log *pNewLog = pNewChannel->CreateLog< Vector2D >( );
		pClip->m_Channels.AddToTail( pNewChannel );

		CreateLaggedLog( static_cast<CDmeVector2Log*>( pChannel->GetLog() ), pNewLog, nSamplesPerSec );
	}
}


//-----------------------------------------------------------------------------
//
// A class used to edit combination operators in Maya.. doesn't connect to targets
//
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeMayaCombinationOperator, CDmeMayaCombinationOperator );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeMayaCombinationOperator::OnConstruction()
{
	m_DeltaStates.Init( this, "deltaStates" );
	m_DeltaStateWeights[COMBO_CONTROL_NORMAL].Init( this, "deltaStateWeights" );
	m_DeltaStateWeights[COMBO_CONTROL_LAGGED].Init( this, "deltaStateWeightsLagged" );
	AddTarget( this );
}

void CDmeMayaCombinationOperator::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Add, remove delta states
//-----------------------------------------------------------------------------
void CDmeMayaCombinationOperator::AddDeltaState( const char *pDeltaStateName )
{
	m_DeltaStates.AddToTail( CreateElement<CDmElement>( pDeltaStateName ) );
	for ( int i = 0; i < COMBO_CONTROL_TYPE_COUNT; ++i )
	{
		m_DeltaStateWeights[i].AddToTail( Vector2D( 1.0f, 1.0f ) );
	}
	ComputeCombinationInfo( 0 );
}

void CDmeMayaCombinationOperator::RemoveDeltaState( const char *pDeltaStateName )
{
	int nIndex = FindDeltaState( pDeltaStateName );
	if ( nIndex >= 0 )
	{
		m_DeltaStates.Remove( nIndex );
		for ( int i = 0; i < COMBO_CONTROL_TYPE_COUNT; ++i )
		{
			m_DeltaStateWeights[i].Remove( nIndex );
		}
	}
	ComputeCombinationInfo( 0 );
}

void CDmeMayaCombinationOperator::RemoveAllDeltaStates()
{
	m_DeltaStates.RemoveAll( );
	for ( int i = 0; i < COMBO_CONTROL_TYPE_COUNT; ++i )
	{
		m_DeltaStateWeights[i].RemoveAll();
	}
	ComputeCombinationInfo( 0 );
}

int CDmeMayaCombinationOperator::FindDeltaState( const char *pDeltaStateName )
{
	int nCount = m_DeltaStates.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pDeltaStateName, m_DeltaStates[i]->GetName() ) )
			return i;
	}
	return -1;
}

int CDmeMayaCombinationOperator::DeltaStateCount() const
{
	return m_DeltaStates.Count();
}

const char *CDmeMayaCombinationOperator::GetDeltaState( int nIndex ) const
{
	return m_DeltaStates[nIndex]->GetName();
}

const Vector2D& CDmeMayaCombinationOperator::GetDeltaStateWeight( int nIndex, CombinationControlType_t type ) const
{
	return m_DeltaStateWeights[type][nIndex];
}
