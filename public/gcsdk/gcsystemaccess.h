//========= Copyright (c), Valve LLC, All rights reserved. ============
//
// Purpose: A utility for tracking access to various systems
//
//=============================================================================
#ifndef GCSYSTEMACCESS_H
#define GCSYSTEMACCESS_H

#pragma once

namespace GCSDK
{

//behaviors that can be triggered for system access violation. Each aspect of this system has
//actions that can be enabled or suppressed. The algorithm for this is to combine all the enabled
//actions, then remove all the suppressed and perform those operations when a violation is found
enum EGCAccessAction
{
	//returns failure to the verifying code so it can handle it accordingly
	GCAccessAction_ReturnFail	= ( 1<<0 ),
	//track this into a stats overview for number of offenses
	GCAccessAction_TrackFail	= ( 1<<1 ),
	//track this into a stats overview for number of accesses
	GCAccessAction_TrackSuccess	= ( 1<<2 ),
	//report this violation to the console/log
	GCAccessAction_Msg			= ( 1<<3 ),
	//generate an assert at the point this is violated
	GCAccessAction_Assert		= ( 1<<4 ),
	//activate a breakpoint
	GCAccessAction_Breakpoint	= ( 1<<5 )
};

//how we identify an actual system
typedef uint32 GCAccessSystem_t;

//a structure that is inserted into a context object and holds onto GCAccessSystem index that the context can then initialize itself from. Classes can derive
//from this to initialize itself, but derived class may NOT have any members as that would change the binary layout of the object
class CGCAContextSystemRef
{
public:
	CGCAContextSystemRef( GCAccessSystem_t id ) : m_SystemID( id )	{}
	const GCAccessSystem_t	m_SystemID;
};

//this will generate a unique system access ID that is guaranteed to be unique within a single run of the GC
GCAccessSystem_t GenerateUniqueGCAccessID();

//defines a system access object class which can be used to validate access to systems. This class can be registered with the 
//GCA_REGISTER_SYSTEM macro using the same name.
#define GCA_SYSTEM( Name ) \
	class CGCA##Name : public CGCAContextSystemRef { \
	public: \
		static const char*		GetName()	{ return #Name; } \
		static GCAccessSystem_t	GetID() 	{ static GCAccessSystem_t s_ID = GenerateUniqueGCAccessID(); return s_ID; } \
		CGCA##Name() : CGCAContextSystemRef( GetID() ) {} \
	};

//similar to the above, but does not auto assign an ID and instead uses the fixed specified ID
#define GCA_SYSTEM_ID( Name, ID ) \
	class CGCA##Name : public CGCAContextSystemRef { \
	public: \
		static const char*		GetName()	{ return #Name; } \
		static GCAccessSystem_t	GetID() 	{ return (ID); } \
		CGCA##Name() : CGCAContextSystemRef( GetID() ) {} \
	};

//called to register the specified system with the GC access system
#define GCA_REGISTER_SYSTEM( Name )			GGCAccess().RegisterSystem( CGCA##Name::GetName(), CGCA##Name::GetID() )

//called to begin a context class which derives from (has the same rights as) a parent context. The proper setup for this is:
// GCA_BEGIN_CONTEXT_PARENT( MyContext, ParentContext )
//    GCA_CONTEXT_SYSTEM( System )
// GCA_END_CONTEXT( MyContext )
#define GCA_BEGIN_CONTEXT_PARENT( ClassName, Parent ) \
	class ClassName : public Parent { \
		public: \
		ClassName() : m_nFirstSystemMarker_##ClassName( 0 ), m_nLastSystemMarker_##ClassName( 0 ) { \
			Init( #ClassName ); \
			for( const GCAccessSystem_t* pSystem = &m_nFirstSystemMarker_##ClassName + 1; pSystem != &m_nLastSystemMarker_##ClassName; pSystem++ ) \
				AddSystem( *pSystem ); \
		} \
		static ClassName& Singleton() { static ClassName s_Singleton; return s_Singleton; } \
		const GCAccessSystem_t m_nFirstSystemMarker_##ClassName;

//this is the same as the above but where you don't specify a parent context
#define GCA_BEGIN_CONTEXT( ClassName )		GCA_BEGIN_CONTEXT_PARENT( ClassName, CGCAccessContext )

//called to add a context to a system. This must come between a BEGIN/END_CONTEXT block
#define GCA_CONTEXT_SYSTEM( SystemName ) \
	const CGCA##SystemName	SystemName;

//called to add a generic system reference to a context. The reference will use the provided class and value name and the specified ID
#define GCA_CONTEXT_SYSTEM_ID( ClassName, VarName, SystemID ) \
	class ClassName : public CGCAContextSystemRef { \
	public: \
		ClassName() : CGCAContextSystemRef( SystemID ) {} \
	} VarName;

//closes out the definition of a context
#define GCA_END_CONTEXT( ClassName ) \
		const GCAccessSystem_t m_nLastSystemMarker_##ClassName; \
	};

//creates a context that is basically the same as another context but aliased. Useful if you want a derived context that doesn't have any additional systems
#define GCA_ALIAS_CONTEXT( NewName, ParentName ) \
	GCA_BEGIN_CONTEXT_PARENT( NewName, ParentName ) \
	GCA_END_CONTEXT( NewName )

//a simple way to define an empty context
#define GCA_EMPTY_CONTEXT( Context )		GCA_BEGIN_CONTEXT( Context ) GCA_END_CONTEXT( Context )

//called to obtain the ID of a system
#define GCA_GET_SYSTEM_ID( SystemName ) ( CGCA##SystemName::GetID() )

//called to validate access to a system, either with an ID or without
#define GCA_VALIDATE_ACCESS( SystemName )					GGCAccess().ValidateAccess( GCA_GET_SYSTEM_ID( SystemName ) )
#define GCA_VALIDATE_ACCESS_STEAMID( SystemName, SteamID )	GGCAccess().ValidateSteamIDAccess( GCA_GET_SYSTEM_ID( SystemName ), ( SteamID ) )

//a context, which is a collection of systems that are valid to be accessed. Each job and GC has a context to validate
//system access
class CGCAccessContext
{
public:

