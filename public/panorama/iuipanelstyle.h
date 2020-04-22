//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef IUIPANELSTYLE_H
#define IUIPANELSTYLE_H

#ifdef _WIN32
#pragma once
#endif

#include "panoramatypes.h"
#include "panorama/layout/uilength.h"
#include "mathlib/beziercurve.h"
#include "layout/stylesymbol.h"
#include "layout/backgroundimage.h"
#include "transformations.h"

namespace panorama
{

//-----------------------------------------------------------------------------
// Purpose: Individual transition property data
//-----------------------------------------------------------------------------
struct TransitionProperty_t
{
	panorama::CStyleSymbol m_symProperty;

	double m_flTransitionSeconds;
	double m_flTransitionDelaySeconds;
	EAnimationTimingFunction m_eTimingFunction;
	CCubicBezierCurve< Vector2D > m_CubicBezier;

	bool operator==(const TransitionProperty_t &rhs) const
	{
		return (m_symProperty == rhs.m_symProperty && m_flTransitionSeconds == rhs.m_flTransitionSeconds &&
			m_flTransitionDelaySeconds == rhs.m_flTransitionDelaySeconds && m_eTimingFunction == rhs.m_eTimingFunction
			&& (m_eTimingFunction != k_EAnimationCustomBezier ||
			(m_CubicBezier.ControlPoint( 0 ) == rhs.m_CubicBezier.ControlPoint( 0 ) &&
			m_CubicBezier.ControlPoint( 1 ) == rhs.m_CubicBezier.ControlPoint( 1 ) &&
			m_CubicBezier.ControlPoint( 2 ) == rhs.m_CubicBezier.ControlPoint( 2 ) &&
			m_CubicBezier.ControlPoint( 3 ) == rhs.m_CubicBezier.ControlPoint( 3 )
			)
			));
	}

	bool operator!=(const TransitionProperty_t &rhs) const { return !(*this == rhs); }
};

struct StyleEntry_t
{
	CStyleSymbol m_StyleSymbol;
	CStyleProperty *m_pStyleProperty;
};

struct PropertyInTransition_t;
class CActiveAnimation;

//-----------------------------------------------------------------------------
// Purpose: Basic panel interface exposing operations used inside of panorama, rather
// than operations that are part of building/laying out controls in the panorama_client module
//-----------------------------------------------------------------------------
class IUIPanelStyle
{
public:

	// Clear the styles
	virtual void Clear() = 0;

	// Get the UI scale factor, this is a property of windows, but since we can have a panel 
	// style with no explicit panel/window (text ranges) we have to set it into this class as well.
	virtual float GetUIScaleFactor() = 0;

	// Checks if the property has any style data at all, base, transition, or animation.
	// Useful to early out any work related to the stype when painting.
	virtual bool BHasAnyStyleDataForProperty( CStyleSymbol hSymbolProperty ) = 0;

	virtual void GetPosition( panorama::CUILength &x, panorama::CUILength &y, panorama::CUILength &z, bool bIncludeUIScaleFactor = true ) = 0;
	virtual void GetInterpolatedPosition( panorama::CUILength &x, panorama::CUILength &y, panorama::CUILength &z, bool bFinal, bool bIncludeUIScaleFactor = true ) = 0;
	virtual void SetPosition( panorama::CUILength x, panorama::CUILength y, panorama::CUILength z, bool bPreScaledByUIScaleFactor = false ) = 0;
	virtual void SetPositionWithoutTransition( panorama::CUILength x, panorama::CUILength y, panorama::CUILength z, bool bPreScaledByUIScaleFactor = false ) = 0;

	virtual void GetPerspectiveOrigin( panorama::CUILength &x, panorama::CUILength &y, bool &bInvert ) = 0;
	virtual void SetPerspectiveOrigin( panorama::CUILength &x, panorama::CUILength &y, bool &bInvert ) = 0;

	virtual void GetTransformOrigin( panorama::CUILength &x, panorama::CUILength &y, bool &bParentLayerRelative ) = 0;
	virtual void SetTransformOrigin( panorama::CUILength &x, panorama::CUILength &y, bool bParentLayerRelative ) = 0;

	virtual void GetPerspective( float &perspective ) = 0;
	virtual void SetPerspective( float perspective ) = 0;

	virtual void GetZIndex( float &zindex ) = 0;
	virtual void SetZIndex( float zIndex ) = 0;

	virtual void GetOverflow( panorama::EOverflowValue &eHorizontal, panorama::EOverflowValue &eVertical ) = 0;
	virtual void SetOverflow( const panorama::EOverflowValue eHorizontal, const panorama::EOverflowValue eVertical ) = 0;

