//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Network dirty field marker for shared objects
//
//=============================================================================

#ifndef GC_DIRTYFIELD_H
#define GC_DIRTYFIELD_H
#ifdef _WIN32
#pragma once
#endif

//#include "sharedobject.h"

namespace GCSDK
{

	class CSharedObject;

//----------------------------------------------------------------------------
// Purpose: Holds the set of dirty fields for this object
//----------------------------------------------------------------------------
class CSharedObjectDirtyFieldList
{
public:
	CSharedObjectDirtyFieldList( CSharedObject *obj );
	~CSharedObjectDirtyFieldList();

	CSharedObject *Obj() const;
	void DirtyField( int index );
	void GetDirtyFieldSet( CUtlVector<int> &fieldSet ) const;

private:
	CSharedObject *m_obj;
	uint32 m_firstFieldBits;
	CUtlVector<int> *m_pExtendedFields;
};


//----------------------------------------------------------------------------
// Purpose: Holds a list of dirty fields on objects
//----------------------------------------------------------------------------
class CSharedObjectDirtyList
{
public:
	CSharedObjectDirtyList();
	~CSharedObjectDirtyList();

	void DirtyObjectField( CSharedObject *obj, int field );

	int InvalidIndex() const;
	int NumDirtyObjects() const;
	int FindIndexByObj( const CSharedObject *pObj ) const;
	bool HasElement( const CSharedObject *pObj ) const;
	bool GetDirtyFieldSetByIndex( int index, CSharedObject **ppObj, CUtlVector<int> &fieldSet ) const;
	bool GetDirtyFieldSetByObj( CSharedObject *pObj, CUtlVector<int> &fieldSet );
	bool FindAndRemove( CSharedObject *pObj );
	void RemoveAll();

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const char *pchName );
#endif

private:

	CUtlVector< CSharedObjectDirtyFieldList > m_sharedObjectDirtyFieldList;
};

inline int CSharedObjectDirtyList::InvalidIndex() const
{
	return m_sharedObjectDirtyFieldList.InvalidIndex();
}

inline int CSharedObjectDirtyList::NumDirtyObjects() const
{
	return m_sharedObjectDirtyFieldList.Count();
}

} // GCSDK


#endif //GC_DIRTYFIELD_H
