//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include <stdio.h>
#include <io.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#include "tier0/platform.h"
#include "tier0/dbg.h"
#include "tier1/utlbuffer.h"

#include "cfgprocessor.h"

// Type conversions should be controlled by programmer explicitly - shadercompile makes use of 64-bit integer arithmetics
#pragma warning( error : 4244 )

namespace
{

	int Usage()
	{
		printf( "Usage:     expparser [OPTIONS] <input.txt 2>>output.txt\n" );
		printf( "Options:   [none]\n" );
		printf( "Input:     Sections in a file:\n" );
		printf( "           #BEGIN\n" );
		printf( "           #DEFINES:\n" );
		printf( "           FASTPATH=0..2\n" );
		printf( "           FOGTYPE=0..5\n" );
		printf( "           #SKIP:\n" );
		printf( "           ($FOGTYPE > 1) && (!$FASTPATH)\n" );
		printf( "           #COMMAND:\n" );
		printf( "           fxc.exe /DFLAGS=0x00\n" );
		printf( "           /Foshader.o myshader.fxc > output.txt\n" );
		printf( "           #END\n" );
		printf( "Version:   expparser compiled on " __DATE__ " @ " __TIME__ ".\n" );

		return -1;
	}
};

namespace
{

static bool s_bNoOutput = true;

void OutputF( FILE *f, char const *szFmt, ... )
{
	if( s_bNoOutput )
		return;

	va_list args;
	va_start( args, szFmt );
	vfprintf( f, szFmt, args );
	va_end( args );
}

};

//////////////////////////////////////////////////////////////////////////
//
// Utility classes:
//	QuickArray<T>
//	QuickStrIdx
//	QuickString
//	QuickStrUnique
//	QuickMap
//
//////////////////////////////////////////////////////////////////////////

#include <unordered_map>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <algorithm>

template < typename T >
class QuickArray : private std::vector < T >
{
public:
	void Append( T const &e ) { push_back( e ); };
	int Size( void ) const { return ( int ) size(); };
	T const & Get( int idx ) const { return at( idx ); };
	T & GetForEdit( int idx ) { return at( idx ); }
	void Clear( void ) { clear(); }
	T const * ArrayBase() const { return empty() ? NULL : &at( 0 ); }
	T * ArrayBaseForEdit() { return empty() ? NULL : &at( 0 ); }
};

template < typename T >
class QuickStack : private std::vector < T >
{
public:
	void Push( T const &e ) { push_back( e ); };
	int Size( void ) const { return ( int ) size(); };
	T const & Top( void ) const { return at( Size() - 1 ); };
	void Pop( void ) { pop_back(); }
	void Clear( void ) { clear(); }
};

template < typename K, typename V >
class QuickMap : private std::map < K, V >
{
public:
	void Append( K const &k, V const &v ) { insert( value_type( k, v ) ); };
	int Size( void ) const { return ( int ) size(); };
	V const & GetLessOrEq( K &k, V const &v ) const;
	V const & Get( K const &k, V const &v ) const { const_iterator it = find( k ); return ( it != end() ? it->second : v ); };
	V & GetForEdit( K const &k, V &v ) { iterator it = find( k ); return ( it != end() ? it->second : v ); };
	void Clear( void ) { clear(); }
};

template < typename K, typename V >
V const & QuickMap< K, V >::GetLessOrEq( K &k, V const &v ) const
{
	const_iterator it = lower_bound( k );
	
	if ( end() == it )
	{
		if ( empty() )
			return v;
		-- it;
	}

	if ( k < it->first )
	{
		if ( begin() == it )
			return v;
		-- it;
	}

	k = it->first;
	return it->second;
}

class QuickStrIdx : private std::unordered_map < std::string, int >
{
public:
	void Append( char const *szName, int idx ) { insert( value_type( szName, idx ) ); };
	int Size( void ) const { return ( int ) size(); };
	int Get( char const *szName ) const { const_iterator it = find( szName ); if ( end() != it ) return it->second; else return -1; };
	void Clear( void ) { clear(); }
};

class QuickStrUnique : private std::set < std::string >
{
public:
	int Size( void ) const { return ( int ) size(); }
	bool Add( char const *szString ) { return insert( szString ).second; }
	void Remove( char const *szString ) { erase( szString ); }
	char const * Lookup( char const *szString ) { const_iterator it = find( szString ); if ( end() != it ) return it->data(); else return NULL; }
	char const * AddLookup( char const *szString ) { iterator it = insert( szString ).first; if ( end() != it ) return it->data(); else return NULL; }
	void Clear( void ) { clear(); }
};

class QuickString : private std::vector< char >
{
public:
	explicit QuickString( char const *szValue, size_t len = -1 );
	int Size() const { return ( int ) ( size() - 1 ); }
	char * Get() { return &at( 0 ); }
};

QuickString::QuickString( char const *szValue, size_t len )
{
	if ( size_t( -1 ) == len )
		len = ( size_t ) strlen( szValue );

	resize( len + 1, 0 );
	memcpy( Get(), szValue, len );
}


//////////////////////////////////////////////////////////////////////////
//
// Define class
//
//////////////////////////////////////////////////////////////////////////

class Define
{
public:
	explicit Define( char const *szName, int min, int max, bool bStatic ) : m_sName( szName ), m_min( min ), m_max( max ), m_bStatic( bStatic ) {}

public:
	char const * Name() const { return m_sName.data(); };
	int Min() const { return m_min; }; 
	int Max() const { return m_max; }; 
	bool IsStatic() const { return m_bStatic; }

protected:
	std::string m_sName;
	int m_min, m_max;
	bool m_bStatic;
};



//////////////////////////////////////////////////////////////////////////
//
// Expression parser
//
//////////////////////////////////////////////////////////////////////////

class IEvaluationContext
{
public:
	virtual int GetVariableValue( int nSlot ) = 0;
	virtual char const * GetVariableName( int nSlot ) = 0;
	virtual int GetVariableSlot( char const *szVariableName ) = 0;
};

