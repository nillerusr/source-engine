//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUITEXTLAYOUT_H
#define IUITEXTLAYOUT_H

#ifdef _WIN32
#pragma once
#endif

#include "mathlib/mathlib.h"
#include "mathlib/vector2d.h"
#include "tier1/utlvector.h"
#include "panorama/panoramatypes.h"

namespace panorama
{

//
// Interface that needs to be implemented for text layout on all platforms
//
class IUITextLayout
{
public:
	virtual ~IUITextLayout() {}

	// Set formatting for character ranges
	virtual void SetFontName( uint32 unCharStartIndex, uint32 unCharEndIndex, const char *pchFontName ) = 0;
	virtual void SetFontSize( uint32 unCharStartIndex, uint32 unCharEndIndex, float flFontSize ) = 0;
	virtual void SetFontStyle( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontStyle eFontStyle ) = 0;
	virtual void SetFontWeight( uint32 unCharStartIndex, uint32 unCharEndIndex, EFontWeight eFontWeight ) = 0;
	virtual void SetUnderline( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bUnderline ) = 0;
	virtual void SetStrikethrough( uint32 unCharStartIndex, uint32 unCharEndIndex, bool bStrikethrough ) = 0;
	virtual void SetInlineObject( uint32 unCharIndex, float flWidth, float flHeight ) = 0;
	virtual void MarkColorRangeForMeasurement( uint32 unCharStartIndex, uint32 unCharEndIndex ) = 0;

	// Gets the size required to fully draw the text layout
	virtual void GetRequiredSize( float &flWidth, float &flHeight ) = 0;

	// Hit tests a point against the text layout.  
	// unHitRunLength returns the character index hit
	// bIsTrailingHit indicates whether the hit is on the leading or trailing side of the char
	// bInside is true if the hit is within the text region, when false the position closest the point is returned as
	// the character hit offset
	virtual void HitTestPoint( Vector2D point, uint32 &unFirstHitOffset, bool &bIsTrailingHit, bool &bIsInsideString ) = 0;

	// Struct used in some calls below
	struct HitTestRegionRect_t
	{
		Vector2D topLeft;
		Vector2D bottomRight;
		uint32 unCharStart;
		uint32 unCharEnd;
		bool bIsText;
		bool bIsTrimmed;
	};

	// Determines the layout coordinates for a given character offset, coordinates are relative to top left of text layout
	virtual void GetCharacterCoordinates( uint32 unCharIndex, HitTestRegionRect_t &charRegionRect ) = 0;

	// Determines a vector of rects enclosing a range of text, normally used for getting selection highlight regions
	virtual void GetCharacterRangeCoordinates( uint32 unCharStartIndex, uint32 unCharEndIndex, CUtlVector<HitTestRegionRect_t> &vecRangeRegionRects ) = 0;
};

} // namespace panorama

#endif // IUITEXTLAYOUT_H