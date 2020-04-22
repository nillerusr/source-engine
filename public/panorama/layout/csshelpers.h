//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef CSSHELPERS_H
#define CSSHELPERS_H

#ifdef _WIN32
#pragma once
#endif

#include "float.h"
#include "tier0/dbg.h"
#include "tier1/utlvector.h"
#include "mathlib/vmatrix.h"
#include "mathlib/beziercurve.h"
#include "uilength.h"
#include "fillbrush.h"
#include "fmtstr.h"
#include "../text/iuitextlayout.h"
#include "panorama/layout/stylesymbol.h"

class CUtlString;
class Color;
class CUtlSymbol;

namespace panorama
{

class CTransform3D;
class CBackgroundPosition;
class CBackgroundRepeat;

const char k_rgchCSSDefaultTerm[] = { ' ', ';', '{', '}', ':' };
const char k_rgchCSSAtRuleNameTerm[] = { ' ', ';', '{', '}' };
const char k_rgchCSSSelectorTerm[] = { ';', '{', '}' };
const char k_rgchCSSValueTerm[] = { ';', '{', '}' };
const char k_rgchCSSValueTermOrEndOfString[] = { ';', '{', '}', '\0' };

//-----------------------------------------------------------------------------
// Purpose: defines 
//-----------------------------------------------------------------------------
const int k_nCSSPropertyNameMax = 128;


//-----------------------------------------------------------------------------
// Purpose: string to type routines
//-----------------------------------------------------------------------------
const char* PchNameFromEFontWeight( int nValue );
const char* PchNameFromEFontStyle( int nValue );
const char* PchNameFromETextAlign( int nValue );
const char* PchNameFromETextDecoration( int nValue );
const char *PchNameFromETextTransform( int nValue );
const char* PchNameFromEAnimationTimingFunction( int nValue );
const char *PchNameFromEHorizontalAlignment( EHorizontalAlignment eHorizontalAlignment );
const char *PchNameFromEVerticalAlignment( EVerticalAlignment eVerticalAlignment );
const char *PchNameFromEContextUIPosition( EContextUIPosition ePosition );
const char *PchNameFromEBackgroundRepeat( int nValue );

EFontWeight EFontWeightFromName( const char *pchName );
EFontStyle EFontStyleFromName( const char *pchName );
ETextAlign ETextAlignFromName( const char *pchName );
ETextDecoration ETextDecorationFromName( const char *pchName );
ETextTransform ETextTransformFromName( const char *pchName );
EAnimationTimingFunction EAnimationTimingFunctionFromName( const char *pchName );


//-----------------------------------------------------------------------------
// Purpose: Common functions for dealing with CSS
//-----------------------------------------------------------------------------
namespace CSSHelpers
{
	bool BParseFillBrushCollection( CFillBrushCollection *pCollection, const char *pchString, const char **pchAfterParse = NULL, float flScalingFactor = 1.0f );
	bool BParseFillBrush( CFillBrush *pBrush, const char *pchString, const char **pchAfterParse = NULL, float flScalingFactor = 1.0f );
	bool BParseGradientColorStop( CGradientColorStop *pColorStop, const char *pchString, const char **pchAfterParse = NULL, float flScalingFactor = 1.0f );
	bool BParseGaussianBlur( float &flPasses, float &flStdDevHorizontal, float &flStdDevVertical, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseColor( Color *pColor, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseRect( CUILength *pTop, CUILength *pRight, CUILength *pBottom, CUILength *pLeft, const char *pchString, const char **pchAfterParse = NULL, float flScalingFactor = 1.0f );
	bool BParseNamedColor( Color *pColor, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseLength( float *pLength, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseNumber( float *pNumber, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseTime( double *pSeconds, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseIdent( char *rgchIdent, int cubIdent, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseIdentToSymbol( CPanoramaSymbol *pSymbol, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseIdentToStyleSymbol( panorama::CStyleSymbol *pSymbol, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseQuotedString( CUtlString &sOutput, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseURL( CUtlString &sPath, const char *pchString, const char **pchAfterParse = NULL );
	bool BParsePercent( float *pPercent, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseIntoUILengthForSizing( CUILength *pLength, const char *pchString, const char **pchAfterParse = NULL, float flScalingFactor = 1.0f );
	bool BParseIntoUILength( CUILength *pLength, const char *pchString, const char **pchAfterParse = NULL, float flScalingFactor = 1.0f );
	bool BParseIntoTwoUILengths( CUILength *pLength1, CUILength *pLength2, const char *pchString, const char **pchAfterParse, float flScalingFactor = 1.0f );
	bool BParseTimingFunction( EAnimationTimingFunction *peTimingFunction, CCubicBezierCurve< Vector2D > *pCubicBezier, const char *pchString, const char **pchAfterParse );
	bool BParseAnimationDirectionFunction( EAnimationDirection *peAnimationDirection, const char *pchString, const char **pchAfterParse );
	bool BParseAngle( float *pDegrees, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseTransformFunction( CTransform3D **pTransform, const char *pchString, const char **pchAfterParse = NULL, float flScalingFactor = 1.0f );
	bool BParseBorderStyle( EBorderStyle *pStyle, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseHorizontalAlignment( EHorizontalAlignment *eHorizontalAlignment, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseVerticalAlignment( EVerticalAlignment *eVerticalAlignment, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseBackgroundPosition( CBackgroundPosition *pPosition, const char *pchString, const char **pchAfterParse = NULL );
	bool BParseBackgroundRepeat( CBackgroundRepeat *pBackgroundRepeat, const char *pchString, const char **pchAfterParse );
	bool BParseFunctionName( CPanoramaSymbol &symFunctionNameOut, const char *pchString, const char **pchAfterParse = NULL );
	
	template <typename T> bool BParseCommaSepList( CUtlVector< T, CUtlMemory<T> > *pvec, bool (*func)( T*, const char *, const char ** ), const char *pchString );
	bool BParseCommaSepList( CUtlVector< EAnimationTimingFunction > *pvec, CUtlVector< CCubicBezierCurve< Vector2D > > *pvec2, bool (*func)( EAnimationTimingFunction *pvec, CCubicBezierCurve< Vector2D > *pvec2, const char *, const char ** ), const char *pchString );
	template <typename T> bool BParseCommaSepListWithScaling( CUtlVector< T, CUtlMemory<T> > *pvec, bool (*func)( T*, const char *, const char **, float ), const char *pchString, float flScalingFactor );

	
	const char *SkipSpaces( const char *pchString );
	bool BSkipComma( const char *pchString, const char **pchAfterParse = NULL );
	bool BSkipLeftParen( const char *pchString, const char **pchAfterParse = NULL );
	bool BSkipRightParen( const char *pchString, const char **pchAfterParse = NULL );
	bool BSkipQuote( const char *pchString, const char **pchAfterParse = NULL );
	bool BSkipSlash( const char *pchString, const char **pchAfterParse = NULL );

	void AppendUILength( CFmtStr1024 *pfmtBuffer, const CUILength &length );
	void AppendFloat( CFmtStr1024 *pfmtBuffer, float flValue );
	void AppendColor( CFmtStr1024 *pfmtBuffer, Color c );
	void AppendTransform( CFmtStr1024 *pfmtBuffer, CTransform3D *pTransform );
	void AppendTime( CFmtStr1024 *pfmtBuffer, float flValue );
	void AppendFillBrushCollection( CFmtStr1024 *pfmtBuffer, const CFillBrushCollection &collection );
	void AppendFillBrush( CFmtStr1024 *pfmtBuffer, const CFillBrush &brush );
	void AppendGradientColorStop( CFmtStr1024 *pfmtBuffer, const CGradientColorStop &stops );
	void AppendURL( CFmtStr1024 *pfmtBuffer, const char *pchURL );
	void AppendLength( CFmtStr1024 *pfmtBuffer, float flValue );

	bool BParseTrueFalse( const char *pchString, bool *pbValue );

	bool EatCSSComment( CUtlBuffer &buffer );
	void EatCSSIgnorables( CUtlBuffer &buffer );
	bool BPeekCSSToken( CUtlBuffer &buffer, char *pchNextChar );
	bool BReadCSSComment( CUtlBuffer &buffer, char *pchBuffer, uint cubBuffer );

	bool BReadCSSToken( CUtlBuffer &buffer, char *pchToken, uint cubToken );
	bool BReadCSSToken( CUtlBuffer &buffer, char *pchToken, uint cubToken, const char *pchStopAt, uint cchStopAt );
};


//-----------------------------------------------------------------------------
// Purpose: Helper to parse comma separated lists with same value type
//-----------------------------------------------------------------------------
template <typename T> bool CSSHelpers::BParseCommaSepList( CUtlVector< T, CUtlMemory<T> > *pvec, bool (*func)( T*, const char *, const char ** ), const char *pchString )
{
	while ( *pchString != '\0' )
	{
		T val;
		if ( !func( &val, pchString, &pchString ) )
			return false;

		pvec->AddToTail( val );

		// done?
		if ( !CSSHelpers::BSkipComma( pchString, &pchString ) )
		{
			// no comma, should be empty string
			pchString = CSSHelpers::SkipSpaces( pchString );
			if ( pchString[0] != '\0' )
				return false;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Purpose: Helper to parse comma separated lists with same value type
//-----------------------------------------------------------------------------
template <typename T> bool CSSHelpers::BParseCommaSepListWithScaling( CUtlVector< T, CUtlMemory<T> > *pvec, bool (*func)( T*, const char *, const char **, float ), const char *pchString, float flScalingFactor )
{
	while ( *pchString != '\0' )
	{
		T val;
		if ( !func( &val, pchString, &pchString, flScalingFactor ) )
			return false;

		pvec->AddToTail( val );

		// done?
		if ( !CSSHelpers::BSkipComma( pchString, &pchString ) )
		{
			// no comma, should be empty string
			pchString = CSSHelpers::SkipSpaces( pchString );
			if ( pchString[0] != '\0' )
				return false;
		}
	}

	return true;
}

} // namespace panorama


#endif //CSSHELPERS_H