class IExpression
{
public:
	virtual int  Evaluate( IEvaluationContext *pCtx ) const = 0;
	virtual void Print( IEvaluationContext *pCtx ) const = 0;
};

#define EVAL virtual int Evaluate( IEvaluationContext *pCtx ) const 
#define PRNT virtual void Print( IEvaluationContext *pCtx ) const

class CExprConstant : public IExpression
{
public:
	CExprConstant( int value ) : m_value( value ) {}
	EVAL { pCtx; return m_value; };
	PRNT { pCtx; OutputF( stdout, "%d", m_value ); }
public:
	int m_value;
};

class CExprVariable : public IExpression
{
public:
	CExprVariable( int nSlot ) : m_nSlot( nSlot ) {}
	EVAL { return (m_nSlot >= 0) ? pCtx->GetVariableValue( m_nSlot ) : 0; };
	PRNT { (m_nSlot >= 0) ? OutputF( stdout, "$%s", pCtx->GetVariableName( m_nSlot ) ) : OutputF( stdout, "$**@**" ); }
public:
	int m_nSlot;
};

class CExprUnary : public IExpression
{
public:
	CExprUnary( IExpression *x ) : m_x( x ) {}
public:
	IExpression *m_x;
};

#define BEGIN_EXPR_UNARY( className ) class className : public CExprUnary { public: className( IExpression *x ) : CExprUnary( x ) {}
#define END_EXPR_UNARY() };

BEGIN_EXPR_UNARY( CExprUnary_Negate )
	EVAL { return ! m_x->Evaluate( pCtx ); };
	PRNT { OutputF( stdout, "!" ); m_x->Print(pCtx); }
END_EXPR_UNARY()

class CExprBinary : public IExpression
{
public:
	CExprBinary( IExpression *x, IExpression *y ) : m_x( x ), m_y( y ) {}
	virtual int Priority() const = 0;
public:
	IExpression *m_x;
	IExpression *m_y;
};

#define BEGIN_EXPR_BINARY( className ) class className : public CExprBinary { public: className( IExpression *x, IExpression *y ) : CExprBinary( x, y ) {}
#define EXPR_BINARY_PRIORITY( nPriority ) virtual int Priority() const { return nPriority; };
#define END_EXPR_BINARY() };

BEGIN_EXPR_BINARY( CExprBinary_And )
	EVAL { return m_x->Evaluate( pCtx ) && m_y->Evaluate( pCtx ); }
	PRNT { OutputF( stdout, "( " ); m_x->Print( pCtx ); OutputF( stdout, " && " ); m_y->Print( pCtx ); OutputF( stdout, " )" ); }
	EXPR_BINARY_PRIORITY( 1 );
END_EXPR_BINARY()

BEGIN_EXPR_BINARY( CExprBinary_Or )
	EVAL { return m_x->Evaluate( pCtx ) || m_y->Evaluate( pCtx ); }
	PRNT { OutputF( stdout, "( " ); m_x->Print( pCtx ); OutputF( stdout, " || " ); m_y->Print( pCtx ); OutputF( stdout, " )" ); }
	EXPR_BINARY_PRIORITY( 2 );
END_EXPR_BINARY()

BEGIN_EXPR_BINARY( CExprBinary_Eq )
	EVAL { return m_x->Evaluate( pCtx ) == m_y->Evaluate( pCtx ); }
	PRNT { OutputF( stdout, "( " ); m_x->Print( pCtx ); OutputF( stdout, " == " ); m_y->Print( pCtx ); OutputF( stdout, " )" ); }
	EXPR_BINARY_PRIORITY( 0 );
END_EXPR_BINARY()

BEGIN_EXPR_BINARY( CExprBinary_Neq )
	EVAL { return m_x->Evaluate( pCtx ) != m_y->Evaluate( pCtx ); }
	PRNT { OutputF( stdout, "( " ); m_x->Print( pCtx ); OutputF( stdout, " != " ); m_y->Print( pCtx ); OutputF( stdout, " )" ); }
	EXPR_BINARY_PRIORITY( 0 );
END_EXPR_BINARY()

BEGIN_EXPR_BINARY( CExprBinary_G )
	EVAL { return m_x->Evaluate( pCtx ) > m_y->Evaluate( pCtx ); }
	PRNT { OutputF( stdout, "( " ); m_x->Print( pCtx ); OutputF( stdout, " > " ); m_y->Print( pCtx ); OutputF( stdout, " )" ); }
	EXPR_BINARY_PRIORITY( 0 );
END_EXPR_BINARY()

BEGIN_EXPR_BINARY( CExprBinary_Ge )
	EVAL { return m_x->Evaluate( pCtx ) >= m_y->Evaluate( pCtx ); }
	PRNT { OutputF( stdout, "( " ); m_x->Print( pCtx ); OutputF( stdout, " >= " ); m_y->Print( pCtx ); OutputF( stdout, " )" ); }
	EXPR_BINARY_PRIORITY( 0 );
END_EXPR_BINARY()

BEGIN_EXPR_BINARY( CExprBinary_L )
	EVAL { return m_x->Evaluate( pCtx ) < m_y->Evaluate( pCtx ); }
	PRNT { OutputF( stdout, "( " ); m_x->Print( pCtx ); OutputF( stdout, " < " ); m_y->Print( pCtx ); OutputF( stdout, " )" ); }
	EXPR_BINARY_PRIORITY( 0 );
END_EXPR_BINARY()

BEGIN_EXPR_BINARY( CExprBinary_Le )
	EVAL { return m_x->Evaluate( pCtx ) <= m_y->Evaluate( pCtx ); }
	PRNT { OutputF( stdout, "( " ); m_x->Print( pCtx ); OutputF( stdout, " <= " ); m_y->Print( pCtx ); OutputF( stdout, " )" ); }
	EXPR_BINARY_PRIORITY( 0 );
END_EXPR_BINARY()


