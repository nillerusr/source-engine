//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Public header for panorama UI framework
//
//
//=============================================================================//
#ifndef PANORAMATYPES_H
#define PANORAMATYPES_H
#pragma once

#ifdef POSIX
#include <float.h>
#endif
#include "mathlib/vector.h"

#ifdef SOURCE2_PANORAMA
#include "tier1/mempool.h"
#include "tier1/checksum_sha1.h"

class ISceneView;
class IRenderContext;
class ISceneLayer;
typedef void (RenderCallbackFunction_t)( ISceneView *, IRenderContext **, ISceneLayer *, float, float, float, float );

#endif

#if !defined( SOURCE2_PANORAMA )
#define VPROF_BUDGET_THREAD VPROF_BUDGET
#else
#define VPROF_BUDGET_THREAD( name, group ) ((void)0)

const uint32 k_cubSHA1Hash = k_cubHash;

#ifndef SAFE_DELETE
#define SAFE_DELETE( x ) if ( (x) != NULL ) { delete (x); x = NULL; }
#endif
#ifndef SAFE_RELEASE
#define SAFE_RELEASE( x ) if ( NULL != ( x ) ) { ( x )->Release(); x = NULL; }
#endif
#define AssertFatalMsg2( b, msg, p1, p2 ) AssertFatalMsg( b, msg, p1, p2 )
#define AssertFatalMsg1( b, msg, p1 ) AssertFatalMsg( b, msg, p1 )

template <class T, class P>
inline void ConstructOneArg( T* pMemory, P const& arg )
{
	HINT( pMemory != 0 );
	::new(pMemory)T( arg );
}

template <class T, class P, class P2 >
inline void ConstructTwoArg( T* pMemory, P const& arg1, P2 const &arg2 )
{
	HINT( pMemory != 0 );
	::new(pMemory)T( arg1, arg2 );
}

template <class T, class P, class P2, class P3, class P4, class P5, class P6, class P7 >
inline void ConstructSevenArg( T* pMemory, P const& arg1, P2 const &arg2, P3 const &arg3, P4 const &arg4, P5 const &arg5, P6 const &arg6, P7 const &arg7 )
{
	HINT( pMemory != 0 );
	::new(pMemory)T( arg1, arg2, arg3, arg4, arg5, arg6, arg7 );
}
#endif


namespace panorama
{

class IUIPanelClient;


//-----------------------------------------------------------------------------
// Purpose: Text/font related types
//-----------------------------------------------------------------------------
enum EFontWeight
{
	k_EFontWeightUnset = -1,
	k_EFontWeightNormal = 0,
	k_EFontWeightMedium = 1,
	k_EFontWeightBold = 2,
	k_EFontWeightBlack = 3,
	k_EFontWeightThin = 4,
	k_EFontWeightLight = 5,
	k_EFontWeightSemiBold = 6,
};

enum EFontStyle
{
	k_EFontStyleUnset = -1,
	k_EFontStyleNormal = 0,
	k_EFontStyleItalic = 2,
};

enum ETextAlign
{
	k_ETextAlignUnset = -1,
	k_ETextAlignLeft = 0,
	k_ETextAlignCenter = 1,
	k_ETextAlignRight = 2
};

enum ETextDecoration
{
	k_ETextDecorationUnset = -1,
	k_ETextDecorationNone = 0,
	k_ETextDecorationUnderline = 1,
	k_ETextDecorationLineThrough = 2,
};

enum ETextTransform
{
	k_ETextTransformUnset = -1,
	k_ETextTransformNone = 0,
	k_ETextTransformUppercase = 1,
	k_ETextTransformLowercase = 2,
};


//-----------------------------------------------------------------------------
// Purpose: Transform related types
//-----------------------------------------------------------------------------
enum ETransform3DType
{
	k_ETransform3DRotate,
	k_ETransform3DTranslate,
	k_ETransform3DScale
};


//-----------------------------------------------------------------------------
// Purpose: Types for border images
//-----------------------------------------------------------------------------
enum EBorderImageRepeatType
{
	k_EBorderImageStretchStretch = 0,
	k_EBorderImageStretchRepeat = 1,
	k_EBorderImageStretchRound = 2,
	k_EBorderImageStretchSpace = 3,
};


enum EBorderImageWidthType
{
	k_EBorderImageWidthAuto = 0,
	k_EBorderImageWidthNumber = 1,
	k_EBorderImageWidthPercentage = 2
};


//-----------------------------------------------------------------------------
// Purpose: Transition timing functions
//-----------------------------------------------------------------------------
enum EAnimationTimingFunction
{
	k_EAnimationNone = 0, // Indicates that there is no transition set, do not animate.  Don't change from zero!
	k_EAnimationEase,
	k_EAnimationEaseIn,
	k_EAnimationEaseOut,
	k_EAnimationEaseInOut,
	k_EAnimationLinear,
	k_EAnimationCustomBezier,

