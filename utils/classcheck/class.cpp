//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <assert.h>
#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include "classcheck_util.h"
#include "class.h"
#include "icodeprocessor.h"

CClass::CClass( const char *name )
{
	m_nVarCount = 0;
	m_nMemberCount = 0;
	m_nTDCount = 0;
	m_nPredTDCount = 0;

	strcpy( m_szName, name );
	m_szBaseClass[0]=0;
	m_pBaseClass = NULL;
	m_szTypedefBaseClass[0]=0;

	m_bDerivedFromCBaseEntity = false;
	m_bHasSaveRestoreData = false;
	m_bHasPredictionData = false;
	m_bConstructPredictableCalled = false;
	m_bHasRecvTableData = false;

	m_nClassDataSize = 0;
}

CClass::~CClass( void )
{
	int i;

	for ( i = 0; i < m_nVarCount; i++ )
	{
		delete m_Variables[ i ];
	}
	m_nVarCount = 0;

	for ( i = 0; i < m_nMemberCount; i++ )
	{
		delete m_Members[ i ];
	}
	m_nMemberCount = 0;
	for ( i = 0; i < m_nTDCount; i++ )
	{
		delete m_TDFields[ i ];
	}
	m_nTDCount = 0;
	for ( i = 0; i < m_nPredTDCount; i++ )
	{
		delete m_PredTDFields[ i ];
	}
	m_nPredTDCount = 0;
}

CTypeDescriptionField *CClass::FindTD( const char *name )
{
	for ( int i = 0; i < m_nTDCount; i++ )
	{
		if ( !strcmp( m_TDFields[ i ]->m_szVariableName, name ) )
			return m_TDFields[ i ];
	}
	return NULL;
}

CTypeDescriptionField *CClass::FindPredTD( const char *name )
{
	for ( int i = 0; i < m_nPredTDCount; i++ )
	{
		if ( !strcmp( m_PredTDFields[ i ]->m_szVariableName, name ) )
			return m_PredTDFields[ i ];
	}
	return NULL;
}

CClassVariable	*CClass::FindVar( const char *name, bool checkbaseclasses /*= false*/ )
{
	CClass *cl = this;
	while ( cl )
	{
		for ( int i = 0; i < cl->m_nVarCount; i++ )
		{
			if ( !strcmp( cl->m_Variables[ i ]->m_szName, name ) )
				return cl->m_Variables[ i ];
		}

		if ( !checkbaseclasses )
			break;

		if ( !cl->m_pBaseClass )
		{
			cl->m_pBaseClass = processor->FindClass( cl->m_szBaseClass );
		}

		cl = cl->m_pBaseClass;
		if ( !cl )
			break;
	}
	return NULL;
}

CClassMemberFunction *CClass::FindMember( const char *name )
{
	for ( int i = 0; i < m_nMemberCount; i++ )
	{
		if ( !strcmp( m_Members[ i ]->m_szName, name ) )
			return m_Members[ i ];
	}
	return NULL;
}

CTypeDescriptionField	*CClass::AddTD( const char *name, const char *type, const char *definetype, bool incomments )
{
	CTypeDescriptionField *td = FindTD( name );
	if ( !td )
	{
		td = new CTypeDescriptionField();
		strcpy( td->m_szVariableName, name );
		strcpy( td->m_szType, type );
		strcpy( td->m_szDefineType, definetype );
		td->m_bCommentedOut = incomments;

		m_TDFields[ m_nTDCount++ ] = td;
		if ( m_nTDCount >= MAX_TDFIELDS )
		{
			vprint( 0, "too many typedescription fields\n" );
			exit( 1 );
		}
	}
	return td;
}

CTypeDescriptionField	*CClass::AddPredTD( const char *name, const char *type, const char *definetype, bool incomments, bool inrecvtable )
{
	CTypeDescriptionField *td = FindPredTD( name );
	if ( !td )
	{
		td = new CTypeDescriptionField();
		strcpy( td->m_szVariableName, name );
		strcpy( td->m_szType, type );
		strcpy( td->m_szDefineType, definetype );
		td->m_bCommentedOut = incomments;
		td->m_bRepresentedInRecvTable = inrecvtable;

		m_PredTDFields[ m_nPredTDCount++ ] = td;
		if ( m_nPredTDCount >= MAX_TDFIELDS )
		{
			vprint( 0, "too many prediction typedescription fields\n" );
			exit( 1 );
		}
	}
	return td;
}

CClassVariable	*CClass::AddVar( const char *name )
{
	CClassVariable *var = FindVar( name );
	if ( !var )
	{
		var = new CClassVariable();
		strcpy( var->m_szName, name );

		m_Variables[ m_nVarCount++ ] = var;
		if ( m_nVarCount >= MAX_VARIABLES )
		{
			vprint( 0, "too many variables\n" );
			exit( 1 );
		}
	}
	return var;
}

CClassMemberFunction *CClass::AddMember( const char *name )
{
	CClassMemberFunction *member = FindMember( name );
	if ( !member )
	{
		member = new CClassMemberFunction();
		strcpy( member->m_szName, name );

		m_Members[ m_nMemberCount++ ] = member;
		if ( m_nMemberCount >= MAX_MEMBERS )
		{
			vprint( 0, "too many members\n" );
			exit( 1 );
		}
	}
	return member;
}