class CComplexExpression : public IExpression
{
public:
	CComplexExpression( IEvaluationContext *pCtx ) : m_pRoot( NULL ), m_pContext( pCtx ) { }
	~CComplexExpression() { Clear(); }

	void Parse( char const *szExpression );
	void Clear( void );

public:
	EVAL { return m_pRoot ? m_pRoot->Evaluate( pCtx ? pCtx : m_pContext ) : 0; };
	PRNT { OutputF( stdout, "[ " ); m_pRoot ? m_pRoot->Print( pCtx ? pCtx : m_pContext ) : OutputF( stdout, "**NEXPR**" ); OutputF( stdout, " ]\n" ); }

protected:
	IExpression * ParseTopLevel( char *&szExpression );
	IExpression * ParseInternal( char *&szExpression );
	IExpression * Allocated( IExpression *pExpression );
	IExpression * AbortedParse( char *&szExpression ) const { *szExpression = 0; return m_pDefFalse; }

protected:
	QuickArray < IExpression * > m_arrAllExpressions;
	IExpression *m_pRoot;
	IEvaluationContext *m_pContext;

	IExpression *m_pDefTrue, *m_pDefFalse;
};

void CComplexExpression::Parse( char const *szExpression )
{
	Clear();

	m_pDefTrue = Allocated( new CExprConstant( 1 ) );
	m_pDefFalse = Allocated( new CExprConstant( 0 ) );

	m_pRoot = m_pDefFalse;

	if (szExpression)
	{
		QuickString qs( szExpression );
		char *szExpression = qs.Get(), *szExpectEnd = szExpression + qs.Size(), *szParse = szExpression;
		m_pRoot = ParseTopLevel( szParse );

		if ( szParse != szExpectEnd )
		{
			m_pRoot = m_pDefFalse;
		}
	}
}

IExpression * CComplexExpression::ParseTopLevel( char *&szExpression )
{
	QuickStack< CExprBinary * > exprStack;
	IExpression *pFirstToken = ParseInternal( szExpression );

	for ( ; ; )
	{
		// Skip whitespace
		while ( *szExpression && V_isspace( *szExpression ) )
		{
			++ szExpression;
		}

		// End of binary expression
		if ( !*szExpression || ( *szExpression == ')' ) )
		{
			break;
		}

		// Determine the binary expression type
		CExprBinary *pBinaryExpression = NULL;

		if ( 0 )
		{
			NULL;
		}
		else if ( !strncmp( szExpression, "&&", 2 ) )
		{
			pBinaryExpression = new CExprBinary_And( NULL, NULL );
			szExpression += 2;
		}
		else if ( !strncmp( szExpression, "||", 2 ) )
		{
			pBinaryExpression = new CExprBinary_Or( NULL, NULL );
			szExpression += 2;
		}
		else if ( !strncmp( szExpression, ">=", 2 ) )
		{
			pBinaryExpression = new CExprBinary_Ge( NULL, NULL );
			szExpression += 2;
		}
		else if ( !strncmp( szExpression, "<=", 2 ) )
		{
			pBinaryExpression = new CExprBinary_Le( NULL, NULL );
			szExpression += 2;
		}
		else if ( !strncmp( szExpression, "==", 2 ) )
		{
			pBinaryExpression = new CExprBinary_Eq( NULL, NULL );
			szExpression += 2;
		}
		else if ( !strncmp( szExpression, "!=", 2 ) )
		{
			pBinaryExpression = new CExprBinary_Neq( NULL, NULL );
			szExpression += 2;
		}
		else if ( *szExpression == '>' )
		{
			pBinaryExpression = new CExprBinary_G( NULL, NULL );
			++ szExpression;
		}
		else if ( *szExpression == '<' )
		{
			pBinaryExpression = new CExprBinary_L( NULL, NULL );
			++ szExpression;
		}
		else
		{
			return AbortedParse( szExpression );
		}

		Allocated( pBinaryExpression );
		pBinaryExpression->m_y = ParseInternal( szExpression );

		// Figure out the expression priority
		int nPriority = pBinaryExpression->Priority();
		IExpression *pLastExpr = pFirstToken;
		while ( exprStack.Size() )
		{
			CExprBinary *pStickTo = exprStack.Top();
			pLastExpr = pStickTo;

			if ( nPriority > pStickTo->Priority() )
				exprStack.Pop();
			else
				break;
		}

		if ( exprStack.Size() )
		{
			CExprBinary *pStickTo = exprStack.Top();
			pBinaryExpression->m_x = pStickTo->m_y;
			pStickTo->m_y = pBinaryExpression;
		}
		else
		{
			pBinaryExpression->m_x = pLastExpr;
		}

		exprStack.Push( pBinaryExpression );
	}

	// Tip-of-the-tree retrieval
	{
		IExpression *pLastExpr = pFirstToken;
		while ( exprStack.Size() )
		{
			pLastExpr = exprStack.Top();
			exprStack.Pop();
		}

		return pLastExpr;
	}
}

IExpression * CComplexExpression::ParseInternal( char *&szExpression )
{
	// Skip whitespace
	while ( *szExpression && V_isspace( *szExpression ) )
	{
		++ szExpression;
	}

	if ( !*szExpression )
		return AbortedParse( szExpression );

	if ( 0 )
	{
		NULL;
	}
	else if ( V_isdigit( *szExpression ) )
	{
		long lValue = strtol( szExpression, &szExpression, 10 );
		return Allocated( new CExprConstant( lValue ) );
	}
	else if ( !strncmp( szExpression, "defined", 7 ) )
	{
		szExpression += 7;
		IExpression *pNext = ParseInternal( szExpression );
		return Allocated( new CExprConstant( pNext->Evaluate( m_pContext ) ) );
	}
	else if ( *szExpression == '(' )
	{
		++ szExpression;
		IExpression *pBracketed = ParseTopLevel( szExpression );
		if ( ')' == *szExpression )
		{
			++ szExpression;
			return pBracketed;
		}
		else
		{
			return AbortedParse( szExpression );
		}
	}
	else if ( *szExpression == '$' )
	{
		size_t lenVariable = 0;
		for ( char *szEndVar = szExpression + 1; *szEndVar; ++ szEndVar, ++ lenVariable )
		{
			if ( !V_isalnum( *szEndVar ) )
			{
				switch ( *szEndVar )
				{
				case '_':
					break;
				default:
					goto parsed_variable_name;
				}
			}
		}

parsed_variable_name:
		int nSlot = m_pContext->GetVariableSlot( QuickString( szExpression + 1, lenVariable ).Get() );
		szExpression += lenVariable + 1;

		return Allocated( new CExprVariable( nSlot ) );
	}
	else if ( *szExpression == '!' )
	{
		++ szExpression;
		IExpression *pNext = ParseInternal( szExpression );
		return Allocated( new CExprUnary_Negate( pNext ) );
	}
	else
	{
		return AbortedParse( szExpression );
	}
}

