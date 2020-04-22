//====== Copyright ©, Valve Corporation, All rights reserved. =================
//
// Purpose: Defines a directory for all the GC processes for an app
//
//=============================================================================


#ifndef DIRECTORY_H
#define DIRECTORY_H
#ifdef _WIN32
#pragma once
#endif

#include "tier1/utlvector.h"
#include "tier1/utlsortvector.h"
#include "tier1/utlmap.h"
#include "tier1/keyvalues.h"
#include "gamecoordinator/igamecoordinator.h"

namespace GCSDK
{


class CGCDirProcess;

//-----------------------------------------------------------------------------
// Purpose: Entry for a GC instance
//-----------------------------------------------------------------------------
class CGCDirTypeInstance
{
public:
	CGCDirTypeInstance( const char* pszTypeName, uint32 nType, uint32 nInstance, const KeyValues *pKVConfig, const CGCDirProcess* pProcess );
	~CGCDirTypeInstance();

	uint32					GetType() const;
	const char*				GetTypeName() const;
	uint32					GetInstance() const;
	KeyValues *				GetConfig() const;
	const CGCDirProcess*	GetProcess() const;

private:
	int32					m_nType;
	uint32					m_nInstance;
	KeyValues *				m_pConfig;
	CUtlConstString			m_sTypeName;
	const CGCDirProcess*	m_pProcess;
};


//-----------------------------------------------------------------------------
// Purpose: Entry for a physical machine hosting one or more GC instances
//-----------------------------------------------------------------------------
class CGCDirProcess
{
public:
	CGCDirProcess( uint32 nDirIndex, const char *pchProcessName, const char* pchProcessType, const KeyValues *pKVConfig );
	~CGCDirProcess( );

	//determines if this process has any GC's of the specified type
	bool						HasInstanceOfType( uint32 nType ) const;
	uint32						GetGCDirIndex() const;
	const char *				GetName() const;
	const char *				GetProcessType() const;
	uint32						GetTypeInstanceCount() const;
	KeyValues *					GetConfig() const;
	const CGCDirTypeInstance *	GetTypeInstance( uint32 nGCIndex ) const;

	//gets the list of unique types contained within this process as IDs (note order does not match)
	const CUtlSortVector< uint32 >&	GetUniqueTypeList() const;

private:
	friend class CDirectory;
	void AddGC( CGCDirTypeInstance *pGC );

	uint32								m_iDirGC;
	CUtlConstString						m_sName;
	CUtlConstString						m_sType;
	KeyValues *							m_pConfig;
	CUtlVector<CGCDirTypeInstance *>	m_vecGCs;
	//a bit mask of which types exist on this GC
	uint64								m_nTypeMask;
	//optimized list for fast searching of which types this process has
	CUtlSortVector< uint32 >			m_vecUniqueTypeList;
};


//-----------------------------------------------------------------------------
// Purpose: Defines what kind of GCs exist for this app in the current universe,
//	the types that exist, and what machines they run on
//-----------------------------------------------------------------------------
class CDirectory
{
public:
	typedef CGCBase *(*GCFactory_t)( const CGCDirProcess *pGCDirProcess );
	static const int s_nInvalidGCType = -1;

	CDirectory();
	~CDirectory();

	bool BInit( KeyValues *pKVDirectory );
	void RegisterGCType( int32 nTypeID, const char *pchName );

	uint32 GetProcessCount() const;
	const CGCDirProcess *GetProcess( int32 nIndex ) const;

	uint32 GetGCTypeInstanceCount() const;
	const CGCDirTypeInstance *GetGCTypeInstance( uint32 nTypeInst ) const;

	int32 GetGCCountForType( int32 nTypeID ) const;
	const CGCDirTypeInstance *GetGCInstanceForType( int32 nTypeID, int32 nInstance ) const;

	const char *GetNameForGCType( int32 nTypeID ) const;
	int32 GetGCTypeForName( const char *pchName ) const;

	void RegisterProcessTypeFactory( const char* pszProcessType, GCFactory_t pfnFactory );
	GCFactory_t GetFactoryForProcessType( const char* pszProcessType ) const;

	//given a GC type, and an instance number, this will return the GC directory index used for routing. If the instance number
	//is greater than what is available, it will be modulo'd into the acceptable range. If no match for that type can be found, -1 will
	//be returned
	int32 GetGCDirIndexForInstance( int32 nTypeID, uint32 nInstanceIndex ) const;

	//given a GC type, this will add all of the GC indices associated with that type onto the provided vector
	void GetGCDirIndicesForType( int32 nTypeID, CUtlVector< uint32 >& nIndices ) const;

private:
	bool						m_bInitialized;
	CUtlVector<CGCDirProcess *>			m_vecProcesses;
	CUtlVector<CGCDirTypeInstance *>	m_vecTypeInstances;
	CUtlMap< int32, CCopyableUtlVector< CGCDirTypeInstance *> >	m_mapGCsByType;

	struct RegisteredGCType_t
	{
		CUtlConstString	m_strName;
	};
	CUtlMap< int32, RegisteredGCType_t > m_mapRegisteredGCTypes;
	CUtlDict< int32 > m_dictRegisteredGCNameToType;
	CUtlDict< GCFactory_t > m_dictProcessTypeToFactory;
};

// Gets the global directory singleton
extern CDirectory *GDirectory();

//the maximum value that a GC type can use. This is to enable bit masks to encode type information primarily (also don't allow the high bit to avoid SQL signed issues)
#define GC_TYPE_MAX_NUMBER						62

// Registration macro for GC types
#define REG_GC_TYPE( typeNum, typeName )													\
	static class CRegGC_##typeName															\
	{																						\
	public:																					\
		CRegGC_##typeName() {																\
			COMPILE_TIME_ASSERT( typeNum <= GC_TYPE_MAX_NUMBER );							\
			GDirectory()->RegisterGCType( ( typeNum ), ( #typeName ) ); }					\
	} g_RegGC_##typeName;

#define REG_GC_PROCESS_TYPE( gcClass, typeName )																\
	static class CRegProcessType_##typeName																		\
	{																											\
	public:																										\
		static CGCBase *Factory( const CGCDirProcess *pDirGC ) { return new gcClass( pDirGC ); }				\
		CRegProcessType_##typeName() { GDirectory()->RegisterProcessTypeFactory( ( #typeName ), &Factory ); }	\
	} g_RegProcessType_##typeName;

} // namespace GCSDK
#endif // DIRECTORY_H