	k_EAnimationUnset,
	// if you add another, update BParseTimingFunction()
};


//-----------------------------------------------------------------------------
// Purpose: Possible animation directions
//-----------------------------------------------------------------------------
enum EAnimationDirection
{
	k_EAnimationDirectionUnset = 0,
	k_EAnimationDirectionNormal,
	k_EAnimationDirectionAlternate,
	k_EAnimationDirectionReverse,
	k_EAnimationDirectionAlternateReverse,

	// if you add another, update BParseAnimationDirectionFunction()
};


//-----------------------------------------------------------------------------
// Purpose: Supported style psuedo classes
//-----------------------------------------------------------------------------
enum EStyleFlags : uint16
{
	k_EStyleFlagNone = 0,
	k_EStyleFlagHover = 1 << 0,			// mouse is over the panel
	k_EStyleFlagFocus = 1 << 1,			// panel has keyboard focus
	k_EStyleFlagActive = 1 << 2,		// panel is being actively used (mouse-down on the panel)
	k_EStyleFlagDisabled = 1 << 3,		// panel is disabled
	k_EStyleFlagInspect = 1 << 4,		// panel is being inspected by the debugger
	k_EStyleFlagSelected = 1 << 5,		// panel is selected (button checked)
	k_EStyleFlagDescendantFocused = 1 << 6,	// a descendant of the panel has keyboard focus
	k_EStyleFlagParentDisabled = 1 << 7, // a parent of this panel is disabled, thus implicitly disabling it
	k_EStyleFlagLayoutLoading = 1 << 8,	// panel is in-progress loading it's layout file and thus may want to display specially
	k_EStyleFlagLayoutLoadFailed = 1 << 9, // panel failed to load requested layout file, and thus may want to display specially
	k_EStyleFlagActivationDisabled = 1 << 10, // panel is disabled for activation, may still be enabled for focus (normal disabled disallows all input/focus)
};


//-----------------------------------------------------------------------------
// Purpose: Supported border styles
//-----------------------------------------------------------------------------
enum EBorderStyle
{
	k_EBorderStyleUnset = -1,
	k_EBorderStyleNone = 0,
	k_EBorderStyleSolid = 1,
};


//-----------------------------------------------------------------------------
// Purpose: Horizontal alignments
//-----------------------------------------------------------------------------
enum EHorizontalAlignment
{
	k_EHorizontalAlignmentUnset,
	k_EHorizontalAlignmentLeft,
	k_EHorizontalAlignmentCenter,
	k_EHorizontalAlignmentRight
};


//-----------------------------------------------------------------------------
// Purpose: Vertical alignments
//-----------------------------------------------------------------------------
enum EVerticalAlignment
{
	k_EVerticalAlignmentUnset,
	k_EVerticalAlignmentTop,
	k_EVerticalAlignmentCenter,
	k_EVerticalAlignmentBottom
};

//-----------------------------------------------------------------------------
// Purpose: ContextUI (tooltips + context menus) position
//-----------------------------------------------------------------------------
enum EContextUIPosition
{
	k_EContextUIPositionUnset,
	k_EContextUIPositionLeft,
	k_EContextUIPositionTop,
	k_EContextUIPositionRight,
	k_EContextUIPositionBottom,
};

//-----------------------------------------------------------------------------
// Purpose: Possible flow directions
//-----------------------------------------------------------------------------
enum EBackgroundRepeat
{
	k_EBackgroundRepeatUnset,
	k_EBackgroundRepeatRepeat,
	k_EBackgroundRepeatSpace,
	k_EBackgroundRepeatRound,
	k_EBackgroundRepeatNo,
};


//-----------------------------------------------------------------------------
// Purpose: Special words for background sizes when not length or percentage
//-----------------------------------------------------------------------------
enum EBackgroundSizeConstant
{
	k_EBackgroundSizeConstantNone,
	k_EBackgroundSizeConstantContain,
	k_EBackgroundSizeConstantCover
};


//-----------------------------------------------------------------------------
// Purpose: type of repaint to do for a render target/panel
//-----------------------------------------------------------------------------
enum EPanelRepaint : uint8
{
	k_EPanelRepaintFull,
	k_EPanelRepaintComposition,
	k_EPanelRepaintNone
};


//-----------------------------------------------------------------------------
// Purpose: Specifies what category a sound is; each one of these maps to a volume control in the UI
//-----------------------------------------------------------------------------
enum ESoundType
{
	k_ESoundType_Ambient,			// ambient sounds, intro movie
	k_ESoundType_Movies,			// movies in store
	k_ESoundType_Effects,			// daisywheel, navigation
	k_ESoundType_Passthrough,		// special sound type for volume-changing UI - bypasses volume controls
};


//-----------------------------------------------------------------------------
// Purpose: Text Input handler types
//-----------------------------------------------------------------------------
enum ETextInputHandlerType_t
{
	k_ETextInputHandlerType_DaisyWheel,
	k_ETextInputHandlerType_DualTouch,