IExpression * CComplexExpression::Allocated( IExpression *pExpression )
{
	m_arrAllExpressions.Append( pExpression );
	return pExpression;
}

void CComplexExpression::Clear( void )
{
	for ( int k = 0; k < m_arrAllExpressions.Size() ; ++ k )
	{
		delete m_arrAllExpressions.Get( k );
	}

	m_arrAllExpressions.Clear();
	m_pRoot = NULL;
}


#undef BEGIN_EXPR_UNARY
#undef BEGIN_EXPR_BINARY

#undef END_EXPR_UNARY
#undef END_EXPR_BINARY

#undef EVAL
#undef PRNT

//////////////////////////////////////////////////////////////////////////
//
// Combo Generator class
//
//////////////////////////////////////////////////////////////////////////

class ComboGenerator : public IEvaluationContext
{
public:
	void AddDefine( Define const &df );
	Define const * const GetDefinesBase( void ) { return m_arrDefines.ArrayBase(); }
	Define const * const GetDefinesEnd( void ) { return m_arrDefines.ArrayBase() + m_arrDefines.Size(); }

	uint64 NumCombos();
	uint64 NumCombos( bool bStaticCombos );
	void RunAllCombos( CComplexExpression const &skipExpr );

	// IEvaluationContext
public:
	virtual int GetVariableValue( int nSlot ) { return m_arrVarSlots.Get( nSlot ); };
	virtual char const * GetVariableName( int nSlot ) { return m_arrDefines.Get( nSlot ).Name(); };
	virtual int GetVariableSlot( char const *szVariableName ) { return m_mapDefines.Get( szVariableName ); };

protected:
	QuickArray< Define >	m_arrDefines;
	QuickStrIdx				m_mapDefines;
	QuickArray < int >		m_arrVarSlots;
};

void ComboGenerator::AddDefine( Define const &df )
{
	m_mapDefines.Append( df.Name(), m_arrDefines.Size() );
	m_arrDefines.Append( df );
	m_arrVarSlots.Append( 1 );
}

uint64 ComboGenerator::NumCombos()
{
	uint64 numCombos = 1;

	for ( int k = 0, kEnd = m_arrDefines.Size(); k < kEnd; ++ k )
	{
		Define const &df = m_arrDefines.Get( k );
		numCombos *= ( df.Max() - df.Min() + 1 );
	}

	return numCombos;
}

uint64 ComboGenerator::NumCombos( bool bStaticCombos )
{
	uint64 numCombos = 1;

	for ( int k = 0, kEnd = m_arrDefines.Size(); k < kEnd; ++ k )
	{
		Define const &df = m_arrDefines.Get( k );
		( df.IsStatic() == bStaticCombos ) ? numCombos *= ( df.Max() - df.Min() + 1 ) : 0;
	}

	return numCombos;
}


struct ComboEmission
{
	std::string m_sPrefix;
	std::string m_sSuffix;
} g_comboEmission;

size_t const g_lenTmpBuffer = 1 * 1024 * 1024; // 1Mb buffer for tmp storage
char g_chTmpBuffer[g_lenTmpBuffer];

void ComboGenerator::RunAllCombos( CComplexExpression const &skipExpr )
{
	// Combo numbers
	uint64 const nTotalCombos = NumCombos();

	// Get the pointers
	int * const pnValues = m_arrVarSlots.ArrayBaseForEdit();
	int * const pnValuesEnd = pnValues + m_arrVarSlots.Size();
	int *pSetValues;

	// Defines
	Define const * const pDefVars = m_arrDefines.ArrayBase();
	Define const *pSetDef;

	// Set all the variables to max values
	for ( pSetValues = pnValues, pSetDef = pDefVars;
		pSetValues < pnValuesEnd;
		++ pSetValues, ++ pSetDef )
	{
		*pSetValues = pSetDef->Max();
	}

	// Expressions distributed [0] = skips, [1] = evaluated
	uint64 nSkipEvalCounters[2] = { 0, 0 };

	// Go ahead and run the iterations
	{
		uint64 nCurrentCombo = nTotalCombos;

next_combo_iteration:
		-- nCurrentCombo;
		int const valExprSkip = skipExpr.Evaluate( this );

		++ nSkipEvalCounters[ !valExprSkip ];

		if ( valExprSkip )
		{
			// TECH NOTE: Giving performance hint to compiler to place a jump here
			// since there will be much more skips and actually less than 0.8% cases
			// will be "OnCombo" hits.
			NULL;
		}
		else
		{
			// ------- OnCombo( nCurrentCombo ); ----------
			OutputF( stderr, "%s ", g_comboEmission.m_sPrefix.data() );
			OutputF( stderr, "/DSHADERCOMBO=%d ", nCurrentCombo );

			for ( pSetValues = pnValues, pSetDef = pDefVars;
				pSetValues < pnValuesEnd;
				++ pSetValues, ++ pSetDef )
			{
				OutputF( stderr, "/D%s=%d ", pSetDef->Name(), *pSetValues );
			}

			OutputF( stderr, "%s\n", g_comboEmission.m_sSuffix.data() );
			// ------- end of OnCombo ---------------------
		}

		// Do a next iteration
		for ( pSetValues = pnValues, pSetDef = pDefVars;
			pSetValues < pnValuesEnd;
			++ pSetValues, ++ pSetDef )
		{
			if ( -- *pSetValues >= pSetDef->Min() )
				goto next_combo_iteration;

			*pSetValues = pSetDef->Max();
		}
	}

	OutputF( stdout, "Generated %d combos: %d evaluated, %d skipped.\n", nTotalCombos, nSkipEvalCounters[1], nSkipEvalCounters[0] );
}


