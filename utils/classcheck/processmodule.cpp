//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include <assert.h>
#include <time.h>
#include "stdafx.h"
#include <stdio.h>
#include <windows.h>
#include "classcheck_util.h"
#include "codeprocessor.h"

/*
================
UTIL_FloatTime
================
*/
double UTIL_FloatTime (void)
{
// more precise, less portable
	clock_t current;
	static clock_t base;
	static bool first = true;

	current = clock();

	if ( first )
	{
		first = false;

		base = current;
	}
	
	return (double)(current - base)/(double)CLOCKS_PER_SEC;
}

CClass *CCodeProcessor::FindClass( const char *name ) const
{
	CClass *cl = m_pClassList;
	while ( cl )
	{
		if ( !stricmp( cl->m_szName, name ) )
			return cl;

		cl = cl->m_pNext;
	}
	return NULL;
}

void ClearMissingTypes();

void CCodeProcessor::Clear( void )
{
	ClearMissingTypes();

	CClass *cl = m_pClassList, *next;
	while ( cl )
	{
		next = cl->m_pNext;
		delete cl;
		cl = next;
	}
	m_pClassList = NULL;
}



int CCodeProcessor::Count( void ) const
{
	int c = 0;
	CClass *cl = m_pClassList;
	while ( cl )
	{
		c++;
		cl = cl->m_pNext;
	}
	return c;
}

int FnClassSortCompare( const void *elem1, const void *elem2 )
{
	CClass *c1 = *(CClass **)elem1;
	CClass *c2 = *(CClass **)elem2;

	return ( stricmp( c1->m_szName, c2->m_szName ) );
}

void CCodeProcessor::SortClassList( void )
{
	int n = Count();
	if ( n <= 1 )
		return;

	CClass **ppList = new CClass *[ n ];
	if ( ppList )
	{
		CClass *cl;
		int i;
		for ( i = 0, cl = m_pClassList; i < n; i++, cl = cl->m_pNext )
		{
			ppList[ i ] = cl;
		}

		qsort( ppList, n, sizeof( CClass * ), FnClassSortCompare );

		for ( i = 0; i < n - 1; i++ )
		{
			ppList[ i ]->m_pNext = ppList[ i + 1 ];
		}
		ppList[ i ]->m_pNext = NULL;
		m_pClassList = ppList[ 0 ];
	}
	delete[] ppList;
}

void CCodeProcessor::ResolveBaseClasses( const char *baseentityclass )
{
	SortClassList();

	CClass *cl = m_pClassList;
	while ( cl )
	{
		if ( cl->m_szBaseClass[0] )
		{
			cl->m_pBaseClass = FindClass( cl->m_szBaseClass );
			if ( !cl->m_pBaseClass )
			{
				//vprint( 0, "couldn't find base class %s for %s\n", cl->m_szBaseClass, cl->m_szName );
			}
		}
		
		cl = cl->m_pNext;
	}

	cl = m_pClassList;
	while ( cl )
	{
		cl->CheckChildOfBaseEntity( baseentityclass );
		cl = cl->m_pNext;
	}
}

void CCodeProcessor::PrintMissingTDFields( void ) const
{
	int classcount;
	int fieldcount;
	int c;

	CClass *cl;

	if ( GetPrintTDs() )
	{
		classcount = 0;
		fieldcount = 0;
		cl = m_pClassList;
		while ( cl )
		{
			if ( cl->m_bDerivedFromCBaseEntity || cl->m_bHasSaveRestoreData )
			{
				if ( cl->CheckForMissingTypeDescriptionFields( c ) )
				{
					classcount++;
					fieldcount += c;
				}
			}
			cl = cl->m_pNext;
		}

		if ( fieldcount )
		{
			vprint( 0, "\nSummary:  %i fields missing from %i classes\n", fieldcount, classcount );
		}
		else
		{
			if ( !classcount )
			{
				vprint( 0, "\nSummary:  no saverestore info present\n");
			}
			else
			{
				vprint( 0, "\nSummary:  no errors for %i classes\n", classcount );
			}
		}

		vprint( 0, "\n" );
	}

	if ( GetPrintPredTDs() )
	{
		//Now check prediction stuff
		classcount = 0;
		fieldcount = 0;
		cl = m_pClassList;
		while ( cl )
		{
			if ( cl->m_bDerivedFromCBaseEntity || cl->m_bHasPredictionData )
			{
				if ( cl->CheckForMissingPredictionFields( c, false ) )
				{
					classcount++;
					fieldcount += c;
				}
			}
			cl = cl->m_pNext;
		}

		if ( fieldcount )
		{
			vprint( 0, "\nSummary:  %i prediction fields missing from %i classes\n", fieldcount, classcount );
		}
		else
		{
			if ( !classcount )
			{
				vprint( 0, "\nSummary:  no prediction info present\n");
			}
			else
			{
				vprint( 0, "\nSummary:  no errors for %i predictable classes\n", classcount );
			}
		}

		vprint( 0, "\n" );
	}

	if ( GetPrintCreateMissingTDs() )
	{
		//Now check prediction stuff
		classcount = 0;
		fieldcount = 0;
		cl = m_pClassList;
		while ( cl )
		{
			if ( cl->m_bDerivedFromCBaseEntity || cl->m_bHasSaveRestoreData )
			{
				if ( cl->CheckForMissingTypeDescriptionFields( c, true ) )
				{
					classcount++;
					fieldcount += c;
				}
			}
			cl = cl->m_pNext;
		}

		if ( fieldcount )
		{
			vprint( 0, "\nSummary:  %i saverestore fields missing from %i classes\n", fieldcount, classcount );
		}
		else
		{
			if ( !classcount )
			{
				vprint( 0, "\nSummary:  no saverestore info present\n");
			}
			else
			{
				vprint( 0, "\nSummary:  no errors for %i classes\n", classcount );
			}
		}

		vprint( 0, "\n" );
	}

	if ( GetPrintCreateMissingPredTDs() )
	{
		//Now check prediction stuff
		classcount = 0;
		fieldcount = 0;
		cl = m_pClassList;
		while ( cl )
		{
			if ( cl->m_bDerivedFromCBaseEntity || cl->m_bHasPredictionData )
			{
				if ( cl->CheckForMissingPredictionFields( c, true ) )
				{
					classcount++;
					fieldcount += c;
				}
			}
			cl = cl->m_pNext;
		}

		if ( fieldcount )
		{
			vprint( 0, "\nSummary:  %i prediction fields missing from %i classes\n", fieldcount, classcount );
		}
		else
		{
			if ( !classcount )
			{
				vprint( 0, "\nSummary:  no prediction info present\n");
			}
			else
			{
				vprint( 0, "\nSummary:  no errors for %i predictable classes\n", classcount );
			}
		}

		vprint( 0, "\n" );
	}

	// Now check for things that are in the prediction TD but not marked correctly as being part of the sendtable
	{
		//Now check prediction stuff
		classcount = 0;
		fieldcount = 0;
		cl = m_pClassList;
		while ( cl )
		{
			if ( cl->m_bDerivedFromCBaseEntity || cl->m_bHasPredictionData )
			{
				if ( cl->CheckForPredictionFieldsInRecvTableNotMarkedAsSuchCorrectly( c ) )
				{
					classcount++;
					fieldcount += c;
				}
			}
			cl = cl->m_pNext;
		}

		vprint( 0, "\n" );
	}

	// Print stuff derived from CBaseEntity that doesn't have save/restore data
	vprint( 0, "\nMissing DATADESC tables:\n\n" );
	cl = m_pClassList;
	while ( cl )
	{
		if ( cl->m_bDerivedFromCBaseEntity && !cl->m_bHasSaveRestoreData && cl->m_nVarCount )
		{
			vprint( 0, "\t%s\n", cl->m_szName );
		}
		cl = cl->m_pNext;
	}
	vprint( 0, "\n" );
}