	k_ETextInputHandlerTypeDefault = k_ETextInputHandlerType_DualTouch,		// if we fail at gathering the information to figure out which is the most appropriate input method, fall back to this one
};


//-----------------------------------------------------------------------------
// Purpose: Possible flow directions
//-----------------------------------------------------------------------------
enum EFlowDirection : uint8
{
	k_EFlowNone,
	k_EFlowDown,
	k_EFlowRight,
	k_EFlowDownWrap,
	k_EFlowRightWrap
};


//-----------------------------------------------------------------------------
// Purpose: Possible overflow settings
//-----------------------------------------------------------------------------
enum EOverflowValue : uint8
{
	k_EOverflowSquish,
	k_EOverflowClip,
	k_EOverflowScroll,
	k_EOverflowNoClip,
};


enum EMouseCanActivate : uint8
{
	k_EMouseCanActivateUnfocused = 0,
	k_EMouseCanActivateIfFocused,
	k_EMouseCanActivateIfParentFocused,
	k_EMouseCanActivateIfAnyParentFocused
};

// Focus navigation
enum EFocusMoveDirection
{
	k_ENextInTabOrder = 1,
	k_EPrevInTabOrder = 2,
	k_ENextByXPosition = 4,
	k_EPrevByXPosition = 8,
	k_ENextByYPosition = 16,
	k_EPrevByYPosition = 32
};


enum ELoadLayoutAsyncDetails
{
	k_ELoadLayoutAsyncDetailsNone,
	k_ELoadLayoutAsyncDetailsNotLoggedIn
};


enum EMixBlendMode
{
	k_EMixBlendModeNormal,
	k_EMixBlendModeMultiply,
	k_EMixBlendModeScreen
};


enum ETextureSampleMode
{
	k_ETextureSampleModeNormal,
	k_ETextureSampleModeAlphaOnly,
};

//-----------------------------------------------------------------------------
// Purpose: Enum for the parts of layout a style invalidates when applied
//-----------------------------------------------------------------------------
enum EStyleInvalidateLayout
{
	k_EStyleInvalidateLayoutNone,
	k_EStyleInvalidateLayoutSizeAndPosition,
	k_EStyleInvalidateLayoutPosition
};


enum EStyleRepaint
{
	k_EStyleRepaintFull,
	k_EStyleRepaintComposition,
	k_EStyleRepaintNone,
};


enum EAntialiasing
{
	k_EAntialiasingNone,
	k_EAntialisingEnabled
};

enum EOverlayWindowAlignment
{
	k_EOverlayWindowAlignment_FullscreenLetterboxed = 1,
	k_EOverlayWindowAlignment_FullscreenNoLetterBox = 2,
	k_EOverlayWindowAlignment_BottomRight = 3,
};

enum EStylePresentFlags : uint32
{
	k_EStylePresentTransformMatrix = 1 << 0,
	k_EStylePresentPerspective = 1 << 1,
	k_EStylePresentPerspectiveOrigin = 1 << 2,
	k_EStylePresentOpacity = 1 << 3,
	k_EStylePresentWashColor = 1 << 4,
	k_EStylePresentDesaturation = 1 << 5,
	k_EStylePresentBlur = 1 << 6,
	k_EStylePresentBorderRadius = 1 << 7,
	k_EStylePresentOpacityMaskImage = 1 << 8,
	k_EStylePresentBackgroundImage = 1 << 9,
	k_EStylePresentBackgroundFillColor = 1 << 10,
	k_EStylePresentBorder = 1 << 11,
	k_EStylePresentBoxShadow = 1 << 12,
	k_EStylePresentBorderImage = 1 << 13,
	k_EStylePresentScale2DCentered = 1 << 14,
	k_EStylePresentRotate2DCentered = 1 << 15,
	k_EStylePresentTextShadow = 1 << 16,
	k_EStylePresentClip = 1 << 17,
	k_EStylePresentMixBlendMode = 1 << 18,

};

// constants, these must remain matching
const float k_flTabIndexInvalid = -1.0 * FLT_MAX;
const float k_flSelectionPosInvalid = k_flTabIndexInvalid;
const float k_flTabIndexAuto = FLT_MAX;
const float k_flSelectionPosAuto = k_flTabIndexAuto;

// These constants are duplicated in CSS files, so if you change them make sure to change the CSS values as well.
const double k_flScrollTransitionTime = 0.2;
const EAnimationTimingFunction k_flScrollTransitionFunc = k_EAnimationEaseInOut;


//-----------------------------------------------------------------------------
// Purpose: bit flags used to control panel construction behavior
//-----------------------------------------------------------------------------
enum EPanelFlags
{
	ePanelFlags_DontAddAsChild = 0x1, // don't automatically add this panel as a child to its parent
	ePanelFlags_DontFireOnLoad = 0x2, // don't fire the onload event when constructed, useful for image panel that waits until its contents load
};


enum EPanelEventSource_t
{
	k_ePanelEventSourceProgram,
	k_ePanelEventSourceGamepad,
	k_ePanelEventSourceKeyboard,
	k_ePanelEventSourceMouse,