namespace ConfigurationProcessing
{
	class CfgEntry
	{
	public:
		CfgEntry() : m_szName( "" ), m_szShaderSrc( "" ), m_pCg( NULL ), m_pExpr( NULL ) { memset( &m_eiInfo, 0, sizeof( m_eiInfo ) ); }
		static void Destroy( CfgEntry const &x ) { delete x.m_pCg; delete x.m_pExpr; }

	public:
		bool operator < ( CfgEntry const &x ) const { return m_pCg->NumCombos() < x.m_pCg->NumCombos(); }

	public:
		char const *m_szName;
		char const *m_szShaderSrc;
		ComboGenerator *m_pCg;
		CComplexExpression *m_pExpr;
		std::string m_sPrefix;
		std::string m_sSuffix;

		CfgProcessor::CfgEntryInfo m_eiInfo;
	};

	QuickStrUnique s_uniqueSections, s_strPool;
	std::multiset< CfgEntry > s_setEntries;

	class ComboHandleImpl : public IEvaluationContext
	{
	public:
		uint64 m_iTotalCommand;
		uint64 m_iComboNumber;
		uint64 m_numCombos;
		CfgEntry const *m_pEntry;

	public:
		ComboHandleImpl() : m_iTotalCommand( 0 ), m_iComboNumber( 0 ), m_numCombos( 0 ), m_pEntry( NULL ) {}

		// IEvaluationContext
	public:
		QuickArray < int >		m_arrVarSlots;
	public:
		virtual int GetVariableValue( int nSlot ) { return m_arrVarSlots.Get( nSlot ); };
		virtual char const * GetVariableName( int nSlot ) { return m_pEntry->m_pCg->GetVariableName( nSlot ); };
		virtual int GetVariableSlot( char const *szVariableName ) { return m_pEntry->m_pCg->GetVariableSlot( szVariableName ); };

		// External implementation
	public:
		bool Initialize( uint64 iTotalCommand, const CfgEntry *pEntry );
		bool AdvanceCommands( uint64 &riAdvanceMore );
		bool NextNotSkipped( uint64 iTotalCommand );
		bool IsSkipped( void ) { return ( m_pEntry->m_pExpr->Evaluate( this ) != 0 ); }
		void FormatCommand( char *pchBuffer );
	};

	QuickMap < uint64, ComboHandleImpl > s_mapComboCommands;

	bool ComboHandleImpl::Initialize( uint64 iTotalCommand, const CfgEntry *pEntry )
	{
		m_iTotalCommand = iTotalCommand;
		m_pEntry = pEntry;
		m_numCombos = m_pEntry->m_pCg->NumCombos();

		// Defines
		Define const * const pDefVars = m_pEntry->m_pCg->GetDefinesBase();
		Define const * const pDefVarsEnd = m_pEntry->m_pCg->GetDefinesEnd();
		Define const *pSetDef;

		// Set all the variables to max values
		for ( pSetDef = pDefVars;
			  pSetDef < pDefVarsEnd;
			  ++ pSetDef )
		{
			m_arrVarSlots.Append( pSetDef->Max() );
		}

		m_iComboNumber = m_numCombos - 1;
		return true;
	}

	bool ComboHandleImpl::AdvanceCommands( uint64 &riAdvanceMore )
	{
		if ( !riAdvanceMore )
			return true;

		// Get the pointers
		int * const pnValues = m_arrVarSlots.ArrayBaseForEdit();
		int * const pnValuesEnd = pnValues + m_arrVarSlots.Size();
		int *pSetValues;

		// Defines
		Define const * const pDefVars = m_pEntry->m_pCg->GetDefinesBase();
		Define const *pSetDef;

		if ( m_iComboNumber < riAdvanceMore )
		{
			riAdvanceMore -= m_iComboNumber;
			return false;
		}

		// Do the advance
		m_iTotalCommand += riAdvanceMore;
		m_iComboNumber -= riAdvanceMore;
		for ( pSetValues = pnValues, pSetDef = pDefVars;
		  	  ( pSetValues < pnValuesEnd ) && ( riAdvanceMore > 0 );
			  ++ pSetValues, ++ pSetDef )
		{
			riAdvanceMore += ( pSetDef->Max() - *pSetValues );
			*pSetValues = pSetDef->Max();
			
			int iInterval = ( pSetDef->Max() - pSetDef->Min() + 1 );
			*pSetValues -= int( riAdvanceMore % iInterval );
			riAdvanceMore /= iInterval;
		}

		return true;
	}

	bool ComboHandleImpl::NextNotSkipped( uint64 iTotalCommand )
	{
		// Get the pointers
		int * const pnValues = m_arrVarSlots.ArrayBaseForEdit();
		int * const pnValuesEnd = pnValues + m_arrVarSlots.Size();
		int *pSetValues;

		// Defines
		Define const * const pDefVars = m_pEntry->m_pCg->GetDefinesBase();
		Define const *pSetDef;

		// Go ahead and run the iterations
		{
next_combo_iteration:
			if ( m_iTotalCommand + 1 >= iTotalCommand ||
				 !m_iComboNumber )
				return false;

			-- m_iComboNumber;
			++ m_iTotalCommand;

			// Do a next iteration
			for ( pSetValues = pnValues, pSetDef = pDefVars;
				pSetValues < pnValuesEnd;
				++ pSetValues, ++ pSetDef )
			{
				if ( -- *pSetValues >= pSetDef->Min() )
					goto have_combo_iteration;

				*pSetValues = pSetDef->Max();
			}

			return false;

have_combo_iteration:
			if ( m_pEntry->m_pExpr->Evaluate( this ) )
				goto next_combo_iteration;
			else
				return true;
		}
	}

