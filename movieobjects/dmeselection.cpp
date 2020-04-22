//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================


#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects/dmeselection.h"


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeComponent, CDmeComponent );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeComponent::OnConstruction()
{
	m_Type.InitAndSet( this, "componentType", COMP_INVALID );
	m_bComplete.InitAndSet( this, "complete", false );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeComponent::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeComponent::Resolve()
{
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeSingleIndexedComponent, CDmeSingleIndexedComponent );


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::OnConstruction()
{
	m_CompleteCount.InitAndSet( this, "completeCount", 0 );
	m_Components.Init( this, "components" );
	m_Weights.Init( this, "weights" );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::OnDestruction()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::Resolve()
{
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmeSingleIndexedComponent::Count() const
{
	return IsComplete() ? m_CompleteCount.Get() : m_Components.Count();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeSingleIndexedComponent::SetType( Component_t type )
{
	switch ( type )
	{
	case COMP_VTX:
	case COMP_FACE:
		m_Type.Set( type );
		return true;
	default:
		break;
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::AddComponent( int component, float weight /*= 1.0f */ )
{
	if ( IsComplete() )
		return;

	const int index( BinarySearch( component ) );
	Assert( index >= 0 );
	Assert( index <= m_Components.Count() );
	if ( index == m_Components.Count() )
	{
		m_Components.AddToTail( component );
		m_Weights.AddToTail( weight );
	}
	else if ( component == m_Components.Get( index ) )
	{
		Assert( index < m_Weights.Count() );
		m_Weights.Set( index, weight );
	}
	else
	{
		m_Components.InsertBefore( index, component );
		m_Weights.InsertBefore( index, weight );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::AddComponents( const CUtlVector< int > &components )
{
	const int nComponents( components.Count() );
	for ( int i( 0 ); i < nComponents; ++i )
	{
		AddComponent( components[ i ] );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::AddComponents(
	const CUtlVector< int > &components, const CUtlVector< float > &weights )
{
	const int nComponents( min( components.Count(), weights.Count() ) );
	for ( int i( 0 ); i < nComponents; ++i )
	{
		AddComponent( components[ i ], weights[ i ] );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::RemoveComponent( int component )
{
	Assert( !IsComplete() );	// TODO: Convert from complete to complete - component

	const int cIndex = BinarySearch( component );
	if ( cIndex >= m_Components.Count() || m_Components[ cIndex ] != component )	// Component not in selection
		return;

	m_Components.Remove( cIndex );
	m_Weights.Remove( cIndex );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeSingleIndexedComponent::GetComponent( int index, int &component, float &weight ) const
{
	if ( index < Count() )
	{
		if ( IsComplete() )
		{
			component = index;
			weight = 1.0f;
		}
		else
		{
			component = m_Components[ index ];
			weight = m_Weights[ index ];
		}

		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::GetComponents( CUtlVector< int > &components, CUtlVector< float > &weights ) const
{
	if ( IsComplete() )
	{
		const int nComponents = Count();

		int *pComponents = reinterpret_cast< int * >( alloca( nComponents * sizeof( int ) ) );
		float *pWeights = reinterpret_cast< float * >( alloca( nComponents * sizeof( float ) ) );

		for ( int i = 0; i < nComponents; ++i )
		{
			pComponents[ i ] = i;
			pWeights[ i ] = 1.0f;
		}

		components.CopyArray( pComponents, nComponents );
		weights.CopyArray( pWeights, nComponents );
	}
	else
	{
		components.RemoveAll();
		components.CopyArray( m_Components.Base(), m_Components.Count() );

		weights.RemoveAll();
		weights.CopyArray( m_Weights.Base(), m_Weights.Count() );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::GetComponents( CUtlVector< int > &components ) const
{
	if ( IsComplete() )
	{
		const int nComponents = Count();

		int *pComponents = reinterpret_cast< int * >( alloca( nComponents * sizeof( int ) ) );

		for ( int i = 0; i < nComponents; ++i )
		{
			pComponents[ i ] = i;
		}

		components.CopyArray( pComponents, nComponents );
	}
	else
	{
		components.RemoveAll();
		components.CopyArray( m_Components.Base(), m_Components.Count() );
	}
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::SetComplete( int nComplete )
{
	m_bComplete.Set( true );
	m_CompleteCount.Set( max( 0, nComplete ) );
	m_Components.Purge();
	m_Weights.Purge();
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CDmeSingleIndexedComponent::GetComplete() const
{
	return IsComplete() ? m_CompleteCount.Get() : 0;
}


//-----------------------------------------------------------------------------
// Reset to an empty selection
//-----------------------------------------------------------------------------
inline void CDmeSingleIndexedComponent::Clear()
{
	CDmeComponent::Clear();
	m_CompleteCount.Set( 0 );
	m_Components.RemoveAll();
	m_Weights.RemoveAll();
}


//-----------------------------------------------------------------------------
// Searches for the component in the sorted component list and returns the
// index if it's found or if it's not found, returns the index at which it
// should be inserted to maintain the sorted order of the component list
//-----------------------------------------------------------------------------
int CDmeSingleIndexedComponent::BinarySearch( int component ) const
{
	const CUtlVector< int > &components( m_Components.Get() );
	const int nComponents( components.Count() );

	int left( 0 );
	int right( nComponents - 1 );
	int mid;

	while ( left <= right )
	{
		mid = ( left + right ) >> 1;	// floor( ( left + right ) / 2.0 )
		if ( component > m_Components[ mid ] )
		{
			left = mid + 1;
		}
		else if ( component < m_Components[ mid ] )
		{
			right = mid - 1;
		}
		else
		{
			return mid;
		}
	}

	return left;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeSingleIndexedComponent::HasComponent( int component ) const
{
	if ( IsComplete() )
		return true;

	const int cIndex = BinarySearch( component );
	if ( cIndex >= m_Components.Count() )
		return false;

	if ( m_Components[ cIndex ] == component )
		return true;

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CDmeSingleIndexedComponent::GetWeight( int component, float &weight ) const
{
	Assert( !IsComplete() );

	const int cIndex = BinarySearch( component );
	if ( cIndex >= m_Components.Count() )
		return false;

	if ( m_Components[ cIndex ] == component )
	{
		weight = m_Weights[ cIndex ];
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::Subtract( const CDmeSingleIndexedComponent &rhs )
{
	const int nLhs = Count();
	const int nRhs = rhs.Count();

	int l = 0;
	int r = 0;

	if ( IsComplete() )
	{
		// TODO
		Assert( 0 );
	}
	else
	{
		CUtlVector< int > newComponents;
		newComponents.EnsureCapacity( nLhs );
		CUtlVector< float > newWeights;
		newWeights.EnsureCapacity( nLhs );

		while ( l < nLhs || r < nRhs )
		{
			// In LHS but not RHS
			while ( l < nLhs && ( r >= nRhs || m_Components[ l ] < rhs.m_Components[ r ] ) )
			{
				newComponents.AddToTail( m_Components[ l ] );
				newWeights.AddToTail( m_Weights[ l ] );
				++l;
			}

			// In RHS but not LHS
			while ( r < nRhs && ( l >= nLhs || m_Components[ l ] > rhs.m_Components[ r ] ) )
			{
				++r;
			}

			// In Both LHS & RHS
			while ( l < nLhs && r < nRhs && m_Components[ l ] == rhs.m_Components[ r ] )
			{
				++l;
				++r;
			}
		}

		m_Components.CopyArray( newComponents.Base(), newComponents.Count() );
		m_Weights.CopyArray( newWeights.Base(), newWeights.Count() );
	}

	m_CompleteCount.Set( 0 );
	m_bComplete.Set( false );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::Add( const CDmeSingleIndexedComponent &rhs )
{
	int nLhs = Count();
	const int nRhs = rhs.Count();

	int l = 0;
	int r = 0;

	if ( IsComplete() )
	{
		if ( rhs.IsComplete() && nRhs > nLhs )
		{
			m_CompleteCount.Set( nRhs );
		}
		else
		{
			while ( r < nRhs )
			{
				if ( rhs.m_Components[ r ] >= nLhs )
				{
					// Got one that's greater than the complete count of this one

					CUtlVector< int > newComponents;
					newComponents.EnsureCapacity( nLhs + nRhs - r );

					CUtlVector< float > newWeights;
					newWeights.EnsureCapacity( nLhs  + nRhs - r );

					GetComponents( newComponents, newWeights );

					while ( r < nRhs )
					{
						newComponents.AddToTail( rhs.m_Components[ r ] );
						newWeights.AddToTail( rhs.m_Weights[ r ] );
						++r;
					}

					m_Components.CopyArray( newComponents.Base(), newComponents.Count() );
					m_Weights.CopyArray( newWeights.Base(), newWeights.Count() );

					m_CompleteCount.Set( 0 );
					m_bComplete.Set( false );

					break;
				}

				++r;
			}
		}
	}
	else
	{
		CUtlVector< int > newComponents;
		newComponents.EnsureCapacity( nLhs + nRhs * 0.5 );	// Just an estimate assuming 50% of the components in rhs aren't in lhs
		CUtlVector< float > newWeights;
		newWeights.EnsureCapacity( nLhs  + nRhs * 0.5 );	// Just an estimate

		while ( l < nLhs || r < nRhs )
		{
			while ( l < nLhs && ( r >= nRhs || m_Components[ l ] < rhs.m_Components[ r ] ) )
			{
				newComponents.AddToTail( m_Components[ l ] );
				newWeights.AddToTail( m_Weights[ l ] );
				++l;
			}

			// In RHS but not LHS
			while ( r < nRhs && ( l >= nLhs || m_Components[ l ] > rhs.m_Components[ r ] ) )
			{
				newComponents.AddToTail( rhs.m_Components[ r ] );
				newWeights.AddToTail( rhs.m_Weights[ r ] );
				++r;
			}

			// In Both LHS & RHS
			while ( l < nLhs && r < nRhs && m_Components[ l ] == rhs.m_Components[ r ] )
			{
				newComponents.AddToTail( m_Components[ l ] );
				newWeights.AddToTail( m_Weights[ l ] );
				++l;
				++r;
			}
		}

		m_Components.CopyArray( newComponents.Base(), newComponents.Count() );
		m_Weights.CopyArray( newWeights.Base(), newWeights.Count() );
	}

	m_CompleteCount.Set( 0 );
	m_bComplete.Set( false );
}


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CDmeSingleIndexedComponent::Intersection( const CDmeSingleIndexedComponent &rhs )
{
	const int nLhs = Count();
	const int nRhs = rhs.Count();

	int l = 0;
	int r = 0;

	if ( IsComplete() )
	{
		// TODO
		Assert( 0 );
	}
	else
	{
		CUtlVector< int > newComponents;
		newComponents.EnsureCapacity( nLhs );
		CUtlVector< float > newWeights;
		newWeights.EnsureCapacity( nLhs );

		while ( l < nLhs || r < nRhs )
		{
			// In LHS but not RHS
			while ( l < nLhs && ( r >= nRhs || m_Components[ l ] < rhs.m_Components[ r ] ) )
			{
				++l;
			}

			// In RHS but not LHS
			while ( r < nRhs && ( l >= nLhs || m_Components[ l ] > rhs.m_Components[ r ] ) )
			{
				++r;
			}

			// In Both LHS & RHS
			while ( l < nLhs && r < nRhs && m_Components[ l ] == rhs.m_Components[ r ] )
			{
				newComponents.AddToTail( m_Components[ l ] );
				newWeights.AddToTail( m_Weights[ l ] );
				++l;
				++r;
			}
		}

		m_Components.CopyArray( newComponents.Base(), newComponents.Count() );
		m_Weights.CopyArray( newWeights.Base(), newWeights.Count() );
	}

	m_CompleteCount.Set( 0 );
	m_bComplete.Set( false );
}