void CCodeProcessor::ReportHungarianNotationErrors()
{
	if ( !GetCheckHungarian() )
		return;

	vprint( 0, "\tChecking for hungarian notation issues\n" );

	CClass *cl = m_pClassList;
	int classcount = 0;
	int warningcount = 0;
	while ( cl )
	{
		int c = 0;
		
		cl->CheckForHungarianErrors( c );
		
		classcount++;
		warningcount += c;

		cl = cl->m_pNext;
	}

	vprint( 0, "\tFound %i notation errors across %i classes\n", classcount, warningcount );
}

void CCodeProcessor::PrintClassList( void ) const
{
	if ( GetPrintHierarchy() )
	{
		vprint( 0, "\nClass Summary\n\n" );
	}

	CClass *cl = m_pClassList;
	
	while ( cl )
	{
		if ( cl->m_bDerivedFromCBaseEntity )
		{
			bool missing = false;
			char missingwarning[ 128 ];

			missingwarning[0]=0;
			if ( cl->m_szTypedefBaseClass[0] )
			{
				if ( stricmp( cl->m_szBaseClass, cl->m_szTypedefBaseClass ) )
				{
					vprint( 0, "class %s has incorrect typedef %s BaseClass\n", cl->m_szName, cl->m_szTypedefBaseClass );
				}
			}
			else if ( cl->m_szBaseClass[ 0 ] )
			{
				missing = true;
				sprintf( missingwarning, ", missing typedef %s BaseClass", cl->m_szBaseClass );
			}

			if ( GetPrintHierarchy() || missing )
			{
				vprint( 0, "class %s%s\n", cl->m_szName, missing ? missingwarning : "" );
			}

			int level = 1;
			CClass *base = cl->m_pBaseClass;
			while ( base )
			{
				if ( GetPrintHierarchy() )
				{
					vprint( level++, "public %s\n", base->m_szName );
				}
				base = base->m_pBaseClass;
			}

			int i; 

			if ( GetPrintHierarchy() && GetPrintMembers() )
			{

				if ( cl->m_nMemberCount )
				{
					vprint( 1, "\nMember functions:\n\n" );
				}

				for ( i = 0; i < cl->m_nMemberCount; i++ )
				{
					CClassMemberFunction *member = cl->m_Members[ i ];

					if ( member->m_szType[0] )
					{
						vprint( 1, "%s %s();\n", member->m_szType, member->m_szName );
					}
					else
					{
						char *p = member->m_szName;
						if ( *p == '~' )
							p++;

						if ( stricmp( p, cl->m_szName ) )
						{
							vprint( 0, "class %s has member function %s with no return type!!!\n",
								cl->m_szName, member->m_szName );
						}
						vprint( 1, "%s();\n", member->m_szName );
					}
				}

				if ( cl->m_nVarCount )
				{
					vprint( 1, "\nMember Variables\n\n" );
				}

				
				for ( i = 0; i < cl->m_nVarCount; i++ )
				{
					CClassVariable *var = cl->m_Variables[ i ];

					if ( var->m_bIsArray )
					{
						if ( var->m_szArraySize[0]==0 )
						{
							vprint( 1, "%s %s[];\n", var->m_szType, var->m_szName );
						}
						else
						{
							vprint( 1, "%s %s[ %s ];\n", var->m_szType, var->m_szName, var->m_szArraySize );
						}
					}
					else
					{
						vprint( 1, "%s %s;\n", var->m_szType, var->m_szName );
					}
				}

				if ( cl->m_nTDCount )
				{
					vprint( 1, "\nSave/Restore TYPEDESCRIPTION\n\n" );
				}

				for ( i = 0; i < cl->m_nTDCount; i++ )
				{
					CTypeDescriptionField *td = cl->m_TDFields[ i ];
					if ( td->m_bCommentedOut )
					{
						vprint( 1, "// " );
					}
					else
					{
						vprint( 1, "" );
					}

					vprint( 0, "%s( %s, %s, %s, ... )\n", td->m_szDefineType, cl->m_szName, td->m_szVariableName, td->m_szType );
				}

				if ( !cl->m_bHasSaveRestoreData )
				{
				//	vprint( 1, "\nSave/Restore TYPEDESCRIPTION not specified for class\n\n" );
				}

				if ( cl->m_nPredTDCount )
				{
					vprint( 1, "\nPrediction TYPEDESCRIPTION\n\n" );
				}

				for ( i = 0; i < cl->m_nPredTDCount; i++ )
				{
					CTypeDescriptionField *td = cl->m_PredTDFields[ i ];
					if ( td->m_bCommentedOut )
					{
						vprint( 1, "// " );
					}
					else
					{
						vprint( 1, "" );
					}

					vprint( 0, "%s( %s, %s, %s, ... )\n", td->m_szDefineType, cl->m_szName, td->m_szVariableName, td->m_szType );
				}

				if ( !cl->m_bHasPredictionData )
				{
				//	vprint( 1, "\nPrediction TYPEDESCRIPTION not specified for class\n\n" );
				}
			}
		
			if ( GetPrintHierarchy() )
			{
				vprint( 0, "\n" );
			}
		}

		cl = cl->m_pNext;
	}
}

