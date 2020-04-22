//========= Copyright Valve Corporation, All rights reserved. ============//
//
// The expression operator class - scalar math calculator
// for a good list of operators and simple functions, see:
//   \\fileserver\user\MarcS\boxweb\aliveDistLite\v4.2.0\doc\alive\functions.txt
// (although we'll want to implement elerp as the standard 3x^2 - 2x^3 with rescale)
//
//=============================================================================
#include "movieobjects/dmeexpressionoperator.h"
#include "movieobjects_interfaces.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "datamodel/dmattribute.h"
#include "mathlib/noise.h"
#include "mathlib/vector.h"
#include <ctype.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//-----------------------------------------------------------------------------
// Parsing helper methods
//-----------------------------------------------------------------------------
bool ParseLiteral( const char *&expr, float &value )
{
	const char *startExpr = expr;
	value = ( float )strtod( startExpr, const_cast< char** >( &expr ) );
	return ( startExpr != expr );
}

bool ParseString( const char *&expr, const char *str )
{
	const char *startExpr = expr;
	while ( ( *expr == ' ' ) || ( *expr == '\t' ) )
		expr++; // skip whitespace

	expr = StringAfterPrefix( expr, str );
	if ( expr )
		return true;

	expr = startExpr;
	return false;
}

bool ParseStringList( const char *&expr, const char **pOps, int &nOp )
{
	while ( nOp-- )
	{
		if ( ParseString( expr, pOps[ nOp ] ) )
			return true;
	}
	return false;
}

bool ParseStringList( const char *&expr, const CUtlVector< CUtlString > &strings, int &nOp )
{
	while ( nOp-- )
	{
		if ( ParseString( expr, strings[ nOp ] ) )
			return true;
	}
	return false;
}

int FindString( const CUtlVector< CUtlString > &strings, const char *str )
{
	uint sn = strings.Count();
	for ( uint si = 0; si < sn; ++si )
	{
		if ( !Q_strcmp( str, strings[ si ] ) )
			return si;
	}
	return -1;
}

class ParseState_t
{
public:
	ParseState_t( const CUtlStack<float> &stack, const char *expr )
		: m_stacksize( stack.Count() ), m_startingExpr( expr ) {}
	void Reset( CUtlStack<float> &stack, const char *&expr )
	{
		Assert( m_stacksize <= stack.Count() );
		stack.PopMultiple( stack.Count() - m_stacksize );
		expr = m_startingExpr;
	}

private:
	int m_stacksize;
	const char* m_startingExpr;
};




void CExpressionCalculator::SetVariable( int nVariableIndex, float value )
{
	m_varValues[ nVariableIndex ] = value;
}

void CExpressionCalculator::SetVariable( const char *var, float value )
{
	int vi = FindString( m_varNames, var );
	if ( vi >= 0 )
	{
		m_varValues[ vi ] = value;
	}
	else
	{
		m_varNames.AddToTail( var );
		m_varValues.AddToTail( value );
	}
}

int CExpressionCalculator::FindVariableIndex( const char *var )
{
	return FindString( m_varNames, var );
}

bool CExpressionCalculator::Evaluate( float &value )
{
	m_bIsBuildingArgumentList = false;
	m_stack.PopMultiple( m_stack.Count() );
	const char *pExpr = m_expr.Get();
	bool success = ParseExpr( pExpr );
	if ( success && m_stack.Count() == 1 )
	{
		value = m_stack.Top();
		return true;
	}

	value = 0.0f;
	return false;
}