void CClass::SetBaseClass( const char *name )
{
	if ( !m_szBaseClass[ 0 ] )
	{
		strcpy( m_szBaseClass, name );
	}
	else if ( stricmp( m_szBaseClass, name ) )
	{
		vprint( 0, "Base class differs for %s %s vs %s\n", m_szName, m_szBaseClass, name );
	}
}

void CClass::CheckChildOfBaseEntity( const char *baseentityclass )
{
	m_bDerivedFromCBaseEntity = false;

	if ( !stricmp( m_szName, baseentityclass ) )
	{
		m_bDerivedFromCBaseEntity = true;
		return;
	}

	CClass *base = m_pBaseClass;
	while ( base )
	{
		// Early out?
		if ( base->m_bDerivedFromCBaseEntity )
		{
			m_bDerivedFromCBaseEntity = true;
			return;
		}

		// Check name
		if ( !stricmp( base->m_szName, baseentityclass ) )
		{
			m_bDerivedFromCBaseEntity = true;
			return;
		}

		// Keep going up hierarchy
		base = base->m_pBaseClass;
	}
}

static bool IsType( char *input, char *test )
{
	char *pMatch = strstr( input, test );
	if ( !pMatch )
		return false;

	size_t nLen = strlen(test);
	if ( ( pMatch[nLen] != 0 ) && (pMatch[nLen] != ' ') )
		return false;

	if ( input != pMatch && (*(pMatch-1) != ' ') )
		return false;

	return true;
}

static char const *TranslateSimpleType( CClassVariable *var )
{
	static char out[ 256 ];
	out[ 0 ] = 0;

	char *input = var->m_szType;

	// Don't know how to handle templatized things yet
	if ( strstr( input, "<" ) )
	{
		return out;
	}

	if ( IsType( input, "bool" ) )
	{
		return "FIELD_BOOLEAN";
	}
	else if ( IsType( input, "short" ) )
	{
		return "FIELD_SHORT";
	}
	else if ( IsType( input, "int" ) )
	{
		return "FIELD_INTEGER";
	}
	else if ( IsType( input, "byte" ) )
	{
		return "FIELD_CHARACTER";
	}
	else if ( IsType( input, "float" ) )
	{
		return "FIELD_FLOAT";
	}
	else if ( IsType( input, "EHANDLE" ) || IsType( input, "CHandle" ) )
	{
		return "FIELD_EHANDLE";
	}
	else if ( IsType( input, "color32" ) )
	{
		return "FIELD_COLOR32";
	}
	else if ( IsType( input, "Vector" ) || IsType( input, "QAngle" ) )
	{
		return "FIELD_VECTOR";
	}
	else if ( IsType( input, "Quaternion" ) )
	{
		return "FIELD_QUATERNION";
	}
	else if ( IsType( input, "VMatrix" ) )
	{
		return "FIELD_VMATRIX";
	}
	else if ( IsType( input, "string_t" ) )
	{
		return "FIELD_STRING";
	}
	else if ( IsType( input, "char" ) )
	{
		return "FIELD_CHARACTER";
	}

	return out;
}

void CClass::ReportTypeMismatches( CClassVariable *var, CTypeDescriptionField *td )
{
	char const *t = TranslateSimpleType( var );
	if ( !t[0] )
		return;
	
	// Special cases
	if ( td->m_bCommentedOut )
		return;

	if ( !strcmp( td->m_szType, "FIELD_TIME" ) )
	{
		if ( !strcmp( t, "FIELD_FLOAT" ) )
			return;
	}

	if ( !strcmp( td->m_szType, "FIELD_TICK" ) )
	{
		if ( !strcmp( t, "FIELD_INTEGER" ) )
			return;
	}

	if ( !strcmp( td->m_szType, "FIELD_MODELNAME" ) || !strcmp( td->m_szType, "FIELD_SOUNDNAME" ) )
	{
		if ( !strcmp( t, "FIELD_STRING" ) )
			return;
	}

	if ( !strcmp( td->m_szType, "FIELD_MODELINDEX" ) || !strcmp( td->m_szType, "FIELD_MATERIALINDEX" ) )
	{
		if ( !strcmp( t, "FIELD_INTEGER" ) )
			return;
	}

	if ( !strcmp( td->m_szType, "FIELD_POSITION_VECTOR" ) )
	{
		if ( !strcmp( t, "FIELD_VECTOR" ) )
			return;
	}

	if ( !strcmp( td->m_szType, "FIELD_VMATRIX_WORLDSPACE" ) )
	{
		if ( !strcmp( t, "FIELD_VMATRIX" ) )
			return;
	}

	if ( strcmp( t, td->m_szType ) )
	{
		vprint( 0, "class %s has an incorrect FIELD_ type for variable '%s (%s, %s)'\n",
			m_szName, var->m_szName, var->m_szType, td->m_szType );
	}
}