CClass *CCodeProcessor::AddClass( const char *classname )
{
	CClass *cl = FindClass( classname );
	if ( !cl )
	{
		cl = new CClass( classname );

		m_nClassesParsed++;

		cl->m_pNext = m_pClassList;
		m_pClassList = cl;
	}
	return cl;
}



char *CCodeProcessor::ParseTypeDescription( char *current, bool fIsMacroized )
{
	// Next token is classname then :: then variablename then braces then = then {
	char classname[ 256 ];
	char variablename[ 256 ];

	if ( !fIsMacroized )
	{
		current = CC_ParseToken( current );
		if ( strlen( com_token ) <= 0 )
			return current;

		strcpy( classname, com_token );
		if ( classname[0]=='*' )
			return current;

		current = CC_ParseToken( current );
		if (stricmp( com_token, ":" ) )
		{
			return current;
		}

		current = CC_ParseToken( current );
		Assert( !stricmp( com_token, ":" ) );

		current = CC_ParseToken( current );
		if ( strlen( com_token ) <= 0 )
			return current;

		strcpy( variablename, com_token );
	}
	else
	{
		current = CC_ParseToken( current );
		if (stricmp( com_token, "(" ) )
		{
			return current;
		}

		current = CC_ParseToken( current );
		if ( strlen( com_token ) <= 0 )
			return current;

		strcpy( classname, com_token );
		if ( classname[0]=='*' )
			return current;

		current = CC_ParseToken( current );
		if (stricmp( com_token, ")" ) )
		{
			return current;
		}

		// It's macro-ized
		strcpy( variablename, "m_DataDesc" );
	}
	if ( !fIsMacroized )
	{
		char ch;
		current = CC_RawParseChar( current, "{", &ch );
		Assert( ch == '{' );
		if ( strlen( com_token ) <= 0 )
			return current;
	}

	com_ignoreinlinecomment = true;
	bool insidecomment = false;

	// Now parse typedescription line by line
	while ( 1 )
	{
		current = CC_ParseToken( current );
		if ( strlen( com_token ) <= 0 )
			break;

		// Go to next line
		if ( !stricmp( com_token, "," ) )
			continue;

		// end
		if ( !fIsMacroized )
		{
			if ( !stricmp( com_token, "}" ) )
				break;
		}
		else
		{
			if ( !stricmp( com_token, "END_DATADESC" ) ||
				 !stricmp( com_token, "END_BYTESWAP_DATADESC" ) )
				break;
		}

		// skip #ifdef's inside of typedescs
		if ( com_token[0]=='#' )
		{
			current = CC_ParseUntilEndOfLine( current );
			continue;
		}

		if ( !stricmp( com_token, "/" ) )
		{
			current = CC_ParseToken( current );
			if ( !stricmp( com_token, "/" ) )
			{
				// There are two styles supported. One is to have the member definition present but commented out:
				//		DEFINE_FIELD( m_member, FIELD_INTEGER ),
				// the other is to have a comment where the first token of the comment is a member name:
				//		m_member
				current = CC_ParseToken( current );
				if ( !strnicmp( com_token, "DEFINE_", 7 ) )
				{
					CC_UngetToken();
					insidecomment = true;
				}
				else
				{
					char commentedvarname[ 256 ];
					strcpy( commentedvarname, com_token );

					CClass *cl = FindClass( classname );
					if ( cl )
					{
						if ( !cl->FindTD( commentedvarname ) )
						{
							cl->AddTD( commentedvarname, "", "", true );
						}
						// Mark that it has a data table
						cl->m_bHasSaveRestoreData = true;
					}
					current = CC_ParseUntilEndOfLine( current );
				}
				continue;
			}
		}

		com_ignoreinlinecomment = false;

		// Parse a typedescription line
		char definetype[ 256 ];
		strcpy( definetype, com_token );

		current = CC_ParseToken( current );
		if ( stricmp( com_token, "(" ) )
			break;

		char varname[ 256 ];
		current = CC_ParseToken( current );

		strcpy( varname, com_token );

		
		char vartype[ 256 ];

		vartype[0]=0;

		if ( !stricmp( definetype, "DEFINE_FUNCTION" ) ||
			!stricmp( definetype, "DEFINE_THINKFUNC" ) ||
			!stricmp( definetype, "DEFINE_ENTITYFUNC" ) ||
			!stricmp( definetype, "DEFINE_USEFUNC" ) ||
			!stricmp( definetype, "DEFINE_OUTPUT" ) ||
			!stricmp( definetype, "DEFINE_INPUTFUNC" ) )
		{
			strcpy( vartype, "funcptr" );
		}
		else if ( !stricmp(definetype, "DEFINE_FIELD") || 
			!stricmp(definetype, "DEFINE_INDEX") ||
			!stricmp(definetype, "DEFINE_KEYFIELD") || 
			!stricmp(definetype, "DEFINE_KEYFIELD_NOT_SAVED") || 
			!stricmp(definetype, "DEFINE_UTLVECTOR") || 
			!stricmp(definetype, "DEFINE_GLOBAL_FIELD") || 
			!stricmp(definetype, "DEFINE_GLOBAL_KEYFIELD") || 
			!stricmp(definetype, "DEFINE_CUSTOM_FIELD") ||
			!stricmp(definetype, "DEFINE_INPUT") ||
			!stricmp(definetype, "DEFINE_AUTO_ARRAY") ||
			!stricmp(definetype, "DEFINE_AUTO_ARRAY_KEYFIELD") ||
			!stricmp(definetype, "DEFINE_AUTO_ARRAY2D") ||
			!stricmp(definetype, "DEFINE_ARRAY") )
		{
			// skip comma
			current = CC_ParseToken( current );
			if (!strcmp( com_token, "[" ))
			{
				// Read array...
				current = CC_ParseToken( current );
				strcat( varname, "[" );
				strcat( varname, com_token );
				current = CC_ParseToken( current );

				// eat everything until the next "]"
				while (strcmp( com_token, "]") != 0)
				{
					strcat( varname, com_token );
					current = CC_ParseToken( current );
				}

				if ( strcmp( com_token, "]" ))
				{
					current = current;
				}

 				strcat( varname, "]" );

				// skip comma
				current = CC_ParseToken( current );
			}

			current = CC_ParseToken( current );

			strcpy( vartype, com_token );
		}

		// Jump to end of definition
		int nParenCount = 1;
		do
		{
			current = CC_ParseToken( current );
			if ( strlen( com_token ) <= 0 )
				break;

			if ( !stricmp( com_token, "(" ) )
			{
				++nParenCount; 
			}
			else if ( !stricmp( com_token, ")" ) )
			{
				if ( --nParenCount == 0 )
				{
					break;
				}
			}

		} while ( 1 );

//		vprint( 2, "%s%s::%s %s %s %s\n",
//			insidecomment ? "// " : "",
//			classname, variablename,
//			definetype, varname, vartype );

		CClass *cl = FindClass( classname );
		if ( cl )
		{
			if ( strcmp( vartype, "funcptr" ) && cl->FindTD( varname ) )
			{
				vprint( 0, "class %s::%s already has typedescription entry for field %s\n", classname, variablename, varname );
			}
			else
			{
				cl->AddTD( varname, vartype, definetype, insidecomment );
			}
			// Mark that it has a data table
			cl->m_bHasSaveRestoreData = true;
		}
		insidecomment = false;
		com_ignoreinlinecomment = true;
	}

	com_ignoreinlinecomment = false;

	return current;
}