	k_ePanelEventSourceInvalid
};


class IUIPanel;
class IUIEvent;
typedef IUIEvent* (*PFN_ParseUIEvent)(panorama::IUIPanel *pTarget, const char *pchEvent, const char **pchEventEnd);
typedef IUIEvent* (*PFN_MakeUIEvent0)(const panorama::IUIPanelClient *pTarget);
typedef IUIEvent* (*PFN_MakeUIEvent1Source)(const panorama::IUIPanelClient *pTarget, EPanelEventSource_t eSource);
typedef IUIEvent* (*PFN_MakeUIEvent1Repeats)(const panorama::IUIPanelClient *pTarget, int nRepeats);
struct UIEventFactory
{
	int m_cParams;
	bool m_bPanelEvent;
	PFN_ParseUIEvent m_pfnParseUIEvent;
	PFN_MakeUIEvent0 m_pfnMakeUIEvent0;
	PFN_MakeUIEvent1Repeats m_pfnMakeUIEvent1Repeats;
	PFN_MakeUIEvent1Source m_pfnMakeUIEvent1Source;
};


//-----------------------------------------------------------------------------
// Simple refcounted base class that doesn't auto-delete when refs hit zero.
//-----------------------------------------------------------------------------
class CLayoutRefCounted
{
public:
	CLayoutRefCounted()
	{
		m_nRefCount = 0;
	}
	int AddRef()
	{
		return ++m_nRefCount;
	}
	int Release()
	{
		int nResult = --m_nRefCount;
		Assert( nResult >= 0 );
		return nResult;
	}
	int GetRefCount() const
	{
		return m_nRefCount;
	}
private:
	int m_nRefCount;
};


//-----------------------------------------------------------------------------
// Pointer to a CLayoutRefCounted that owns a ref of the pointee
//-----------------------------------------------------------------------------
template< class T >
class CLayoutRefPtr
{
public:
	CLayoutRefPtr()                                        	: m_pObject( NULL ) {}
	CLayoutRefPtr( T *pFrom )                        		{ Set( pFrom ); }
	CLayoutRefPtr( const CLayoutRefPtr< T > &from )			{ Set( from.m_pObject ); }
	~CLayoutRefPtr()
	{
		if ( m_pObject )
		{
			m_pObject->Release();
		}
	}