bool CClass::CheckForMissingTypeDescriptionFields( int& missingcount, bool createtds )
{
	bool bret = false;
	missingcount = 0;
	// Didn't specify a TYPEDESCRIPTION at all
	if ( !m_bHasSaveRestoreData )
		return bret;

	for ( int i = 0; i < m_nVarCount; i++ )
	{
		CClassVariable *var = m_Variables[ i ];

		bool isstatic = false;
		char *p = var->m_szType;
		while ( 1 )
		{
			p = CC_ParseToken( p );
			if ( strlen( com_token ) <= 0 )
				break;
			if ( !stricmp( com_token, "static" ) )
			{
				isstatic = true;
				break;
			}
		}

		// Statics aren't encoded
		if ( isstatic )
			continue;

		char *goodname = var->m_szName;

		// Skip * pointer modifier
		while ( *goodname && *goodname == '*' )
		{
			goodname++;
		}

		CTypeDescriptionField *td = FindTD( goodname );
		if ( td )
		{
			ReportTypeMismatches( var, td );
			continue;
		}

		bret = true;
		missingcount++;

		if ( !createtds )
		{
			vprint( 0, "class %s missing typedescription_t field for variable '%s %s'\n",
				m_szName, var->m_szType, var->m_szName );
			continue;
		}

		char const *t = TranslateSimpleType( var );

		vprint( 0, "//\tClass %s:\n", m_szName );
		if ( var->m_bIsArray && 
			(
				stricmp( var->m_szType, "char" ) ||
				stricmp( t, "FIELD_STRING" )
			) )
		{
			if ( *t )
			{
				vprint( 0, "\tDEFINE_ARRAY( %s, %s, %s ),\n", goodname, t, var->m_szArraySize );
			}
			else
			{
				vprint( 0, "\t// DEFINE_ARRAY( %s, %s, %s ),\n", goodname, var->m_szType, var->m_szArraySize );
			}
		}
		else
		{
			if ( *t )
			{
				vprint( 0, "\tDEFINE_FIELD( %s, %s ),\n", goodname, t );
			}
			else
			{
				vprint( 0, "\t// DEFINE_FIELD( %s, %s ),\n", goodname, var->m_szType );
			}
		}
	}

	return bret;
}

bool CClass::CheckForPredictionFieldsInRecvTableNotMarkedAsSuchCorrectly( int &missingcount )
{
	bool bret = false;
	missingcount = 0;
	// Didn't specify a TYPEDESCRIPTION at all
	if ( !m_bHasPredictionData )
		return bret;

	if ( !m_bHasRecvTableData )
		return bret;

	for ( int i = 0; i < m_nVarCount; i++ )
	{
		CClassVariable *var = m_Variables[ i ];
		bool inreceivetable = var->m_bInRecvTable;

		bool isstatic = false;
		char *p = var->m_szType;
		while ( 1 )
		{
			p = CC_ParseToken( p );
			if ( strlen( com_token ) <= 0 )
				break;
			if ( !stricmp( com_token, "static" ) )
			{
				isstatic = true;
				break;
			}
		}

		// Statics aren't encoded
		if ( isstatic )
			continue;

		char *goodname = var->m_szName;

		// Skip * pointer modifier
		while ( *goodname && *goodname == '*' )
			goodname++;

		CTypeDescriptionField *td = FindPredTD( goodname );
		// Missing variables are caught in a different routine
		td = FindPredTD( goodname );
		if ( !td )
			continue;
		
		// These are implicitly ok
		if ( !strcmp( td->m_szDefineType, "DEFINE_PRED_TYPEDESCRIPTION" ) )
		{
			CClass *cl2 = processor->FindClass( td->m_szType );
			if ( cl2 )
			{
				bret = cl2->CheckForPredictionFieldsInRecvTableNotMarkedAsSuchCorrectly( missingcount );
			}
			continue;
		}

		// Looks good (either in or out!)
		// Check for appripriate flags
		if ( inreceivetable == td->m_bRepresentedInRecvTable )
			continue;

		bret = true;
		missingcount++;

		if ( inreceivetable && !td->m_bRepresentedInRecvTable )
		{
			vprint( 0, "%s::%s:  Missing FTYPEDESC_INSENDTABLE flag in prediction typedescription\n", m_szName, var->m_szName );
		}
		else
		{
			vprint( 0, "%s::%s:  Field marked as FTYPEDESC_INSENDTABLE in prediction typedescription missing from RecvTable\n", m_szName, var->m_szName );
		}
	}

	return bret;
}