	virtual void SetTransform3D( const CUtlVector<CTransform3D *> &vecTransforms ) = 0;
	virtual void SetTransform3DWithoutTransition( const CUtlVector<CTransform3D *> &vecTransforms ) = 0;
	virtual VMatrix GetTransform3DMatrix() = 0;

	virtual void GetOpacity( float &opacity ) = 0;
	virtual void SetOpacity( float opacity ) = 0;

	virtual void SetScale2DCentered( float flX, float flY ) = 0;

	virtual void SetRotate2DCentered( float flDegrees ) = 0;

	virtual void GetDesaturation( float &desaturation ) = 0;
	virtual void SetDesaturation( float desaturation ) = 0;

	virtual void GetGaussianBlur( float &passes, float &stddevhor, float &stddevver ) = 0;
	virtual void SetGaussianBlur( float passes, float stddevhor, float stddevver ) = 0;

	virtual void GetOpacityMaskImage( IImageSource *& pImage, float *pflOpacityMaskOpacity ) = 0;

	virtual void GetWashColor( Color &c ) = 0;
	virtual void SetWashColor( const Color &c ) = 0;

	virtual EMixBlendMode GetMixBlendMode() = 0;

	virtual ETextureSampleMode GetTexturesSampleMode() = 0;

	virtual void SetBackgroundColor( const char *pchColor ) = 0;
	virtual void SetSimpleBackgroundColor( const Color &c ) = 0;
	virtual bool GetSimpleBackgroundColor( Color &c ) = 0;

	virtual void SetForegroundColor( const char *pchColor ) = 0;
	virtual void SetSimpleForegroundColor( const Color &c ) = 0;
	virtual bool GetSimpleForegroundColor( Color &c ) = 0;

	virtual void SetFontStyle( const char *pchFontFamily, float flSize, EFontStyle style, EFontWeight weight ) = 0;
	virtual void GetFontStyle( const char **pchFontFamily, float &flSize, EFontStyle &style, EFontWeight &weight ) = 0;
	virtual void GetFontStyleNoDefaults( const char **pchFontFamily, float &flSize, EFontStyle &style, EFontWeight &weight ) = 0;
	virtual void GetForegroundFillBrushCollectionMsg( CMsgFillBrushCollection &msg, float flRenderWidth, float flRenderHeight ) = 0;
	virtual void GetLineHeight( float &flLineHeight ) = 0;
	virtual void GetTextAlign( ETextAlign &align ) = 0;
	virtual void GetTextDecoration( ETextDecoration &decoration ) = 0;
	virtual void GetTextTransform( ETextTransform &transform ) = 0;
	virtual void GetTextLetterSpacing( int &spacing ) = 0;

	virtual bool BHasPossibleBackgroundColor() = 0;

	virtual void GetWidth( panorama::CUILength &width ) = 0;
	virtual void SetWidth( panorama::CUILength width ) = 0;
	virtual void GetHeight( panorama::CUILength &height ) = 0;
	virtual void SetHeight( panorama::CUILength height ) = 0;
	virtual void GetMinWidth( panorama::CUILength &minWidth ) = 0;
	virtual void GetMinHeight( panorama::CUILength &minHeight ) = 0;
	virtual void GetMaxWidth( panorama::CUILength &maxWidth ) = 0;
	virtual void GetMaxHeight( panorama::CUILength &maxHeight ) = 0;
	virtual void GetInterpolatedWidth( panorama::CUILength &width, bool bFinal ) = 0;
	virtual void GetInterpolatedHeight( panorama::CUILength &height, bool bFinal ) = 0;
	virtual void GetInterpolatedMaxWidth( panorama::CUILength &width, bool bFinal ) = 0;
	virtual void GetInterpolatedMaxHeight( panorama::CUILength &height, bool bFinal ) = 0;

	virtual void GetVisibility( bool &bVisible ) = 0;
	virtual void SetVisibility( bool bVisible ) = 0;

	virtual void GetFlowChildren( panorama::EFlowDirection &eFlowDirection ) = 0;
	virtual void SetFlowChildren( panorama::EFlowDirection eFlowDirection ) = 0;

	virtual void GetInterpolatedBorderWidth( panorama::CUILength &left, panorama::CUILength &top, panorama::CUILength &right, panorama::CUILength &bottom, bool bFinal ) = 0;

	virtual void GetWhitespaceWrap( bool &bWrap ) = 0;
	virtual void GetTextOverflow( bool &bEllipsis ) = 0;

	// Content inset is padding+border-width
	virtual void GetContentInset( float flBoxWidth, float flBoxHeight, bool bFinalDimensions, float &left, float &top, float &right, float &bottom ) = 0;

	// You shouldn't need this outside layout code normally you want GetContentInset instead!
	virtual void GetPadding( panorama::CUILength &left, panorama::CUILength &top, panorama::CUILength &right, panorama::CUILength &bottom ) = 0;