char *CCodeProcessor::ParseReceiveTable( char *current )
{
	// Next token is open paren, then classname close paren, then {
	char classname[ 256 ];

	current = CC_ParseToken( current );
	if (stricmp( com_token, "(" ) )
	{
		return current;
	}

	current = CC_ParseToken( current );
	if ( strlen( com_token ) <= 0 )
		return current;

	strcpy( classname, com_token );
	if ( classname[0]=='*' )
		return current;
	if ( !strcmp( classname, "className" ) )
		return current;
	if ( !strcmp( classname, "clientClassName" ) )
		return current;

	CClass *cl = FindClass( classname );
	if ( cl )
	{
		cl->m_bHasRecvTableData = true;
	}

	CClass *leafClass = cl;

	// parse until end of line
	current = CC_ParseUntilEndOfLine( current );

	// Now parse recvtable entries line by line
	while ( 1 )
	{
		cl = leafClass;

		current = CC_ParseToken( current );
		if ( strlen( com_token ) <= 0 )
			break;

		// Go to next line
		if ( !stricmp( com_token, "," ) )
			continue;

		// end
		if ( !stricmp( com_token, "END_RECV_TABLE" ) )
			break;

		// skip #ifdef's inside of recv tables
		if ( com_token[0]=='#' )
		{
			current = CC_ParseUntilEndOfLine( current );
			continue;
		}

		// Parse recproxy line
		char recvproptype[ 256 ];
		strcpy( recvproptype, com_token );

		if ( strnicmp( recvproptype, "RecvProp", strlen( "RecvProp" ) ) )
		{
			current = CC_ParseUntilEndOfLine( current );
			continue;
		}

		if ( !strcmp( recvproptype, "RecvPropArray" ) )
		{
			current = CC_ParseToken( current );
			if ( stricmp( com_token, "(" ) )
				break;

			current = CC_ParseToken( current );
			if ( strnicmp( recvproptype, "RecvProp", strlen( "RecvProp" ) ) )
			{
				current = CC_ParseUntilEndOfLine( current );
				continue;
			}
		}

		current = CC_ParseToken( current );
		if ( stricmp( com_token, "(" ) )
			break;

		// Read macro or fieldname
		current = CC_ParseToken( current );

		char varname[ 256 ];

		if ( !strnicmp( com_token, "RECVINFO", strlen( "RECVINFO" ) ) )
		{
			current = CC_ParseToken( current );
			if ( stricmp( com_token, "(" ) )
				break;
			current = CC_ParseToken( current );
		}
		else
		{
			current = CC_ParseUntilEndOfLine( current );
			continue;
		}

		strcpy( varname, com_token );

		current = CC_ParseUntilEndOfLine( current );

		if ( cl )
		{
			// Look up the var
			CClassVariable *classVar = cl->FindVar( varname, true );
			if ( classVar )
			{
				classVar->m_bInRecvTable = true;
			}
			else
			{
				char cropped[ 256 ];
				char root[ 256 ];
				strcpy( cropped, varname );

				while ( 1 )
				{
					// See if varname is an embedded var
					char *spot = strstr( cropped, "." );
					if ( spot )
					{
						strcpy( root, cropped );
						root[ spot - cropped ] = 0;
						strcpy( cropped, spot + 1 );

						classVar = cl->FindVar( root, true );	
					}
					else
					{
						classVar = cl->FindVar( cropped, true );
						break;
					}

					if ( classVar )
						break;
				}

				if ( !classVar )
				{
					vprint( 0, "class %s::%s missing, but referenced by RecvTable!!!\n", classname, varname );
				}
				else
				{
					classVar->m_bInRecvTable = true;
				}
			}
		}
		else
		{
			vprint( 0, "class %s::%s found in RecvTable, but no such class is known!!!\n", classname, varname );
		}
	}

	return current;
}