bool CClass::CheckForMissingPredictionFields( int& missingcount, bool createtds )
{
	bool bret = false;
	missingcount = 0;
	// Didn't specify a TYPEDESCRIPTION at all
	if ( !m_bHasPredictionData )
		return bret;

	for ( int i = 0; i < m_nVarCount; i++ )
	{
		CClassVariable *var = m_Variables[ i ];

		// private and protected variables can't be referenced in data tables right now
		//if ( var->m_Type != CClassVariable::TPUBLIC )
		//	continue;

		bool isstatic = false;
		char *p = var->m_szType;
		while ( 1 )
		{
			p = CC_ParseToken( p );
			if ( strlen( com_token ) <= 0 )
				break;
			if ( !stricmp( com_token, "static" ) )
			{
				isstatic = true;
				break;
			}
		}

		// Statics aren't encoded
		if ( !isstatic )
		{
			char *goodname = var->m_szName;

			// Skip * pointer modifier
			while ( *goodname && *goodname == '*' )
				goodname++;

			CTypeDescriptionField *td = FindPredTD( goodname );
			td = FindPredTD( goodname );
			if ( !td )
			{
				bret = true;
				missingcount++;

				if ( !createtds )
				{
					vprint( 0, "class %s missing prediction typedescription_t field for variable '%s %s'\n",
						m_szName, var->m_szType, var->m_szName );
				}
				else
				{
					char const *t = TranslateSimpleType( var );

					vprint( 0, "//\tClass %s:\n", m_szName );
					if ( var->m_bIsArray && 
						(
							stricmp( var->m_szType, "char" ) ||
							stricmp( t, "FIELD_STRING" )
						) )
					{
						if ( *t )
						{
							vprint( 0, "\tDEFINE_ARRAY( %s, %s, %s ),\n", goodname, t, var->m_szArraySize );
						}
						else
						{
							vprint( 0, "\t// DEFINE_ARRAY( %s, %s, %s ),\n", goodname, var->m_szType, var->m_szArraySize );
						}
					}
					else
					{
						if ( *t )
						{
							vprint( 0, "\tDEFINE_FIELD( %s, %s ),\n", goodname, t );
						}
						else
						{
							vprint( 0, "\t// DEFINE_FIELD( %s, %s ),\n", goodname, var->m_szType );
						}
					}
				}
			}
		}
	}

	return bret;
}

void AppendType( const char *token, char *type )
{
	strcat( type, token );
	strcat( type, " " );
}

class MissingType
{
public:
	CClass *owning_class;
	CClassVariable *var;
};

#include "utldict.h"
CUtlDict< MissingType, unsigned short > missing_types;

bool IsMissingType( CClass *cl, CClassVariable *var )
{
	unsigned short lookup;

	lookup = missing_types.Find( var->m_szType );
	if ( lookup != missing_types.InvalidIndex() )
		return true;

	MissingType t;
	t.owning_class = cl;
	t.var = var;
	missing_types.Insert( var->m_szType, t );
	return true;
}

void ReportMissingTypes( void )
{
	int c = missing_types.Count();
	for ( int i= 0; i < c; i++ )
	{
		MissingType *t = &missing_types[ i ];
		if ( !t )
			continue;
		
		if ( !t->owning_class )
			continue;

		if ( !t->owning_class->m_bDerivedFromCBaseEntity )
			continue;

		if ( !t->var )
			continue;

		vprint( 0, "Can't compute size of %s %s %s\n", 
			t->owning_class->m_szName, t->var->m_szType, t->var->m_szName );
	}
}

void ClearMissingTypes()
{
	missing_types.Purge();
}

static int GetTypeSize( CClass *cl, CClassVariable *var )
{
	int out = 0;

	char *input = var->m_szType;

	// Don't know how to handle templatized things yet
	if ( strstr( input, "<" ) )
	{
		IsMissingType( cl, var );
		return out;
	}

	if ( strstr( var->m_szName, "*" ) )
	{
		return sizeof( void * );
	}

	if ( strstr( input, "bool" ) )
	{
		return sizeof( bool );
	}
	else if ( strstr( input, "int64" ) )
	{
		return sizeof( __int64 );
	}
	else if ( strstr( input, "short" ) )
	{
		return sizeof( short );
	}
	else if ( strstr( input, "unsigned short" ) )
	{
		return sizeof( unsigned short );
	}
	else if ( strstr( input, "int" ) )
	{
		return sizeof( int );
	}
	else if ( strstr( input, "float" ) )
	{
		return sizeof( float );
	}
	else if ( strstr( input, "vec_t" ) )
	{
		return sizeof( float );
	}
	else if ( strstr( input, "Vector" ) || strstr( input, "QAngle" ) )
	{
		return 3 * sizeof( float );
	}
	else if ( strstr( input, "vec3_t" ) )
	{
		return 3 * sizeof( float );
	}	
	else if ( strstr( input, "char" ) )
	{
		return sizeof( char );
	}
	else if ( strstr( input, "unsigned char" ) )
	{
		return sizeof( unsigned char );
	}
	else if ( strstr( input, "BYTE" ) )
	{
		return sizeof( char );
	}
	else if ( strstr( input, "byte" ) )
	{
		return sizeof( char );
	}
	else if ( !strcmp( input, "unsigned" ) )
	{
		return sizeof(unsigned int);
	}
	else if ( strstr( input, "long" ) )
	{
		return sizeof( int );
	}
	else if ( strstr( input, "color32" ) )
	{
		return sizeof( int );
	}
	// It's a pointer
	else if ( strstr( input, "*" ) )
	{
		return sizeof( void * );
	}
	// Static data doesn't count
	else if ( strstr( input, "static" ) )
	{
		return 0;
	}	
	
	
	// Okay, see if it's a classname
	CClass *base = processor->FindClass( input );
	if ( base )
	{
		return base->m_nClassDataSize;
	}

	IsMissingType( cl, var );

	return out;
}