	virtual void GetMargin( panorama::CUILength &left, panorama::CUILength &top, panorama::CUILength &right, panorama::CUILength &bottom ) = 0;
	virtual void GetMargin( float flBoxWidth, float flBoxHeight, float &left, float &top, float &right, float &bottom ) = 0;
	virtual void SetMargin( panorama::CUILength &left, panorama::CUILength &top, panorama::CUILength &right, panorama::CUILength &bottom ) = 0;

	virtual CUtlVector< CBackgroundImageLayer * > *GetBackgroundImages() = 0;
	virtual void SetBackgroundImages( const CUtlVector< CBackgroundImageLayer * > &vecLayers ) = 0;

	virtual void GetAlignment( EHorizontalAlignment &eHorizontalAlignment, EVerticalAlignment &eVerticalAlignment ) = 0;

	virtual void SetTooltipPositions( const EContextUIPosition( &eTooltipPositions )[ 4 ] ) = 0;
	virtual void GetTooltipPositions( EContextUIPosition( &eTooltipPositions )[ 4 ] ) = 0;
	virtual void SetTooltipBodyPosition( const panorama::CUILength &horizontalPosition, const panorama::CUILength &verticalPosition ) = 0;
	virtual void GetTooltipBodyPosition( panorama::CUILength &horizontalPosition, panorama::CUILength &verticalPosition ) = 0;
	virtual void SetTooltipArrowPosition( const panorama::CUILength &horizontalPosition, const panorama::CUILength &verticalPosition ) = 0;
	virtual void GetTooltipArrowPosition( panorama::CUILength &horizontalPosition, panorama::CUILength &verticalPosition ) = 0;

	virtual void SetContextMenuPositions( const EContextUIPosition( &eContextMenuPositions )[ 4 ] ) = 0;
	virtual void GetContextMenuPositions( EContextUIPosition( &eContextMenuPositions )[ 4 ] ) = 0;
	virtual void SetContextMenuBodyPosition( const panorama::CUILength &horizontalPosition, const panorama::CUILength &verticalPosition ) = 0;
	virtual void GetContextMenuBodyPosition( panorama::CUILength &horizontalPosition, panorama::CUILength &verticalPosition ) = 0;
	virtual void SetContextMenuArrowPosition( const panorama::CUILength &horizontalPosition, const panorama::CUILength &verticalPosition ) = 0;
	virtual void GetContextMenuArrowPosition( panorama::CUILength &horizontalPosition, panorama::CUILength &verticalPosition ) = 0;

	virtual void GetAnimationNames( CUtlVector< CPanoramaSymbol > *pvecAnimations ) = 0;
	virtual void ResetAnimations() = 0;

	// Get actual parent sizes, which we need to convert some % units to px
	virtual float GetParentActualRenderWidth() = 0;
	virtual float GetParentActualRenderHeight() = 0;

	// Checks if the property has any data for transition
	virtual bool BHasAnyTransition() = 0;

	// Checks if this panel has any active transitions or animations
	virtual bool BHasAnyTransitionOrAnimation( bool bExcludeStylesImpactingOnlyCompositing ) = 0;

	virtual bool BHasAnimatingBackground() = 0;

	// Checks if the panel is completely transparent, and has no current animation/transition of opacity.  
	// If this is true it means we don't need to draw the panel.
	virtual bool BIsTransparentWithNoOpacityTransition() = 0;

	// Set a new UI scale factor from the one we constructed with
	virtual void SetUIScaleFactor( float flScaleFactor ) = 0;

	// Set transition properties
	virtual void SetTransitionProperties( const CUtlVector< TransitionProperty_t > &vecTransitionProperties ) = 0;

	// Get animation control curve points
	virtual void GetAnimationCurveControlPoints( EAnimationTimingFunction eTransitionEffect, Vector2D vecPoints[4] ) = 0;

	// Low level search for property info for debugger use
	virtual void FindPropertyInfo( CStyleSymbol hSymbol, CStyleProperty **ppProperty, PropertyInTransition_t **ppTransitionData, CUtlVector< CActiveAnimation * > *pvecAnimations ) = 0;

	virtual const CUtlVector<StyleEntry_t> &PropertiesSetFromElement() const = 0;
	virtual const CStyleProperty *GetPropertyNoInherit( CStyleSymbol symProperty ) = 0;

	// properties set on element style (set from code)
	virtual bool BPropertySetFromElement( CStyleSymbol symProperty ) const = 0;
	virtual void ClearPropertySetFromElement( CStyleSymbol symProperty ) = 0;

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName ) = 0;
#endif
};

}
#endif // IUIPANELSTYLE_H