	void operator=( const CLayoutRefPtr<T> &from ) 			{ Set( from.m_pObject ); }

	operator const T *() const							    { return m_pObject; }
	operator T *()											{ return m_pObject; }

	operator bool() const									{ return ( m_pObject != NULL ); }

	T *operator=( T *p )									{ Set( p ); return p; }

    bool operator!() const									{ return ( !m_pObject ); }
	bool operator==( T *p ) const							{ return ( m_pObject == p ); }
	bool operator!=( T *p ) const							{ return ( m_pObject != p ); }
	bool operator==( const CLayoutRefPtr<T> &p ) const		{ return ( m_pObject == p.m_pObject ); }
	bool operator!=( const CLayoutRefPtr<T> &p ) const		{ return ( m_pObject != p.m_pObject ); }

	T *  		operator->()								{ return m_pObject; }
	T &  		operator *()								{ return *m_pObject; }
	T ** 		operator &()								{ return &m_pObject; }

	const T *   operator->() const							{ return m_pObject; }
	const T &   operator *() const							{ return *m_pObject; }
	T * const * operator &() const							{ return &m_pObject; }

	void Set( T *pObject )
	{
		if ( m_pObject )
		{
			m_pObject->Release();
		}

		m_pObject = pObject;

		if ( pObject )
		{
			pObject->AddRef();
		}
	}

protected:
	T *m_pObject;
};


enum EWindowFocusBehavior
{
	// By default, controls in this window will take focus whenever they're interacted with.
	k_EWindowFocusBehavior_Default,

	// By default, clicking on controls in this window will avoid taking focus unless it's necessary.
	// Useful if you only plan on supporting keyboard focus in a few specific spots and otherwise
	// don't display keyboard focus.
	k_EWindowFocusBehavior_Shy,
};


struct SteamPadPointer_t
{
	bool bVisible;
	Vector2D vecCenter;
	float flRadius;
	uint32 nTextureID;
	int iControllerID;
	float (* funcPreRenderCalculatePadOffset)( float, bool );
};


} // namespace panorama

#endif // PANORAMATYPES_H