	void ComboHandleImpl::FormatCommand( char *pchBuffer )
	{
		// Get the pointers
		int * const pnValues = m_arrVarSlots.ArrayBaseForEdit();
		int * const pnValuesEnd = pnValues + m_arrVarSlots.Size();
		int *pSetValues;

		// Defines
		Define const * const pDefVars = m_pEntry->m_pCg->GetDefinesBase();
		Define const *pSetDef;
		
		{
			// ------- OnCombo( nCurrentCombo ); ----------
			sprintf( pchBuffer, "%s ", m_pEntry->m_sPrefix.data() );
			pchBuffer += strlen( pchBuffer );

			sprintf( pchBuffer, "/DSHADERCOMBO=%llu ", m_iComboNumber );
			pchBuffer += strlen( pchBuffer );

			for ( pSetValues = pnValues, pSetDef = pDefVars;
				  pSetValues < pnValuesEnd;
				  ++ pSetValues, ++ pSetDef )
			{
				sprintf( pchBuffer, "/D%s=%d ", pSetDef->Name(), *pSetValues );
				pchBuffer += strlen( pchBuffer );
			}

			sprintf( pchBuffer, "%s\n", m_pEntry->m_sSuffix.data() );
			pchBuffer += strlen( pchBuffer );
			// ------- end of OnCombo ---------------------
		}
	}

	struct CAutoDestroyEntries {
		~CAutoDestroyEntries( void ) {
			std::for_each( s_setEntries.begin(), s_setEntries.end(), CfgEntry::Destroy );
		}
	} s_autoDestroyEntries;

	
	FILE *& GetInputStream( FILE * )
	{
		static FILE *s_fInput = stdin;
		return s_fInput;
	}

	CUtlInplaceBuffer *& GetInputStream( CUtlInplaceBuffer * )
	{
		static CUtlInplaceBuffer *s_fInput = NULL;
		return s_fInput;
	}

	char * GetLinePtr_Private( void )
	{
		if ( CUtlInplaceBuffer *pUtlBuffer = GetInputStream( ( CUtlInplaceBuffer * ) NULL ) )
			return pUtlBuffer->InplaceGetLinePtr();

		if ( FILE *fInput = GetInputStream( ( FILE * ) NULL ) )
			return fgets( g_chTmpBuffer, g_lenTmpBuffer, fInput );

		return NULL;
	}

	bool LineEquals( char const *sz1, char const *sz2, int nLen )
	{
		return 0 == strncmp( sz1, sz2, nLen );
	}

	char * NextLine( void )
	{
		if ( char *szLine = GetLinePtr_Private() )
		{
			// Trim trailing whitespace as well
			size_t len = ( size_t ) strlen( szLine );
			while ( len -- > 0 && V_isspace( szLine[ len ] ) )
			{
				szLine[ len ] = 0;
			}
			return szLine;
		}
		return NULL;
	}

	char * WaitFor( char const *szWaitString, int nMatchLength )
	{
		while ( char *pchResult = NextLine() )
		{
			if ( LineEquals( pchResult, szWaitString, nMatchLength ) )
				return pchResult;
		}

		return NULL;
	}

	bool ProcessSection( CfgEntry &cfge )
	{
		bool bStaticDefines;

		// Read the next line for the section src file
		if ( char *szLine = NextLine() )
		{
			cfge.m_szShaderSrc = s_strPool.AddLookup( szLine );
		}

		if ( char *szLine = WaitFor( "#DEFINES-", 9 ) )
		{
			bStaticDefines = ( szLine[9] == 'S' );
		}
		else
			return false;

		// Combo generator
		ComboGenerator &cg = *( cfge.m_pCg = new ComboGenerator );
		CComplexExpression &exprSkip = *( cfge.m_pExpr = new CComplexExpression( &cg ) );

		// #DEFINES:
		while ( char *szLine = NextLine() )
		{
			if ( LineEquals( szLine, "#SKIP", 5 ) )
				break;

			// static defines
			if ( LineEquals( szLine, "#DEFINES-", 9 ) )
			{
				bStaticDefines = ( szLine[9] == 'S' );
				continue;
			}

			while ( *szLine && V_isspace(*szLine) )
			{
				++ szLine;
			}

			// Find the eq
			char *pchEq = strchr( szLine, '=' );
			if ( !pchEq )
				continue;

			char *pchStartRange = pchEq + 1;
			*pchEq = 0;
			while ( -- pchEq >= szLine &&
				V_isspace( *pchEq ) )
			{
				*pchEq = 0;
			}
			if ( !*szLine )
				continue;

			// Find the end of range
			char *pchEndRange = strstr( pchStartRange, ".." );
			if ( !pchEndRange )
				continue;
			pchEndRange += 2;

			// Create the define
			Define df( szLine, atoi( pchStartRange ), atoi( pchEndRange ), bStaticDefines );
			if ( df.Max() < df.Min() )
				continue;

			// Add the define
			cg.AddDefine( df );
		}

		// #SKIP:
		if ( char *szLine = NextLine() )
		{
			exprSkip.Parse( szLine );
		}
		else
			return false;

		// #COMMAND:
		if ( !WaitFor( "#COMMAND", 8 ) )
			return false;
		if ( char *szLine = NextLine() )
			cfge.m_sPrefix = szLine;
		if ( char *szLine = NextLine() )
			cfge.m_sSuffix = szLine;

		// #END
		if ( !WaitFor( "#END", 4 ) )
			return false;

		return true;
	}