char *CCodeProcessor::ParsePredictionTypeDescription( char *current )
{
	// Next token is open paren, then classname close paren, then {
	char classname[ 256 ];
	char variablename[ 256 ];

	current = CC_ParseToken( current );
	if (stricmp( com_token, "(" ) )
	{
		return current;
	}

	current = CC_ParseToken( current );
	if ( strlen( com_token ) <= 0 )
		return current;

	strcpy( classname, com_token );
	if ( classname[0]=='*' )
		return current;

	CClass *cl = FindClass( classname );
	if ( cl )
	{
		cl->m_bHasPredictionData = true;
	}

	current = CC_ParseToken( current );
	if (stricmp( com_token, ")" ) )
	{
		return current;
	}

	// It's macro-ized
	strcpy( variablename, "m_PredDesc" );

	com_ignoreinlinecomment = true;
	bool insidecomment = false;

	// Now parse typedescription line by line
	while ( 1 )
	{
		current = CC_ParseToken( current );
		if ( strlen( com_token ) <= 0 )
			break;

		// Go to next line
		if ( !stricmp( com_token, "," ) )
			continue;

		// end
		if ( !stricmp( com_token, "END_PREDICTION_DATA" ) )
			break;

		// skip #ifdef's inside of typedescs
		if ( com_token[0]=='#' )
		{
			current = CC_ParseUntilEndOfLine( current );
			continue;
		}

		if ( !stricmp( com_token, "/" ) )
		{
			current = CC_ParseToken( current );
			if ( !stricmp( com_token, "/" ) )
			{
				current = CC_ParseToken( current );
				if ( !strnicmp( com_token, "DEFINE_", 7 ) )
				{
					CC_UngetToken();
					insidecomment = true;
				}
				else
				{
					current = CC_ParseUntilEndOfLine( current );
				}
				continue;
			}
		}

		com_ignoreinlinecomment = false;

		// Parse a typedescription line
		char definetype[ 256 ];
		strcpy( definetype, com_token );

		current = CC_ParseToken( current );
		if ( stricmp( com_token, "(" ) )
			break;

		char varname[ 256 ];
		current = CC_ParseToken( current );

		strcpy( varname, com_token );

		
		char vartype[ 256 ];

		vartype[0]=0;

		if ( stricmp( definetype, "DEFINE_FUNCTION" ) )
		{
			// skip comma
			current = CC_ParseToken( current );

			current = CC_ParseToken( current );

			strcpy( vartype, com_token );
		}
		else
		{
			strcpy( vartype, "funcptr" );
		}

		bool inrecvtable = false;
		// Jump to end of definition
		int nParenCount = 1;
		do
		{
			current = CC_ParseToken( current );
			if ( strlen( com_token ) <= 0 )
				break;

			if ( !stricmp( com_token, "(" ) )
			{
				++nParenCount; 
			}
			else if ( !stricmp( com_token, ")" ) )
			{
				if ( --nParenCount == 0 )
				{
					break;
				}
			}

			if ( !stricmp( com_token, "FTYPEDESC_INSENDTABLE" ) )
			{
				inrecvtable = true;
			}

		} while ( 1 );

		/*
		vprint( 2, "%s%s::%s %s %s %s\n",
			insidecomment ? "// " : "",
			classname, variablename,
			definetype, varname, vartype );
		*/

		if ( cl )
		{
			if ( cl->FindPredTD( varname ) )
			{
				vprint( 0, "class %s::%s already has prediction typedescription entry for field %s\n", classname, variablename, varname );
			}
			else
			{
				cl->AddPredTD( varname, vartype, definetype, insidecomment, inrecvtable );
			}
		}
		insidecomment = false;
		com_ignoreinlinecomment = true;
	}

	com_ignoreinlinecomment = false;

	return current;
}

void CCodeProcessor::AddHeader( int depth, const char *filename, const char *rootmodule )
{
//	if ( depth < 1 )
//		return;
	if ( depth != 1 )
		return;

	// Check header list
	int idx = m_Headers.Find( filename );
	if ( idx != m_Headers.InvalidIndex() )
	{
		vprint( 0, "%s included twice in module %s\n", filename, rootmodule );
		return;
	}

	CODE_MODULE module;
	module.skipped = false;

	m_Headers.Insert( filename, module );
}

bool CCodeProcessor::CheckShouldSkip( bool forcequiet, int depth, char const *filename, int& numheaders, int& skippedfiles)
{
	int idx = m_Modules.Find( filename );
	if ( idx == m_Modules.InvalidIndex() )
		return false;

	CODE_MODULE *module = &m_Modules[ idx ];

	if ( forcequiet )
	{
		m_nHeadersProcessed++;
		numheaders++;

		if ( module->skipped )
		{
			skippedfiles++;
		}
	}

	AddHeader( depth, filename, m_szCurrentCPP );
	return true;
}

bool CCodeProcessor::LoadFile( char **buffer, char *filename, char const *module, bool forcequiet, 
	int depth, int& filelength, int& numheaders, int& skippedfiles,
	char const *srcroot, char const *root, char const *baseroot )
{
	for ( int i = 0; i < m_IncludePath.Count(); ++i )
	{
		// Load the base module
		sprintf( filename, "%s\\%s", m_IncludePath[i], module );
		strlwr( filename );

		if ( CheckShouldSkip( forcequiet, depth, filename, numheaders, skippedfiles ) )
		{
			return false;
		}

		*buffer = (char *)COM_LoadFile( filename, &filelength );
		if ( *buffer )
			return true;
	}

	return false;
}

static bool SkipFile( char const *module )
{
	if ( !stricmp( module, "predictable_entity.h" ) )
		return true;
	if ( !stricmp( module, "baseentity_shared.h" ) )
		return true;
	if ( !stricmp( module, "baseplayer_shared.h" ) )
		return true;
	if ( !stricmp( module, "tf_tacticalmap.cpp" ) )
		return true;
	if ( !stricmp( module, "techtree.cpp" ) )
		return true;
	if ( !stricmp( module, "techtree_parse.cpp" ) )
		return true;

	return false;
}