void CClass::AddVariable( int protection, char *type, char *name, bool array, char *arraysize )
{
	CClassVariable *var = AddVar( name );
	if ( !var )
		return;

	strcpy( var->m_szType, type );
	var->m_Type = (CClassVariable::VARTYPE)protection;
	var->m_TypeSize = GetTypeSize( this, var );

	m_nClassDataSize += var->m_TypeSize;

	if ( array )
	{
		var->m_bIsArray = true;
		strcpy( var->m_szArraySize, arraysize );
	}
	else
	{
		var->m_bIsArray = false;
	}
}


//-----------------------------------------------------------------------------
// Parses information to determine the base class of this class
//-----------------------------------------------------------------------------
bool CClass::ParseBaseClass( char *&input )
{
	if ( !strcmp( com_token, "DECLARE_CLASS" ) 
			|| !strcmp( com_token, "DECLARE_CLASS_GAMEROOT" ) 
			|| !strcmp( com_token, "DECLARE_CLASS_NOFRIEND" ) )
	{
		input = CC_ParseToken( input );
		Assert( !strcmp( com_token, "(") );
		input = CC_ParseToken( input );

		do
		{
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ",") );

		m_szTypedefBaseClass[0] = 0;
		input = CC_ParseToken( input );
		do
		{
			strcat( m_szTypedefBaseClass, com_token );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ")") );
		return true;
	}
	else if ( !strcmp( com_token, "DECLARE_CLASS_NOBASE" ) )
	{
		input = CC_ParseToken( input );
		Assert( !strcmp( com_token, "(") );
		input = CC_DiscardUntilMatchingCharIncludingNesting( input, "()" );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Parses networkvars
//-----------------------------------------------------------------------------
bool CClass::ParseNetworkVar( char *&input, int protection )
{
	MemberVarParse_t var;

	if ( !strcmp( com_token, "CNetworkVar" ) || 
		!strcmp( com_token, "CNetworkVarForDerived" ) || 
		!strcmp( com_token, "CNetworkVarEmbedded" ) )
	{
		input = CC_ParseToken( input );
		Assert( !strcmp( com_token, "(") );

		input = CC_ParseToken( input );
		do
		{
			strcat( var.m_pType, com_token );
			strcat( var.m_pType, " " );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ",") );

		input = CC_ParseToken( input );
		do
		{
			strcat( var.m_pName, com_token );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ")") );

		AddVariable( protection, var.m_pType, var.m_pName, false );
		return true;
	}

	if ( !strcmp( com_token, "CNetworkHandle" ) || !strcmp( com_token, "CNetworkHandleForDerived" ) )
	{
		input = CC_ParseToken( input );
		Assert( !strcmp( com_token, "(") );

		input = CC_ParseToken( input );
		strcpy( var.m_pType, "CHandle<" );
		do
		{
			strcat( var.m_pType, com_token );
			strcat( var.m_pType, " " );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ",") );
		strcat( var.m_pType, ">" );

		input = CC_ParseToken( input );
		do
		{
			strcat( var.m_pName, com_token );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ")") );

		AddVariable( protection, "EHANDLE", var.m_pName, false );
		return true;
	}

	if ( !strcmp( com_token, "CNetworkVector" ) || 
		!strcmp( com_token, "CNetworkVectorForDerived" ) || 
		!strcmp( com_token, "CNetworkQAngle" ) )
	{
		input = CC_ParseToken( input );
		Assert( !strcmp( com_token, "(") );

		input = CC_ParseToken( input );
		do
		{
			strcat( var.m_pName, com_token );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ")") );

		AddVariable( protection, "Vector", var.m_pName, false );
		return true;
	}

	if ( !strcmp( com_token, "CNetworkColor32" ) )
	{
		input = CC_ParseToken( input );
		Assert( !strcmp( com_token, "(") );

		input = CC_ParseToken( input );
		do
		{
			strcat( var.m_pName, com_token );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ")") );

		AddVariable( protection, "color32", var.m_pName, false );
		return true;
	}

	if ( !strcmp( com_token, "CNetworkString" ) )
	{
		input = CC_ParseToken( input );
		Assert( !strcmp( com_token, "(") );

		input = CC_ParseToken( input );
		do
		{
			strcat( var.m_pName, com_token );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ",") );

		input = CC_ParseToken( input );
		do
		{
			strcat( var.m_pArraySize, com_token );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ")") );

		AddVariable( protection, "char *", var.m_pName, true, var.m_pArraySize );
		return true;
	}

	if ( !strcmp( com_token, "CNetworkArray" ) || !strcmp( com_token, "CNetworkArrayForDerived" ) )
	{
		input = CC_ParseToken( input );
		Assert( !strcmp( com_token, "(") );

		input = CC_ParseToken( input );
		do
		{
			strcat( var.m_pType, com_token );
			strcat( var.m_pType, " " );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ",") );

		input = CC_ParseToken( input );
		do
		{
			strcat( var.m_pName, com_token );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ",") );

		input = CC_ParseToken( input );
		do
		{
			strcat( var.m_pArraySize, com_token );
			input = CC_ParseToken( input );
		} while( strcmp( com_token, ")") );

		AddVariable( protection, var.m_pType, var.m_pName, true, var.m_pArraySize );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Parses a class member definition