	void UnrollSectionCommands( CfgEntry const &cfge )
	{
		// Execute the combo computation
		//
		//

		g_comboEmission.m_sPrefix = cfge.m_sPrefix;
		g_comboEmission.m_sSuffix = cfge.m_sSuffix;

		OutputF( stdout, "Preparing %d combos for %s...\n", cfge.m_pCg->NumCombos(), cfge.m_szName );
		OutputF( stderr, "#%s\n", cfge.m_szName );

		time_t tt_start = time( NULL );
		cfge.m_pCg->RunAllCombos( *cfge.m_pExpr );
		time_t tt_end = time( NULL );

		OutputF( stderr, "#%s\n", cfge.m_szName );
		OutputF( stdout, "Prepared %s combos. %d sec.\n", cfge.m_szName, ( int ) difftime( tt_end, tt_start ) );

		g_comboEmission.m_sPrefix = "";
		g_comboEmission.m_sSuffix = "";
	}

	void RunSection( CfgEntry const &cfge )
	{
		// Execute the combo computation
		//
		//

		g_comboEmission.m_sPrefix = cfge.m_sPrefix;
		g_comboEmission.m_sSuffix = cfge.m_sSuffix;

		OutputF( stdout, "Preparing %d combos for %s...\n", cfge.m_pCg->NumCombos(), cfge.m_szName );
		OutputF( stderr, "#%s\n", cfge.m_szName );

		time_t tt_start = time( NULL );
		cfge.m_pCg->RunAllCombos( *cfge.m_pExpr );
		time_t tt_end = time( NULL );

		OutputF( stderr, "#%s\n", cfge.m_szName );
		OutputF( stdout, "Prepared %s combos. %d sec.\n", cfge.m_szName, ( int ) difftime( tt_end, tt_start ) );

		g_comboEmission.m_sPrefix = "";
		g_comboEmission.m_sSuffix = "";
	}

	void ProcessConfiguration()
	{
		static bool s_bProcessOnce = false;

		while ( char *szLine = WaitFor( "#BEGIN", 6 ) )
		{
			if ( ' ' == szLine[6] && !s_uniqueSections.Add(szLine + 7) )
				continue;

			CfgEntry cfge;
			cfge.m_szName = s_uniqueSections.Lookup( szLine + 7 );
			ProcessSection( cfge );
			s_setEntries.insert( cfge );
		}

		uint64 nCurrentCommand = 0;
		for( std::multiset< CfgEntry >::reverse_iterator it = s_setEntries.rbegin(),
			 itEnd = s_setEntries.rend(); it != itEnd; ++ it )
		{
			// We establish a command mapping for the beginning of the entry
			ComboHandleImpl chi;
			chi.Initialize( nCurrentCommand, &*it );
			s_mapComboCommands.Append( nCurrentCommand, chi );

			// We also establish mapping by either splitting the
			// combos into 500 intervals or stepping by every 1000 combos.
			int iPartStep = ( int ) max( 1000, (int)( chi.m_numCombos / 500 ) );
			for ( uint64 iRecord = nCurrentCommand + iPartStep;
				  iRecord < nCurrentCommand + chi.m_numCombos;
				  iRecord += iPartStep )
			{
				uint64 iAdvance = iPartStep;
				chi.AdvanceCommands( iAdvance );
				s_mapComboCommands.Append( iRecord, chi );
			}

			nCurrentCommand += chi.m_numCombos;
		}

		// Establish the last command terminator
		{
			static CfgEntry s_term;
			s_term.m_eiInfo.m_iCommandStart = s_term.m_eiInfo.m_iCommandEnd = nCurrentCommand;
			s_term.m_eiInfo.m_numCombos = s_term.m_eiInfo.m_numStaticCombos = s_term.m_eiInfo.m_numDynamicCombos = 1;
			s_term.m_eiInfo.m_szName = s_term.m_eiInfo.m_szShaderFileName = "";
			ComboHandleImpl chi;
			chi.m_iTotalCommand = nCurrentCommand;
			chi.m_pEntry = &s_term;
			s_mapComboCommands.Append( nCurrentCommand, chi );
		}
	}

}; // namespace ConfigurationProcessing

/*
int main( int argc, char **argv )
{
	if ( _isatty( _fileno( stdin ) ) )
	{
		return Usage();
	}

	// Go ahead processing the configuration
	ConfigurationProcessing::ProcessConfiguration();

	return 0;
}
*/

namespace CfgProcessor
{