void CCodeProcessor::ProcessModule( bool forcequiet, int depth, int& maxdepth, int& numheaders, int& skippedfiles, 
	const char *srcroot, const char *baseroot, const char *root, const char *module )
{
	char filename[ 256 ];

	if ( depth > maxdepth )
	{
		maxdepth = depth;
	}
	int filelength;
	char *buffer = NULL;
		
	// Always skip these particular modules/headers
	if ( SkipFile( module ) )
	{
		CODE_MODULE module;
		module.skipped = true;

		m_Modules.Insert( filename, module );

		skippedfiles++;
		return;
	}

	if ( !LoadFile( &buffer, filename, module, forcequiet, depth, filelength, numheaders, skippedfiles,
		srcroot, root, baseroot ) )
	{
		CODE_MODULE module;
		module.skipped = true;
		m_Modules.Insert( filename, module );
		skippedfiles++;
		return;
	}

	Assert( buffer );

	m_nBytesProcessed += filelength;

	CODE_MODULE m;
	m.skipped = false;
	m_Modules.Insert( filename, m );

	if ( !forcequiet )
	{
		strcpy( m_szCurrentCPP, filename );
	}

	AddHeader( depth, filename, m_szCurrentCPP );

	bool onclient = !strnicmp( m_szBaseEntityClass, "C_", 2 ) ? true : false;

	// Parse tokens looking for #include directives or class starts
	char *current = buffer;

	current = CC_ParseToken( current );
	while ( 1 )
	{
		// No more tokens
		if ( !current )
			break;

		if ( !stricmp( com_token, "#include" ) )
		{
			current = CC_ParseToken( current );
			if ( strlen( com_token ) > 0 &&
				com_token[ 0 ] != '<' )
			{
				//vprint( "#include %s\n", com_token );
				m_nHeadersProcessed++;
				numheaders++;
				ProcessModule( true, depth + 1, maxdepth, numheaders, skippedfiles, srcroot, baseroot, root, com_token );
			}
		}
		else if ( !stricmp( com_token, "class" ) ||
			 !stricmp( com_token, "struct" ) )
		{
			current = CC_ParseToken( current );
			if ( strlen( com_token ) > 0 )
			{
				//vprint( depth, "class %s\n", com_token );

				CClass *cl = AddClass( com_token );

				// Now see if there's a base class
				current = CC_ParseToken( current );
				if ( !stricmp( com_token, ":" ) )
				{
					// Parse out public and then classname an
					current = CC_ParseToken( current );
					if ( !stricmp( com_token, "public" ) )
					{
						current = CC_ParseToken( current );
						if ( strlen( com_token ) > 0 )
						{
							cl->SetBaseClass( com_token );

							do
							{
								current = CC_ParseToken( current );
							} while ( strlen( com_token ) && stricmp( com_token, "{" ) );

							if ( !stricmp( com_token, "{" ) )
							{
								current = cl->ParseClassDeclaration( current );
							}
						}
					}
				}
				else if ( !stricmp( com_token, "{" ) )
				{
					current = cl->ParseClassDeclaration( current );
				}
			}
		}
		else if ( !strnicmp( com_token, "PREDICTABLE_CLASS", strlen( "PREDICTABLE_CLASS" ) ) )
		{
			char prefix[ 32 ];
			prefix[ 0 ] = 0;
			int type = 0;
			int bases = 1;
			int usebase = 0;

			if ( !stricmp( com_token, "PREDICTABLE_CLASS_ALIASED" ) )
			{
				type = 2;
				bases = 2;
				if ( onclient )
				{
					strcpy( prefix, "C_" );
				}
				else
				{
					strcpy( prefix, "C" );
					usebase = 1;
				}
			}
			else if ( !stricmp( com_token, "PREDICTABLE_CLASS_SHARED" ) )
			{
				type = 1;
				bases = 1;
			}
			else if ( !stricmp( com_token, "PREDICTABLE_CLASS" ) )
			{
				type = 0;
				bases = 1;
				if ( onclient )
				{
					strcpy( prefix, "C_" );
				}
				else
				{
					strcpy( prefix, "C" );
				}
			}
			else if ( !stricmp( com_token, "PREDICTABLE_CLASS_ALIASED_PREFIXED" ) )
			{
				// Nothing
			}
			else
			{
				vprint( 0, "PREDICTABLE_CLASS of unknown type!!! %s\n", com_token );
			}

			// parse the (
			current = CC_ParseToken( current );
			if ( !strcmp( com_token, "(" ) )
			{
				// Now the classname
				current = CC_ParseToken( current );
				if ( strlen( com_token ) > 0 )
				{
					//vprint( depth, "class %s\n", com_token );

					CClass *cl = AddClass( com_token );

					// Now see if there's a base class
					current = CC_ParseToken( current );
					if ( !stricmp( com_token, "," ) )
					{
						// Parse out public and then classname an
						current = CC_ParseToken( current );
						if ( strlen( com_token ) > 0 )
						{
							char basename[ 256 ];
							sprintf( basename, "%s%s", prefix, com_token );

							bool valid = true;

							if ( bases == 2 )
							{
								valid = false;

								current = CC_ParseToken( current );
								if ( !stricmp( com_token, "," ) )
								{
									current = CC_ParseToken( current );
									if ( strlen( com_token ) > 0 )
									{
										valid = true;
										if ( usebase == 1 )
										{
											sprintf( basename, "%s%s", prefix, com_token );
										}
									}
								}
							}

							if ( valid )
							{
								cl->SetBaseClass( basename );
								strcpy( cl->m_szTypedefBaseClass, basename );
							}
							
							do
							{
								current = CC_ParseToken( current );
							} while ( strlen( com_token ) && stricmp( com_token, ")" ) );

							if ( !stricmp( com_token, ")" ) )
							{
								current = cl->ParseClassDeclaration( current );
							}
						}
					}
					else if ( !stricmp( com_token, ")" ) )
					{
						current = cl->ParseClassDeclaration( current );
					}
				}
			}
		}
		else if ( !strcmp( com_token, "TYPEDESCRIPTION" ) || 
			    !strcmp( com_token, "typedescription_t" ) )
		{
			current = ParseTypeDescription( current, false );
		}
		else if ( !strcmp( com_token, "BEGIN_DATADESC" ) ||
				  !strcmp( com_token, "BEGIN_DATADESC_NO_BASE" ) ||
				  !strcmp( com_token, "BEGIN_SIMPLE_DATADESC" ) ||
				  !strcmp( com_token, "BEGIN_BYTESWAP_DATADESC" ) )
		{
			current = ParseTypeDescription( current, true );
		}
		else if ( !strcmp( com_token, "BEGIN_PREDICTION_DATA" ) ||
			!strcmp( com_token, "BEGIN_EMBEDDED_PREDDESC" ) )
		{
			current = ParsePredictionTypeDescription( current );
		}
		else if (	!strcmp( com_token, "BEGIN_RECV_TABLE" ) ||
					!strcmp( com_token, "BEGIN_RECV_TABLE_NOBASE" ) ||
					!strcmp( com_token, "IMPLEMENT_CLIENTCLASS_DT" ) ||
					!strcmp( com_token, "IMPLEMENT_CLIENTCLASS_DT_NOBASE" ) )
		{
			current = ParseReceiveTable( current );
		}
		else if ( !strcmp( com_token, "IMPLEMENT_PREDICTABLE_NODATA" ) )
		{
			current = CC_ParseToken( current );
			if ( !strcmp( com_token, "(" ) )
			{
				current = CC_ParseToken( current );

				CClass *cl = FindClass( com_token );
				if ( cl )
				{
					if ( cl->m_bHasPredictionData )
					{
						if ( !forcequiet )
						{
							vprint( 0, "Class %s declared predictable and implemented with IMPLEMENT_PREDICTABLE_NODATA in typedescription\n",
								cl->m_szName );
						}

						cl->m_bHasPredictionData = false;
					}
				}

				current = CC_ParseToken( current );
			}
		}

		current = CC_ParseToken( current );
	}

	COM_FreeFile( (unsigned char *)buffer );

	if ( !forcequiet && !GetQuiet() )
	{
		vprint( 0, " %s: headers (%i game / %i total)", (char *)&filename[ m_nOffset ], numheaders - skippedfiles, numheaders );
		if ( maxdepth > 1 )
		{
			vprint( 0, ", depth %i", maxdepth );
		}
		vprint( 0, "\n" );
	}

	m_nLinesOfCode += linesprocessed;
	linesprocessed = 0;
}

