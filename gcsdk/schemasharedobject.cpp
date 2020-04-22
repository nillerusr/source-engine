//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Shared object based on a CBaseRecord subclass
//
//=============================================================================
#include "stdafx.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

namespace GCSDK
{

#ifdef GC

//----------------------------------------------------------------------------
// Purpose: Returns true if the object needs to be saved to the database
//----------------------------------------------------------------------------
bool CSchemaSharedObjectHelper::BYieldingAddToDatabase( CRecordBase *pRecordBase ) 
{
	if( pRecordBase->GetPSchema()->GetRecordInfo()->BHasIdentity() )
	{
		CSQLAccess sqlAccess;
		return sqlAccess.BYieldingInsertWithIdentity( pRecordBase );
	}
	return false;
}


//----------------------------------------------------------------------------
// Purpose: Returns true if the object needs to be saved to the database
//----------------------------------------------------------------------------
bool CSchemaSharedObjectHelper::BYieldingAddInsertToTransaction( CSQLAccess & sqlAccess, CRecordBase *pRecordBase )
{
	Assert( !pRecordBase->GetPSchema()->GetRecordInfo()->BHasIdentity() );
	return sqlAccess.BYieldingInsertRecord( pRecordBase );
}


//----------------------------------------------------------------------------
// Purpose: Reads the object from the database via its primary key. Initialize
//			the primary key fields before you call this.
//----------------------------------------------------------------------------
bool CSchemaSharedObjectHelper::BYieldingReadFromDatabase( CRecordBase *pRecordBase ) 
{
	CSQLAccess sqlAccess;
	CColumnSet csPK( pRecordBase->GetPSchema()->GetRecordInfo() );
	csPK.MakePrimaryKey();
	CColumnSet csReadFields( pRecordBase->GetPSchema()->GetRecordInfo() );
	csReadFields.MakeInverse( csPK );
	if( k_EResultOK != sqlAccess.YieldingReadRecordWithWhereColumns( pRecordBase, csReadFields, csPK ) )
		return false;

	return true;
}


//----------------------------------------------------------------------------
// Purpose: Writes the non-PK fields on the object to the database
//----------------------------------------------------------------------------
bool CSchemaSharedObjectHelper::BYieldingAddWriteToTransaction( CSQLAccess & sqlAccess, CRecordBase *pRecordBase, const CColumnSet & csDatabaseDirty ) 
{
	CColumnSet csPK( pRecordBase->GetPSchema()->GetRecordInfo() );
	csPK.MakePrimaryKey();

	if(	!sqlAccess.BYieldingUpdateRecord( *pRecordBase, csPK, csDatabaseDirty ) )
		return false;
	return true;
}


//----------------------------------------------------------------------------
// Purpose: Adds the primary key for this object to the network message
//----------------------------------------------------------------------------
bool CSchemaSharedObjectHelper::BYieldingAddRemoveToTransaction( CSQLAccess & sqlAccess, CRecordBase *pRecordBase )
{
	CColumnSet csPK( pRecordBase->GetPSchema()->GetRecordInfo() );
	csPK.MakePrimaryKey();

	if(	!sqlAccess.BYieldingDeleteRecords( *pRecordBase, csPK ) )
		return false;

	return true;
}
#endif // GC


//----------------------------------------------------------------------------
// Purpose: Dumps diagnostic information about the shared object
//----------------------------------------------------------------------------
void CSchemaSharedObjectHelper::Dump( const CRecordBase *pRecordBase )
{
	CFmtStr1024 sOutput;
	const CRecordInfo *pRecordInfo = pRecordBase->GetPRecordInfo();
	for( uint32 unColumn = 0; unColumn < (uint32)pRecordInfo->GetNumColumns(); unColumn++ )
	{
		if( unColumn > 0 )
		{
			sOutput += ", ";
		}

		sOutput += pRecordInfo->GetColumnInfo( unColumn ).GetName();
		sOutput += "=";

		char pchFieldData[128];
		pRecordBase->RenderField( unColumn, sizeof(pchFieldData), pchFieldData );
		sOutput += pchFieldData;
	}

	EmitInfo( SPEW_CONSOLE, SPEW_ALWAYS, LOG_ALWAYS, "\t\t%s\n", sOutput.Access() );
}


//=============================================================================



#ifdef GC

//----------------------------------------------------------------------------
// Purpose: Returns true if the object needs to be saved to the database
//----------------------------------------------------------------------------
bool CSchemaSharedObjectBase::BYieldingAddToDatabase() 
{
	if( GetPObject()->GetPSchema()->GetRecordInfo()->BHasIdentity() )
	{
		return CSchemaSharedObjectHelper::BYieldingAddToDatabase( GetPObject() );
	}
	else
		return CSharedObject::BYieldingAddToDatabase();
}


//----------------------------------------------------------------------------
// Purpose: Returns true if the object needs to be saved to the database
//----------------------------------------------------------------------------
bool CSchemaSharedObjectBase::BYieldingAddInsertToTransaction( CSQLAccess & sqlAccess )
{
	return CSchemaSharedObjectHelper::BYieldingAddInsertToTransaction( sqlAccess, GetPObject() );
}


//----------------------------------------------------------------------------
// Purpose: Reads the object from the database via its primary key. Initialize
//			the primary key fields before you call this.
//----------------------------------------------------------------------------
bool CSchemaSharedObjectBase::BYieldingReadFromDatabase() 
{
	return CSchemaSharedObjectHelper::BYieldingReadFromDatabase( GetPObject() );
}

CColumnSet CSchemaSharedObjectBase::GetDatabaseDirtyColumnSet( const CUtlVector< int > &fields ) const
{
	CColumnSet cs( GetPObject()->GetPRecordInfo() );
	FOR_EACH_VEC( fields, i )
	{
		cs.BAddColumn( fields[i] );
	}

	return cs;
}

//----------------------------------------------------------------------------
// Purpose: Writes the non-PK fields on the object to the database
//----------------------------------------------------------------------------
bool CSchemaSharedObjectBase::BYieldingAddWriteToTransaction( CSQLAccess & sqlAccess, const CUtlVector< int > &fields ) 
{
	CColumnSet csDatabaseDirty = GetDatabaseDirtyColumnSet(fields);
	return CSchemaSharedObjectHelper::BYieldingAddWriteToTransaction( sqlAccess, GetPObject(), csDatabaseDirty );
}


//----------------------------------------------------------------------------
// Purpose: Adds the primary key for this object to the network message
//----------------------------------------------------------------------------
bool CSchemaSharedObjectBase::BYieldingAddRemoveToTransaction( CSQLAccess & sqlAccess )
{
	if ( CSchemaSharedObjectHelper::BYieldingAddRemoveToTransaction( sqlAccess, GetPObject() ) )
	{
		return true;
	}
	return false;
}


#endif // GC

//----------------------------------------------------------------------------
// Purpose: Returns true if this is less than than the object in soRHS. This
//			comparison is deterministic, but it may not be pleasing to a user
//			since it is just going to compare raw memory. If you need a sort 
//			that is user-visible you will need to do it at a higher level that
//			actually knows what the data in these objects means.
//----------------------------------------------------------------------------
bool CSchemaSharedObjectBase::BIsKeyLess( const CSharedObject & soRHS ) const
{
	Assert( GetTypeID() == soRHS.GetTypeID() );
	const CSchemaSharedObjectBase & soSchemaRHS = (const CSchemaSharedObjectBase &)soRHS;

	CColumnSet csPK( GetPObject()->GetPSchema()->GetRecordInfo() );
	csPK.MakePrimaryKey();

	FOR_EACH_COLUMN_IN_SET( csPK, unColumnIndex )
	{
		uint8 *pubMyData, *pubTheirData;
		uint32 cubMyData, cubTheirData;
		int nColumn = csPK.GetColumn( unColumnIndex );
		DbgVerify( GetPObject()->BGetField( nColumn, &pubMyData, &cubMyData ) );
		DbgVerify( soSchemaRHS.GetPObject()->BGetField( nColumn, &pubTheirData, &cubTheirData ) );

		int nCmp = Q_memcmp( pubMyData, pubTheirData, MIN( cubMyData, cubTheirData ) );

		// longer chunks of memory are greater
		if( nCmp == 0 )
		{
			nCmp = cubTheirData > cubMyData;
		}

		if( nCmp != 0 )
			return nCmp < 0;
	}

	return false;
}


//----------------------------------------------------------------------------
// Purpose: Copy the data from the specified schema shared object into this. 
//			Both objects must be of the same type.
//----------------------------------------------------------------------------
void CSchemaSharedObjectBase::Copy( const CSharedObject & soRHS )
{
	Assert( GetTypeID() == soRHS.GetTypeID() );
	CSchemaSharedObjectBase & soRHSBase = (CSchemaSharedObjectBase &)soRHS;
	*GetPObject() = *(soRHSBase.GetPObject());
}


//----------------------------------------------------------------------------
// Purpose: Dumps diagnostic information about the shared object
//----------------------------------------------------------------------------
void CSchemaSharedObjectBase::Dump() const
{
	CSchemaSharedObjectHelper::Dump( GetPObject() );
}


//----------------------------------------------------------------------------
// Purpose: Returns the record info object for this type of schema object
//----------------------------------------------------------------------------
const CRecordInfo * CSchemaSharedObjectBase::GetRecordInfo() const
{
	return GetPObject()->GetPRecordInfo();
}


//----------------------------------------------------------------------------
// Purpose: Claims all memory for the object.
//----------------------------------------------------------------------------
#ifdef DBGFLAG_VALIDATE
void CSchemaSharedObjectBase::Validate( CValidator &validator, const char *pchName )
{
	CSharedObject::Validate( validator, pchName );

	// these are INSIDE the function instead of outside so the interface 
	// doesn't change
	VALIDATE_SCOPE();
}
#endif


} // namespace GCSDK