	CGCAccessContext();

	void Init( const char* pszName, uint32 nActions = 0, uint32 nSuppressActions = 0 );
	void AddSystem( GCAccessSystem_t nSystem );
	
	const char* GetName() const			{ return m_sName.String(); }
	uint32 GetActions() const			{ return m_nActions; }
	uint32 GetSuppressActions() const	{ return m_nSuppressActions; }
	void SetAction( EGCAccessAction eAction, bool bSet );
	void SetSuppressAction( EGCAccessAction eAction, bool bSet );
	//determines if this system has the 
	bool HasSystem( GCAccessSystem_t system ) const;
	//given a context, this will verify that it is a proper subset of the provided context
	bool IsSubsetOf( const CGCAccessContext& context ) const;
	//given another context, this will add all of its systems to this context
	void AddSystemsFrom( const CGCAccessContext& context );

private:
	//the textual name of this context
	CUtlString		m_sName;
	//which actions to enable or disable in particular based upon this context
	uint32			m_nActions;
	uint32			m_nSuppressActions;
	//which systems that the context has enabled, in sorted order for fast lookup
	CUtlSortVector< GCAccessSystem_t >	m_nSystems;
};

class CGCAccessSystem;

//the global GC access tracker which has the systems registered and handles accumulating stats, presenting reports, etc
class CGCAccess
{
public:
	CGCAccess();
	
	//-----------------------------------------
	// Setup

	//called to register a system information
	void RegisterSystem( const char* pszName, GCAccessSystem_t nID, uint32 nActions = 0, uint32 nSupressActions = 0 );
	
	//-----------------------------------------
	// Validation

	//called to verify access to a system. The return of this will be false if it is an invalid access and the return fail action is specified
	bool ValidateAccess( GCAccessSystem_t nSystem );
	//verify access to a system that also requires that the active SteamID of the current job matches the provided Steam ID, and count it as a fail
	bool ValidateSteamIDAccess( GCAccessSystem_t nSystem, CSteamID steamID ); 

	//called to suppress tracking of access for the specified access type. This should typically only be accessed via the access supress utility object
	void SuppressAccess( GCAccessSystem_t nSystem, bool bEnable );

	//-----------------------------------------
	// Report and console operations

	//when displaying a report, this will determine how stats should be filtered
	enum EDisplay
	{
		eDisplay_Referenced,
		eDisplay_Violations,
		eDisplay_IDViolations,
		eDisplay_All
	};

	//called to reset all accumulated stats for all the systems
	void ClearSystemStats();
	//called to generate a report of every access for a specific job
	void ReportJobs( const char* pszContext, EDisplay eDisplay ) const;
	//called to generate a report of each system in summary
	void ReportSystems( const char* pszContext, EDisplay eDisplay ) const;
	//dumps all the collected stats
	void FullReport( const char* pszSystemFilter, const char* pszContextFilter, const char* pszJobFilter, EDisplay eDisplay ) const;
	//dumps a dependency report for a given system. Essentially for every job that depends upon the named system, what are the other
	//systems that it also relies upon
	void DependencyReport( const char* pszSystem, EDisplay eDisplay ) const;

	//called to register a one time assert that will fire the next time the job/context/system pair is hit
	bool CatchSingleAssert( const char* pszSystem, bool bContext, const char* pszContextOrJob );
	//clears all registered single asserts that have not yet fired
	void ClearSingleAsserts();

	//if there is not a specific job in flight, this global context is what will be checked for access
	CGCAccessContext	m_GlobalContext;
	
	//global options that are set for typical system violation
	uint32		m_nActions;
	uint32		m_nSuppressActions;

private:

	//handles internally validating the system. Takes an optional parameter indicating if the steam ID check failed
	bool InternalValidateAccess( GCAccessSystem_t nSystem, CSteamID steamID, CSteamID expectedID );

	//the list of single fire asserts that we want to catch and report
	struct SingleAssert_t
	{
		bool				m_bContext;
		GCAccessSystem_t	m_System;
		CUtlString			m_sContextOrJob;
	};
	CUtlVector< SingleAssert_t* >	m_SingleAsserts;

	//systems that have their access suppressed
	struct SuppressAccess_t
	{
		GCAccessSystem_t	m_nSystem;
		uint32				m_nCount;
	};
	CUtlVector< SuppressAccess_t >	m_SuppressAccess;

	//the registered systems
	CUtlHashMapLarge< GCAccessSystem_t, CGCAccessSystem* >		m_Systems;

	//steam ID 
};
//global singleton accessor
CGCAccess& GGCAccess();

//utility class to temporarily disable access tracking for a system while within the scope of this object
class CGCAccessSupressTracking
{
public:
	CGCAccessSupressTracking( GCAccessSystem_t nSystem ) : m_nSystem( nSystem )		{ GGCAccess().SuppressAccess( m_nSystem, false ); }
	~CGCAccessSupressTracking()														{ GGCAccess().SuppressAccess( m_nSystem, true ); }
private:
	GCAccessSystem_t	m_nSystem;
};

} //namespace GCSDK

#endif