//-----------------------------------------------------------------------------
bool CClass::ParseClassMember( char *&input, int protection )
{
	MemberVarParse_t var;

	bool	isfunction = false;
	bool	wascomma = false;
	bool	skipvar = false;

	if ( ParseNetworkVar( input, protection ) )
		return true;

	strcpy( var.m_pName, com_token );
	if ( !stricmp( var.m_pName, "SHARED_CLASSNAME" ) )
	{
		input = CC_ParseToken( input );
		if ( !stricmp( com_token, "(" ) )
		{
			char inside[ 256 ];
			char *saveinput = input;
			input = CC_DiscardUntilMatchingCharIncludingNesting( input, "()" );

			int len = input - saveinput;
			strncpy( inside, saveinput, len );
			inside[ len ] =0;

			strcat( var.m_pName, "(" );
			strcat( var.m_pName, inside );
		}
	}

	do
	{
		input = CC_ParseToken( input );
		if ( strlen( com_token ) <= 0 )
			break;

		if ( !stricmp( com_token, "(" ) )
		{
			char *saveinput = input;

			isfunction = true;

			input = CC_DiscardUntilMatchingCharIncludingNesting( input, "()" );

			// see if the function is being declared in line here
			input = CC_ParseToken( input );

			if ( !stricmp( com_token, "const" ) )
			{
				// Swallow const if we see it
				input = CC_ParseToken( input );
			}

			if ( !stricmp( com_token, "{" ) )
			{
				input = CC_DiscardUntilMatchingCharIncludingNesting( input, "{}" );
			}
			// pure virtual function?
			else if ( !stricmp( com_token, "=" ) )
			{
				char ch;
				input = CC_RawParseChar( input, ";", &ch );
			}
			// this was a pointer to a base function
			else if ( !stricmp( com_token, "(" ) )
			{
				char *end = input - 2;
				input = saveinput;
				
				char pfn[ 256 ];
				int len = end - saveinput;
				strncpy( pfn, input, len );
				pfn[ len ] = 0;

				do
				{ 
					input = CC_ParseToken( input );
					if ( strlen( com_token ) <= 0 )
						break;

					if ( com_token[0] == '*' )
					{
						break;
					}
				} while ( 1 );

				if ( com_token[0] == '*' )
				{
					// com_token is the variable name
					sprintf( var.m_pType, "%s (%s)", var.m_pName, pfn );
					strcpy( var.m_pName, com_token );
					input = end + 1;
				}

				if ( *input == '(' )
					input++;
				input = CC_DiscardUntilMatchingCharIncludingNesting( input, "()" );

				isfunction = false;
			}

			break;
		}
		else if ( !stricmp( com_token, "[" ) )
		{
			// It's an array
			var.m_bArray = true;
			char ch;
			char *oldinput = input;
			do
			{
				input = CC_RawParseChar( input, "]", &ch );
				if ( *input && ( *input == '[' ) )
				{
					input++;
					continue;
				}

				break;
			} while ( 1 );
			int len = input-oldinput - 1;
			if ( len > 0 )
			{
				strncpy( var.m_pArraySize, oldinput, len );
			}
			var.m_pArraySize[ len ] = 0;
			break;
		}
		else if ( !stricmp( com_token, ";" ) )
		{
			break;
		}
		else if ( !stricmp( com_token, ":" ) && !isfunction )
		{
			// Eliminate the length specification
			input = CC_ParseToken( input );
			continue;
		}
		else if ( !stricmp( com_token, "," ) )
		{
			wascomma = true;
			break;
		}
		// It's a templatized var
		else if (( com_token[ strlen( com_token ) - 1 ] == '<' ) && strcmp(var.m_pName, "operator") )
		{
			do
			{
				AppendType( var.m_pName, var.m_pType );
				strcpy( var.m_pName, com_token );

				input = CC_ParseToken( input );
				if ( strlen( com_token ) <= 0 )
					break;
			}
			while ( strcmp( com_token, ">" ) );

			AppendType( var.m_pName, var.m_pType );
			strcpy( var.m_pName, com_token );
		}
		else
		{
			if ( !stricmp( var.m_pName, "typedef" ) ||
				 !stricmp( var.m_pName, "enum" ) ||
				 !stricmp( var.m_pName, "friend" ) )
			{
				skipvar = true;
			}
			AppendType( var.m_pName, var.m_pType );
			strcpy( var.m_pName, com_token );
			continue;
		}

	} while ( 1 );

	if ( strlen( var.m_pType ) >= 1 )
	{
		var.m_pType[ strlen( var.m_pType ) - 1 ] = 0;
	}

	if ( var.m_pType[0]==0 && 
		( !strcmp( var.m_pName, "CUSTOM_SCHEDULES" ) ||
		  !strcmp( var.m_pName, "DEFINE_CUSTOM_SCHEDULE_PROVIDER" ) ||
		  !strcmp( var.m_pName, "DEFINE_CUSTOM_AI" ) ||
		  !strcmp( var.m_pName, "DECLARE_DATADESC" ) ||
		  !strcmp( var.m_pName, "DECLARE_EMBEDDED_DATADESC" ) ||
		  !strcmp( var.m_pName, "DECLARE_SERVERCLASS" ) ||
		  !strcmp( var.m_pName, "DECLARE_CLIENTCLASS" ) ||
		  !strcmp( var.m_pName, "DECLARE_ENTITY_PANEL" ) ||
		  !strcmp( var.m_pName, "DECLARE_MINIMAP_PANEL" ) ||
		  !strcmp( var.m_pName, "MANUALMODE_GETSET_PROP" ) ) )
	{
		return true;
	}

	if ( var.m_pType[0]==0 && 
		( !strcmp( var.m_pName, "DECLARE_PREDICTABLE" ) ||
		 !strcmp( var.m_pName, "DECLARE_EMBEDDED_PREDDESC" ) ) )
	{
		m_bHasPredictionData = true;
		return true;
	}

	/*
	if ( var.m_pName[0] == '*' )
	{
		strcat( type, " *" );

		char newname[ 256 ];
		strcpy( newname, &var.m_pName[1] );
		strcpy( var.m_pName, newname );
	}
	*/

	if ( isfunction )
	{
		CClassMemberFunction *member = AddMember( var.m_pName );
		if ( member )
		{
			strcpy( member->m_szType, var.m_pType );
			member->m_Type = (CClassMemberFunction::MEMBERTYPE)protection;
		}
	}
	else
	{
		// It's a variable
		do
		{
			if ( !skipvar )
			{
				AddVariable( protection, var.m_pType, var.m_pName, var.m_bArray, var.m_pArraySize );
			}
			else if ( !stricmp( var.m_pName, "BaseClass" ) )
			{
				if ( !m_szTypedefBaseClass[0] )
				{
					char *p = var.m_pType;
					p = CC_ParseToken( p );
					p = CC_ParseToken( p );
					strcpy( m_szTypedefBaseClass, com_token );
				}
			}

			if ( !wascomma )
				break;

			input = CC_ParseToken( input );
			if ( strlen( com_token ) <= 0 )
				break;

			// Remove length specifiers
			if ( !stricmp( com_token, ":" ) )
			{
				input = CC_ParseToken( input );
				input = CC_ParseToken( input );
			}

			if ( !stricmp( com_token, "," ) )
			{
				input = CC_ParseToken( input );
			}

			if ( !stricmp( com_token, ";" ) )
				break;
			
			strcpy( var.m_pName, com_token );

		} while ( 1 );
	}

	return true;
}