//-----------------------------------------------------------------------------
// Builds a list of variable names from the expression
//-----------------------------------------------------------------------------
bool CExpressionCalculator::BuildVariableListFromExpression( )
{
	m_bIsBuildingArgumentList = true;
	m_stack.PopMultiple( m_stack.Count() );
	const char *pExpr = m_expr.Get();
	bool bSuccess = ParseExpr( pExpr );
	m_bIsBuildingArgumentList = false;
	if ( !bSuccess || m_stack.Count() != 1 )
	{
		m_varNames.RemoveAll();
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Iterate over variables
//-----------------------------------------------------------------------------
int CExpressionCalculator::VariableCount()
{
	return m_varNames.Count();
}

const char *CExpressionCalculator::VariableName( int nIndex )
{
	return m_varNames[nIndex];
}



bool CExpressionCalculator::ParseExpr( const char *&expr )
{
	return ( expr != NULL ) && ParseConditional( expr );
}

bool CExpressionCalculator::ParseConditional( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseOr( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	ParseState_t ps1( m_stack, expr );
	if ( ParseString( expr, "?" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ":" ) &&
		ParseExpr( expr ) )
	{
		float f3 = m_stack.Top();
		m_stack.Pop();
		float f2 = m_stack.Top();
		m_stack.Pop();
		float f1 = m_stack.Top();
		m_stack.Pop();
		m_stack.Push( f1 != 0.0f ? f2 : f3 );
		return true; // and matched
	}
	ps1.Reset( m_stack, expr );
	return true; // equality (or lower) matched
}

bool CExpressionCalculator::ParseOr( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseAnd( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	ParseState_t ps1( m_stack, expr );
	if ( ParseString( expr, "||" ) &&
		ParseOr( expr ) )
	{
		float f2 = m_stack.Top();
		m_stack.Pop();
		float f1 = m_stack.Top();
		m_stack.Pop();
		m_stack.Push( ( f1 != 0.0f ) || ( f2 != 0.0f ) ? 1 : 0 );
		return true; // and matched
	}
	ps1.Reset( m_stack, expr );
	return true; // equality (or lower) matched
}

bool CExpressionCalculator::ParseAnd( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseEquality( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	ParseState_t ps1( m_stack, expr );
	if ( ParseString( expr, "&&" ) &&
		ParseAnd( expr ) )
	{
		float f2 = m_stack.Top();
		m_stack.Pop();
		float f1 = m_stack.Top();
		m_stack.Pop();
		m_stack.Push( ( f1 != 0.0f ) && ( f2 != 0.0f ) ? 1 : 0 );
		return true; // and matched
	}
	ps1.Reset( m_stack, expr );
	return true; // equality (or lower) matched
}

bool CExpressionCalculator::ParseEquality( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseLessGreater( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	const char *pOps[] = { "==", "!=" };
	int nOp = 2;

	ParseState_t ps1( m_stack, expr );
	if ( ParseStringList( expr, pOps, nOp ) &&
		ParseEquality( expr ) )
	{
		float f2 = m_stack.Top();
		m_stack.Pop();
		float f1 = m_stack.Top();
		m_stack.Pop();
		switch ( nOp )
		{
		case 0: // ==
			m_stack.Push( f1 == f2 ? 1 : 0 );
			break;
		case 1: // !=
			m_stack.Push( f1 != f2 ? 1 : 0 );
			break;
		}
		return true; // equality matched
	}
	ps1.Reset( m_stack, expr );
	return true; // lessgreater (or lower) matched
}

bool CExpressionCalculator::ParseLessGreater( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseAddSub( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	const char *pOps[] = { "<", ">", "<=", ">=" };
	int nOp = 4;

	ParseState_t ps1( m_stack, expr );
	if ( ParseStringList( expr, pOps, nOp ) &&
		ParseLessGreater( expr ) )
	{
		float f2 = m_stack.Top();
		m_stack.Pop();
		float f1 = m_stack.Top();
		m_stack.Pop();
		switch ( nOp )
		{
		case 0: // <
			m_stack.Push( f1 < f2 ? 1 : 0 );
			break;
		case 1: // >
			m_stack.Push( f1 > f2 ? 1 : 0 );
			break;
		case 2: // <=
			m_stack.Push( f1 <= f2 ? 1 : 0 );
			break;
		case 3: // >=
			m_stack.Push( f1 >= f2 ? 1 : 0 );
			break;
		}
		return true; // inequality matched
	}
	ps1.Reset( m_stack, expr );
	return true; // addsub (or lower) matched
}

bool CExpressionCalculator::ParseAddSub( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseDivMul( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	const char *pOps[] = { "+", "-" };
	int nOp = 2;

	ParseState_t ps1( m_stack, expr );
	if ( ParseStringList( expr, pOps, nOp ) &&
		ParseAddSub( expr ) )
	{
		float f2 = m_stack.Top();
		m_stack.Pop();
		float f1 = m_stack.Top();
		m_stack.Pop();
		switch ( nOp )
		{
		case 0: // +
			m_stack.Push( f1 + f2 );
			break;
		case 1: // -
			m_stack.Push( f1 - f2 );
			break;
		}
		return true; // addsub matched
	}
	ps1.Reset( m_stack, expr );
	return true; // divmul (or lower) matched
}

bool CExpressionCalculator::ParseDivMul( const char *&expr )
{
	ParseState_t ps0( m_stack, expr );
	if ( !ParseUnary( expr ) )
	{
		ps0.Reset( m_stack, expr );
		return false; // nothing matched
	}

	const char *pOps[] = { "*", "/", "%" };
	int nOp = 3;

	ParseState_t ps1( m_stack, expr );
	if ( ParseStringList( expr, pOps, nOp ) &&
		ParseDivMul( expr ) )
	{
		float f2 = m_stack.Top();
		m_stack.Pop();
		float f1 = m_stack.Top();
		m_stack.Pop();
		switch ( nOp )
		{
		case 0: // *
			m_stack.Push( f1 * f2 );
			break;
		case 1: // /
			m_stack.Push( f1 / f2 );
			break;
		case 2: // %
			m_stack.Push( fmod( f1, f2 ) );
			break;
		}
		return true; // divmul matched
	}
	ps1.Reset( m_stack, expr );
	return true; // unary (or lower) matched
}

bool CExpressionCalculator::ParseUnary( const char *&expr )
{
	ParseState_t ps( m_stack, expr );

	const char *pOps[] = { "+", "-", "!" };
	int nOp = 3;

	if ( ParseStringList( expr, pOps, nOp ) &&
		ParseUnary( expr ) )
	{
		float f1 = m_stack.Top();
		m_stack.Pop();
		switch ( nOp )
		{
		case 0: // +
			m_stack.Push( f1 );
			break;
		case 1: // -
			m_stack.Push( -f1 );
			break;
		case 2: // !
			m_stack.Push( f1 == 0 ? 1 : 0 );
			break;
		}
		return true;
	}

	ps.Reset( m_stack, expr );
	if ( ParsePrimary( expr ) )
		return true;

	ps.Reset( m_stack, expr );
	return false;
}

bool CExpressionCalculator::ParsePrimary( const char *&expr )
{
	ParseState_t ps( m_stack, expr );

	float value = 0.0f;
	if ( ParseLiteral( expr, value ) )
	{
		m_stack.Push( value );
		return true;
	}

	ps.Reset( m_stack, expr );
	int nVar = m_varNames.Count();
	if ( ParseStringList( expr, m_varNames, nVar) )
	{
		m_stack.Push( m_varValues[ nVar ] );
		return true;
	}

	ps.Reset( m_stack, expr );
	if ( ParseString( expr, "(" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ")" ) )
	{
		return true;
	}

	ps.Reset( m_stack, expr );
	if ( Parse1ArgFunc( expr ) ||
		Parse2ArgFunc( expr ) ||
		Parse3ArgFunc( expr ) ||
//		Parse4ArgFunc( expr ) ||
		Parse5ArgFunc( expr ) )
	{
		return true;
	}

	// If we're parsing it to discover names of variable names, add them here
	if ( !m_bIsBuildingArgumentList )
		return false;

	// Variables can't start with a number
	if ( isdigit( *expr ) )
		return false;

	const char *pStart = expr;
	while ( isalnum( *expr ) || *expr == '_' )
	{
		++expr;
	}

	int nLen = (size_t)expr - (size_t)pStart;
	char *pVariableName = (char*)_alloca( nLen+1 );
	memcpy( pVariableName, pStart, nLen );
	pVariableName[nLen] = 0;

	SetVariable( pVariableName, 0.0f );
	m_stack.Push( 0.0f );
	return true;
}

/*
dtor(d) : converts degrees to radians
rtod(r) : converts radians to degrees

abs(a)     : absolute value
floor(a)   : rounds down to the nearest integer
ceiling(a) : rounds up to the nearest integer
round(a)   : rounds to the nearest integer
sgn(a)     : if a < 0 returns -1 else 1
sqr(a)     : returns a * a
sqrt(a)    : returns sqrt(a)

sin(a)     : sin(a), a is in degrees
asin(a)    : asin(a) returns degrees
cos(a)     : cos(a), a is in degrees
acos(a)    : acos(a) returns degrees
tan(a)     : tan(a), a is in degrees

exp(a)   : returns the exponential function of a
log(a)   : returns the natural logaritm of a
*/
bool CExpressionCalculator::Parse1ArgFunc( const char *&expr )
{
	ParseState_t ps( m_stack, expr );

	const char *pFuncs[] = 
	{
		"abs", "sqr", "sqrt", "sin", "asin", "cos", "acos", "tan",
		"exp", "log", "dtor", "rtod", "floor", "ceiling", "round", "sign"
	};
	int nFunc = 16;

	if ( ParseStringList( expr, pFuncs, nFunc ) &&
		ParseString( expr, "(" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ")" ) )
	{
		float f1 = m_stack.Top();
		m_stack.Pop();
		switch ( nFunc )
		{
		case 0: // abs
			m_stack.Push( fabs( f1 ) );
			break;
		case 1: // sqr
			m_stack.Push( f1 * f1 );
			break;
		case 2: // sqrt
			m_stack.Push( sqrt( f1 ) );
			break;
		case 3: // sin
			m_stack.Push( sin( f1 ) );
			break;
		case 4: // asin
			m_stack.Push( asin( f1 ) );
			break;
		case 5: // cos
			m_stack.Push( cos( f1 ) );
			break;
		case 6: // acos
			m_stack.Push( acos( f1 ) );
			break;
		case 7: // tan
			m_stack.Push( tan( f1 ) );
			break;
		case 8: // exp
			m_stack.Push( exp( f1 ) );
			break;
		case 9: // log
			m_stack.Push( log( f1 ) );
			break;
		case 10: // dtor
			m_stack.Push( DEG2RAD( f1 ) );
			break;
		case 11: // rtod
			m_stack.Push( RAD2DEG( f1 ) );
			break;
		case 12: // floor
			m_stack.Push( floor( f1 ) );
			break;
		case 13: // ceiling
			m_stack.Push( ceil( f1 ) );
			break;
		case 14: // round
			m_stack.Push( floor( f1 + 0.5f ) );
			break;
		case 15: // sign
			m_stack.Push( f1 >= 0.0f ? 1.0f : -1.0f );
			break;
		}
		return true;
	}
	return false;
}

/*
min(a,b)     : if a<b returns a else b
max(a,b)     : if a>b returns a else b
atan2(a,b) : atan2(a/b) returns degrees
pow(a,b) : function returns a raised to the power of b
*/
bool CExpressionCalculator::Parse2ArgFunc( const char *&expr )
{
	ParseState_t ps( m_stack, expr );

	const char *pFuncs[] = { "min", "max", "atan2", "pow" };
	int nFunc = 4;

	if ( ParseStringList( expr, pFuncs, nFunc ) &&
		ParseString( expr, "(" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ")" ) )
	{
		float f2 = m_stack.Top();
		m_stack.Pop();
		float f1 = m_stack.Top();
		m_stack.Pop();
		switch ( nFunc )
		{
		case 0: // min
			m_stack.Push( min( f1, f2 ) );
			break;
		case 1: // max
			m_stack.Push( max( f1, f2 ) );
			break;
		case 2: // atan2
			m_stack.Push( atan2( f1, f2 ) );
			break;
		case 3: // pow
			m_stack.Push( pow( f1, f2 ) );
			break;
		}
		return true;
	}
	return false;
}

/*
inrange(x,a,b) : if x is between a and b, returns 1 else returns 0
clamp(x,a,b)   : see bound() above

ramp(value,a,b)        : returns 0 -> 1 as value goes from a to b
lerp(factor,a,b)       : returns a -> b as value goes from 0 to 1

cramp(value,a,b)        : clamp(ramp(value,a,b),0,1)
clerp(factor,a,b)       : clamp(lerp(factor,a,b),a,b)

elerp(x,a,b)         : ramp( 3*x*x - 2*x*x*x, a, b)
//elerp(factor,a,b)    : lerp(lerp(sind(clerp(factor,-90,90)),0.5,1.0),a,b)

noise(a,b,c) : { solid noise pattern (improved perlin noise) indexed with three numbers }
*/

float ramp( float x, float a, float b )
{
	return ( x - a ) / ( b - a );
}

float lerp( float x, float a, float b )
{
	return a + x * ( b - a );
}

float smoothstep( float x )
{
	return 3*x*x - 2*x*x*x;
}

bool CExpressionCalculator::Parse3ArgFunc( const char *&expr )
{
	ParseState_t ps( m_stack, expr );

	const char *pFuncs[] = { "inrange", "clamp", "ramp", "lerp", "cramp", "clerp", "elerp", "noise" };
	int nFunc = 8;

	if ( ParseStringList( expr, pFuncs, nFunc ) &&
		ParseString( expr, "(" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ")" ) )
	{
		float f3 = m_stack.Top();
		m_stack.Pop();
		float f2 = m_stack.Top();
		m_stack.Pop();
		float f1 = m_stack.Top();
		m_stack.Pop();
		switch ( nFunc )
		{
		case 0: // inrange
			m_stack.Push( ( f1 >= f2 ) && ( f1 <= f3 ) ? 1.0f : 0.0f );
			break;
		case 1: // clamp
			m_stack.Push( clamp( f1, f2, f3 ) );
			break;
		case 2: // ramp
			m_stack.Push( ramp( f1, f2, f3 ) );
			break;
		case 3: // lerp
			m_stack.Push( lerp( f1, f2, f3 ) );
			break;
		case 4: // cramp
			m_stack.Push( clamp( ramp( f1, f2, f3 ), 0, 1 ) );
			break;
		case 5: // clerp
			m_stack.Push( clamp( lerp( f1, f2, f3 ), f2, f3 ) );
			break;
		case 6: // elerp
			m_stack.Push( lerp( smoothstep( f1 ), f2, f3 ) );
			break;
		case 7: // noise
			m_stack.Push( ImprovedPerlinNoise( Vector( f1, f2, f3 ) ) );
			break;
		}
		return true;
	}
	return false;
}

//bool CExpressionCalculator::Parse4ArgFunc( const char *&expr );

/*
rescale (X,Xa,Xb,Ya,Yb) : lerp(ramp(X,Xa,Xb),Ya,Yb)
crescale(X,Xa,Xb,Ya,Yb) : clamp(rescale(X,Xa,Xb,Ya,Yb),Ya,Yb)
*/
float rescale( float x, float a, float b, float c, float d )
{
	return lerp( ramp( x, a, b ), c, d );
}

bool CExpressionCalculator::Parse5ArgFunc( const char *&expr )
{
	ParseState_t ps( m_stack, expr );

	const char *pFuncs[] = { "rescale", "crescale" };
	int nFunc = 2;

	if ( ParseStringList( expr, pFuncs, nFunc ) &&
		ParseString( expr, "(" ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, "," ) &&
		ParseExpr( expr ) &&
		ParseString( expr, ")" ) )
	{
		float f5 = m_stack.Top();
		m_stack.Pop();
		float f4 = m_stack.Top();
		m_stack.Pop();
		float f3 = m_stack.Top();
		m_stack.Pop();
		float f2 = m_stack.Top();
		m_stack.Pop();
		float f1 = m_stack.Top();
		m_stack.Pop();
		switch ( nFunc )
		{
		case 0: // rescale
			m_stack.Push( rescale( f1, f2, f3, f4, f5 ) );
			break;
		case 1: // crescale
			m_stack.Push( clamp( rescale( f1, f2, f3, f4, f5 ), f4, f5 ) );
			break;
		}
		return true;
	}
	return false;
}

void TestCalculator( const char *expr, float answer )
{
	CExpressionCalculator calc( expr );
	float result = 0.0f;

#ifdef DBGFLAG_ASSERT
	bool success =
#endif
		calc.Evaluate( result );
	Assert( success && ( result == answer ) );
}

void TestCalculator( const char *expr, float answer, const char *var, float value )
{
	CExpressionCalculator calc( expr );
	calc.SetVariable( var, value );
	float result = 0.0f;

#ifdef DBGFLAG_ASSERT
	bool success =
#endif
		calc.Evaluate( result );
	Assert( success && ( result == answer ) );
}

void TestCalculator()
{
//	TestCalculator( "-1", 1 );
	TestCalculator( "2 * 3 + 4", 10 );
	TestCalculator( "2 + 3 * 4", 14 );
	TestCalculator( "2 * 3 * 4", 24 );
	TestCalculator( "2 * -3 + 4", -2 );
	TestCalculator( "12.0 / 2.0", 6 );
	TestCalculator( "(2*3)+4", 10 );
	TestCalculator( "( 1 + 2 ) / (1+2)", 1 );
	TestCalculator( "(((5)))", 5 );
	TestCalculator( "--5", 5 );
	TestCalculator( "3.5 % 2", 1.5 );
	TestCalculator( "1e-2", 0.01 );
	TestCalculator( "9 == ( 3 * ( 1 + 2 ) )", 1 );
	TestCalculator( "9 != ( 3 * ( 1 + 2 ) )", 0 );
	TestCalculator( "9 <= ( 3 * ( 1 + 2 ) )", 1 );
	TestCalculator( "9 < ( 3 * ( 1 + 2 ) )", 0 );
	TestCalculator( "9 < 3", 0 );
	TestCalculator( "10 >= 5", 1 );
//	TestCalculator( "9 < ( 3 * ( 2 + 2 ) )", 0 );
	TestCalculator( "x + 1", 5, "x", 4 );
	TestCalculator( "pi - 3.14159", 0, "pi", 3.14159 );
//	TestCalculator( "pi / 2", 0, "pi", 3.14159 );
	TestCalculator( "abs(-10)", 10 );
	TestCalculator( "sqr(-5)", 25 );
	TestCalculator( "sqrt(9)", 3 );
//	TestCalculator( "sqrt(-9)", -3 );
	TestCalculator( "pow(2,3)", 8 );
	TestCalculator( "min(abs(-4),2+3/2)", 3.5 );
	TestCalculator( "round(0.5)", 1 );
	TestCalculator( "round(0.49)", 0 );
	TestCalculator( "round(-0.5)", 0 );
	TestCalculator( "round(-0.51)", -1 );
	TestCalculator( "inrange( 5, -8, 10 )", 1 );
	TestCalculator( "inrange( 5, 5, 10 )", 1 );
	TestCalculator( "inrange( 5, 6, 10 )", 0 );
	TestCalculator( "elerp( 1/4, 0, 1 )", 3/16.0f - 1/32.0f );
	TestCalculator( "rescale( 0.5, -1, 1, 0, 100 )", 75 );
	TestCalculator( "1 > 2 ? 6 : 9", 9 );
	TestCalculator( "1 ? 1 ? 2 : 4 : 1 ? 6 : 8", 2 );
	TestCalculator( "0 ? 1 ? 2 : 4 : 1 ? 6 : 8", 6 );
	TestCalculator( "noise( 0.123, 4.56, 78.9 )", ImprovedPerlinNoise( Vector( 0.123, 4.56, 78.9 ) ) );
}


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeExpressionOperator, CDmeExpressionOperator );


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CDmeExpressionOperator::OnConstruction()
{
	m_result.Init( this, "result" );
	m_expr.Init( this, "expr" );
	m_bSpewResult.Init( this, "spewresult" );

#ifdef _DEBUG
	TestCalculator();
#endif _DEBUG
}

void CDmeExpressionOperator::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CDmeExpressionOperator::IsInputAttribute( CDmAttribute *pAttribute )
{
	const char *pName = pAttribute->GetName( );
#if 0 // skip this test, since none of these are float attributes, but leave the code as a reminder
	if ( Q_strcmp( pName, "name" ) == 0 )
		return false;

	if ( Q_strcmp( pName, "expr" ) == 0 )
		return false;
#endif

	if ( Q_strcmp( pName, "result" ) == 0 )
		return false;

	if ( pAttribute->GetType() != AT_FLOAT )
		return false;

	return true;
}

void CDmeExpressionOperator::Operate()
{
	CExpressionCalculator calc( m_expr.Get() );

	for ( CDmAttribute *pAttribute = FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( IsInputAttribute( pAttribute ) )
		{
			const char *pName = pAttribute->GetName( );
			calc.SetVariable( pName, pAttribute->GetValue< float >() );
		}
	}

	float oldValue = m_result;

	calc.Evaluate( oldValue );

	m_result = oldValue;

	if ( m_bSpewResult )
	{
		Msg( "%s = '%f'\n", GetName(), (float)m_result );
	}
}

void CDmeExpressionOperator::GetInputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	for ( CDmAttribute *pAttribute = FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		if ( IsInputAttribute( pAttribute ) )
		{
			attrs.AddToTail( pAttribute );
		}
	}

	attrs.AddToTail( m_expr.GetAttribute() );
}

void CDmeExpressionOperator::GetOutputAttributes( CUtlVector< CDmAttribute * > &attrs )
{
	attrs.AddToTail( m_result.GetAttribute() );
}


void CDmeExpressionOperator::SetSpewResult( bool state )
{
	m_bSpewResult = state;
}