void CCodeProcessor::ProcessModules( const char *srcroot, const char *root, const char *rootmodule )
{
	m_nFilesProcessed++;

	// Reset header list per module
	m_Headers.RemoveAll();

	int numheaders = 0;
	int maxdepth = 0;
	int skippedfiles = 0;
	ProcessModule( false, 0, maxdepth, numheaders, skippedfiles, srcroot, root, root, rootmodule );
}

void ReportMissingTypes();

void CCodeProcessor::PrintResults( const char *baseentityclass )
{
	vprint( 0, "\nChecking for errors and totaling...\n\n" );

	ResolveBaseClasses( baseentityclass );
	PrintClassList();
	PrintMissingTDFields();
	ReportMissingTypes();
	ReportHungarianNotationErrors();

	vprint( 0, "%i total classes parsed from %i files ( %i headers parsed )\n",
		m_nClassesParsed,
		m_nFilesProcessed,
		m_nHeadersProcessed );

	vprint( 0, "%.3f K lines of code processed\n",
		(double)m_nLinesOfCode / 1024.0 );
	
	double elapsed = ( m_flEnd - m_flStart );

	if ( elapsed > 0.0 )
	{
		vprint( 0, "%.2f K processed in %.3f seconds, throughput %.2f KB/sec\n\n",
			(double)m_nBytesProcessed / 1024.0, elapsed, (double)m_nBytesProcessed / ( 1024.0 * elapsed ) );
	}

	Clear();
}

CCodeProcessor::CCodeProcessor( void )
{
	m_pClassList					= NULL;
	
	m_Modules.RemoveAll();

	m_bQuiet						= false;
	m_bPrintHierarchy				= false;
	m_bPrintMembers					= true;
	m_bPrintTypedescriptionErrors	= true;
	m_bPrintPredictionDescErrors	= true;
	m_bCreateMissingTDs				= false;
	m_bLogToFile					= false;
	m_bCheckHungarian				= false;

	m_nFilesProcessed				= 0;
	m_nHeadersProcessed				= 0;
	m_nClassesParsed				= 0;
	m_nOffset						= 0;
	m_nBytesProcessed				= 0;
	m_nLinesOfCode					= 0;
	m_flStart						= 0.0;
	m_flEnd							= 0.0;

	m_szCurrentCPP[ 0 ]				= 0;
	m_szBaseEntityClass[ 0 ]		= 0;
}

CCodeProcessor::~CCodeProcessor( void )
{
}