//-----------------------------------------------------------------------------
// Parses a nested class definition
//-----------------------------------------------------------------------------
bool CClass::ParseNestedClass( char *&input )
{
	if ( stricmp( com_token, "struct" ) && stricmp( com_token, "class" ) )
		return false;
	
	input = CC_ParseToken( input );
	if ( strlen( com_token ) > 0 )
	{
		//vprint( depth, "class %s\n", com_token );
		char decorated[ 256 ];
		sprintf( decorated, "%s::%s", m_szName, com_token );

		CClass *cl = processor->AddClass( decorated );

		// Now see if there's a base class
		input = CC_ParseToken( input );
		if ( !stricmp( com_token, ":" ) )
		{
			// Parse out public and then classname an
			input = CC_ParseToken( input );
			if ( !stricmp( com_token, "public" ) )
			{
				input = CC_ParseToken( input );
				if ( strlen( com_token ) > 0 )
				{
					cl->SetBaseClass( com_token );

					do
					{
						input = CC_ParseToken( input );
					} while ( strlen( com_token ) && stricmp( com_token, "{" ) );

					if ( !stricmp( com_token, "{" ) )
					{
						input = cl->ParseClassDeclaration( input );
					}
				}
			}
		}
		else if ( !stricmp( com_token, "{" ) )
		{
			input = cl->ParseClassDeclaration( input );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Parses public/protected/private
//-----------------------------------------------------------------------------
bool CClass::ParseProtection( char *&input, int &protection )
{
	if ( !stricmp( com_token, "public" ) )
	{
		protection = 0;
		input = CC_ParseToken( input );
		Assert( !stricmp( com_token, ":" ) );
		return true;
	}
	else if ( !stricmp( com_token, "protected" ) )
	{
		protection = 1;
		input = CC_ParseToken( input );
		Assert( !stricmp( com_token, ":" ) );
		return true;
	}
	else if ( !stricmp( com_token, "private" ) )
	{
		protection = 2;
		input = CC_ParseToken( input );
		Assert( !stricmp( com_token, ":" ) );
		return true;
	}

	return false;
}


// parse until } found
// public:, private:, protected: set protection mode, private is initial default
// if token is not one of those, then parse and concatenate all tokens up to the first
//   ; or (
//

char *CClass::ParseClassDeclaration( char *input )
{
	int nestcount = 1;

	// public = 0, protected = 1, private = 2;
	int protection = 2;

	do
	{
		input = CC_ParseToken( input );
		if ( strlen( com_token ) <= 0 )
			break;

		if ( com_token[ 1 ] == 0 )
		{
			if ( com_token[ 0 ] == '{' )
			{
				nestcount++;
			}
			else if ( com_token[ 0 ] == '}' )
			{
				nestcount--;
			}
		}

		if ( ParseProtection( input, protection ) )
			continue;

		if ( !stricmp( com_token, ";" ) )
			continue;

		if ( com_token[0] == '#' )
		{
			// swallow rest of line
			input = CC_ParseUntilEndOfLine( input );
			continue;
		}

		if ( ParseNestedClass( input ) )
			continue;

		if ( nestcount == 1 )
		{
			// See if we found a line that describes the base class
			if ( ParseBaseClass( input ) )
				continue;

			ParseClassMember( input, protection );
		}

	} while ( nestcount != 0 && ( strlen( com_token ) >= 0 ) );

	return input;
}

static bool ShouldHungarianCheck( char const *name )
{
	if ( !Q_strncmp( name, "m_", 2 ) ||
		 !Q_strncmp( name, "g_", 2 ) ||
		 !Q_strncmp( name, "s_", 2 ) )
	{
		return true;
	}

	return false;
}

enum Required
{
	NEVER = 0,
	ALWAYS
};

struct Impermissible
{
	char const *prefix;
	char const *mustinclude;
	int			required;  // if true, then must match to be permitted
};

static Impermissible g_Permissibles[] =
{
	{  "fl",		"float",		ALWAYS },
	{  "b",			"bool",			ALWAYS },
	{  "n",			"int",			ALWAYS },
	{  "isz",		"string_t",		ALWAYS },
	{  "i",			"float",		NEVER },
	{  "i",			"bool",			NEVER },
	{  "i",			"short",		NEVER },
	{  "i",			"long",			NEVER },
	{  "ui",		"int",			ALWAYS },
	{  "sz",		"char",			ALWAYS },
	{  "ch",		"char",			ALWAYS },
	{  "uch",		"float",		NEVER },
	{  "uch",		"int",			NEVER },
	{  "uch",		"short",		NEVER },
	{  "uch",		"long",			NEVER },
	{  "s",			"short",		ALWAYS },
	{  "us",		"short",		ALWAYS },
	{  "l",			"long",			ALWAYS },
	{  "ul",		"long",			ALWAYS },
//	{  "f",			"int",			NEVER },
//	{  "f",			"short",		NEVER },
//	{  "f",			"int",			NEVER },
	{  "a",			"UtlVector",	ALWAYS },
	{  "h",			"handle",		ALWAYS },
	{  "p",			"*",			ALWAYS },
};

void CClass::CheckForHungarianErrors( int& warnings )
{
	int testcount = sizeof( g_Permissibles ) / sizeof( g_Permissibles[ 0 ] );

	for ( int i = 0; i < m_nVarCount; i++ )
	{
		CClassVariable *var = m_Variables[ i ];

		// Only check m_, s_, and g_ variables for now
		if ( !ShouldHungarianCheck( var->m_szName ) )
		{
			continue;
		}

		bool isstatic = false;
		char *p = var->m_szType;
		while ( 1 )
		{
			p = CC_ParseToken( p );
			if ( strlen( com_token ) <= 0 )
				break;
			if ( !stricmp( com_token, "static" ) )
			{
				isstatic = true;
				break;
			}
		}

		// Check for errors
		for ( int j = 0; j < testcount; ++j )
		{
			Impermissible *tst = &g_Permissibles[ j ];

			bool match = !Q_strncmp( var->m_szName + 2, tst->prefix, Q_strlen( tst->prefix ) ) ? true : false;
			if ( !match )
				continue;

			// The first character after the prefix must be upper case or we skip...
			int nextchar = 2 + Q_strlen( tst->prefix );

			if ( !isupper( var->m_szName[ nextchar ] ) )
				continue;

			bool typeFound = Q_stristr( var->m_szType, tst->mustinclude ) ? true : false;

			switch ( tst->required )
			{
			default:
			case ALWAYS:
				{
					if ( !typeFound )
					{
						vprint( 1, "%s might have wrong type %s\n", var->m_szName, var->m_szType );
						++warnings;
					}
					else
					{
						return;
					}
				}
				break;
			case NEVER:
				{
					if ( typeFound )
					{
						vprint( 1, "%s might have wrong type %s\n", var->m_szName, var->m_szType );
						++warnings;
					}
					else
					{
						return;
					}
				}
				break;
			}
		}


		if ( !Q_strncmp( var->m_szName, "m_f", 3 ) &&
			 Q_strncmp( var->m_szName, "m_fl", 4 ) && isupper( var->m_szName[3] ) )
		{
			// If it's a "flag" and not a "float" type, it better be a bool or an int
			if ( !Q_stristr( var->m_szType, "bool" ) &&
				 !Q_strstr( var->m_szType, "int" ) )
			{
				vprint( 1, "%s might have wrong type %s\n", var->m_szName, var->m_szType );
				++warnings;
				return;
			}
		}
	}

}
