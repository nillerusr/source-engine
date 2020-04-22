//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Network dirty field marker for shared objects
//
//=============================================================================
#include "stdafx.h"
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{
 
CSharedObjectDirtyFieldList::CSharedObjectDirtyFieldList( CSharedObject *obj )
: m_obj( obj )
, m_firstFieldBits( 0 )
, m_pExtendedFields( NULL )
{
}

CSharedObjectDirtyFieldList::~CSharedObjectDirtyFieldList()
{
	if ( m_pExtendedFields )
	{
		delete m_pExtendedFields;
	}
}

CSharedObject *CSharedObjectDirtyFieldList::Obj() const
{
	return m_obj;
}

void CSharedObjectDirtyFieldList::DirtyField( int index )
{
	// if the field index fits within our dirty fields struct, set a bit to track it
	if ( index < 32 )
	{
		m_firstFieldBits |= (1 << index);
	}
	// if the field index is too big, store it in our backup structure; this is less efficient but more
	// flexible, especially when dealing with fields that themselves contain flags so would be interpreted
	// as huge numbers
	else
	{
		if ( !m_pExtendedFields )
			m_pExtendedFields = new CUtlVector<int>;

		if ( !m_pExtendedFields->HasElement( index ) )
			m_pExtendedFields->AddToTail( index );
	}
}

void CSharedObjectDirtyFieldList::GetDirtyFieldSet( CUtlVector<int> &fieldSet ) const
{
	fieldSet.Purge();
	if( !m_firstFieldBits && !m_pExtendedFields )
	{
		return;
	}

	if( m_firstFieldBits )
	{
		for( int i = 0; i < 32; i++ )
		{
			// handle dirty bits
			if( m_firstFieldBits & ( 1 << i ) )
			{
				fieldSet.AddToTail( i );
			}
		}
	}

	// handle higher order dirty-fields list if present
	if ( m_pExtendedFields )
	{
		fieldSet.AddVectorToTail( *m_pExtendedFields );
	}
}

//-----------------------------------------------------------------------------
CSharedObjectDirtyList::CSharedObjectDirtyList()
{
}

CSharedObjectDirtyList::~CSharedObjectDirtyList()
{
}

void CSharedObjectDirtyList::DirtyObjectField( CSharedObject *obj, int field )
{
	MEM_ALLOC_CREDIT_CLASS();
	Assert( obj != NULL );
	if (!obj)
	{
		return;
	}

	int index = FindIndexByObj( obj );
	if ( index == InvalidIndex() )
	{
		index = m_sharedObjectDirtyFieldList.AddToTail( CSharedObjectDirtyFieldList( obj ) );
	}

	m_sharedObjectDirtyFieldList[index].DirtyField( field );
}

int CSharedObjectDirtyList::FindIndexByObj( const CSharedObject *pObj ) const
{
	// Slow method for now, we can speed this up with a map later
	for ( int i = 0; i < m_sharedObjectDirtyFieldList.Count(); ++i )
	{
		if( m_sharedObjectDirtyFieldList[i].Obj() == pObj )
		{
			return i;
		}
	}

	return InvalidIndex();
}

bool CSharedObjectDirtyList::HasElement( const CSharedObject *pObj ) const
{
	return FindIndexByObj( pObj ) != InvalidIndex();
}

bool CSharedObjectDirtyList::GetDirtyFieldSetByIndex( int index, CSharedObject **ppObj, CUtlVector<int> &fieldSet ) const
{
	VPROF_BUDGET( "CSharedObjectDirtyList::GetDirtyFieldSetByIndex", VPROF_BUDGETGROUP_STEAM );
	if( !m_sharedObjectDirtyFieldList.IsValidIndex( index ) )
	{
		fieldSet.Purge();
		if( ppObj )
		{
			*ppObj = NULL;
		}
		return false;
	}
	const CSharedObjectDirtyFieldList &fieldList = m_sharedObjectDirtyFieldList[index];
	if( ppObj )
	{
		*ppObj = fieldList.Obj();
	}
	fieldList.GetDirtyFieldSet( fieldSet );
	return true;
}

bool CSharedObjectDirtyList::GetDirtyFieldSetByObj( CSharedObject *pObj, CUtlVector<int> &fieldSet )
{
	int index = FindIndexByObj( pObj );
	if ( index == InvalidIndex() )
	{
		fieldSet.Purge();
		return false;
	}

	CSharedObjectDirtyFieldList &fieldList = m_sharedObjectDirtyFieldList[index];
	fieldList.GetDirtyFieldSet( fieldSet );
	return true;
}

bool CSharedObjectDirtyList::FindAndRemove( CSharedObject *pObj )
{
	int index = FindIndexByObj( pObj );
	if ( index == InvalidIndex() )
	{
		return false;
	}

	m_sharedObjectDirtyFieldList.Remove( index );
	return true;
}

void CSharedObjectDirtyList::RemoveAll()
{
	m_sharedObjectDirtyFieldList.RemoveAll();
}

#ifdef DBGFLAG_VALIDATE
void CSharedObjectDirtyList::Validate( CValidator &validator, const char *pchName )
{
	VALIDATE_SCOPE();

	ValidateObj( m_sharedObjectDirtyFieldList );
}
#endif

} // namespace GCSDK