void CCodeProcessor::ConstructModuleList_R( int level, const char *baseentityclass, 
	const char *gamespecific, const char *root, const char *srcroot )
{
	char directory[ 256 ];
	char filename[ 256 ];
	WIN32_FIND_DATA wfd;
	HANDLE ff;

	sprintf( directory, "%s\\*.*", root );

	if ( ( ff = FindFirstFile( directory, &wfd ) ) == INVALID_HANDLE_VALUE )
		return;

	do
	{
		if ( wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
		{

			if ( wfd.cFileName[ 0 ] == '.' )
				continue;

			// Once we descend down a branch, don't keep looking for hl2/tf2 in name, just recurse through all children
			if ( level == 0 && !strstr( wfd.cFileName, gamespecific ) )
				continue;

			// Recurse down directory
			sprintf( filename, "%s\\%s", root, wfd.cFileName );
			ConstructModuleList_R( level+1, baseentityclass, gamespecific, filename, srcroot );
		}
		else
		{
			if ( strstr( wfd.cFileName, ".cpp" ) )
			{
				ProcessModules( srcroot, root, wfd.cFileName );
			}
		}
	} while ( FindNextFile( ff, &wfd ) );
}

void CCodeProcessor::CleanupIncludePath()
{
	for ( int i = m_IncludePath.Count(); --i >= 0; )
	{
		delete [] m_IncludePath[i];
	}
	m_IncludePath.RemoveAll();
}

void CCodeProcessor::AddIncludePath( const char *pPath )
{
	int i = m_IncludePath.AddToTail();
	int nLen = strlen(pPath) + 1;
	m_IncludePath[i] = new char[nLen];
	memcpy( m_IncludePath[i], pPath, nLen );
}

void CCodeProcessor::SetupIncludePath( const char *sourcetreebase, const char *subdir, const char *gamespecific )
{
	CleanupIncludePath();

	char path[MAX_PATH];
	sprintf( path, "%s\\%s", sourcetreebase, subdir );
	strlwr( path );
	AddIncludePath( path );

	char modsubdir[128];
	if ( !stricmp(subdir, "dlls") )
	{
		sprintf(modsubdir,"%s\\%s_dll", subdir, gamespecific );
	}
	else if ( !stricmp(subdir, "cl_dll") )
	{
		sprintf(modsubdir,"%s\\%s_hud", subdir, gamespecific );
	}
	else
	{
		sprintf(modsubdir,"%s\\%s", subdir, gamespecific );
	}

	sprintf( path, "%s\\%s", sourcetreebase, modsubdir );
	strlwr( path );
	AddIncludePath( path );

	// Game shared
	sprintf( path, "%s\\game_shared", sourcetreebase );
	strlwr( path );
	AddIncludePath( path );

	sprintf( path, "%s\\game_shared\\%s", sourcetreebase, gamespecific );
	strlwr( path );
	AddIncludePath( path );

	sprintf( path, "%s\\public", sourcetreebase );
	strlwr( path );
	AddIncludePath( path );
}


void CCodeProcessor::Process( const char *baseentityclass, const char *gamespecific, const char *sourcetreebase, const char *subdir )
{
	SetupIncludePath( sourcetreebase, subdir, gamespecific );

	strcpy( m_szBaseEntityClass, baseentityclass );

	m_nBytesProcessed	= 0;
	m_nFilesProcessed	= 0;
	m_nHeadersProcessed = 0;
	m_nClassesParsed	= 0;
	m_nLinesOfCode		= 0;

	linesprocessed		= 0;

	m_Modules.RemoveAll();
	m_Headers.RemoveAll();

	m_flStart = UTIL_FloatTime();

	char rootdirectory[ 256 ];
	sprintf( rootdirectory, "%s\\%s", sourcetreebase, subdir );

	vprint( 0, "--- Processing %s\n\n", rootdirectory );

	m_nOffset = strlen( rootdirectory ) + 1;

	ConstructModuleList_R( 0, baseentityclass, gamespecific, rootdirectory, sourcetreebase );

	sprintf( rootdirectory, "%s\\%s", sourcetreebase, "game_shared" );

	vprint( 0, "--- Processing %s\n\n", rootdirectory );

	m_nOffset = strlen( rootdirectory ) + 1;
	
	ConstructModuleList_R( 0, baseentityclass, gamespecific, rootdirectory, sourcetreebase );

	m_flEnd = UTIL_FloatTime();

	PrintResults( baseentityclass );
}

void CCodeProcessor::Process( const char *baseentityclass, const char *gamespecific, 
	const char *sourcetreebase, const char *subdir, const char *pFileName )
{
	SetupIncludePath( sourcetreebase, subdir, gamespecific );

	strcpy( m_szBaseEntityClass, baseentityclass );

	m_nBytesProcessed	= 0;
	m_nFilesProcessed	= 0;
	m_nHeadersProcessed = 0;
	m_nClassesParsed	= 0;
	m_nLinesOfCode		= 0;

	linesprocessed		= 0;

	m_Modules.RemoveAll();
	m_Headers.RemoveAll();

	m_flStart = UTIL_FloatTime();

	char rootdirectory[ 256 ];
	sprintf( rootdirectory, "%s\\%s", sourcetreebase, subdir );

	vprint( 0, "--- Processing %s\n\n", rootdirectory );

	m_nOffset = strlen( rootdirectory ) + 1;

	ProcessModules( sourcetreebase, rootdirectory, pFileName );

	m_flEnd = UTIL_FloatTime();

	PrintResults( baseentityclass );
}

void CCodeProcessor::SetQuiet( bool quiet )
{
	m_bQuiet = quiet;
}

bool CCodeProcessor::GetQuiet( void ) const
{
	return m_bQuiet;
}

void CCodeProcessor::SetPrintHierarchy( bool print )
{
	m_bPrintHierarchy = print;
}

bool CCodeProcessor::GetPrintHierarchy( void ) const
{
	return m_bPrintHierarchy;
}

void CCodeProcessor::SetPrintMembers( bool print )
{
	m_bPrintMembers = print;
}

bool CCodeProcessor::GetPrintMembers( void ) const
{
	return m_bPrintMembers;
}

void CCodeProcessor::SetPrintTDs( bool print )
{
	m_bPrintTypedescriptionErrors = print;
}

bool CCodeProcessor::GetPrintTDs( void ) const
{
	return m_bPrintTypedescriptionErrors;
}

void CCodeProcessor::SetLogFile( bool log )
{
	m_bLogToFile = log;
}

bool CCodeProcessor::GetLogFile( void ) const
{
	return m_bLogToFile;
}

void CCodeProcessor::SetPrintPredTDs( bool print )
{
	m_bPrintPredictionDescErrors = print;
}

bool CCodeProcessor::GetPrintPredTDs( void ) const
{
	return m_bPrintPredictionDescErrors;
}

void CCodeProcessor::SetPrintCreateMissingTDs( bool print )
{
	m_bCreateMissingTDs = print;
}

bool CCodeProcessor::GetPrintCreateMissingTDs( void ) const
{
	return m_bCreateMissingTDs;
}

void CCodeProcessor::SetPrintCreateMissingPredTDs( bool print )
{
	m_bCreateMissingPredTDs = print;
}

bool CCodeProcessor::GetPrintCreateMissingPredTDs( void ) const
{
	return m_bCreateMissingPredTDs;
}

void CCodeProcessor::SetCheckHungarian( bool check )
{
	m_bCheckHungarian = check;
}

bool CCodeProcessor::GetCheckHungarian() const
{
	return m_bCheckHungarian;
}

static CCodeProcessor g_Processor;
ICodeProcessor *processor = ( ICodeProcessor * )&g_Processor;