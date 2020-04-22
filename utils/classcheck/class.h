//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#if !defined( CLASS_H )
#define CLASS_H
#ifdef _WIN32
#pragma once
#endif


class CTypeDescriptionField
{
public:
	CTypeDescriptionField()
	{
		m_szVariableName[ 0 ] = 0;
		m_szType[ 0 ] = 0;
		m_szDefineType[ 0 ] = 0;
		m_bCommentedOut = false;
		m_bRepresentedInRecvTable = false;
	}

	char			m_szVariableName[ 128 ];
	char			m_szType[ 128 ];
	char			m_szDefineType[ 128 ];
	bool			m_bCommentedOut;
	bool			m_bRepresentedInRecvTable;
};

class CClassVariable
{
public:
	CClassVariable()
	{
		m_szName[ 0 ] = 0;
		m_szType[ 0 ] = 0;

		m_Type = TPUBLIC;
		
		m_bKnownType = false;
		m_nTypeSize = 0;

		m_bIsArray = false;
		m_szArraySize[ 0 ] = 0;

		m_bInRecvTable = false;

		m_TypeSize = 0;
	}

	typedef enum 
	{
		TPUBLIC = 0,
		TPROTECTED,
		TPRIVATE
	} VARTYPE;

	char			m_szName[ 128 ];
	char			m_szType[ 128 ];

	VARTYPE			m_Type;
	
	bool			m_bKnownType;
	int				m_nTypeSize;

	bool			m_bIsArray;
	char			m_szArraySize[ 128 ];

	bool			m_bInRecvTable;

	int				m_TypeSize;
};

class CClassMemberFunction
{
public:
	typedef enum 
	{
		TPUBLIC = 0,
		TPROTECTED,
		TPRIVATE
	} MEMBERTYPE;

	char			m_szName[ 128 ];
	// Return type
	char			m_szType[ 128 ];

	MEMBERTYPE		m_Type;

};

class CClassTypedef
{
public:
	char			m_szTypeName[ 128 ];
	char			m_szAlias[ 128 ];

//	bool			m_bIsTypedefForBaseClass;
};

class CClass
{
public:
	enum
	{
		MAX_VARIABLES = 1024,
		MAX_MEMBERS   = 1024,
		MAX_TDFIELDS  = 1024,
	};

					CClass( const char *name );
					~CClass( void );
	
	char			*ParseClassDeclaration( char *input );


	void			SetBaseClass( const char *name );
	void			CheckChildOfBaseEntity( const char *baseentityclass );
	bool			CheckForMissingTypeDescriptionFields( int& missingcount, bool createtds = false );
	bool			CheckForMissingPredictionFields( int& missingcount, bool createtds = false );
	bool			CheckForPredictionFieldsInRecvTableNotMarkedAsSuchCorrectly( int& missingcount );
	void			AddVariable( int protection, char *type, char *name, bool array, char *arraysize = 0 );

	// Parsing helper methods
	bool			ParseProtection( char *&input, int &protection );
	bool			ParseNestedClass( char *&input );
	bool			ParseBaseClass( char *&input );
	bool			ParseClassMember( char *&input, int protection );
	bool			ParseNetworkVar( char *&input, int protection );
	void			ReportTypeMismatches( CClassVariable *var, CTypeDescriptionField *td );

	void			CheckForHungarianErrors( int& warnings );

	char			m_szName[ 128 ];
	char			m_szBaseClass[ 128 ];
	char			m_szTypedefBaseClass[ 128 ];

	CClassVariable	*FindVar( const char *name, bool checkbaseclasses = false );
	CClassVariable	*AddVar( const char *name );
	int				m_nVarCount;
	CClassVariable			*m_Variables[ MAX_VARIABLES ];

	CClassMemberFunction *FindMember( const char *name );
	CClassMemberFunction *AddMember( const char *name );
	int				m_nMemberCount;
	CClassMemberFunction	*m_Members[ MAX_MEMBERS ];

	CTypeDescriptionField	*FindTD( const char *name );
	CTypeDescriptionField   *AddTD(  const char *name, const char *type, const char *definetype, bool incomments );
	int						m_nTDCount;
	CTypeDescriptionField	*m_TDFields[ MAX_TDFIELDS ];

	CTypeDescriptionField	*FindPredTD( const char *name );
	CTypeDescriptionField   *AddPredTD(  const char *name, const char *type, const char *definetype, bool incomments, bool inrecvtable );
	int						m_nPredTDCount;
	CTypeDescriptionField	*m_PredTDFields[ MAX_TDFIELDS ];


	CClass			*m_pBaseClass;

	CClass			*m_pNext;

	bool			m_bDerivedFromCBaseEntity;
	bool			m_bHasSaveRestoreData;
	bool			m_bHasPredictionData;
	bool			m_bHasRecvTableData;
	bool			m_bConstructPredictableCalled;

	int				m_nClassDataSize;

private:
	struct MemberVarParse_t
	{
		char m_pType[256];
		char m_pTypeModifier[256];
		char m_pName[256];
		char m_pArraySize[ 128 ];
		bool m_bArray;

		MemberVarParse_t() { Reset(); }

		void Reset()
		{
			m_pType[0] = 0;
			m_pTypeModifier[0] = 0;
			m_pName[0] = 0;
			m_pArraySize[0] = 0;
			m_bArray = false;
		}
	};
};

#endif // CLASS_H