	typedef ConfigurationProcessing::ComboHandleImpl CPCHI_t;
	CPCHI_t * FromHandle( ComboHandle hCombo ) { return reinterpret_cast < CPCHI_t * > ( hCombo ); }
	ComboHandle AsHandle( CPCHI_t *pImpl ) { return reinterpret_cast < ComboHandle > ( pImpl ); }

void ReadConfiguration( FILE *fInputStream )
{
	CAutoPushPop < FILE * > pushInputStream( ConfigurationProcessing::GetInputStream( fInputStream ), fInputStream );
	ConfigurationProcessing::ProcessConfiguration();
}

void ReadConfiguration( CUtlInplaceBuffer *fInputStream )
{
	CAutoPushPop < CUtlInplaceBuffer * > pushInputStream( ConfigurationProcessing::GetInputStream( fInputStream ), fInputStream );
	ConfigurationProcessing::ProcessConfiguration();
}

void DescribeConfiguration( CArrayAutoPtr < CfgEntryInfo > &rarrEntries )
{
	rarrEntries.Delete();
	rarrEntries.Attach( new CfgEntryInfo [ ConfigurationProcessing::s_setEntries.size() + 1 ] );

	CfgEntryInfo *pInfo = rarrEntries.Get();
	uint64 nCurrentCommand = 0;

	for( std::multiset< ConfigurationProcessing::CfgEntry >::reverse_iterator it =
		 ConfigurationProcessing::s_setEntries.rbegin(),
		 itEnd = ConfigurationProcessing::s_setEntries.rend();
		 it != itEnd; ++ it, ++ pInfo )
	{
		ConfigurationProcessing::CfgEntry const &e = *it;

		pInfo->m_szName = e.m_szName;
		pInfo->m_szShaderFileName = e.m_szShaderSrc;

		pInfo->m_iCommandStart = nCurrentCommand;
		pInfo->m_numCombos = e.m_pCg->NumCombos();
		pInfo->m_numDynamicCombos = e.m_pCg->NumCombos( false );
		pInfo->m_numStaticCombos = e.m_pCg->NumCombos( true );
		pInfo->m_iCommandEnd = pInfo->m_iCommandStart + pInfo->m_numCombos;

		const_cast< CfgEntryInfo & > ( e.m_eiInfo ) = *pInfo;

		nCurrentCommand += pInfo->m_numCombos;
	}

	// Terminator
	memset( pInfo, 0, sizeof( CfgEntryInfo ) );
	pInfo->m_iCommandStart = nCurrentCommand;
	pInfo->m_iCommandEnd = nCurrentCommand;
}

ComboHandle Combo_GetCombo( uint64 iCommandNumber )
{
	// Find earlier command
	uint64 iCommandFound = iCommandNumber;
	CPCHI_t emptyCPCHI;
	CPCHI_t const &chiFound = ConfigurationProcessing::s_mapComboCommands.GetLessOrEq( iCommandFound, emptyCPCHI );

	if ( chiFound.m_iTotalCommand < 0 ||
		 chiFound.m_iTotalCommand > iCommandNumber )
		 return NULL;

	// Advance the handle as needed
	CPCHI_t *pImpl = new CPCHI_t( chiFound );
	
	uint64 iCommandFoundAdvance = iCommandNumber - iCommandFound;
	pImpl->AdvanceCommands( iCommandFoundAdvance );

	return AsHandle( pImpl );
}

ComboHandle Combo_GetNext( uint64 &riCommandNumber, ComboHandle &rhCombo, uint64 iCommandEnd )
{
	// Combo handle implementation
	CPCHI_t *pImpl = FromHandle( rhCombo );

	if ( !rhCombo )
	{
		// We don't have a combo handle that corresponds to the command

		// Find earlier command
		uint64 iCommandFound = riCommandNumber;
		CPCHI_t emptyCPCHI;
		CPCHI_t const &chiFound = ConfigurationProcessing::s_mapComboCommands.GetLessOrEq( iCommandFound, emptyCPCHI );

		if ( !chiFound.m_pEntry ||
			 !chiFound.m_pEntry->m_pCg ||
			 !chiFound.m_pEntry->m_pExpr ||
			 chiFound.m_iTotalCommand < 0 ||
			 chiFound.m_iTotalCommand > riCommandNumber )
			 return NULL;

		// Advance the handle as needed
		pImpl = new CPCHI_t( chiFound );
		rhCombo = AsHandle( pImpl );
		
		uint64 iCommandFoundAdvance = riCommandNumber - iCommandFound;
		pImpl->AdvanceCommands( iCommandFoundAdvance );

		if ( !pImpl->IsSkipped() )
			return rhCombo;
	}

	for ( ; ; )
	{
		// We have the combo handle now
		if ( pImpl->NextNotSkipped( iCommandEnd ) )
		{
			riCommandNumber = pImpl->m_iTotalCommand;
			return rhCombo;
		}

		// We failed to get the next combo command (out of range)
		if ( pImpl->m_iTotalCommand + 1 >= iCommandEnd )
		{
			delete pImpl;
			rhCombo = NULL;
			riCommandNumber = iCommandEnd;
			return NULL;
		}

		// Otherwise we just have to obtain the next combo handle
		riCommandNumber = pImpl->m_iTotalCommand + 1;

		// Delete the old combo handle
		delete pImpl;
		rhCombo = NULL;

		// Retrieve the next combo handle data
		uint64 iCommandLookup = riCommandNumber;
		CPCHI_t emptyCPCHI;
		CPCHI_t const &chiNext = ConfigurationProcessing::s_mapComboCommands.GetLessOrEq( iCommandLookup, emptyCPCHI );
		Assert( ( iCommandLookup == riCommandNumber ) && ( chiNext.m_pEntry ) );

		// Set up the new combo handle
		pImpl = new CPCHI_t( chiNext );
		rhCombo = AsHandle( pImpl );
		
		if ( !pImpl->IsSkipped() )
			return rhCombo;
	}
}

void Combo_FormatCommand( ComboHandle hCombo, char *pchBuffer )
{
	CPCHI_t *pImpl = FromHandle( hCombo );
	pImpl->FormatCommand( pchBuffer );
}

uint64 Combo_GetCommandNum( ComboHandle hCombo )
{
	if ( CPCHI_t *pImpl = FromHandle( hCombo ) )
		return pImpl->m_iTotalCommand;
	else
		return ~uint64(0);
}

uint64 Combo_GetComboNum( ComboHandle hCombo )
{
	if ( CPCHI_t *pImpl = FromHandle( hCombo ) )
		return pImpl->m_iComboNumber;
	else
		return ~uint64(0);
}

CfgEntryInfo const *Combo_GetEntryInfo( ComboHandle hCombo )
{
	if ( CPCHI_t *pImpl = FromHandle( hCombo ) )
		return &pImpl->m_pEntry->m_eiInfo;
	else
		return NULL;
}

ComboHandle Combo_Alloc( ComboHandle hComboCopyFrom )
{
	if ( hComboCopyFrom )
		return AsHandle( new CPCHI_t( * FromHandle( hComboCopyFrom ) ) );
	else
		return AsHandle( new CPCHI_t );
}

void Combo_Assign( ComboHandle hComboDst, ComboHandle hComboSrc )
{
	Assert( hComboDst );
	* FromHandle( hComboDst ) = * FromHandle( hComboSrc );
}

void Combo_Free( ComboHandle &rhComboFree )
{
	delete FromHandle( rhComboFree );
	rhComboFree = NULL;
}

}; // namespace CfgProcessor
