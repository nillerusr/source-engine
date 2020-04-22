//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: 
//=============================================================================//

#ifndef STYLEPROPERTIES_H
#define STYLEPROPERTIES_H

#ifdef _WIN32
#pragma once
#endif

#include "layout/stylesymbol.h"
#include "layout/csshelpers.h"
#include "tier1/utlptrarray.h"
#include "tier1/utlhashmap.h"
#include "iuipanelstyle.h"
#include "panoramatypes.h"

namespace panorama
{

// Helper for getting actual size of parent, which will expand up to window size if top level
void GetParentSizeAvailable( IUIPanel *pPanel, float &flParentWidth, float &flParentHeight, float &flParentPerspective );
void GetAnimationCurveControlPoints( EAnimationTimingFunction eTransitionEffect, Vector2D vecPoints[4] );



//-----------------------------------------------------------------------------
// Purpose: Information on a property in the middle of a transition
//-----------------------------------------------------------------------------
struct PropertyInTransition_t
{
	PropertyInTransition_t() { m_pStyleProperty = NULL; }
	~PropertyInTransition_t()
	{
		if( m_pStyleProperty )
			UIEngine()->UIStyleFactory()->FreeStyleProperty( m_pStyleProperty );
	}

	TransitionProperty_t m_transitionData;
	double m_flTransitionStartTime;
	CStyleProperty *m_pStyleProperty;

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName );
#endif

private:
	PropertyInTransition_t( const PropertyInTransition_t & );
	PropertyInTransition_t &operator=(const PropertyInTransition_t &rhs);
};


//-----------------------------------------------------------------------------
// Purpose: Individual animation property data
//-----------------------------------------------------------------------------
struct AnimationProperty_t
{
	CPanoramaSymbol m_symName;							// keyframe name

	double m_flDuration;
	EAnimationTimingFunction m_eTimingFunction;
	CCubicBezierCurve< Vector2D > m_CubicBezier;
	float m_flIteration;							// value or infinite
	EAnimationDirection m_eAnimationDirection;
	double m_flDelay;

	bool operator==(const AnimationProperty_t &rhs) const
	{
		return (m_symName == rhs.m_symName && m_flDuration == rhs.m_flDuration && m_eTimingFunction == rhs.m_eTimingFunction &&
			(m_eTimingFunction != k_EAnimationCustomBezier ||
			(m_CubicBezier.ControlPoint( 0 ) == rhs.m_CubicBezier.ControlPoint( 0 ) &&
			m_CubicBezier.ControlPoint( 1 ) == rhs.m_CubicBezier.ControlPoint( 1 ) &&
			m_CubicBezier.ControlPoint( 2 ) == rhs.m_CubicBezier.ControlPoint( 2 ) &&
			m_CubicBezier.ControlPoint( 3 ) == rhs.m_CubicBezier.ControlPoint( 3 )
			)
			) &&
			m_flIteration == rhs.m_flIteration && m_eAnimationDirection == rhs.m_eAnimationDirection && m_flDelay == rhs.m_flDelay);
	}

	bool operator!=(const AnimationProperty_t &rhs) const
	{
		return !(*this == rhs);
	}
};


//-----------------------------------------------------------------------------
// Purpose: Information on a property in the middle of a transition
//-----------------------------------------------------------------------------
class CActiveAnimation
{
public:
	CActiveAnimation( float flAnimationStart, AnimationProperty_t &animationProperty );
	~CActiveAnimation();

	CPanoramaSymbol GetName() const { return m_animationData.m_symName; }
	const AnimationProperty_t &GetAnimationData() const { return m_animationData; }
	void AddFrameData( float flPercent, EAnimationTimingFunction eTimingFunction, const CCubicBezierCurve<Vector2D> &cubicBezier, CStyleProperty *pProperty, float flUIScaleFactor );

	bool BHasFrameDataForProperty( CStyleSymbol symStyleProperty );

	double GetStartTime() const { return m_flAnimationStartTime; }
	double GetStartTimeWithDelay() const { return m_flAnimationStartTime + m_animationData.m_flDelay; }
	float CalculateAnimationEndTime() const;
	void Reset();
	void GetAffectedPanelLayoutFlags( CPanelStyle *pPanelStyle, bool *pbAffectsSize, bool *pbAffectsPosition );
	bool BAffectsCompositionOnly();
	bool BAffectsPanelLayoutFlags( CPanelStyle *pPanelStyle );

	void UpdateUIScaleFactor( float flOldScaleFactor, float flNewScaleFactor );

	// keyframe data
	struct PropertyFrameData_t
	{
		float m_flPercent;							// percent into animation when styles apply. 
		EAnimationTimingFunction m_eTimingFunction;	// timing function can be overridden per keyframe
		CCubicBezierCurve< Vector2D > m_CubicBezier;
		CStyleProperty *m_pStyleProperty;			// property to apply
	};
	typedef CUtlVector< PropertyFrameData_t > VecPropertyFrameData_t;
	const VecPropertyFrameData_t *GetFrameData( CStyleSymbol symProperty );

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

private:
	CActiveAnimation();
	CActiveAnimation( const CActiveAnimation & );
	CActiveAnimation &operator=(const CActiveAnimation &rhs);

	AnimationProperty_t m_animationData;
	double m_flAnimationStartTime;
	CUtlMap< CStyleSymbol, VecPropertyFrameData_t *, short, CDefLess< CStyleSymbol > > m_mapFrameData;
};


//-----------------------------------------------------------------------------
// Purpose: Represents all components that can be included in an individual CSS selector
//-----------------------------------------------------------------------------
class CStyleSelector
{
public:
	CStyleSelector()
	{
		m_pchID = NULL;
		m_eStyleFlags = k_EStyleFlagNone;
		m_unSelectorFlags = 0;
	}

	~CStyleSelector()
	{
		ClearID();
	}

	void SetID( const char *pchID )
	{
		ClearID();
		m_pchID = strdup( pchID );
	}

	void ClearID()
	{
		if( m_pchID )
		{
			free( (void*)m_pchID );
			m_pchID = NULL;
		}
	}

	const char *GetID() const { return m_pchID; }

	void SetPanelType( CPanoramaSymbol symType ) { m_symPanelType = symType; }
	CPanoramaSymbol GetPanelType() const { return m_symPanelType; }

	void SetClasses( CPanoramaSymbol *pSymbols, int cSymbols ) { m_classes.Copy( pSymbols, cSymbols ); }
	const CUtlPtrArray< CPanoramaSymbol > &GetClasses() const { return m_classes; }

	void SetStyleFlags( EStyleFlags eFlags ) { m_eStyleFlags = eFlags; }
	EStyleFlags GetStyleFlags() const { return m_eStyleFlags; }

	// allows setting an ID w/o allocating for searching. Skips memory management
	void SetIDForSearch( const char *pchID )
	{
		m_pchID = pchID;
	}

	// allows setting classes w/o allocationg for searching. Skips memory management
	void SetClassesForSearch( CPanoramaSymbol *pSymbols, int cSymbols )
	{
		if( pSymbols )
			m_classes.TakeOwnership( pSymbols, cSymbols );
		else
			m_classes.DetatchAndClear();
	}

	void SetChildMatchesNextStyle()
	{
		m_unSelectorFlags |= k_ESelectorFlagsChildCombinator;
	}

	bool BChildMatchesNextStyle() const
	{
		return ((m_unSelectorFlags & k_ESelectorFlagsChildCombinator) != 0);
	}

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		validator.ClaimMemory( (void *)m_pchID );
		ValidateObj( m_classes );
	}
#endif

private:
	enum ESelectorFlags
	{
		k_ESelectorFlagsNone = 0,
		k_ESelectorFlagsChildCombinator = 1,				// next selector must match child of panel that matched this style (.A > .B)
	};

	CStyleSelector( CStyleSelector const& ) { Assert( 0 ); }

	// id is a char pointer instead of CUtlString so we can do searches w/o allocations
	const char *m_pchID;							// not empty if an ID is required
	CPanoramaSymbol m_symPanelType;					// if invalid, applies to all panels
	CUtlPtrArray< CPanoramaSymbol > m_classes;		// can be empty if id or panel type are set	
	EStyleFlags m_eStyleFlags;						// required style flags
	uint32 m_unSelectorFlags;						// ESelectorFlags
};


//-----------------------------------------------------------------------------
// Purpose: Base class for every style property, they all have a symbol for a name.
//-----------------------------------------------------------------------------
class CStyleProperty
{
public:
	CStyleProperty( CStyleSymbol symPropertyName )
	{
		m_symPropertyName = symPropertyName;
		m_bDisallowTransition = false;
	}

	virtual ~CStyleProperty() {}

	bool operator < (const CStyleProperty &rhs) const
	{
		return m_symPropertyName < rhs.m_symPropertyName;
	}

	CStyleSymbol GetPropertySymbol() const { return m_symPropertyName; }

	// Merges data to the target, this will only overwrite unset properties, and shouldn't clobber
	// already set ones.
	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( m_bDisallowTransition )
			pTarget->m_bDisallowTransition = true;
	}

	// Can this property support animation?
	virtual bool BCanTransition() = 0;

	// Interpolation func for animation of this property, must implement if you override BCanTransition to return true.
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) = 0;

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString ) = 0;

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const = 0;

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	// Example: For margin, if we have only seen margin-length, should return false until top, bottom, right are set
	virtual bool BFullySet() { return true; }

	// called when applied to a panel before comparing with set values. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues() {}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor ) {}

	// called when applied to a panel after comparing with set values
	virtual void OnAppliedToPanel( IUIPanel *pPanel ) {}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty ) { return "&lt;Needs a description&gt;"; }

	// Does this property only affecting compositing when rendering, or does it affect drawing within the composition
	// layer for the panel it is applied to?
	virtual bool BAffectsCompositionOnly() { return false; }

	// Get suggested values based on current text for style property value
	virtual void GetSuggestedValues( CStyleSymbol symAlias, const char *pchTextSoFar, CUtlVector< CUtlString > &vecSuggestions ) { }

	// Comparison function
	virtual bool operator==(const CStyleProperty &rhs) const = 0;
	bool operator!=(const CStyleProperty &rhs) const { return !(*this == rhs); }

	// What layout attributes can be invalidated when the property is applied to a panel (compared to prior value)
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const = 0;

	// Layout pieces that can be invalidated when the property is applied to a panel
	inline bool BInvalidatesSizeAndPosition( CStyleProperty *pCompareProperty ) const { return (GetInvalidateLayout( pCompareProperty ) == k_EStyleInvalidateLayoutSizeAndPosition); }
	inline bool BInvalidatesPosition( CStyleProperty *pCompareProperty ) const { return (GetInvalidateLayout( pCompareProperty ) == k_EStyleInvalidateLayoutPosition); }

	// If the style supports transition (BCanTransition), it may still be that this style instance disables transition and forces immediate apply, this controls that
	void SetDisallowTransition( bool bDisallowTransition ) { m_bDisallowTransition = bDisallowTransition; }

	// If the style supports transition (BCanTransition), it may still be that this style instance disables transition and forces immediate apply, this checks that
	bool BDisallowTransition() { return m_bDisallowTransition; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

	// CLANG requires delete to be visible to derived classes even if new is not
protected:
#if !defined( SOURCE2_PANORAMA )
#include "tier0/memdbgoff.h"
#endif
	void *operator new(size_t size) throw(){ return NULL; }
	void *operator new(size_t size, int nBlockUse, const char *pFileName, int nLine) throw(){ return NULL; }
	void operator delete(void* p) { Assert( false ); }
	void operator delete(void* p, int nBlockUse, const char *pFileName, int nLine) { Assert( false ); }
#if !defined( SOURCE2_PANORAMA )
#include "tier0/memdbgon.h"
#endif


private:
	CStyleSymbol m_symPropertyName;
	bool m_bDisallowTransition;
};


typedef CUtlHashMap< panorama::CStyleSymbol, panorama::CStyleProperty *, CDefEquals< panorama::CStyleSymbol > > StylePropertyHash_t;

//-----------------------------------------------------------------------------
// Purpose: Represents a style's properties and position in a file
//-----------------------------------------------------------------------------
struct StyleFromFile_t
{
	StyleFromFile_t()
	{
		m_pProperties = NULL;
		m_unFileLocation = 0;
		m_pNext = NULL;
	}

	~StyleFromFile_t()
	{
		if( m_pProperties )
		{
			FOR_EACH_RBTREE_FAST( *m_pProperties, i )
			{
				UIEngine()->UIStyleFactory()->FreeStyleProperty( m_pProperties->Element( i ) );
			}

			SAFE_DELETE( m_pProperties );
		}

		SAFE_DELETE( m_pNext );
	}

	const CStyleProperty *GetProperty( CStyleSymbol symProperty ) const
	{
		if( !m_pProperties )
			return NULL;

		short i = m_pProperties->Find( symProperty );
		if( i == m_pProperties->InvalidIndex() )
			return NULL;

		return m_pProperties->Element( i );
	}

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_selector );

		ValidateObj( m_parentSelectors );
		FOR_EACH_PTR_ARRAY( m_parentSelectors, i )
		{
			ValidateObj( m_parentSelectors[i] );
		}

		ValidatePtr( m_pProperties );

		// Don't validate individual properties, they are pooled and we validate the pools elsewhere.  Do have
		// them validate their own members.
		FOR_EACH_MAP_FAST( *m_pProperties, i )
		{
			m_pProperties->Element( i )->Validate( validator, "CStyleProperty" );
		}


		if( m_pNext )
			ValidatePtr( m_pNext );
	}
#endif

	CStyleSelector m_selector;						// selector for this element
	CUtlPtrArray< CStyleSelector > m_parentSelectors;	// selectors for parents, ordered farthest to our closest parent

	StylePropertyHash_t *m_pProperties;				// list of parsed properties
	uint m_unFileLocation;							// style offset into the file (Nth style, higher value means defined later and takes priority when determining
	// cascading order (CSS2 TR 6.4.1)

	StyleFromFile_t *m_pNext;						// allows chaining of objects with the same same symbol in CStyleFile
};


//-----------------------------------------------------------------------------
// Purpose: Represents a single keyframe
//-----------------------------------------------------------------------------
class CStyleKeyFrame
{
public:
	CStyleKeyFrame( float flPercent, EAnimationTimingFunction eTimingFunc, const CCubicBezierCurve<Vector2D> &cubicBezier, StylePropertyHash_t *pProperties )
	{
		m_flPercent = flPercent;
		m_eTimingFunction = eTimingFunc;
		m_CubicBezier = cubicBezier;
		m_pProperties = pProperties;
	}

	~CStyleKeyFrame()
	{
		if( m_pProperties )
		{
			FOR_EACH_HASHMAP( *m_pProperties, i )
			{
				CStyleProperty *pProperty = m_pProperties->Element( i );
				if( pProperty )
					UIEngine()->UIStyleFactory()->FreeStyleProperty( pProperty );
			}

			SAFE_DELETE( m_pProperties );
		}
	}

	float GetPercent() const { return m_flPercent; }
	EAnimationTimingFunction GetTimingFunction() const { return m_eTimingFunction; }
	const CCubicBezierCurve< Vector2D >& GetCubicBezier() const { return m_CubicBezier; }
	StylePropertyHash_t *GetProperties() const { return m_pProperties; }

	// ordered by percent
	bool operator<(const CStyleKeyFrame &rhs) const { return m_flPercent < rhs.m_flPercent; }

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();

		ValidatePtr( m_pProperties );

		// Don't validate each individual property, they are pooled and the pool is validated elsewhere
		FOR_EACH_MAP_FAST( *m_pProperties, i )
		{
			(*m_pProperties)[i]->Validate( validator, "CStyleProperty" );
		}
	}
#endif

private:
	CStyleKeyFrame();
	CStyleKeyFrame( const CStyleKeyFrame &rhs );
	CStyleKeyFrame& operator=(const CStyleKeyFrame &rhs) const;

	float m_flPercent;
	EAnimationTimingFunction m_eTimingFunction;		// each keyframe can override the timing funciton
	CCubicBezierCurve<Vector2D> m_CubicBezier;
	StylePropertyHash_t *m_pProperties;
};

typedef CStyleKeyFrame* CStyleKeyFramePtr;
bool StyleKeyFrameLessPtr( const CStyleKeyFramePtr &lhs, const CStyleKeyFramePtr &rhs, void *pCtx );
#if defined( SOURCE2_PANORAMA )
class CUtlSortVectorCStyleKeyFramePtrLess
{
public:
	bool Less( const CStyleKeyFramePtr& lhs, const CStyleKeyFramePtr& rhs, void * )
	{
		return *lhs < *rhs;
	}
};

typedef CUtlSortVector< CStyleKeyFrame*, CUtlSortVectorCStyleKeyFramePtrLess > VecKeyFrames_t;
#else
typedef CUtlSortVector< CStyleKeyFrame* > VecKeyFrames_t;
#endif


//-----------------------------------------------------------------------------
// Purpose: Represents an animation
//-----------------------------------------------------------------------------
class CStyleAnimation
{
public:
	CStyleAnimation( CPanoramaSymbol symName, CPanoramaSymbol symStyleFile, uint unFileLocation ) 
#if !defined( SOURCE2_PANORAMA )
: m_vecKeyFrames( StyleKeyFrameLessPtr )
#endif
	{
		m_symName = symName;
		m_symStyleFile = symStyleFile;
		m_unFileLocation = unFileLocation;
	}

	~CStyleAnimation()
	{
		ClearFrames();
	}

	void ClearFrames()
	{
		m_vecKeyFrames.PurgeAndDeleteElements();
	}

	void InsertFrame( CStyleKeyFrame *pFrame )
	{
		int iVec = m_vecKeyFrames.Find( pFrame );
		if( iVec != m_vecKeyFrames.InvalidIndex() )
		{
			delete m_vecKeyFrames.Element( iVec );
			m_vecKeyFrames.Remove( iVec );
		}

		m_vecKeyFrames.Insert( pFrame );
	}

	CPanoramaSymbol GetName() const { return m_symName; }
	CPanoramaSymbol GetStyleFile() const { return m_symStyleFile; }
	uint GetFileLocation() const { return m_unFileLocation; }
	const VecKeyFrames_t &GetFrames() const { return m_vecKeyFrames; }

#ifdef DBGFLAG_VALIDATE
	void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();

		ValidateObj( m_vecKeyFrames );
		FOR_EACH_VEC( m_vecKeyFrames, i )
		{
			ValidatePtr( m_vecKeyFrames[i] );
		}
	}
#endif

private:
	CStyleAnimation();
	CStyleAnimation( const CStyleAnimation &rhs );
	CStyleAnimation& operator=(const CStyleAnimation &rhs) const;

	CPanoramaSymbol m_symName;				// name of animation
	CPanoramaSymbol m_symStyleFile;			// path to style file this animation was created from
	uint m_unFileLocation;				// location within style file for this animation
	VecKeyFrames_t m_vecKeyFrames;		// frames for animation
};

//-----------------------------------------------------------------------------
// Purpose: Position property
//-----------------------------------------------------------------------------
class CStylePropertyPosition : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	static const CStyleSymbol symbolX;
	static const CStyleSymbol symbolY;
	static const CStyleSymbol symbolZ;

	CStylePropertyPosition() : CStyleProperty( CStylePropertyPosition::symbol )
	{

	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyPosition::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyPosition *p = (CStylePropertyPosition *)pTarget;

		if( !p->x.IsSet() )
			p->x = x;

		if( !p->y.IsSet() )
			p->y = y;

		if( !p->z.IsSet() )
			p->z = z;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ );

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return true; }

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		bool bSuccess = false;
		if( symParsedName == symbol )
		{
			bSuccess = CSSHelpers::BParseIntoUILength( &x, pchString, &pchString ) &&
				CSSHelpers::BParseIntoUILength( &y, pchString, &pchString ) &&
				CSSHelpers::BParseIntoUILength( &z, pchString, &pchString );
		}
		else if( symParsedName == symbolX )
		{
			bSuccess = CSSHelpers::BParseIntoUILength( &x, pchString, &pchString );
		}
		else if( symParsedName == symbolY )
		{
			bSuccess = CSSHelpers::BParseIntoUILength( &y, pchString, &pchString );
		}
		else if( symParsedName == symbolZ )
		{
			bSuccess = CSSHelpers::BParseIntoUILength( &z, pchString, &pchString );
		}

		return bSuccess;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		x.ScaleLengthValue( flScaleFactor );
		y.ScaleLengthValue( flScaleFactor );
		z.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, x );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, y );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, z );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found by looking at lower weight styles
	virtual bool BFullySet()
	{
		return (x.IsSet() && y.IsSet() && z.IsSet());
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		if( !x.IsSet() )
			x.SetLength( 0.0f );

		if( !y.IsSet() )
			y.SetLength( 0.0f );

		if( !z.IsSet() )
			z.SetLength( 0.0f );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets the x, y, z position for a panel. Must not be in a flowing layout.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"position: 3% 20px 0px;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyPosition &rhs = (const CStylePropertyPosition&)other;
		return (x == rhs.x && y == rhs.y && z == rhs.z);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		return k_EStyleInvalidateLayoutNone;
	}

	CUILength x, y, z;
};


//-----------------------------------------------------------------------------
// Purpose: PerspectiveOrigin property
//-----------------------------------------------------------------------------
class CStylePropertyTransformOrigin : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline void GetDefault( CUILength *x, CUILength *y, bool *pbParent )
	{
		x->SetPercent( 50 );
		y->SetPercent( 50 );
		*pbParent = false;
	}

	CStylePropertyTransformOrigin() : CStyleProperty( CStylePropertyTransformOrigin::symbol )
	{
		GetDefault( &x, &y, &m_bParentRelative );
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTransformOrigin::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyTransformOrigin *p = (CStylePropertyTransformOrigin *)pTarget;
		p->x = x;
		p->y = y;
		p->m_bParentRelative = m_bParentRelative;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ );

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		bool bSuccess = (CSSHelpers::BParseIntoUILength( &x, pchString, &pchString ) && CSSHelpers::BParseIntoUILength( &y, pchString, &pchString ));
		pchString = CSSHelpers::SkipSpaces( pchString );
		if( V_strnicmp( pchString, "parent-relative", V_strlen( "parent-relative" ) ) == 0 )
			m_bParentRelative = true;
		else if( V_strlen( pchString ) > 0 )
			return false;

		return bSuccess;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		x.ScaleLengthValue( flScaleFactor );
		y.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, x );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, y );
		if( m_bParentRelative )
			pfmtBuffer->Append( " parent-relative" );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets the transform origin about which transforms will be applied.  Default is 50% 50% on the panel so a rotation/scale is centered.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"transform-origin: 50% 50%"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyTransformOrigin &rhs = (const CStylePropertyTransformOrigin&)other;
		return (x == rhs.x && y == rhs.y && m_bParentRelative == rhs.m_bParentRelative);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	CUILength x;
	CUILength y;
	bool m_bParentRelative;
};

//-----------------------------------------------------------------------------
// Purpose: PerspectiveOrigin property
//-----------------------------------------------------------------------------
class CStylePropertyPerspectiveOrigin : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline void GetDefault( CUILength *x, CUILength *y, bool *pbInvert )
	{
		x->SetPercent( 50 );
		y->SetPercent( 50 );
		*pbInvert = false;
	}

	CStylePropertyPerspectiveOrigin() : CStyleProperty( CStylePropertyPerspectiveOrigin::symbol )
	{
		GetDefault( &x, &y, &m_bInvert );
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyPerspectiveOrigin::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyPerspectiveOrigin *p = (CStylePropertyPerspectiveOrigin *)pTarget;
		p->x = x;
		p->y = y;
		p->m_bInvert = m_bInvert;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ );

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		bool bSuccess = (CSSHelpers::BParseIntoUILength( &x, pchString, &pchString ) && CSSHelpers::BParseIntoUILength( &y, pchString, &pchString ));
		pchString = CSSHelpers::SkipSpaces( pchString );
		if( V_strnicmp( pchString, "invert", V_strlen( "invert" ) ) == 0 )
			m_bInvert = true;
		else if( V_strlen( pchString ) > 0 )
			return false;
		return bSuccess;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		x.ScaleLengthValue( flScaleFactor );
		y.ScaleLengthValue( flScaleFactor );
	}

	// Handle getting applied to a panel
	virtual void OnAppliedToPanel( IUIPanel *pPanel )
	{

	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, x );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, y );
		if( m_bInvert )
			pfmtBuffer->Append( " invert" );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets the perspective origin which will be used when transforming children of this panel.  This can be "
			"thought of as the camera x/y position relative to the panel.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"perspective-origin: 50% 50%;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyPerspectiveOrigin &rhs = (const CStylePropertyPerspectiveOrigin&)other;
		return (x == rhs.x && y == rhs.y && m_bInvert == rhs.m_bInvert);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	bool m_bInvert;
	CUILength x;
	CUILength y;
};


//-----------------------------------------------------------------------------
// Purpose: Perspective property
//-----------------------------------------------------------------------------
class CStylePropertyPerspective : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline float GetDefault() { return 1000.0f; }

	CStylePropertyPerspective() : CStyleProperty( CStylePropertyPerspective::symbol )
	{
		perspective = GetDefault();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyPerspective::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyPerspective *p = (CStylePropertyPerspective *)pTarget;
		p->perspective = perspective;
	}

	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		perspective *= flScaleFactor;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		if( target.GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyPerspective::Interpolate" );
			return;
		}

		const CStylePropertyPerspective *p = (const CStylePropertyPerspective *)&target;
		perspective = perspective + (p->perspective - perspective) * flProgress;
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( !CSSHelpers::BParseNumber( &perspective, pchString ) )
		{
			return CSSHelpers::BParseLength( &perspective, pchString );
		}
		return true;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendFloat( pfmtBuffer, perspective );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets the perspective depth space available for children of the panel.  Default of 1000 would mean that "
			"children at 1000px zpos are right at the viewers eye, -1000px are just out of view distance faded to nothing.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"perspective: 1000;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyPerspective &rhs = (const CStylePropertyPerspective&)other;
		return (perspective == rhs.perspective);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	float perspective;
};


//-----------------------------------------------------------------------------
// Purpose: z-index, this determines z sorting within panels that are peers with indentical actual zpos
//-----------------------------------------------------------------------------
class CStylePropertyZIndex : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline float GetDefault() { return 0.0f; }

	CStylePropertyZIndex() : CStyleProperty( CStylePropertyZIndex::symbol )
	{
		zindex = GetDefault();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyZIndex::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyZIndex *p = (CStylePropertyZIndex *)pTarget;
		p->zindex = zindex;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		if( target.GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyZIndex::Interpolate" );
			return;
		}

		const CStylePropertyZIndex *p = (const CStylePropertyZIndex *)&target;
		zindex = zindex + (p->zindex - zindex) * flProgress;
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( !CSSHelpers::BParseNumber( &zindex, pchString ) )
		{
			return false;
		}
		return true;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendFloat( pfmtBuffer, zindex );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets the z-index for a panel, panels will be sorted and painted in order within a parent panel.  The sorting "
			"first sorts by the z-pos computed from position and transforms, then if panels have matching zpos zindex is used. "
			"z-index is different than z-pos in that it doesn't affect rendering perspective, just paint/hit-test ordering. "
			"The default z-index value is 0, and any floating point value is accepted.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"z-index: 1;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyZIndex &rhs = (const CStylePropertyZIndex&)other;
		return (zindex == rhs.zindex);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	float zindex;
};


//-----------------------------------------------------------------------------
// Purpose: Perspective property
//-----------------------------------------------------------------------------
class CStylePropertyOpacity : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline float GetDefault() { return 1.0f; }

	CStylePropertyOpacity() : CStyleProperty( CStylePropertyOpacity::symbol )
	{
		opacity = GetDefault();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyOpacity::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyOpacity *p = (CStylePropertyOpacity *)pTarget;
		p->opacity = opacity;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		if( target.GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyOpacity::Interpolate" );
			return;
		}

		const CStylePropertyOpacity *p = (const CStylePropertyOpacity *)&target;
		opacity = opacity + (p->opacity - opacity) * flProgress;
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		return (CSSHelpers::BParseNumber( &opacity, pchString, &pchString ));
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendFloat( pfmtBuffer, opacity );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets the opacity or amount of transparency applied to the panel and all it's children during composition. "
			"Default of 1.0 means fully opaque, 0.0 means fully transparent.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"opacity: 0.8;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyOpacity &rhs = (const CStylePropertyOpacity&)other;
		return (opacity == rhs.opacity);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		return k_EStyleInvalidateLayoutNone;
	}

	float opacity;
};


//-----------------------------------------------------------------------------
// Purpose: scale-2d-centered property
//-----------------------------------------------------------------------------
class CStylePropertyScale2DCentered : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline float GetDefaultX() { return 1.0f; }
	static inline float GetDefaultY() { return 1.0f; }

	CStylePropertyScale2DCentered() : CStyleProperty( CStylePropertyScale2DCentered::symbol )
	{
		m_flX = GetDefaultX();
		m_flY = GetDefaultY();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyScale2DCentered::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyScale2DCentered *p = (CStylePropertyScale2DCentered *)pTarget;
		p->m_flX = m_flX;
		p->m_flY = m_flY;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		if( target.GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyScale2DCentered::Interpolate" );
			return;
		}

		const CStylePropertyScale2DCentered *p = (const CStylePropertyScale2DCentered *)&target;
		m_flX = m_flX + (p->m_flX - m_flX) * flProgress;
		m_flY = m_flY + (p->m_flY - m_flY) * flProgress;
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		// should be "x, y"
		CUtlVector< float > vecValues;
		if( !CSSHelpers::BParseCommaSepList( &vecValues, CSSHelpers::BParseNumber, pchString ) )
			return false;
		if( vecValues.Count() < 1 || vecValues.Count() > 2 )
			return false;

		if( vecValues.Count() == 2 )
		{
			m_flX = vecValues[0];
			m_flY = vecValues[1];
		}
		else if( vecValues.Count() == 1 )
		{
			m_flX = m_flY = vecValues[0];
		}

		return true;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendFloat( pfmtBuffer, m_flX );
		pfmtBuffer->Append( ", " );
		CSSHelpers::AppendFloat( pfmtBuffer, m_flY );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets 2 dimensional X/Y scale factors that apply to the quad for this panel prior to 3 dimensional transforms. "
			"This scaling applies without perspective and leaves the panel centered at the same spot as it started. "
			"Default of 1.0 means no scaling, 0.5 would be half size.<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"pre-transform-scale2d: 0.8\n"
			"pre-transform-scale2d: 0.4, 0.6"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyScale2DCentered &rhs = (const CStylePropertyScale2DCentered&)other;
		return (m_flX == rhs.m_flX && m_flY == rhs.m_flY);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	float m_flX, m_flY;
};


//-----------------------------------------------------------------------------
// Purpose: pre-transform-rotate2d property
//-----------------------------------------------------------------------------
class CStylePropertyRotate2DCentered : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline float GetDefault() { return 0.0f; }

	CStylePropertyRotate2DCentered() : CStyleProperty( CStylePropertyRotate2DCentered::symbol )
	{
		m_flDegrees = GetDefault();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyRotate2DCentered::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyRotate2DCentered *p = (CStylePropertyRotate2DCentered *)pTarget;
		p->m_flDegrees = m_flDegrees;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		if( target.GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyScale2DCentered::Interpolate" );
			return;
		}

		const CStylePropertyRotate2DCentered *p = (const CStylePropertyRotate2DCentered *)&target;
		m_flDegrees = m_flDegrees + (p->m_flDegrees - m_flDegrees) * flProgress;
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		// should be "degrees"
		CUtlVector< float > vecValues;
		if( !CSSHelpers::BParseCommaSepList( &vecValues, CSSHelpers::BParseAngle, pchString ) )
			return false;
		if( vecValues.Count() < 1 || vecValues.Count() > 1 )
			return false;

		m_flDegrees = vecValues[0];

		return true;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendFloat( pfmtBuffer, m_flDegrees );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets 2 dimensional rotation degrees that apply to the quad for this panel prior to 3 dimensional transforms. "
			"This rotation applies without perspective and leaves the panel centered at the same spot as it started.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"pre-transform-rotate2d: 45deg;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyRotate2DCentered &rhs = (const CStylePropertyRotate2DCentered&)other;
		return (m_flDegrees == rhs.m_flDegrees);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	float m_flDegrees;
};



//-----------------------------------------------------------------------------
// Purpose: Perspective property
//-----------------------------------------------------------------------------
class CStylePropertyDesaturation : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline double GetDefault() { return 0.0f; }

	CStylePropertyDesaturation() : CStyleProperty( CStylePropertyDesaturation::symbol )
	{
		desaturation = GetDefault();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyDesaturation::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyDesaturation *p = (CStylePropertyDesaturation *)pTarget;
		p->desaturation = desaturation;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		if( target.GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyDesaturation::Interpolate" );
			return;
		}

		const CStylePropertyDesaturation *p = (const CStylePropertyDesaturation *)&target;
		desaturation = desaturation + (p->desaturation - desaturation) * flProgress;
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( V_strnicmp( pchString, "none", V_strlen( "none" ) ) == 0 )
		{
			desaturation = 0.0f;
			return true;
		}

		return (CSSHelpers::BParseNumber( &desaturation, pchString, &pchString ));
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendFloat( pfmtBuffer, desaturation );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets the amount of desaturation to apply to the panel and all it's children during composition.  "
			"Default of 0.0 means no adjustment, 1.0 means fully desaturated to gray scale.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"desaturation: 0.6;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyDesaturation &rhs = (const CStylePropertyDesaturation&)other;
		return (desaturation == rhs.desaturation);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	float desaturation;
};


//-----------------------------------------------------------------------------
// Purpose: Perspective property
//-----------------------------------------------------------------------------
class CStylePropertyBlur : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline double GetDefaultStdDev() { return 0.0f; }
	static inline double GetDefaultPasses() { return 1.0f; }

	CStylePropertyBlur() : CStyleProperty( CStylePropertyBlur::symbol )
	{
		stddevhor = GetDefaultStdDev();
		stddevver = GetDefaultStdDev();
		passes = GetDefaultPasses();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyBlur::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyBlur *p = (CStylePropertyBlur *)pTarget;
		p->stddevhor = stddevhor;
		p->stddevver = stddevver;
		p->passes = passes;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		if( target.GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyBlur::Interpolate" );
			return;
		}

		const CStylePropertyBlur *p = (const CStylePropertyBlur *)&target;
		stddevhor = stddevhor + (p->stddevhor - stddevhor) * flProgress;
		stddevver = stddevver + (p->stddevver - stddevver) * flProgress;
		passes = passes + (p->passes - passes) * flProgress;
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( !CSSHelpers::BParseGaussianBlur( passes, stddevhor, stddevver, pchString, &pchString ) )
			return false;

		return true;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		if( stddevhor == 0.0f && stddevver == 0.0f && passes == 0.0f )
		{
			pfmtBuffer->Append( "none" );
		}
		else
		{
			pfmtBuffer->Append( "gaussian( " );
			CSSHelpers::AppendFloat( pfmtBuffer, stddevhor );
			pfmtBuffer->Append( ", " );
			CSSHelpers::AppendFloat( pfmtBuffer, stddevver );
			pfmtBuffer->Append( ", " );
			CSSHelpers::AppendFloat( pfmtBuffer, passes );
			pfmtBuffer->Append( ")" );
		}
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets the amount of blur to apply to the panel and all it's children during composition.  "
			"Default is no blur, for now Gaussian is the only blur type and takes a horizontal standard deviation, "
			"vertical standard deviation, and number of passes.  Good std deviation values are around 0-10, if 10 is "
			"still not intense enough consider more passes, but more than one pass is bad for perf.  As shorthand you can "
			"specify with just one value, which will be used for the standard deviation in both directions and 1 pass "
			"will be set.<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"blur: gaussian( 2.5 );\n"
			"blur: gaussian( 6, 6, 1 );"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyBlur &rhs = (const CStylePropertyBlur&)other;
		return (stddevhor == rhs.stddevhor && stddevver == rhs.stddevver && passes == rhs.passes);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	float stddevhor;
	float stddevver;
	float passes;
};


//-----------------------------------------------------------------------------
// Purpose: Box shadow property
//-----------------------------------------------------------------------------
class CStylePropertyBoxShadow : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static Color GetDefaultShadowColor() { return Color( 33, 33, 33, 80 ); }

	CStylePropertyBoxShadow() : CStyleProperty( CStylePropertyBoxShadow::symbol )
	{
		m_bFill = false;
		m_bInset = false;
		m_ShadowColor = GetDefaultShadowColor();
		m_HorizontalOffset.SetLength( 0 );
		m_VerticalOffset.SetLength( 0 );
		m_SpreadDistance.SetLength( 0 );
		m_BlurRadius.SetLength( 0 );
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyBoxShadow::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyBoxShadow *p = (CStylePropertyBoxShadow *)pTarget;
		p->m_bInset = m_bInset;
		p->m_bFill = m_bFill;
		p->m_HorizontalOffset = m_HorizontalOffset;
		p->m_VerticalOffset = m_VerticalOffset;
		p->m_SpreadDistance = m_SpreadDistance;
		p->m_BlurRadius = m_BlurRadius;
		p->m_ShadowColor = m_ShadowColor;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ );

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		// <shadow> = inset? && [ <length>{2,4} && <color>? ]
		// Browsers let color/inset appear before or after lengths, we will too.

		if( V_strnicmp( pchString, "none", V_strlen( "none" ) ) == 0 )
		{
			m_bInset = false;
			m_BlurRadius.SetLength( 0.0f );
			m_HorizontalOffset.SetLength( 0.0f );
			m_VerticalOffset.SetLength( 0.0f );
			m_ShadowColor.SetColor( 0, 0, 0, 0 );
			m_SpreadDistance.SetLength( 0.0f );
			return true;
		}

		bool bInsetParsed = false;
		bool bColorParsed = false;
		bool bLengthsParsed = false;
		bool bFillParsed = false;
		while( 1 )
		{
			pchString = CSSHelpers::SkipSpaces( pchString );

			if( !bInsetParsed && V_strnicmp( pchString, "inset", V_strlen( "inset" ) ) == 0 )
			{
				pchString += V_strlen( "inset" );
				bInsetParsed = true;
				m_bInset = true;
				continue;
			}
			if( !bFillParsed && V_strnicmp( pchString, "fill", V_strlen( "fill" ) ) == 0 )
			{
				pchString += V_strlen( "fill" );
				bFillParsed = true;
				m_bFill = true;
				continue;
			}
			else if( !bColorParsed && CSSHelpers::BParseColor( &m_ShadowColor, pchString, &pchString ) )
			{
				bColorParsed = true;
				continue;
			}
			else if( !bLengthsParsed )
			{
				bLengthsParsed = true;
				if( !CSSHelpers::BParseIntoUILength( &m_HorizontalOffset, pchString, &pchString ) )
				{
					return false;
				}

				if( !CSSHelpers::BParseIntoUILength( &m_VerticalOffset, pchString, &pchString ) )
				{
					return false;
				}

				// These two are optional
				CSSHelpers::BParseIntoUILength( &m_BlurRadius, pchString, &pchString );
				CSSHelpers::BParseIntoUILength( &m_SpreadDistance, pchString, &pchString );
			}
			else
			{
				// Lengths are the only thing required, if we get here and have them, we are good.
				if( bLengthsParsed )
				{
					return true;
				}

				return false;
			}

		}
		return true;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_HorizontalOffset.ScaleLengthValue( flScaleFactor );
		m_VerticalOffset.ScaleLengthValue( flScaleFactor );

		// Although this is a blur pixel radius in css terminology it's really a stddev for our gaussian blur internally,
		// hence it should not scale.

		//m_BlurRadius.ScaleLengthValue( flScaleFactor );
		m_SpreadDistance.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		if( m_bInset )
			pfmtBuffer->Append( "inset " );
		if( m_bFill )
			pfmtBuffer->Append( "fill " );

		CSSHelpers::AppendUILength( pfmtBuffer, m_HorizontalOffset );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_VerticalOffset );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_BlurRadius );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_SpreadDistance );
		pfmtBuffer->Append( " " );

		CSSHelpers::AppendColor( pfmtBuffer, m_ShadowColor );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies outer shadows for boxes, or inset shadows/glows.  The shadow shape will match the border box for the panel,"
			"so use border-radius to affect rounding.  Syntax takes optional 'inset', optional 'fill' then color, and then horizontal offset pixels, "
			"vertical offset pixels, blur radius pixels, and spread distance in pixels. Inset means the shadow is an inner shadow/glow, fill is valid"
			"only on outer shadows and means draw the shadow behind the entire box, not clipping it to outside the border area only.<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"box-shadow: #ffffff80 4px 4px 8px 0px; // outer\n"
			"box-shadow: fill #ffffff80 4px 4px 8px 0px; // outer, filled\n"
			"box-shadow: inset #333333b0 0px 0px 8px 12px; // inner"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyBoxShadow &rhs = (const CStylePropertyBoxShadow&)other;
		return (m_bFill == rhs.m_bFill && m_bInset == rhs.m_bInset && m_HorizontalOffset == rhs.m_HorizontalOffset &&
			m_VerticalOffset == rhs.m_VerticalOffset && m_BlurRadius == rhs.m_BlurRadius && m_SpreadDistance == rhs.m_SpreadDistance &&
			m_ShadowColor == rhs.m_ShadowColor);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	bool m_bFill;
	bool m_bInset;
	CUILength m_HorizontalOffset;
	CUILength m_VerticalOffset;
	CUILength m_BlurRadius;
	CUILength m_SpreadDistance;
	Color m_ShadowColor;

};


//-----------------------------------------------------------------------------
// Purpose: Text shadow property
//-----------------------------------------------------------------------------
class CStylePropertyTextShadow : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static Color GetDefaultShadowColor() { return Color( 33, 33, 33, 80 ); }

	CStylePropertyTextShadow() : CStyleProperty( CStylePropertyTextShadow::symbol )
	{
		m_ShadowColor = GetDefaultShadowColor();
		m_HorizontalOffset.SetLength( 0 );
		m_VerticalOffset.SetLength( 0 );
		m_BlurRadius.SetLength( 0 );
		m_flStrength = 1.0f;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if ( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTextShadow::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyTextShadow *p = (CStylePropertyTextShadow *)pTarget;
		p->m_HorizontalOffset = m_HorizontalOffset;
		p->m_VerticalOffset = m_VerticalOffset;
		p->m_BlurRadius = m_BlurRadius;
		p->m_ShadowColor = m_ShadowColor;
		p->m_flStrength = m_flStrength;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ );

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if ( V_strnicmp( pchString, "none", V_strlen( "none" ) ) == 0 )
		{
			m_BlurRadius.SetLength( 0.0f );
			m_flStrength = 1.0f;
			m_HorizontalOffset.SetLength( 0.0f );
			m_VerticalOffset.SetLength( 0.0f );
			m_ShadowColor.SetColor( 0, 0, 0, 0 );
			return true;
		}

		bool bColorParsed = false;
		bool bLengthsParsed = false;
		while ( 1 )
		{
			pchString = CSSHelpers::SkipSpaces( pchString );

			if ( !bColorParsed && CSSHelpers::BParseColor( &m_ShadowColor, pchString, &pchString ) )
			{
				bColorParsed = true;
				continue;
			}
			else if ( !bLengthsParsed )
			{
				bLengthsParsed = true;
				if ( !CSSHelpers::BParseIntoUILength( &m_HorizontalOffset, pchString, &pchString ) )
				{
					return false;
				}

				if ( !CSSHelpers::BParseIntoUILength( &m_VerticalOffset, pchString, &pchString ) )
				{
					return false;
				}

				// These two are optional
				CSSHelpers::BParseIntoUILength( &m_BlurRadius, pchString, &pchString );
				CSSHelpers::BParseNumber( &m_flStrength, pchString, &pchString );
			}
			else
			{
				// Lengths are the only thing required, if we get here and have them, we are good.
				if ( bLengthsParsed )
				{
					return true;
				}

				return false;
			}

		}
		return true;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_HorizontalOffset.ScaleLengthValue( flScaleFactor );
		m_VerticalOffset.ScaleLengthValue( flScaleFactor );

		// Although this is a blur pixel radius in css terminology it's really a stddev for our gaussian blur internally,
		// hence it should not scale.

		//m_BlurRadius.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, m_HorizontalOffset );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_VerticalOffset );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_BlurRadius );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendFloat( pfmtBuffer, m_flStrength );
		pfmtBuffer->Append( " " );

		CSSHelpers::AppendColor( pfmtBuffer, m_ShadowColor );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies text shadows.  The shadow shape will match the text the panel can generate,"
			"and this is only meaningful for labels.  Syntax takes horizontal offset pixels, "
			"vertical offset pixels, blur radius pixels, strength, and then shadow color.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"text-shadow: 2px 2px 8px 3.0 #333333b0;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if ( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyTextShadow &rhs = (const CStylePropertyTextShadow&)other;
		return ( m_HorizontalOffset == rhs.m_HorizontalOffset &&
			m_VerticalOffset == rhs.m_VerticalOffset && 
			m_BlurRadius == rhs.m_BlurRadius &&
			m_ShadowColor == rhs.m_ShadowColor &&
			m_flStrength == rhs.m_flStrength );
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	CUILength m_HorizontalOffset;
	CUILength m_VerticalOffset;
	CUILength m_BlurRadius;
	float m_flStrength;
	Color m_ShadowColor;
};


class CStylePropertyClip : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;

	CStylePropertyClip() : CStyleProperty( CStylePropertyClip::symbol )
	{

	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyClip::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyClip *p = (CStylePropertyClip *)pTarget;

		if ( !p->m_Left.IsSet() )
			p->m_Left = m_Left;
		if( !p->m_Top.IsSet() )
			p->m_Top = m_Top;
		if( !p->m_Right.IsSet() )
			p->m_Right = m_Right;
		if( !p->m_Bottom.IsSet() )
			p->m_Bottom = m_Bottom;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ );

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( V_strnicmp( pchString, "none", V_strlen( "none" ) ) == 0 )
		{
			return true;
		}

		return CSSHelpers::BParseRect( &m_Top, &m_Right, &m_Bottom, &m_Left, pchString );
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_Left.ScaleLengthValue( flScaleFactor );
		m_Top.ScaleLengthValue( flScaleFactor );
		m_Right.ScaleLengthValue( flScaleFactor );
		m_Bottom.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		pfmtBuffer->Append( "rect( " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_Top );
		pfmtBuffer->Append( ", " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_Right );
		pfmtBuffer->Append( ", " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_Bottom );
		pfmtBuffer->Append( ", " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_Left );
		pfmtBuffer->Append( ")" );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies a clip region within the panel, where contents will be clipped at render time. "
			"This clipping has no impact on layout, and is fast and supported for transitions/animations.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"clip: rect( 10%, 90%, 90%, 10% );"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyClip &rhs = (const CStylePropertyClip&)other;
		return (m_Left == rhs.m_Left &&
			m_Right == rhs.m_Right &&
			m_Top == rhs.m_Top &&
			m_Bottom == rhs.m_Bottom );
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		if( !m_Left.IsSet() ) 
			m_Left.SetPercent( 0 );

		if( !m_Top.IsSet() )
			m_Top.SetPercent( 0 );

		if( !m_Right.IsSet() )
			m_Right.SetPercent( 100 );

		if( !m_Bottom.IsSet() )
			m_Bottom.SetPercent( 100 );
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	CUILength m_Left;
	CUILength m_Top;
	CUILength m_Right;
	CUILength m_Bottom;
};


//-----------------------------------------------------------------------------
// Purpose: color wash property, applies basically like drawing the color over
// the top of the panel after all child rendering is complete, alpha value affects
// how much of the child drawing colors come through.
//-----------------------------------------------------------------------------
class CStylePropertyWashColor : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline Color GetDefault() { return Color( 255, 255, 255, 255 ); }

	CStylePropertyWashColor() : CStyleProperty( CStylePropertyWashColor::symbol )
	{
		m_color = GetDefault();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyWashColor::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyWashColor *p = (CStylePropertyWashColor *)pTarget;
		p->m_color = m_color;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		if( target.GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyWashColor::Interpolate" );
			return;
		}

		const CStylePropertyWashColor *p = (const CStylePropertyWashColor *)&target;
		int r = m_color.r() + (p->m_color.r() - m_color.r()) * flProgress;
		int b = m_color.b() + (p->m_color.b() - m_color.b()) * flProgress;
		int g = m_color.g() + (p->m_color.g() - m_color.g()) * flProgress;
		int a = m_color.a() + (p->m_color.a() - m_color.a()) * flProgress;

		m_color.SetColor( r, g, b, a );
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( CSSHelpers::BParseColor( &m_color, pchString ) )
		{
			return true;
		}
		else if( V_stricmp( "transparent", pchString ) == 0 )
		{
			m_color.SetColor( 0, 0, 0, 0 );
			return true;
		}

		return false;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendColor( pfmtBuffer, m_color );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies a 'wash' color, which means a color that will be blended over the panel and all it's children at "
			"composition time, tinting them.  The alpha value of the color determines the intensity of the tinting.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"wash-color: #39b0d325;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyWashColor &rhs = (const CStylePropertyWashColor&)other;
		return (m_color == rhs.m_color);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	Color m_color;
};



//-----------------------------------------------------------------------------
// Purpose: Overflow property
//-----------------------------------------------------------------------------
class CStylePropertyOverflow : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	CStylePropertyOverflow() : CStyleProperty( CStylePropertyOverflow::symbol )
	{
		m_eHorizontal = k_EOverflowSquish;
		m_eVertical = k_EOverflowSquish;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyOverflow::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyOverflow *p = (CStylePropertyOverflow *)pTarget;
		p->m_eHorizontal = m_eHorizontal;
		p->m_eVertical = m_eVertical;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) { }

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( !BParseOverflow( &m_eHorizontal, pchString, &pchString ) )
			return false;

		// only 1 value specified?
		pchString = CSSHelpers::SkipSpaces( pchString );
		if( pchString[0] == '\0' )
		{
			m_eVertical = m_eHorizontal;
			return true;
		}

		// get vertical
		if( !BParseOverflow( &m_eVertical, pchString, &pchString ) )
			return false;

		return true;
	}

	bool BParseOverflow( EOverflowValue *peOverflow, const char *pchString, const char **pchAfterParse = NULL )
	{
		char rgchProperty[128];
		if( !CSSHelpers::BParseIdent( rgchProperty, V_ARRAYSIZE( rgchProperty ), pchString, &pchString ) )
			return false;

		// special case none
		if( V_stricmp( rgchProperty, "squish" ) == 0 )
			*peOverflow = k_EOverflowSquish;
		else if( V_stricmp( rgchProperty, "clip" ) == 0 )
			*peOverflow = k_EOverflowClip;
		else if( V_stricmp( rgchProperty, "scroll" ) == 0 )
			*peOverflow = k_EOverflowScroll;
		else if( V_stricmp( rgchProperty, "noclip" ) == 0 )
			*peOverflow = k_EOverflowNoClip;
		else
			return false;

		if( pchAfterParse )
			*pchAfterParse = pchString;

		return true;
	}

	void BAppendOverflow( CFmtStr1024 *pfmtBuffer, const EOverflowValue &eOverflow ) const
	{
		if( eOverflow == k_EOverflowSquish )
			pfmtBuffer->Append( "squish" );
		else if( eOverflow == k_EOverflowClip )
			pfmtBuffer->Append( "clip" );
		else if( eOverflow == k_EOverflowScroll )
			pfmtBuffer->Append( "scroll" );
		else if( eOverflow == k_EOverflowNoClip )
			pfmtBuffer->Append( "noclip" );
		else
			AssertMsg1( false, "Unknown overflow value: %d", eOverflow );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		BAppendOverflow( pfmtBuffer, m_eHorizontal );
		pfmtBuffer->Append( " " );
		BAppendOverflow( pfmtBuffer, m_eVertical );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies what to do with contents that overflow the available space for the panel. "
			"Possible values:<br>"
			"\"squish\" - Children are squished to fit within the panel's bounds if needed (default)<br>"
			"\"clip\" - Children maintain their desired size but their contents are clipped<br>"
			"\"scroll\" - Children maintain their desired size and a scrollbar is added to this panel<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"overflow: squish squish; // squishes contents in horizontal and vertical directions\n"
			"overflow: squish scroll; // scrolls contents in the Y direction"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyOverflow &rhs = (const CStylePropertyOverflow&)other;
		return (m_eHorizontal == rhs.m_eHorizontal && m_eVertical == rhs.m_eVertical);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	EOverflowValue m_eHorizontal;
	EOverflowValue m_eVertical;
};


//-----------------------------------------------------------------------------
// Purpose: Font property
//-----------------------------------------------------------------------------
class CStylePropertyFont : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	static const CStyleSymbol fontFamily;
	static const CStyleSymbol fontSize;
	static const CStyleSymbol fontStyle;
	static const CStyleSymbol fontWeight;

	CStylePropertyFont() : CStyleProperty( CStylePropertyFont::symbol )
	{
		m_flFontSize = k_flFloatNotSet;
		m_eFontStyle = k_EFontStyleUnset;
		m_eFontWeight = k_EFontWeightUnset;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyFont::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyFont *p = (CStylePropertyFont *)pTarget;
		if( p->m_strFontFamily.IsEmpty() )
			p->m_strFontFamily = m_strFontFamily;

		if( p->m_flFontSize == k_flFloatNotSet )
			p->m_flFontSize = m_flFontSize;

		if( p->m_eFontStyle == k_EFontStyleUnset )
			p->m_eFontStyle = m_eFontStyle;

		if( p->m_eFontWeight == k_EFontWeightUnset )
			p->m_eFontWeight = m_eFontWeight;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) { Assert( !"You can't interpolate a font" ); }

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( symParsedName == fontSize )
		{
			bool bSuccess = false;
			if( !CSSHelpers::BParseNumber( &m_flFontSize, pchString ) )
			{
				if( CSSHelpers::BParseLength( &m_flFontSize, pchString ) )
				{
					bSuccess = true;
				}
			}
			else
			{
				bSuccess = true;
			}

			return bSuccess;
		}
		else if( symParsedName == fontFamily )
		{
			char rgchFontFamily[512] = "";

			pchString = CSSHelpers::SkipSpaces( pchString );
			CSSHelpers::BSkipQuote( pchString, &pchString );
			V_strncpy( rgchFontFamily, pchString, V_ARRAYSIZE( rgchFontFamily ) );
			int len = V_strlen( rgchFontFamily );
			if( len > 1 )
			{
				if( rgchFontFamily[len - 1] == '\'' || rgchFontFamily[len - 1] == '"' )
					rgchFontFamily[len - 1] = 0;
			}
			V_StrTrim( rgchFontFamily );

			m_strFontFamily = rgchFontFamily;
			return !m_strFontFamily.IsEmpty();
		}
		else if( symParsedName == fontWeight )
		{
			if( V_stristr( pchString, "normal" ) )
				m_eFontWeight = k_EFontWeightNormal;
			else if( V_stristr( pchString, "medium" ) )
				m_eFontWeight = k_EFontWeightMedium;
			else if( V_stristr( pchString, "bold" ) )
				m_eFontWeight = k_EFontWeightBold;
			else if( V_stristr( pchString, "black" ) )
				m_eFontWeight = k_EFontWeightBlack;
			else if( V_stristr( pchString, "thin" ) )
				m_eFontWeight = k_EFontWeightThin;
			else if ( V_stristr( pchString, "light" ) )
				m_eFontWeight = k_EFontWeightLight;
			else if ( V_stristr( pchString, "semi-bold" ) )
				m_eFontWeight = k_EFontWeightSemiBold;

			if( m_eFontWeight != k_EFontWeightUnset )
				return true;
		}
		else if( symParsedName == fontStyle )
		{
			if( V_stristr( pchString, "normal" ) )
				m_eFontStyle = k_EFontStyleNormal;
			else if( V_stristr( pchString, "italic" ) )
				m_eFontStyle = k_EFontStyleItalic;

			if( m_eFontStyle != k_EFontStyleUnset )
				return true;
		}

		return false;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		if( m_flFontSize != k_flFloatNotSet )
			m_flFontSize *= flScaleFactor;
	}

	// Get suggested values based on current text for style property value
	virtual void GetSuggestedValues( CStyleSymbol symAlias, const char *pchTextSoFar, CUtlVector< CUtlString > &vecSuggestions )
	{
		if( symAlias == fontFamily )
		{
			if( pchTextSoFar[0] == '"' )
				++pchTextSoFar;

			int cchText = V_strlen( pchTextSoFar );
			if( cchText > 0 && pchTextSoFar[cchText - 1] == '"' )
				--cchText;

			const CUtlSortVector< CUtlString > &vecValues = UIEngine()->GetSortedValidFontNames();
			int iVec = vecValues.FindLessOrEqual( pchTextSoFar );
			if( iVec == vecValues.InvalidIndex() )
				iVec = 0;

			while( vecValues.IsValidIndex( iVec ) )
			{
				const char *pchValue = vecValues[iVec].String();

				if( cchText > 0 )
				{
					int nCmp = V_strncmp( pchValue, pchTextSoFar, cchText );
					if( nCmp > 0 )
						break;

					// if less than, keep going
					if( nCmp < 0 )
					{
						iVec++;
						continue;
					}
				}

				CFmtStr1024 strValue( "\"%s\"", vecValues[iVec].String() );
				vecSuggestions.AddToTail( strValue.String() );
				iVec++;
			}
		}
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		pfmtBuffer->AppendFormat( "%s %s ", PchNameFromEFontStyle( m_eFontStyle ), PchNameFromEFontWeight( m_eFontWeight ) );
		CSSHelpers::AppendFloat( pfmtBuffer, m_flFontSize );
		pfmtBuffer->AppendFormat( " %s", m_strFontFamily.String() );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		return (m_flFontSize != k_flFloatNotSet &&
			!m_strFontFamily.IsEmpty() &&
			m_eFontStyle != k_EFontStyleUnset &&
			m_eFontWeight != k_EFontWeightUnset);
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		// leave unset rather than defaulting. We will fix things up when accessed		
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		if( symProperty == fontFamily )
		{
			return "Specifies the font face to use.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"font-family: Arial;\n"
				"font-family: \"Comic Sans MS\";"
				"</pre>";
		}
		else if( symProperty == fontSize )
		{
			return "Specifies the target font face height in pixels.<br><br>"
				"<b>Example:</b>"
				"<pre>"
				"font-size: 12;"
				"</pre>";
		}
		else if( symProperty == fontWeight )
		{
			return "Specifies the font weight to use. Supported values are light, thin, normal, medium, bold, and black.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"font-weight: normal;\n"
				"font-weight: bold;\n"
				"font-weight: thin;"
				"</pre>";
		}
		else if( symProperty == fontStyle )
		{
			return "Specifies the font style to use. Supported values are normal, and italic<br><br>"
				"<b>Example:</b>"
				"<pre>"
				"font-style: normal;"
				"</pre>";
		}

		return CStyleProperty::GetDescription( symProperty );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyFont &rhs = (const CStylePropertyFont&)other;
		return (m_strFontFamily == rhs.m_strFontFamily && m_flFontSize == rhs.m_flFontSize && m_eFontStyle == rhs.m_eFontStyle && m_eFontWeight == rhs.m_eFontWeight);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_strFontFamily );
		CStyleProperty::Validate( validator, pchName );
	}
#endif

	CUtlString m_strFontFamily;
	float m_flFontSize;
	EFontStyle m_eFontStyle;
	EFontWeight m_eFontWeight;
};


//-----------------------------------------------------------------------------
// Purpose: Text align property
//-----------------------------------------------------------------------------
class CStylePropertyTextAlign : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;

	CStylePropertyTextAlign() : CStyleProperty( CStylePropertyTextAlign::symbol )
	{
		m_eAlign = k_ETextAlignUnset;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTextAlign::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyTextAlign *p = (CStylePropertyTextAlign *)pTarget;
		p->m_eAlign = m_eAlign;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) { Assert( !"You can't interpolate a text-align" ); }

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( symParsedName == symbol )
		{
			if( V_stristr( pchString, "left" ) )
				m_eAlign = k_ETextAlignLeft;
			else if( V_stristr( pchString, "center" ) )
				m_eAlign = k_ETextAlignCenter;
			else if( V_stristr( pchString, "right" ) )
				m_eAlign = k_ETextAlignRight;
			else
				return false;

			return true;
		}

		return false;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		pfmtBuffer->Append( PchNameFromETextAlign( m_eAlign ) );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		return (m_eAlign != k_ETextAlignUnset);
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		// bugbug jmccaskey - someday maybe default to right aligned on rtl languages?
		if( m_eAlign == k_ETextAlignUnset )
			m_eAlign = k_ETextAlignLeft;
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies the text alignment for text within this panel, defaults to left.<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"text-align: left;\n"
			"text-align: right;\n"
			"text-align: center;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyTextAlign &rhs = (const CStylePropertyTextAlign&)other;
		return (m_eAlign == rhs.m_eAlign);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	ETextAlign m_eAlign;
};


//-----------------------------------------------------------------------------
// Purpose: Letter spacing property
//-----------------------------------------------------------------------------
class CStylePropertyTextLetterSpacing : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	CStylePropertyTextLetterSpacing() : CStyleProperty( CStylePropertyTextLetterSpacing::symbol )
	{
		m_nLetterSpacing = 0;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if ( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTextLetterSpacing::MergeTo" );
			return;
		}

		CStylePropertyTextLetterSpacing *p = (CStylePropertyTextLetterSpacing *)pTarget;
		p->m_nLetterSpacing = m_nLetterSpacing;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) { Assert( !"You can't interpolate a letter-spacing" ); }

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		bool bSuccess = false;
		pchString = CSSHelpers::SkipSpaces( pchString );
		if ( V_strnicmp( pchString, "normal", V_ARRAYSIZE( "normal" ) ) == 0 )
		{
			m_nLetterSpacing = 0;
			bSuccess = true;
		}
		else
		{
			float flLength;
			bSuccess = CSSHelpers::BParseLength( &flLength, pchString, NULL );
			m_nLetterSpacing = roundf(flLength);
		}
		return bSuccess;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_nLetterSpacing = (float)roundf(m_nLetterSpacing*flScaleFactor);
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendLength( pfmtBuffer, m_nLetterSpacing );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return	"Sets letter-spacing for text in a string. "
			"Possible values:<br>"
			"normal - no manual spacing<br>"
			"&lt;pixels&gt; - Any fixed pixel value (ex: \"1px\")";
	}

	// Comparison function
	virtual bool operator==( const CStyleProperty &other ) const
	{
		if ( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyTextLetterSpacing &rhs = (const CStylePropertyTextLetterSpacing &)other;
		return ( m_nLetterSpacing == rhs.m_nLetterSpacing );
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if ( !pCompareProperty || !( *this == *pCompareProperty ) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	int m_nLetterSpacing;
};


//-----------------------------------------------------------------------------
// Purpose: Text decoration property
//-----------------------------------------------------------------------------
class CStylePropertyTextDecoration : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline ETextDecoration GetDefault() { return k_ETextDecorationNone; }

	CStylePropertyTextDecoration() : CStyleProperty( CStylePropertyTextDecoration::symbol )
	{
		m_eDecoration = GetDefault();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTextDecoration::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyTextDecoration *p = (CStylePropertyTextDecoration *)pTarget;
		p->m_eDecoration = m_eDecoration;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) { Assert( !"You can't interpolate a text-decorations" ); }

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( symParsedName == symbol )
		{
			m_eDecoration = ETextDecorationFromName( pchString );
			return (m_eDecoration != k_ETextDecorationUnset);
		}

		return false;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		pfmtBuffer->Append( PchNameFromETextDecoration( m_eDecoration ) );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		return (m_eDecoration != k_ETextDecorationUnset);
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return	"Specifies the decoration for text within this panel, defaults to none. Possible values: none, underline, line-through.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"text-decoration: underline;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyTextDecoration &rhs = (const CStylePropertyTextDecoration&)other;
		return (m_eDecoration == rhs.m_eDecoration);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	ETextDecoration m_eDecoration;
};


//-----------------------------------------------------------------------------
// Purpose: Text decoration property
//-----------------------------------------------------------------------------
class CStylePropertyTextTransform : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline ETextTransform GetDefault() { return k_ETextTransformNone; }

	CStylePropertyTextTransform() : CStyleProperty( CStylePropertyTextTransform::symbol )
	{
		m_eTransform = GetDefault();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTextDecoration::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyTextTransform *p = (CStylePropertyTextTransform *)pTarget;
		p->m_eTransform = m_eTransform;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) { Assert( !"You can't interpolate a text-decorations" ); }

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( symParsedName == symbol )
		{
			m_eTransform = ETextTransformFromName( pchString );
			return (m_eTransform != k_ETextTransformUnset);
		}

		return false;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		pfmtBuffer->Append( PchNameFromETextTransform( m_eTransform ) );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		return (m_eTransform != k_ETextTransformUnset);
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return	"Specifies the transform for text within this panel, defaults to none. Possible values: none, uppercase, lowercase.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"text-transform: uppercase;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyTextTransform &rhs = (const CStylePropertyTextTransform&)other;
		return (m_eTransform == rhs.m_eTransform);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	ETextTransform m_eTransform;
};


//-----------------------------------------------------------------------------
// Purpose: Line height property
//-----------------------------------------------------------------------------
class CStylePropertyLineHeight : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;

	CStylePropertyLineHeight() : CStyleProperty( CStylePropertyLineHeight::symbol )
	{
		m_flLineHeight = k_flFloatNotSet;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyLineHeight::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyLineHeight *p = (CStylePropertyLineHeight *)pTarget;
		p->m_flLineHeight = m_flLineHeight;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) { Assert( !"You can't interpolate a line height" ); }

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( symParsedName == symbol )
		{
			bool bSuccess = false;
			if( !CSSHelpers::BParseNumber( &m_flLineHeight, pchString ) )
			{
				bSuccess = CSSHelpers::BParseLength( &m_flLineHeight, pchString );
			}
			else
			{
				bSuccess = true;
			}

			return bSuccess;
		}

		return false;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		if( m_flLineHeight != k_flFloatNotSet )
			m_flLineHeight *= flScaleFactor;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendFloat( pfmtBuffer, m_flLineHeight );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		return (m_flLineHeight != k_flFloatNotSet);
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		// Leave unset rather than defaulting, will mean don't apply and let font-size set
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies the line height (distance between top edge of line above and line below) to use for text.  By default "
			"this is unset and a value that matches the font-size reasonably will be used automatically.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"line-height: 20px;"
			"</pre>";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyLineHeight &rhs = (const CStylePropertyLineHeight&)other;
		return (m_flLineHeight == rhs.m_flLineHeight);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	float m_flLineHeight;
};


//-----------------------------------------------------------------------------
// Purpose: Fill color property, this is not a real style property, but instead 
// a base that others derive from if they include a fill color.
//-----------------------------------------------------------------------------
class CStylePropertyFillColor : public CStyleProperty
{
public:

	CStylePropertyFillColor( CStyleSymbol symbol ) : CStyleProperty( symbol )
	{
	}

	virtual ~CStylePropertyFillColor() {}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyFillColor::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyFillColor *p = (CStylePropertyFillColor *)pTarget;
		p->m_FillBrushCollection = m_FillBrushCollection;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property, must implement if you override BCanTransition to return true.
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ );

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		m_FillBrushCollection.Clear();
		return CSSHelpers::BParseFillBrushCollection( &m_FillBrushCollection, pchString, NULL );
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_FillBrushCollection.ScaleLengthValues( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendFillBrushCollection( pfmtBuffer, m_FillBrushCollection );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyFillColor &rhs = (const CStylePropertyFillColor&)other;
		return (m_FillBrushCollection == rhs.m_FillBrushCollection);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_FillBrushCollection );
		CStyleProperty::Validate( validator, pchName );
	}
#endif

	CFillBrushCollection m_FillBrushCollection;
};


//-----------------------------------------------------------------------------
// Purpose: foreground color property
//-----------------------------------------------------------------------------
class CStylePropertyForegroundColor : public CStylePropertyFillColor
{
public:

	static const CStyleSymbol symbol;
	CStylePropertyForegroundColor() : CStylePropertyFillColor( CStylePropertyForegroundColor::symbol )
	{
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets the foreground fill color/gradient/combination for a panel.  This color is the color used to render text within the panel.<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"color: #FFFFFFFF;\n"
			"color: gradient( linear, 0% 0%, 0% 100%, from( #cbcbcbff ), to( #a0a0a0a0 ) );"
			"</pre>";
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		bool bResult = CStylePropertyFillColor::BSetFromString( symParsedName, pchString );

#ifdef _DEBUG
		const CUtlVectorFixed< CFillBrushCollection::FillBrush_t, MAX_FILL_BRUSHES_PER_COLLECTION > &vecBrushes = m_FillBrushCollection.AccessBrushes();
		FOR_EACH_VEC( vecBrushes, i )
		{
			AssertMsg( vecBrushes[i].m_Brush.GetType() != CFillBrush::k_EStrokeTypeParticleSystem, "Particle systems not supported as foreground color, only background.  Won't render." );
		}
#endif

		return bResult;
	}
};


//-----------------------------------------------------------------------------
// Purpose: 3D transform properties
//-----------------------------------------------------------------------------
class CStylePropertyTransform3D : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static inline VMatrix GetDefault() { return VMatrix::GetIdentityMatrix(); }

	CStylePropertyTransform3D() : CStyleProperty( CStylePropertyTransform3D::symbol )
	{
		m_bDirty = true;
		m_bInterpolated = false;
		m_Matrix = VMatrix::GetIdentityMatrix();
		m_flCachedParentWidth = 0.0f;
		m_flCachedParentHeight = 0.0f;
	};

	virtual ~CStylePropertyTransform3D()
	{
		m_vecTransforms.PurgeAndDeleteElements();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		Assert( !m_bInterpolated );
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTransform3D::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyTransform3D *p = (CStylePropertyTransform3D *)pTarget;
		p->m_bDirty = true;
		p->m_Matrix = m_Matrix;
		p->m_vecTransforms.EnsureCapacity( p->m_vecTransforms.Count() + m_vecTransforms.Count() );
		FOR_EACH_VEC( m_vecTransforms, i )
		{
			p->m_vecTransforms.AddToTail( m_vecTransforms[i]->Clone() );
		}
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ );

	bool BHasNon2DTransforms()
	{
		FOR_EACH_VEC( m_vecTransforms, i )
		{
			if( !m_vecTransforms[i]->BOnlyImpacts2DValues() )
				return true;
		}

		return false;
	}

	VMatrix GetTransformMatrix( float flParentWidth, float flParentHeight ) const
	{
		// If dirty, or if called with new width/height (and not interpolated, because once interpolated we can't recompute values), then recompute the matrix
		// with the specified width/height for the parent
		if( m_bDirty || (!m_bInterpolated && (fabs( flParentWidth - m_flCachedParentWidth ) > 0.0001f || fabs( flParentHeight - m_flCachedParentHeight ) > 0.0001f ) ) )
		{
			// Should never be "dirty" when interpolated, this checks that 
			Assert( !m_bInterpolated );

			m_flCachedParentWidth = flParentWidth;
			m_flCachedParentHeight = flParentHeight;
			m_bDirty = false;
			m_Matrix = VMatrix::GetIdentityMatrix();
			FOR_EACH_VEC( m_vecTransforms, i )
			{
				m_Matrix = m_vecTransforms[i]->GetTransformMatrix( flParentWidth, flParentHeight ) * m_Matrix;
			}
		}
		return m_Matrix;
	}

	void AddTransform3D( CTransform3D *pTransform )
	{
		if( !pTransform )
		{
			Assert( pTransform );
			return;
		}

		Assert( !m_bInterpolated );
		m_bDirty = true;
		m_vecTransforms.AddToTail( pTransform );
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		Assert( !m_bInterpolated );
		m_bDirty = true;
		m_vecTransforms.PurgeAndDeleteElements();

		// special case none
		if( V_stricmp( pchString, "none" ) == 0 )
			return true;

		// space separated string of transform-functions
		while( pchString[0] != '\0' )
		{
			CTransform3D *pTransform = NULL;
			if( !CSSHelpers::BParseTransformFunction( &pTransform, pchString, &pchString ) )
				return false;

			Assert( pTransform );
			AddTransform3D( pTransform );
			pchString = CSSHelpers::SkipSpaces( pchString );
		}

		return true;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		if( flScaleFactor == 1.0f )
			return;

		if( !m_bInterpolated )
		{
			FOR_EACH_VEC( m_vecTransforms, i )
			{
				m_vecTransforms[i]->ScaleLengthValues( flScaleFactor );
				m_bDirty = true;
			}
		}
		else
		{
			if( m_Matrix.IsIdentity() )
				return;

			// Can't make an interpolated matrix dirty as we don't have data to recreate it fully, must
			// decompose and scale the appropriate values that way
			DecomposedMatrix_t current = DecomposeTransformMatrix( m_Matrix );
			current.m_flTranslationXYZ[0] *= flScaleFactor;
			current.m_flTranslationXYZ[1] *= flScaleFactor;
			current.m_flTranslationXYZ[2] *= flScaleFactor;
			m_Matrix = RecomposeTransformMatrix( current );
		}
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		if( m_vecTransforms.Count() == 0 )
		{
			pfmtBuffer->Append( "none" );
			return;
		}

		FOR_EACH_VEC( m_vecTransforms, i )
		{
			if( i > 0 )
				pfmtBuffer->Append( "\n" );

			CSSHelpers::AppendTransform( pfmtBuffer, m_vecTransforms[i] );
		}
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets the transforms to apply to the panel in 2d or 3d space.  You can combine various transforms (comma separated) "
			"and they will be applied in order to create a 4x4 3d transform matrix.  The possible operations are: translate3d( x, y, z ), "
			"translatex( x ), translatey( y ), translatez( z ), scale3d( x, y, z), rotate3d( x, y, z ), rotatex( x ), rotatey( y ), rotatez( z ).<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"transform: translate3d( -100px, -100px, 0px );\n"
			"transform: rotateZ( -32deg ) rotateX( 30deg ) translate3d( 125px, 520px, 230px );"
			"</pre>";
	}

	// Comparison function
#ifdef WIN32
#pragma optimize( "", off )
#endif
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyTransform3D &rhs = (const CStylePropertyTransform3D&)other;

		if( m_bInterpolated || rhs.m_bInterpolated )
		{
			AssertMsg( false, "Can't compare two CStylePropertyTransform3D that have been interpolated" );
			return false;
		}

		if( m_vecTransforms.Count() != rhs.m_vecTransforms.Count() )
			return false;

		FOR_EACH_VEC( m_vecTransforms, i )
		{
			CTransform3D *pLHS = m_vecTransforms[i];
			CTransform3D *pRHS = rhs.m_vecTransforms[i];

			ETransform3DType eType1 = pLHS->GetType();
			Assert( eType1 == k_ETransform3DRotate || eType1 == k_ETransform3DTranslate || eType1 == k_ETransform3DScale );
			ETransform3DType eType2 = pRHS->GetType();
			Assert( eType2 == k_ETransform3DRotate || eType2 == k_ETransform3DTranslate || eType2 == k_ETransform3DScale );

			if( *pLHS != *pRHS )
				return false;
		}

		return true;
	}
#ifdef WIN32
#pragma optimize( "", on )
#endif

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_vecTransforms );
		FOR_EACH_VEC( m_vecTransforms, i )
		{
			validator.ClaimMemory( m_vecTransforms[i] );
		}
		CStyleProperty::Validate( validator, pchName );
	}
#endif

private:
	CUtlVector<CTransform3D *> m_vecTransforms;

	mutable float m_flCachedParentWidth;
	mutable float m_flCachedParentHeight;
	mutable bool m_bDirty;
	mutable VMatrix m_Matrix;
	bool m_bInterpolated;			// once interpolated, only m_Matrix will be set. m_vecTransforms will be empty.
};


//-----------------------------------------------------------------------------
// Purpose: Transition property
//-----------------------------------------------------------------------------
class CStylePropertyTransitionProperties : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	static const CStyleSymbol symbolProperty;
	static const CStyleSymbol symbolDuration;
	static const CStyleSymbol symbolTiming;
	static const CStyleSymbol symbolDelay;

	CStylePropertyTransitionProperties() : CStyleProperty( CStylePropertyTransitionProperties::symbol ) { m_bImmediate = false; }

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTransitionProperties::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyTransitionProperties *p = (CStylePropertyTransitionProperties *)pTarget;

		// bugbug cboyd - need to think about what to do when property counts dont match. currently, we do not change the number of properties in the
		// target when a property name has been set as that count should override ours
		bool bPropertiesSet = (p->m_vecTransitionProperties.Count() > 0) ? p->m_vecTransitionProperties[0].m_symProperty.IsValid() : false;

		// add properties to target to match our count
		if( !bPropertiesSet )
		{
			p->m_vecTransitionProperties.EnsureCapacity( m_vecTransitionProperties.Count() );
			for( int i = p->m_vecTransitionProperties.Count(); i < m_vecTransitionProperties.Count(); i++ )
				p->AddNewProperty();
		}

		// set all unset properties on target
		FOR_EACH_VEC( m_vecTransitionProperties, i )
		{
			// we have more properties than the other guy. Stop processing.
			if( bPropertiesSet && p->m_vecTransitionProperties.Count() - 1 < i )
				break;

			TransitionProperty_t &transOther = p->m_vecTransitionProperties[i];
			const TransitionProperty_t &transUs = m_vecTransitionProperties[i];

			if( !transOther.m_symProperty.IsValid() )
				transOther.m_symProperty = transUs.m_symProperty;

			if( transOther.m_flTransitionSeconds == k_flFloatNotSet )
				transOther.m_flTransitionSeconds = transUs.m_flTransitionSeconds;

			if( transOther.m_eTimingFunction == k_EAnimationUnset )
			{
				transOther.m_eTimingFunction = transUs.m_eTimingFunction;
				transOther.m_CubicBezier = transUs.m_CubicBezier;
			}

			if( transOther.m_flTransitionDelaySeconds == k_flFloatNotSet )
				transOther.m_flTransitionDelaySeconds = transUs.m_flTransitionDelaySeconds;
		}

		if( !p->m_bImmediate )
			p->m_bImmediate = m_bImmediate;
	}

	virtual bool BCanTransition() { return false; }
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) {}

	void AddNewProperty()
	{
		int iVec = m_vecTransitionProperties.AddToTail();
		TransitionProperty_t &transition = m_vecTransitionProperties[iVec];

		// copy other set properties forward
		if( iVec == 0 )
		{
			transition.m_eTimingFunction = k_EAnimationUnset;

			Vector2D vec[4];
			
			panorama::GetAnimationCurveControlPoints( k_EAnimationUnset, vec );
			transition.m_CubicBezier.SetControlPoints( vec );

			transition.m_flTransitionSeconds = k_flFloatNotSet;
			transition.m_flTransitionDelaySeconds = k_flFloatNotSet;
		}
		else
		{
			transition.m_eTimingFunction = m_vecTransitionProperties[iVec - 1].m_eTimingFunction;
			transition.m_CubicBezier = m_vecTransitionProperties[iVec - 1].m_CubicBezier;
			transition.m_flTransitionSeconds = m_vecTransitionProperties[iVec - 1].m_flTransitionSeconds;
			transition.m_flTransitionDelaySeconds = m_vecTransitionProperties[iVec - 1].m_flTransitionDelaySeconds;
		}
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		// need a temp buffer that we can terminate if !immediate is on the end of the string
		char rgchBuffer[1024];

		V_strncpy( rgchBuffer, pchString, V_ARRAYSIZE( rgchBuffer ) );
		if( !BParseAndRemoveImmediate( m_bImmediate ? NULL : &m_bImmediate, rgchBuffer ) )
			return false;

		pchString = rgchBuffer;

		if( symParsedName == symbol )
		{
			m_vecTransitionProperties.RemoveAll();

			// transition property format: property duration timing-function delay [,*]
			// we also support 'none'
			if( V_strcmp( "none", pchString ) == 0 )
				return true;

			while( *pchString != '\0' )
			{
				TransitionProperty_t transition;

				// get property name			
				char rgchProperty[k_nCSSPropertyNameMax];
				if( !CSSHelpers::BParseIdent( rgchProperty, V_ARRAYSIZE( rgchProperty ), pchString, &pchString ) )
					return false;

				transition.m_symProperty = panorama::CStyleSymbol( rgchProperty );

				if( !BCanPropertyTransition( transition.m_symProperty ) )
					return false;

				// get rest
				if( !CSSHelpers::BParseTime( &transition.m_flTransitionSeconds, pchString, &pchString ) ||
					!CSSHelpers::BParseTimingFunction( &transition.m_eTimingFunction, &transition.m_CubicBezier, pchString, &pchString ) ||
					!CSSHelpers::BParseTime( &transition.m_flTransitionDelaySeconds, pchString, &pchString ) )
				{
					return false;
				}

				m_vecTransitionProperties.AddToTail( transition );

				// see if there is another transition property
				if( !CSSHelpers::BSkipComma( pchString, &pchString ) )
				{
					// end of list.. should be an empty string
					pchString = CSSHelpers::SkipSpaces( pchString );
					return (pchString[0] == '\0');
				}
			}

			return true;
		}
		else if( symParsedName == symbolProperty )
		{
			// comma separated list of porperty names
			CUtlVector< panorama::CStyleSymbol > vecProperties;
			if( !CSSHelpers::BParseCommaSepList( &vecProperties, CSSHelpers::BParseIdentToStyleSymbol, pchString ) )
				return false;

			// if we have less properties specified than other params, error
			if( m_vecTransitionProperties.Count() > vecProperties.Count() )
				return false;

			m_vecTransitionProperties.EnsureCapacity( vecProperties.Count() );
			FOR_EACH_VEC( vecProperties, i )
			{
				if( vecProperties[i] == "none" )
					continue;

				if( !BCanPropertyTransition( vecProperties[i] ) )
					return false;

				// new property?
				if( m_vecTransitionProperties.Count() <= i )
					AddNewProperty();

				m_vecTransitionProperties[i].m_symProperty = vecProperties[i];
			}

			return true;
		}
		else if( symParsedName == symbolDuration )
		{
			return BParseAndAddProperty( &TransitionProperty_t::m_flTransitionSeconds, CSSHelpers::BParseTime, pchString );
		}
		else if( symParsedName == symbolDelay )
		{
			return BParseAndAddProperty( &TransitionProperty_t::m_flTransitionDelaySeconds, CSSHelpers::BParseTime, pchString );
		}
		else if( symParsedName == symbolTiming )
		{
			return BParseAndAddTimingFunction( CSSHelpers::BParseTimingFunction, pchString );
		}

		return false;
	}

	bool BParseAndRemoveImmediate( bool *pbImmediate, char *pchString )
	{
		const char k_rgchImmediate[] = "!immediate";
		char *pchImmediate = V_strstr( pchString, k_rgchImmediate );
		if( !pchImmediate )
		{
			if( pbImmediate )
				*pbImmediate = false;
			return true;
		}

		pchString = pchImmediate + V_ARRAYSIZE( k_rgchImmediate ) - 1;
		pchString = (char*)CSSHelpers::SkipSpaces( pchString );
		if( pchString[0] != '\0' )
			return false;

		if( pbImmediate )
			*pbImmediate = true;

		pchImmediate[0] = '\0';
		return true;
	}

	template < typename T >
	bool BParseAndAddProperty( T TransitionProperty_t::*pProp, bool( *func )(T*, const char *, const char **), const char *pchString )
	{
		// parse comma separated list
		CUtlVector< T > vec;
		if( !CSSHelpers::BParseCommaSepList( &vec, func, pchString ) || vec.Count() == 0 )
			return false;

		FOR_EACH_VEC( vec, i )
		{
			// new property?
			if( m_vecTransitionProperties.Count() <= i )
				AddNewProperty();

			m_vecTransitionProperties[i].*pProp = vec[i];
		}

		// if provided list is less than the total number of properties, fill in rest with last value
		for( int i = vec.Count(); i < m_vecTransitionProperties.Count(); i++ )
			m_vecTransitionProperties[i].*pProp = m_vecTransitionProperties[i - 1].*pProp;

		return true;
	}

	bool BParseAndAddTimingFunction( bool( *func )(EAnimationTimingFunction *pTimeingFunc, CCubicBezierCurve< Vector2D > *pBezier, const char *, const char **), const char *pchString )
	{
		// parse comma separated list
		CUtlVector< EAnimationTimingFunction > vec;
		CUtlVector< CCubicBezierCurve< Vector2D > > vec2;
		if( !CSSHelpers::BParseCommaSepList( &vec, &vec2, func, pchString ) || vec.Count() == 0 )
			return false;

		Assert( vec.Count() == vec2.Count() );

		FOR_EACH_VEC( vec, i )
		{
			// new property?
			if( m_vecTransitionProperties.Count() <= i )
				AddNewProperty();

			m_vecTransitionProperties[i].m_eTimingFunction = vec[i];
			m_vecTransitionProperties[i].m_CubicBezier = vec2[i];
		}

		// if provided list is less than the total number of properties, fill in rest with last value
		for( int i = vec.Count(); i < m_vecTransitionProperties.Count(); i++ )
		{
			m_vecTransitionProperties[i].m_eTimingFunction = m_vecTransitionProperties[i - 1].m_eTimingFunction;
			m_vecTransitionProperties[i].m_CubicBezier = m_vecTransitionProperties[i - 1].m_CubicBezier;
		}

		return true;
	}

	bool BCanPropertyTransition( CStyleSymbol symProperty )
	{
		// make sure the property is valid, and is not an alias
		if( !UIEngine()->UIStyleFactory()->BRegisteredProperty( symProperty ) )
			return false;

		// bugbug cboyd - need less expensive check for valid property symbol and if it supports transition
		CStyleProperty *pStyleProperty = UIEngine()->UIStyleFactory()->CreateStyleProperty( symProperty );
		if( !pStyleProperty )
			return false;

		bool bCanTransition = pStyleProperty->BCanTransition();
		UIEngine()->UIStyleFactory()->FreeStyleProperty( pStyleProperty );
		return bCanTransition;
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found by looking at lower weight styles
	virtual bool BFullySet()
	{
		if( m_vecTransitionProperties.Count() < 1 )
			return false;

		// if the first transition property is fully set, all other transition properties should be set
		TransitionProperty_t &transitionProperty = m_vecTransitionProperties[0];
		return (transitionProperty.m_symProperty.IsValid() &&
			transitionProperty.m_flTransitionSeconds != k_flFloatNotSet &&
			transitionProperty.m_eTimingFunction != k_EAnimationUnset &&
			transitionProperty.m_flTransitionDelaySeconds != k_flFloatNotSet);
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		if( m_vecTransitionProperties.Count() < 1 )
			return;

		// if property names aren't set or transition time isn't set (would default to 0), remove all and exit
		TransitionProperty_t &firstProperty = m_vecTransitionProperties[0];
		if( !firstProperty.m_symProperty.IsValid() || firstProperty.m_flTransitionSeconds == k_flFloatNotSet )
		{
			m_vecTransitionProperties.RemoveAll();
			return;
		}

		// default to 'ease' timing function and 0 delay
		FOR_EACH_VEC( m_vecTransitionProperties, i )
		{
			TransitionProperty_t &transitionProperty = m_vecTransitionProperties[i];

			if( transitionProperty.m_eTimingFunction == k_EAnimationUnset )
			{
				transitionProperty.m_eTimingFunction = k_EAnimationEase;

				Vector2D vec[4];
				panorama::GetAnimationCurveControlPoints( transitionProperty.m_eTimingFunction, vec );
				transitionProperty.m_CubicBezier.SetControlPoints( vec );
			}

			if( transitionProperty.m_flTransitionDelaySeconds == k_flFloatNotSet )
				transitionProperty.m_flTransitionDelaySeconds = 0;
		}
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		if( m_vecTransitionProperties.Count() == 0 )
		{
			pfmtBuffer->Append( "none" );
			return;
		}

		FOR_EACH_VEC( m_vecTransitionProperties, i )
		{
			if( i > 0 )
				pfmtBuffer->Append( ",\n" );

			const TransitionProperty_t &transition = m_vecTransitionProperties[i];

			pfmtBuffer->Append( transition.m_symProperty.String() );
			pfmtBuffer->Append( " " );
			CSSHelpers::AppendTime( pfmtBuffer, transition.m_flTransitionSeconds );
			if( transition.m_eTimingFunction != k_EAnimationCustomBezier )
				pfmtBuffer->AppendFormat( " %s ", PchNameFromEAnimationTimingFunction( transition.m_eTimingFunction ) );
			else
				pfmtBuffer->AppendFormat( " cubic-bezier( %1.3f, %1.3f, %1.3f, %1.3f ) ",
				transition.m_CubicBezier.ControlPoint( 1 ).x,
				transition.m_CubicBezier.ControlPoint( 1 ).y,
				transition.m_CubicBezier.ControlPoint( 2 ).x,
				transition.m_CubicBezier.ControlPoint( 2 ).y );

			CSSHelpers::AppendTime( pfmtBuffer, transition.m_flTransitionDelaySeconds );
		}
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		if( symProperty == symbol )
		{
			return "Specifies which properties should transition smoothly to new values if a class/pseudo class changes the styles.  "
				"Also specifies duration, timing function, and delay.  Valid timing functions are: ease, ease-in, ease-out, ease-in-out, linear.<br><br>"
				"<b>Example:</b>"
				"<pre>"
				"transition: position 2.0s ease-in-out 0.0s, perspective-origin 1.2s ease-in-out 0.8s;"
				"</pre>";
		}
		else if( symProperty == symbolProperty )
		{
			return "Specifies which properties should transition smoothly to new values if a class/pseudo class changes the styles.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"transition: position, transform, background-color;"
				"</pre>";
		}
		else if( symProperty == symbolTiming )
		{
			return "Specifies the timing function to use for transition properties on this panel, if more than one comma delimited value is specified "
				"then the values are applied to each property specified in 'transition-property' in order.  If only one value is specified then "
				"it applies to all the properties specified in transition-property. Valid timing functions are: ease, ease-in, ease-out, ease-in-out, linear.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"transition-timing-function: ease-in-out;\n"
				"transition-timing-function: ease-in-out, linear;\n"
				"transition-timing-function: cubic-bezier( 0.785, 0.385, 0.555, 1.505 );"
				"</pre>";

		}
		else if( symProperty == symbolDuration )
		{
			return "Specifies the durating in seconds to use for transition properties on this panel, if more than one comma delimited value is specified "
				"then the values are applied to each property specified in 'transition-property' in order.  If only one value is specified then "
				"it applies to all the properties specified in transition-property.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"transition-duration: 2.0s;\n"
				"transition-duration: 2.0s, 1.2s, 1.2s, 4.0s, 2.0s;"
				"</pre>";

		}
		else if( symProperty == symbolDelay )
		{
			return "Specifies the delay in seconds to use for transition properties on this panel, if more than one comma delimited value is specified "
				"then the values are applied to each property specified in 'transition-property' in order.  If only one value is specified then "
				"it applies to all the properties specified in transition-property.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"transition-delay: 0.0s;\n"
				"transition-delay: 0.0s, 1.2s;"
				"</pre>";
		}

		return CStyleProperty::GetDescription( symProperty );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyTransitionProperties &rhs = (const CStylePropertyTransitionProperties &)other;
		if( m_vecTransitionProperties.Count() != rhs.m_vecTransitionProperties.Count() )
			return false;

		FOR_EACH_VEC( m_vecTransitionProperties, i )
		{
			if( m_vecTransitionProperties[i] != rhs.m_vecTransitionProperties[i] )
				return false;
		}

		// should not compare immediate

		return true;
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	// looks up transition data for a property
	TransitionProperty_t *GetTransitionData( CStyleSymbol hSymbol )
	{
		FOR_EACH_VEC( m_vecTransitionProperties, i )
		{
			if( m_vecTransitionProperties[i].m_symProperty == hSymbol )
				return &m_vecTransitionProperties[i];
		}

		return NULL;
	}


#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_vecTransitionProperties );
		CStyleProperty::Validate( validator, pchName );
	}
#endif

	CUtlVector< TransitionProperty_t > m_vecTransitionProperties;
	bool m_bImmediate;
};


//-----------------------------------------------------------------------------
// Purpose: background-color property
//-----------------------------------------------------------------------------
class CStylePropertyBackgroundColor : public CStylePropertyFillColor
{
public:

	static const CStyleSymbol symbol;
	CStylePropertyBackgroundColor() : CStylePropertyFillColor( CStylePropertyBackgroundColor::symbol )
	{
	}

	bool BHasAnyParticleSystems()
	{
		return m_FillBrushCollection.GetNumParticleSystems() > 0 ? true : false;
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Sets the background fill color/gradient/combination for a panel.<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"background-color: #FFFFFFFF;\n"
			"background-color: gradient( linear, 0% 0%, 0% 100%, from( #fbfbfbff ), to( #c0c0c0c0 ) );\n"
			"background-color: gradient( linear, 0% 0%, 0% 100%, from( #fbfbfbff ), color-stop( 0.3, #ebebebff ), to( #c0c0c0c0 ) );\n"
			"background-color: gradient( radial, 50% 50%, 0% 0%, 80% 80%, from( #00ff00ff ), to( #0000ffff ) );\n"
			"background-color: #0d1c22ff, gradient( radial, 100% -0%, 100px -40px, 320% 270%, from( #3a464bff ), color-stop( 0.23, #0d1c22ff ), to( #0d1c22ff ) );"
			"</pre>";
	}

};


//-----------------------------------------------------------------------------
// Purpose: border property
//-----------------------------------------------------------------------------
class CStylePropertyBorder : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;

	static const CStyleSymbol symTop;
	static const CStyleSymbol symRight;
	static const CStyleSymbol symBottom;
	static const CStyleSymbol symLeft;

	static const CStyleSymbol symStyle;
	static const CStyleSymbol symTopStyle;
	static const CStyleSymbol symRightStyle;
	static const CStyleSymbol symBottomStyle;
	static const CStyleSymbol symLeftStyle;

	static const CStyleSymbol symWidth;
	static const CStyleSymbol symTopWidth;
	static const CStyleSymbol symRightWidth;
	static const CStyleSymbol symBottomWidth;
	static const CStyleSymbol symLeftWidth;

	static const CStyleSymbol symColor;
	static const CStyleSymbol symTopColor;
	static const CStyleSymbol symRightColor;
	static const CStyleSymbol symBottomColor;
	static const CStyleSymbol symLeftColor;

	CStylePropertyBorder() : CStyleProperty( CStylePropertyBorder::symbol )
	{
		for( int i = 0; i < 4; i++ )
		{
			m_rgBorderStyle[i] = k_EBorderStyleUnset;
			m_rgColorsSet[i] = false;
			m_rgBorderColor[i] = Color( 0, 0, 0, 0 );
		}
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyBorder::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyBorder *pBorderTarget = (CStylePropertyBorder*)pTarget;
		for( int i = 0; i < 4; i++ )
		{
			if( pBorderTarget->m_rgBorderStyle[i] == k_EBorderStyleUnset )
				pBorderTarget->m_rgBorderStyle[i] = m_rgBorderStyle[i];

			if( !pBorderTarget->m_rgBorderWidth[i].IsSet() )
				pBorderTarget->m_rgBorderWidth[i] = m_rgBorderWidth[i];

			if( pBorderTarget->m_rgColorsSet[i] == false )
			{
				pBorderTarget->m_rgColorsSet[i] = m_rgColorsSet[i];
				pBorderTarget->m_rgBorderColor[i] = m_rgBorderColor[i];
			}
		}

	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ );

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( symParsedName == symbol )
		{
			// shorthand for width, style, color
			if( CSSHelpers::BParseIntoUILength( &m_rgBorderWidth[0], pchString, &pchString ) )
			{
				m_rgBorderWidth[1] = m_rgBorderWidth[2] = m_rgBorderWidth[3] = m_rgBorderWidth[0];

				if( CSSHelpers::BParseBorderStyle( &m_rgBorderStyle[0], pchString, &pchString ) )
				{
					m_rgBorderStyle[1] = m_rgBorderStyle[2] = m_rgBorderStyle[3] = m_rgBorderStyle[0];

					if( CSSHelpers::BParseColor( &m_rgBorderColor[0], pchString, &pchString ) )
					{
						m_rgColorsSet[0] = m_rgColorsSet[1] = m_rgColorsSet[2] = m_rgColorsSet[3] = true;
						m_rgBorderColor[1] = m_rgBorderColor[2] = m_rgBorderColor[3] = m_rgBorderColor[0];
					}
				}

				// bugbug - should reset all values, as well as border-image and border-radius as well, see 
				// http://www.w3.org/TR/css3-background/#ltborder-widthgt

				return true;
			}

			return false;
		}
		else if ( symParsedName == symTop )
		{
			return BParseSingleEdgeShortcutProperty( 0, pchString );
		}
		else if ( symParsedName == symRight )
		{
			return BParseSingleEdgeShortcutProperty( 1, pchString );
		}
		else if ( symParsedName == symBottom )
		{
			return BParseSingleEdgeShortcutProperty( 2, pchString );
		}
		else if ( symParsedName == symLeft )
		{
			return BParseSingleEdgeShortcutProperty( 3, pchString );
		}
		else if( symParsedName == symWidth )
		{
			// "top right bottom left" in order
			if( CSSHelpers::BParseIntoUILength( &m_rgBorderWidth[0], pchString, &pchString ) )
			{
				if( CSSHelpers::BParseIntoUILength( &m_rgBorderWidth[1], pchString, &pchString ) )
				{
					if( CSSHelpers::BParseIntoUILength( &m_rgBorderWidth[2], pchString, &pchString ) )
					{
						if( !CSSHelpers::BParseIntoUILength( &m_rgBorderWidth[3], pchString, &pchString ) )
							m_rgBorderWidth[3] = m_rgBorderWidth[1];
					}
					else
					{
						m_rgBorderWidth[2] = m_rgBorderWidth[0];
						m_rgBorderWidth[3] = m_rgBorderWidth[1];
					}
				}
				else
				{
					m_rgBorderWidth[1] = m_rgBorderWidth[2] = m_rgBorderWidth[3] = m_rgBorderWidth[0];
				}

				return true;
			}
			return false;
		}
		else if( symParsedName == symTopWidth )
		{
			if( CSSHelpers::BParseIntoUILength( &m_rgBorderWidth[0], pchString, &pchString ) )
			{
				return true;
			}

			return false;
		}
		else if( symParsedName == symRightWidth )
		{
			if( CSSHelpers::BParseIntoUILength( &m_rgBorderWidth[1], pchString, &pchString ) )
			{
				return true;
			}

			return false;
		}
		else if( symParsedName == symBottomWidth )
		{
			if( CSSHelpers::BParseIntoUILength( &m_rgBorderWidth[2], pchString, &pchString ) )
			{
				return true;
			}

			return false;
		}
		else if( symParsedName == symLeftWidth )
		{
			if( CSSHelpers::BParseIntoUILength( &m_rgBorderWidth[3], pchString, &pchString ) )
			{
				return true;
			}

			return false;
		}
		else if( symParsedName == symStyle )
		{
			// "top right bottom left" in order
			if( CSSHelpers::BParseBorderStyle( &m_rgBorderStyle[0], pchString, &pchString ) )
			{
				if( CSSHelpers::BParseBorderStyle( &m_rgBorderStyle[1], pchString, &pchString ) )
				{
					if( CSSHelpers::BParseBorderStyle( &m_rgBorderStyle[2], pchString, &pchString ) )
					{
						if( !CSSHelpers::BParseBorderStyle( &m_rgBorderStyle[3], pchString, &pchString ) )
							m_rgBorderStyle[3] = m_rgBorderStyle[1];
					}
					else
					{
						m_rgBorderStyle[2] = m_rgBorderStyle[0];
						m_rgBorderStyle[3] = m_rgBorderStyle[1];
					}
				}
				else
				{
					m_rgBorderStyle[1] = m_rgBorderStyle[2] = m_rgBorderStyle[3] = m_rgBorderStyle[0];
				}
				return true;
			}
			return false;
		}
		else if( symParsedName == symTopStyle )
		{
			if( CSSHelpers::BParseBorderStyle( &m_rgBorderStyle[0], pchString, &pchString ) )
				return true;

			return false;
		}
		else if( symParsedName == symRightStyle )
		{
			if( CSSHelpers::BParseBorderStyle( &m_rgBorderStyle[1], pchString, &pchString ) )
				return true;

			return false;
		}
		else if( symParsedName == symBottomStyle )
		{
			if( CSSHelpers::BParseBorderStyle( &m_rgBorderStyle[2], pchString, &pchString ) )
				return true;

			return false;
		}
		else if( symParsedName == symLeftStyle )
		{
			if( CSSHelpers::BParseBorderStyle( &m_rgBorderStyle[3], pchString, &pchString ) )
				return true;

			return false;
		}
		else if( symParsedName == symColor )
		{
			// "top right bottom left" in order
			if( CSSHelpers::BParseColor( &m_rgBorderColor[0], pchString, &pchString ) )
			{
				if( CSSHelpers::BParseColor( &m_rgBorderColor[1], pchString, &pchString ) )
				{
					if( CSSHelpers::BParseColor( &m_rgBorderColor[2], pchString, &pchString ) )
					{
						if( !CSSHelpers::BParseColor( &m_rgBorderColor[3], pchString, &pchString ) )
							m_rgBorderColor[3] = m_rgBorderColor[1];
					}
					else
					{
						m_rgBorderColor[2] = m_rgBorderColor[0];
						m_rgBorderColor[3] = m_rgBorderColor[1];
					}
				}
				else
				{
					m_rgBorderColor[1] = m_rgBorderColor[2] = m_rgBorderColor[3] = m_rgBorderColor[0];
				}
				m_rgColorsSet[0] = m_rgColorsSet[1] = m_rgColorsSet[2] = m_rgColorsSet[3] = true;
				return true;
			}
			return false;
		}
		else if( symParsedName == symTopColor )
		{
			if( CSSHelpers::BParseColor( &m_rgBorderColor[0], pchString, &pchString ) )
			{
				m_rgColorsSet[0] = true;
				return true;
			}

			return false;
		}
		else if( symParsedName == symRightColor )
		{
			if( CSSHelpers::BParseColor( &m_rgBorderColor[1], pchString, &pchString ) )
			{
				m_rgColorsSet[1] = true;
				return true;
			}

			return false;
		}
		else if( symParsedName == symBottomColor )
		{
			if( CSSHelpers::BParseColor( &m_rgBorderColor[2], pchString, &pchString ) )
			{
				m_rgColorsSet[2] = true;
				return true;
			}

			return false;
		}
		else if( symParsedName == symLeftColor )
		{
			if( CSSHelpers::BParseColor( &m_rgBorderColor[3], pchString, &pchString ) )
			{
				m_rgColorsSet[3] = true;
				return true;
			}

			return false;
		}

		return false;
	}

	// helper function to parse the shortcut property for a single edge (border-top, border-right, border-bottom, border-left)
	bool BParseSingleEdgeShortcutProperty( int nEdgeIndex, const char *pchString )
	{
		// shorthand for width, style, color
		if ( CSSHelpers::BParseIntoUILength( &m_rgBorderWidth[ nEdgeIndex ], pchString, &pchString ) )
		{
			if ( CSSHelpers::BParseBorderStyle( &m_rgBorderStyle[ nEdgeIndex ], pchString, &pchString ) )
			{
				if ( CSSHelpers::BParseColor( &m_rgBorderColor[ nEdgeIndex ], pchString, &pchString ) )
				{
					m_rgColorsSet[ nEdgeIndex ] = true;
				}
			}

			return true;
		}

		return false;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_rgBorderWidth[0].ScaleLengthValue( flScaleFactor );
		m_rgBorderWidth[1].ScaleLengthValue( flScaleFactor );
		m_rgBorderWidth[2].ScaleLengthValue( flScaleFactor );
		m_rgBorderWidth[3].ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		// bugbug jmccaskey - this emits an invalid value for now... can't really put it all into just "border:"...

		pfmtBuffer->Append( m_rgBorderStyle[0] == k_EBorderStyleSolid ? "solid " : "none " );
		pfmtBuffer->Append( m_rgBorderStyle[1] == k_EBorderStyleSolid ? "solid " : "none " );
		pfmtBuffer->Append( m_rgBorderStyle[2] == k_EBorderStyleSolid ? "solid " : "none " );
		pfmtBuffer->Append( m_rgBorderStyle[3] == k_EBorderStyleSolid ? "solid " : "none " );

		pfmtBuffer->Append( "/ " );


		CSSHelpers::AppendUILength( pfmtBuffer, m_rgBorderWidth[0] );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_rgBorderWidth[1] );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_rgBorderWidth[2] );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_rgBorderWidth[3] );
		pfmtBuffer->Append( " " );

		pfmtBuffer->AppendFormat( "/ " );

		if( m_rgColorsSet[0] )
		{
			CSSHelpers::AppendColor( pfmtBuffer, m_rgBorderColor[0] );
			pfmtBuffer->Append( " " );
		}
		else
			pfmtBuffer->Append( "none " );

		if( m_rgColorsSet[1] )
		{
			CSSHelpers::AppendColor( pfmtBuffer, m_rgBorderColor[1] );
			pfmtBuffer->Append( " " );
		}
		else
			pfmtBuffer->Append( "none " );

		if( m_rgColorsSet[2] )
		{
			CSSHelpers::AppendColor( pfmtBuffer, m_rgBorderColor[2] );
			pfmtBuffer->Append( " " );
		}
		else
			pfmtBuffer->Append( "none " );

		if( m_rgColorsSet[3] )
		{
			CSSHelpers::AppendColor( pfmtBuffer, m_rgBorderColor[3] );
			pfmtBuffer->Append( " " );
		}
		else
			pfmtBuffer->Append( "none " );

	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		for( int i = 0; i < 4; ++i )
		{
			if( m_rgBorderStyle[i] == k_EBorderStyleUnset )
				return false;
			if( !m_rgBorderWidth[i].IsSet() )
				return false;
			if( m_rgColorsSet[i] == false )
				return false;
		}
		return true;
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		for( int i = 0; i < 4; ++i )
		{
			if( !m_rgBorderWidth[i].IsSet() )
				m_rgBorderWidth[i].SetLength( 0 );

			if( m_rgBorderStyle[i] == k_EBorderStyleUnset )
				m_rgBorderStyle[i] = k_EBorderStyleNone;

			if( m_rgColorsSet[i] == false )
			{
				m_rgColorsSet[i] = true;
				m_rgBorderColor[i] = Color( 0, 0, 0, 0 );
			}
		}
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		if( symProperty == symbol )
		{
			return "Shorthand for setting panel border. Specify width, style, and color.  Supported styles are: solid, none.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border: 2px solid #111111FF;"
				"</pre>";
		}
		else if ( symProperty == symTop )
		{
			return "Shorthand for setting the top panel border. Specify width, style, and color.  Supported styles are: solid, none.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-top: 2px solid #111111FF;"
				"</pre>";
		}
		else if ( symProperty == symRight )
		{
			return "Shorthand for setting the right panel border. Specify width, style, and color.  Supported styles are: solid, none.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-right: 2px solid #111111FF;"
				"</pre>";
		}
		else if ( symProperty == symBottom )
		{
			return "Shorthand for setting the bottom panel border. Specify width, style, and color.  Supported styles are: solid, none.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-bottom: 2px solid #111111FF;"
				"</pre>";
		}
		else if ( symProperty == symLeft )
		{
			return "Shorthand for setting the left panel border. Specify width, style, and color.  Supported styles are: solid, none.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-left: 2px solid #111111FF;"
				"</pre>";
		}
		else if ( symProperty == symColor )
		{
			return "Specifies border color for panel.  If a single color value is set it applies to all sides, if 2 are set the first is top/bottom and "
				"the second is left/right, if all four are set then they are top, right, bottom, left in order.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-color: #111111FF;\n"
				"border-color: #FF0000FF #00FF00FF #0000FFFF #00FFFFFF;"
				"</pre>";
		}
		else if( symProperty == symTopColor )
		{
			return "Specifies border color for the top edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-top-color: #111111FF;"
				"</pre>";
		}
		else if( symProperty == symRightColor )
		{
			return "Specifies border color for the right edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-right-color: #111111FF;"
				"</pre>";
		}
		else if( symProperty == symBottomColor )
		{
			return "Specifies border color for the bottom edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-bottom-color: #111111FF;"
				"</pre>";
		}
		else if( symProperty == symLeftColor )
		{
			return "Specifies border color for the left edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-left-color: #111111FF;"
				"</pre>";
		}
		else if( symProperty == symWidth )
		{
			return "Specifies border width for panel.  If a single width value is set it applies to all sides, if 2 are set the first is top/bottom and "
				"the second is left/right, if all four are set then they are top, right, bottom, left in order.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-width: 1px;'\n"
				"border-width: 20px 1px 20px 1px;"
				"</pre>";
		}
		else if( symProperty == symTopWidth )
		{
			return "Specifies border width for the top edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-top-width: 2px;"
				"</pre>";
		}
		else if( symProperty == symRightWidth )
		{
			return "Specifies border width for the right edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-right-width: 2px;"
				"</pre>";
		}
		else if( symProperty == symBottomWidth )
		{
			return "Specifies border width for the bottom edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-bottom-width: 2px;"
				"</pre>";
		}
		else if( symProperty == symLeftWidth )
		{
			return "Specifies border width for the left edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-left-width: 2px;"
				"</pre>";
		}
		else if( symProperty == symStyle )
		{
			return "Specifies border style for panel.  If a single style value is set it applies to all sides, if 2 are set the first is top/bottom and "
				"the second is left/right, if all four are set then they are top, right, bottom, left in order.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-style: solid;\n"
				"border-style: solid none solid none;"
				"</pre>";
		}
		else if( symProperty == symTopStyle )
		{
			return "Specifies border style for the top edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-top-style: solid;"
				"</pre>";
		}
		else if( symProperty == symRightStyle )
		{
			return "Specifies border style for the right edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-right-style: solid;"
				"</pre>";
		}
		else if( symProperty == symBottomStyle )
		{
			return "Specifies border style for the bottom edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-bottom-style: solid;"
				"</pre>";
		}
		else if( symProperty == symLeftStyle )
		{
			return "Specifies border style for the left edge of the panel. <br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-left-style: solid;"
				"</pre>";
		}

		return CStyleProperty::GetDescription( symProperty );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyBorder &rhs = (const CStylePropertyBorder &)other;

		if( V_memcmp( m_rgBorderStyle, rhs.m_rgBorderStyle, sizeof( m_rgBorderStyle ) ) != 0 )
			return false;

		for( int i = 0; i < V_ARRAYSIZE( m_rgBorderWidth ); i++ )
		{
			if( m_rgBorderWidth[i] != rhs.m_rgBorderWidth[i] )
				return false;
		}

		if( V_memcmp( m_rgColorsSet, rhs.m_rgColorsSet, sizeof( m_rgColorsSet ) ) != 0 )
			return false;


		for( int i = 0; i < V_ARRAYSIZE( m_rgBorderColor ); i++ )
		{
			if( m_rgBorderColor[i] != rhs.m_rgBorderColor[i] )
				return false;
		}

		return true;
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		const CStylePropertyBorder &rhs = (const CStylePropertyBorder &)*pCompareProperty;
		if( m_rgBorderWidth[0] != rhs.m_rgBorderWidth[0] || m_rgBorderWidth[1] != rhs.m_rgBorderWidth[1]
			|| m_rgBorderWidth[2] != rhs.m_rgBorderWidth[2] || m_rgBorderWidth[3] != rhs.m_rgBorderWidth[3] )
		{
			return k_EStyleInvalidateLayoutSizeAndPosition;
		}

		return k_EStyleInvalidateLayoutNone;
	}

	EBorderStyle m_rgBorderStyle[4];
	CUILength m_rgBorderWidth[4];
	bool m_rgColorsSet[4];
	Color m_rgBorderColor[4];
};


//-----------------------------------------------------------------------------
// Purpose: border radius property
//-----------------------------------------------------------------------------
class CStylePropertyBorderRadius : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	static const CStyleSymbol topRightRadius;
	static const CStyleSymbol bottomRightRadius;
	static const CStyleSymbol bottomLeftRadius;
	static const CStyleSymbol topLeftRadius;

	CStylePropertyBorderRadius() : CStyleProperty( CStylePropertyBorderRadius::symbol )
	{

	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyBorderRadius::MergeTo" );
			return;
		}

		CStyleProperty::MergeTo( pTarget );

		CStylePropertyBorderRadius *p = (CStylePropertyBorderRadius *)pTarget;
		for( int i = 0; i < k_ECornerMax; ++i )
		{
			if( !p->m_rgCornerRaddi[i].m_HorizontalRadii.IsSet() )
				p->m_rgCornerRaddi[i].m_HorizontalRadii = m_rgCornerRaddi[i].m_HorizontalRadii;
			if( !p->m_rgCornerRaddi[i].m_VerticalRadii.IsSet() )
				p->m_rgCornerRaddi[i].m_VerticalRadii = m_rgCornerRaddi[i].m_VerticalRadii;
		}
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ );

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( symParsedName == symbol )
		{
			// horizontal before /, vertical after, vertical matches horizontal if unset
			//[ <length> | <percentage> ]{1,4} [ / [ <length> | <percentage> ]{1,4} ]

			CUILength topLeft;
			CUILength topRight;
			CUILength bottomRight;
			CUILength bottomLeft;

			if( !CSSHelpers::BParseIntoUILength( &topLeft, pchString, &pchString ) )
				return false;

			m_rgCornerRaddi[k_ECornerTopLeft].m_HorizontalRadii = topLeft;

			if( !CSSHelpers::BParseIntoUILength( &topRight, pchString, &pchString ) )
			{
				// All horizontal values then equal topLeft
				m_rgCornerRaddi[k_ECornerTopRight].m_HorizontalRadii = topLeft;
				m_rgCornerRaddi[k_ECornerBottomRight].m_HorizontalRadii = topLeft;
				m_rgCornerRaddi[k_ECornerBottomLeft].m_HorizontalRadii = topLeft;
			}
			else
			{
				m_rgCornerRaddi[k_ECornerTopRight].m_HorizontalRadii = topRight;

				if( !CSSHelpers::BParseIntoUILength( &bottomRight, pchString, &pchString ) )
				{
					// Bottom right matches top left then, and bottom left matches top right
					m_rgCornerRaddi[k_ECornerBottomRight].m_HorizontalRadii = topLeft;
					m_rgCornerRaddi[k_ECornerBottomLeft].m_HorizontalRadii = topRight;
				}
				else
				{
					m_rgCornerRaddi[k_ECornerBottomRight].m_HorizontalRadii = bottomRight;

					if( !CSSHelpers::BParseIntoUILength( &bottomLeft, pchString, &pchString ) )
					{
						// Bottom left matches top right then
						m_rgCornerRaddi[k_ECornerBottomLeft].m_HorizontalRadii = topRight;
					}
					else
					{
						m_rgCornerRaddi[k_ECornerBottomLeft].m_HorizontalRadii = bottomLeft;
					}
				}
			}

			// Look for the / then, which will have vertical values after.  If not found set them all
			// to match horizontal.
			pchString = CSSHelpers::SkipSpaces( pchString );
			if( pchString[0] != '/' )
			{
				for( int i = 0; i < k_ECornerMax; ++i )
					m_rgCornerRaddi[i].m_VerticalRadii = m_rgCornerRaddi[i].m_HorizontalRadii;
			}
			else
			{
				// Skip /
				pchString++;
				CSSHelpers::SkipSpaces( pchString );

				// Insist we get one value after the /, having a trailing / with no vertical values is bogus
				if( !CSSHelpers::BParseIntoUILength( &topLeft, pchString, &pchString ) )
				{
					// Set to match horizontal anyway so our values are kind of valid
					for( int i = 0; i < k_ECornerMax; ++i )
						m_rgCornerRaddi[i].m_VerticalRadii = m_rgCornerRaddi[i].m_HorizontalRadii;

					return false;
				}

				m_rgCornerRaddi[k_ECornerTopLeft].m_VerticalRadii = topLeft;

				if( !CSSHelpers::BParseIntoUILength( &topRight, pchString, &pchString ) )
				{
					// All vertical values then equal topLeft
					m_rgCornerRaddi[k_ECornerTopRight].m_VerticalRadii = topLeft;
					m_rgCornerRaddi[k_ECornerBottomRight].m_VerticalRadii = topLeft;
					m_rgCornerRaddi[k_ECornerBottomLeft].m_VerticalRadii = topLeft;
				}
				else
				{
					m_rgCornerRaddi[k_ECornerTopRight].m_VerticalRadii = topRight;

					if( !CSSHelpers::BParseIntoUILength( &bottomRight, pchString, &pchString ) )
					{
						// Bottom right matches top left then, and bottom left matches top right
						m_rgCornerRaddi[k_ECornerBottomRight].m_VerticalRadii = topLeft;
						m_rgCornerRaddi[k_ECornerBottomLeft].m_VerticalRadii = topRight;
					}
					else
					{
						m_rgCornerRaddi[k_ECornerBottomRight].m_VerticalRadii = bottomRight;

						if( !CSSHelpers::BParseIntoUILength( &bottomLeft, pchString, &pchString ) )
						{
							// Bottom left matches top right then
							m_rgCornerRaddi[k_ECornerBottomLeft].m_VerticalRadii = topRight;
						}
						else
						{
							m_rgCornerRaddi[k_ECornerBottomLeft].m_VerticalRadii = bottomLeft;
						}
					}
				}
			}

			return true;
		}
		else
		{
			int iCorner = k_ECornerInvalid;
			if( symParsedName == topLeftRadius )
				iCorner = k_ECornerTopLeft;
			else if( symParsedName == topRightRadius )
				iCorner = k_ECornerTopRight;
			else if( symParsedName == bottomRightRadius )
				iCorner = k_ECornerBottomRight;
			else if( symParsedName == bottomLeftRadius )
				iCorner = k_ECornerBottomLeft;

			if( iCorner == k_ECornerInvalid )
				return false;

			CUILength length;
			if( !CSSHelpers::BParseIntoUILength( &length, pchString, &pchString ) )
				return false;

			m_rgCornerRaddi[iCorner].m_HorizontalRadii = length;

			// Second value is optional, otherwise vertical matches horizontal
			if( !CSSHelpers::BParseIntoUILength( &length, pchString, &pchString ) )
				m_rgCornerRaddi[iCorner].m_VerticalRadii = m_rgCornerRaddi[iCorner].m_HorizontalRadii;
			else
				m_rgCornerRaddi[iCorner].m_VerticalRadii = length;

			return true;
		}
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_rgCornerRaddi[k_ECornerTopLeft].m_HorizontalRadii.ScaleLengthValue( flScaleFactor );
		m_rgCornerRaddi[k_ECornerTopLeft].m_VerticalRadii.ScaleLengthValue( flScaleFactor );
		m_rgCornerRaddi[k_ECornerTopRight].m_HorizontalRadii.ScaleLengthValue( flScaleFactor );
		m_rgCornerRaddi[k_ECornerTopRight].m_VerticalRadii.ScaleLengthValue( flScaleFactor );
		m_rgCornerRaddi[k_ECornerBottomRight].m_HorizontalRadii.ScaleLengthValue( flScaleFactor );
		m_rgCornerRaddi[k_ECornerBottomRight].m_VerticalRadii.ScaleLengthValue( flScaleFactor );
		m_rgCornerRaddi[k_ECornerBottomLeft].m_HorizontalRadii.ScaleLengthValue( flScaleFactor );
		m_rgCornerRaddi[k_ECornerBottomLeft].m_VerticalRadii.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, m_rgCornerRaddi[k_ECornerTopLeft].m_HorizontalRadii );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_rgCornerRaddi[k_ECornerTopRight].m_HorizontalRadii );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_rgCornerRaddi[k_ECornerBottomRight].m_HorizontalRadii );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_rgCornerRaddi[k_ECornerBottomLeft].m_HorizontalRadii );

		pfmtBuffer->AppendFormat( " / " );

		CSSHelpers::AppendUILength( pfmtBuffer, m_rgCornerRaddi[k_ECornerTopLeft].m_VerticalRadii );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_rgCornerRaddi[k_ECornerTopRight].m_VerticalRadii );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_rgCornerRaddi[k_ECornerBottomRight].m_VerticalRadii );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_rgCornerRaddi[k_ECornerBottomLeft].m_VerticalRadii );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		for( int i = 0; i < (int)k_ECornerMax; ++i )
		{
			if( !m_rgCornerRaddi[i].m_HorizontalRadii.IsSet() )
				return false;
			if( !m_rgCornerRaddi[i].m_VerticalRadii.IsSet() )
				return false;
		}
		return true;
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		for( int i = 0; i < (int)k_ECornerMax; ++i )
		{
			if( !m_rgCornerRaddi[i].m_HorizontalRadii.IsSet() )
				m_rgCornerRaddi[i].m_HorizontalRadii.SetLength( 0 );
			if( !m_rgCornerRaddi[i].m_VerticalRadii.IsSet() )
				m_rgCornerRaddi[i].m_VerticalRadii.SetLength( 0 );
		}
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		if( symProperty == symbol )
		{
			return "Shorthand to set border radius for all corners at once.  Border radius rounds off corners of the panel, adjusting the border to "
				"smoothly round and also clipping background image/color and contents to the specified elliptical or circular values.  In this shorthand "
				"version you may specify a single value for all raddi, or horizontal / vertical separated by the '/' character.  For both horizontal and "
				"vertical you may specify 1 to 4 values in pixels or %, they will be taken in order as top-left, top-right, bottom-right, bottom-left radii "
				"values.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"// 2 px circular corners on all sides\n"
				"border-radius: 2px;\n"
				"// Perfect circular or elliptical panel (circular if box was square)\n"
				"border-radius: 50% / 50%;\n"
				"// 2 px horizontal radii 4px vertical elliptical corners on all sides\n"
				"border-radius: 2px / 4px;\n"
				"// All corners fully specified \n"
				"border-radius: 2px 3px 4px 2px / 2px 3px 3px 2px;"
				"</pre>";
		}
		else if( symProperty == topLeftRadius )
		{
			return "Specifies border-radius for top-left corner which rounds off border and clips background/foreground content to rounded edge.  "
				"Takes 1 or 2 values in px or %, first value is horizontal radii for elliptical corner, second is vertical radii, if only one is specified "
				"then horizontal/vertical will both be set and corner will be circular.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-top-left-radius: 2px 2px;\n"
				"border-top-left-radius: 5%;"
				"</pre>";
		}
		else if( symProperty == topRightRadius )
		{
			return "Specifies border-radius for top-right corner which rounds off border and clips background/foreground content to rounded edge.  "
				"Takes 1 or 2 values in px or %, first value is horizontal radii for elliptical corner, second is vertical radii, if only one is specified "
				"then horizontal/vertical will both be set and corner will be circular.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-top-right-radius: 2px 2px;\n"
				"border-top-right-radius: 5%;"
				"</pre>";
		}
		else if( symProperty == bottomRightRadius )
		{
			return "Specifies border-radius for bottom-right corner which rounds off border and clips background/foreground content to rounded edge.  "
				"Takes 1 or 2 values in px or %, first value is horizontal radii for elliptical corner, second is vertical radii, if only one is specified "
				"then horizontal/vertical will both be set and corner will be circular.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-bottom-right-radius: 2px 2px;\n"
				"border-bottom-right-radius: 5%;"
				"</pre>";
		}
		else if( symProperty == bottomLeftRadius )
		{
			return "Specifies border-radius for bottom-left corner which rounds off border and clips background/foreground content to rounded edge.  "
				"Takes 1 or 2 values in px or %, first value is horizontal radii for elliptical corner, second is vertical radii, if only one is specified "
				"then horizontal/vertical will both be set and corner will be circular.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-bottom-left-radius: 2px 2px;\n"
				"border-bottom-left-radius: 5%;"
				"</pre>";
		}

		return CStyleProperty::GetDescription( symProperty );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyBorderRadius &rhs = (const CStylePropertyBorderRadius &)other;

		for( int i = 0; i < V_ARRAYSIZE( m_rgCornerRaddi ); i++ )
		{
			if( m_rgCornerRaddi[i] != rhs.m_rgCornerRaddi[i] )
				return false;
		}

		return true;
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	enum ECorner
	{
		k_ECornerInvalid = -1,
		k_ECornerTopLeft = 0,
		k_ECornerTopRight,
		k_ECornerBottomRight,
		k_ECornerBottomLeft,

		// Sentinel, for array iteration
		k_ECornerMax
	};

	struct CornerRadii_t
	{
		CUILength m_HorizontalRadii;
		CUILength m_VerticalRadii;

		bool operator==(const CornerRadii_t &rhs) const { return (m_HorizontalRadii == rhs.m_HorizontalRadii && m_VerticalRadii == rhs.m_VerticalRadii); }
		bool operator!=(const CornerRadii_t &rhs) const { return !(*this == rhs); }
	};

	CornerRadii_t m_rgCornerRaddi[k_ECornerMax];
};


//-----------------------------------------------------------------------------
// Purpose: border image property
//-----------------------------------------------------------------------------
class CStylePropertyBorderImage : public CStyleProperty
{
public:

	// forward decl
	struct BorderImageWidth_t;

	static const CStyleSymbol symbol;
	static const CStyleSymbol symImageSource;
	static const CStyleSymbol symImageSlice;
	static const CStyleSymbol symImageWidth;
	static const CStyleSymbol symImageOutset;
	static const CStyleSymbol symImageRepeat;

	CStylePropertyBorderImage() : CStyleProperty( CStylePropertyBorderImage::symbol )
	{
		m_bFillCenter = false;
		m_pImage = NULL;
		m_bSlicesSet = false;
		m_bWidthsSet = false;
		m_bOutsetsSet = false;
		m_bStretchSet = false;
		for( int i = 0; i < 4; ++i )
		{
			m_Width[i].m_eType = k_EBorderImageWidthNumber;
			m_Width[i].m_flValue = 1.0f;
			m_Outsets[i].SetLength( 0.0f );
			m_Slices[i].SetPercent( 100.0f );
		}
		m_Stretch[0] = k_EBorderImageStretchStretch;
		m_Stretch[1] = k_EBorderImageStretchStretch;
	}

	virtual ~CStylePropertyBorderImage()
	{
		SAFE_RELEASE( m_pImage );
	}

	// Merge properties
	virtual void MergeTo( CStyleProperty *pTarget ) const;

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) { }

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString );

	// Parse image source
	bool BParseImageSource( const char *pchString, const char **pchAfterParse );

	// Parse image slices
	bool BParseImageSlices( const char *pchString, const char **pchAfterParse );

	// Parse image widths
	bool BParseImageWidths( const char *pchString, const char **pchAfterParse );

	// Parse image outsets
	bool BParseImageOutsets( const char *pchString, const char **pchAfterParse );

	// Parse image repeats
	bool BParseImageRepeats( const char *pchString, const char **pchAfterParse );

	// Parse individual border image width
	bool BParseImageWidth( BorderImageWidth_t *pWidth, const char *pchString, const char **pchAfterParse );

	// Parse individual repeat
	bool BParseRepeat( EBorderImageRepeatType *pRepeat, const char *pchString, const char **pchAfterParse );

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const;

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		if( m_bSlicesSet && m_bStretchSet && m_bWidthsSet && m_bOutsetsSet && !m_sURLPath.IsEmpty() )
			return true;

		return false;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor );

	// called when applied to a panel after comparing with set values
	virtual void OnAppliedToPanel( IUIPanel *pPanel, float flScaleFactor );

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		if( symProperty == symbol )
		{
			return "Shorthand for specifying all the border-image related properties at once.\n"
				"Technical syntax is: &lt;border-image-source&gt; || &lt;border-image-slice&gt; [ / &lt;border-image-width&gt;? [ / &lt;border-image-outset&gt; ]? ]? || &lt;border-image-repeat&gt;, "
				"see the explanations for individual properties for details on each.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-image: url( \"file://message_border.tga\" ) 25% repeat;\n"
				"border-image: url( \"file://message_border.tga\" ) 25% / 1 / 20px repeat;"
				"</pre>";
		}
		else if( symProperty == symImageSource )
		{
			return "Specifies the source image to use as the 9-slice border-image.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-image-source: url( \"file://message_border.tga\" );\n"
				"border-image-source: url( \"http://store.steampowered.com/public/images/steam/message_border.tga\" );"
				"</pre>";
		}
		else if( symProperty == symImageSlice )
		{
			return "Specifies the insets for top, right, bottom, and left (in order) slice offsets to use for slicing the source image into 9 regions.  "
				"The 'fill' keyword may optionally appear before or after the length values and specifies to draw the middle region as a fill for the body "
				"background of the panel, without it the middle region will not be drawn.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-image-slice: 10px 10px 10px 10px;\n"
				"border-image-slice: 20% 10% 20% 10% fill;"
				"</pre>";
		}
		else if( symProperty == symImageWidth )
		{
			return "By default after slicing the image as specified in border-image-slice the 9 regions will be used to fill the space specified "
				"by the standard border-width property.  This border-image-width property may be used to override that and specify different widths.  "
				"The values appear in top, right, bottom, left order, the 2nd through 4th may be ommited and corresponding earlier values will be used.  "
				"Values may be straight floats which specify a multiple of the corresponding border-width value, a percentage (which is relative "
				"to the size of the border image in the corresponding dimension), or 'auto' which means to use the intrinsic size of the corresponding "
				"border-image-slice.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-image-width: 1 1 1 1;\n"
				"border-image-slice: 50% 50% 50% 50%;\n"
				"border-image-slice: auto;"
				"</pre>";
		}
		else if( symProperty == symImageOutset )
		{
			return "Specifies the amount by which the border image should draw outside of the normal content/border box, this allows the border image "
				"to extend into the margin area and draw outside the panels bounds.  This may still result in clipping of the image by a parent panel if "
				"the parents bounds are too close to the edges of the panel with the border-image.  Values are specified as px or % in top, right, bottom, left "
				"order with the 2nd through 4th values optional.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-image-outset: 0px;\n"
				"border-image-outset: 20px 20px 20px 20px;"
				"</pre>";
		}
		else if( symProperty == symImageRepeat )
		{
			return "Specifies how the top/right/bottom/left/middle images of the 9 slice regions are stretched to fit the available space.  "
				"Options are stretch, repeat, round or space.  Stretch/repeat are self explanatory, round means tile (repeat) but scale first"
				"to ensure that a whole number of tiles is used with no partial tile at the edge of the space, space means tile (repeat) but add "
				"padding between tiles to ensure a whole number of tiles with no partial tile at the edge is needed.\n"
				"Two values are specified, the first applies to how we stretch the top/middle/bottom horizontally to fill space, the second "
				"applies to how we stretch the left/middle/right vertically to fill space.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"border-image-repeat: stretch stretch;\n"
				"border-image-outset: repeat;\n"
				"border-image-outset: round;\n"
				"border-image-outset: stetch space;"
				"</pre>";
		}

		return CStyleProperty::GetDescription( symProperty );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyBorderImage &rhs = (const CStylePropertyBorderImage &)other;

		if( m_sURLPath != rhs.m_sURLPath || m_bSlicesSet != rhs.m_bSlicesSet )
			return false;

		for( int i = 0; i < V_ARRAYSIZE( m_Slices ); i++ )
		{
			if( m_Slices[i] != rhs.m_Slices[i] )
				return false;
		}

		if( m_bFillCenter != rhs.m_bFillCenter )
			return false;

		if( m_bWidthsSet != rhs.m_bWidthsSet )
			return false;

		if( m_bWidthsSet && V_memcmp( m_Width, rhs.m_Width, sizeof( m_Width ) ) != 0 )
			return false;

		if( m_bOutsetsSet != rhs.m_bOutsetsSet )
			return false;

		if( m_bOutsetsSet )
		{
			for( int i = 0; i < V_ARRAYSIZE( m_Outsets ); i++ )
			{
				if( m_Outsets[i] != rhs.m_Outsets[i] )
					return false;
			}
		}

		if( m_bStretchSet != rhs.m_bStretchSet )
			return false;

		if( m_bStretchSet && V_memcmp( m_Stretch, rhs.m_Stretch, sizeof( m_Stretch ) ) != 0 )
			return false;

		return true;
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	// border image source
	IImageSource *m_pImage;
	CUtlString m_sURLPath;

	// border image slice
	bool m_bSlicesSet;
	CUILength m_Slices[4];
	bool m_bFillCenter;

	// border image width
	struct BorderImageWidth_t
	{
		EBorderImageWidthType m_eType;
		float m_flValue;
	};
	bool m_bWidthsSet;
	BorderImageWidth_t m_Width[4];

	// border image outsets
	bool m_bOutsetsSet;
	CUILength m_Outsets[4];

	// border image stretch values
	bool m_bStretchSet;
	EBorderImageRepeatType m_Stretch[2];
};


//-----------------------------------------------------------------------------
// Purpose: Background-image property
//-----------------------------------------------------------------------------
class CStylePropertyBackgroundImage : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	static const CStyleSymbol backgroundSize;
	static const CStyleSymbol backgroundPosition;
	static const CStyleSymbol backgroundRepeat;

	CStylePropertyBackgroundImage();
	virtual ~CStylePropertyBackgroundImage();

	// from CStyleProperty
	virtual void MergeTo( CStyleProperty *pTarget ) const;
	virtual bool BCanTransition() { return false; }
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) { Assert( !"You can't interpolate an image" ); }
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString );
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const;
	virtual bool BFullySet();
	virtual void ResolveDefaultValues();
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }
	virtual bool operator==(const CStyleProperty &rhs) const;
	virtual void ApplyUIScaleFactor( float flScaleFactor );
	virtual void OnAppliedToPanel( IUIPanel *pPanel );
	virtual const char *GetDescription( CStyleSymbol symProperty );

	CUtlVector< CBackgroundImageLayer * > &AccessLayers() { return m_vecLayers; }
	void Set( const CUtlVector< CBackgroundImageLayer * > &vecLayers );

	bool BHasActiveMovie();
	void EnableBackgroundMovies( bool bEnable );


#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif	

private:
	CBackgroundImageLayer *GetOrAddLayer( int i );
	void AddLayer();
	void Clear();
	void RemoveUnsetLayers();

	CUtlVector< CBackgroundImageLayer * > m_vecLayers;
};


//-----------------------------------------------------------------------------
// Purpose: opacity-mask property
//-----------------------------------------------------------------------------
class CStylePropertyOpacityMask : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	static const CStyleSymbol symbolScrollUp;
	static const CStyleSymbol symbolScrollDown;
	static const CStyleSymbol symbolScrollUpDown;

	CStylePropertyOpacityMask() : CStyleProperty( CStylePropertyOpacityMask::symbol )
	{
		m_pImage = NULL;
		m_flOpacityMaskOpacity = 1.0f;
		m_pImageUp = NULL;
		m_flOpacityMaskOpacityUp = 1.0f;
		m_pImageDown = NULL;
		m_flOpacityMaskOpacityDown = 1.0f;
		m_pImageUpDown = NULL;
		m_flOpacityMaskOpacityUpDown = 1.0f;

		m_bWasSet = false;
	}

	virtual ~CStylePropertyOpacityMask()
	{
		SAFE_RELEASE( m_pImage );
		SAFE_RELEASE( m_pImageUp );
		SAFE_RELEASE( m_pImageDown );
		SAFE_RELEASE( m_pImageUpDown );
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyOpacityMask::MergeTo" );
			return;
		}


		CStylePropertyOpacityMask *p = (CStylePropertyOpacityMask *)pTarget;
		if( !p->m_bWasSet )
		{
			p->m_pImage = m_pImage;
			p->m_sURL = m_sURL;
			p->m_flOpacityMaskOpacity = m_flOpacityMaskOpacity;

			p->m_sURLUp = m_sURLUp;
			p->m_pImageUp = m_pImageUp;
			p->m_sURLUp = m_sURLUp;

			p->m_sURLDown = m_sURLDown;
			p->m_pImageDown = m_pImageDown;
			p->m_sURLDown = m_sURLDown;

			p->m_sURLUpDown = m_sURLUpDown;
			p->m_pImageUpDown = m_pImageUpDown;
			p->m_sURLUpDown = m_sURLUpDown;
		}
	}

	// Does the style only affect compositing of it's panels and not drawing within a composition layer?
	virtual bool BAffectsCompositionOnly() { return true; }

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		CStylePropertyOpacityMask *p = (CStylePropertyOpacityMask *)&target;
		if( !p->m_sURL.IsValid() )
		{
			// Fade out
			m_flOpacityMaskOpacity = Lerp( flProgress, m_flOpacityMaskOpacity, 0.0f );
		}
		else if( !m_sURL.IsValid() && p->m_sURL.IsValid() )
		{
			// Fade in
			m_sURL = p->m_sURL;
			m_flOpacityMaskOpacity = Lerp( flProgress, 0.0f, p->m_flOpacityMaskOpacity );
		}
		else
		{
			m_sURL = p->m_sURL;
			m_flOpacityMaskOpacity = Lerp( flProgress, m_flOpacityMaskOpacity, p->m_flOpacityMaskOpacity );
		}

		m_sURLUp = p->m_sURLUp;
		m_pImageUp = p->m_pImageUp;
		m_sURLUp = p->m_sURLUp;

		m_sURLDown = p->m_sURLDown;
		m_pImageDown = p->m_pImageDown;
		m_sURLDown = p->m_sURLDown;

		m_sURLUpDown = p->m_sURLUpDown;
		m_pImageUpDown = p->m_pImageUpDown;
		m_sURLUpDown = p->m_sURLUpDown;
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( symParsedName == symbol )
		{
			if( BParseProperty( pchString, m_flOpacityMaskOpacity, &m_pImage, m_sURL ) )
			{
				m_bWasSet = true;
				return true;
			}
			return false;
		}
		else if( symParsedName == symbolScrollUp )
		{
			return BParseProperty( pchString, m_flOpacityMaskOpacityUp, &m_pImageUp, m_sURLUp );
		}
		else if( symParsedName == symbolScrollDown )
		{
			return BParseProperty( pchString, m_flOpacityMaskOpacityDown, &m_pImageDown, m_sURLDown );
		}
		else if( symParsedName == symbolScrollUpDown )
		{
			return BParseProperty( pchString, m_flOpacityMaskOpacityUpDown, &m_pImageUpDown, m_sURLUpDown );
		}

		return false;
	}


	bool BParseProperty( const char *pchString, float &flOpacityMaskOpacity, IImageSource **ppImage, CUtlString &sURL )
	{
		if( V_stricmp( "none", pchString ) == 0 )
		{
			*ppImage = NULL;
			flOpacityMaskOpacity = 0.0f;
			return true;
		}
		else
		{
			CUtlString sURLPath;
			if( !CSSHelpers::BParseURL( sURLPath, pchString, &pchString ) )
				return false;

			sURL = sURLPath;
			if( !sURL.IsValid() )
				return false;

			// Optional opacity value following
			pchString = CSSHelpers::SkipSpaces( pchString );
			if( !CSSHelpers::BParseNumber( &flOpacityMaskOpacity, pchString, &pchString ) )
				flOpacityMaskOpacity = 1.0f;

			return true;
		}
	}


	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		if( !m_sURL.IsValid() )
		{
			pfmtBuffer->Append( "none" );
		}
		else
		{
			CSSHelpers::AppendURL( pfmtBuffer, m_sURL.String() );
			pfmtBuffer->Append( " " );
			pfmtBuffer->AppendFormat( "%f", m_flOpacityMaskOpacity );
		}
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		if( symProperty == symbol )
		{
			return "Applies an image as an opacity mask that stretches to the panel bounds and fades out it's content based on the alpha channel. "
				"The second float value is an optional opacity value for the mask itself, the image won't interpolate/cross-fade, but you can animate the opacity to fade the mask in/out. "
				"The -scroll-up, -scroll-down, and -scroll-up-down varients override the mask and apply only when the various vertical scroll scenarios affect the panel based on the overflow property.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"opacity-mask: url( \"file://{images}/upper_row_mask.tga\" );\n"
				"opacity-mask: url( \"file://{images}/upper_row_mask.tga\" ) 0.5;\n"
				"opacity-mask-scroll-up: url( \"file://{images}/upper_row_mask_up.tga\" ) 0.5;\n"
				"opacity-mask-scroll-down: url( \"file://{images}/upper_row_mask_down.tga\" ) 0.5;\n"
				"opacity-mask-scroll-up-down: url( \"file://{images}/upper_row_mask_up_down.tga\" ) 0.5;"
				"</pre>";
		}
		return "";
	}

	virtual void OnAppliedToPanel( IUIPanel *pPanel );

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyOpacityMask &rhs = (const CStylePropertyOpacityMask &)other;
		return (
			m_sURL == rhs.m_sURL && m_flOpacityMaskOpacity == rhs.m_flOpacityMaskOpacity &&
			m_sURLUp == rhs.m_sURLUp && m_flOpacityMaskOpacityUp == rhs.m_flOpacityMaskOpacityUp &&
			m_sURLDown == rhs.m_sURLDown && m_flOpacityMaskOpacityDown == rhs.m_flOpacityMaskOpacityDown &&
			m_sURLUpDown == rhs.m_sURLUpDown && m_flOpacityMaskOpacityUpDown == rhs.m_flOpacityMaskOpacityUpDown
			);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName );
#endif

	IImageSource *m_pImage;
	IImageSource *m_pImageUp;
	IImageSource *m_pImageDown;
	IImageSource *m_pImageUpDown;

	float m_flOpacityMaskOpacity;
	float m_flOpacityMaskOpacityUp;
	float m_flOpacityMaskOpacityDown;
	float m_flOpacityMaskOpacityUpDown;

private:
	bool m_bWasSet;
	CUtlString m_sURL;
	CUtlString m_sURLUp;
	CUtlString m_sURLDown;
	CUtlString m_sURLUpDown;
};


//-----------------------------------------------------------------------------
// Purpose: White-space property
//-----------------------------------------------------------------------------
class CStylePropertyWhiteSpace : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	CStylePropertyWhiteSpace() : CStyleProperty( CStylePropertyWhiteSpace::symbol )
	{
		m_bSet = false;
		m_bWrap = true;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyWhiteSpace::MergeTo" );
			return;
		}

		CStylePropertyWhiteSpace *p = (CStylePropertyWhiteSpace *)pTarget;
		if( !p->m_bSet )
		{
			p->m_bWrap = m_bWrap;
			p->m_bSet = m_bSet;
		}
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		pchString = CSSHelpers::SkipSpaces( pchString );
		if( V_strnicmp( pchString, "nowrap", V_ARRAYSIZE( "nowrap" ) ) == 0 )
		{
			m_bWrap = false;
			m_bSet = true;
			return true;
		}
		else if( V_strnicmp( pchString, "normal", V_ARRAYSIZE( "normal" ) ) == 0 )
		{
			m_bWrap = true;
			m_bSet = true;
			return true;
		}

		return false;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		if( m_bWrap )
			pfmtBuffer->Append( "normal" );
		else
			pfmtBuffer->Append( "nowrap" );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		if( symProperty == symbol )
		{
			return "Controls white-space wrapping on rendered text.  \"normal\" means wrap on whitespace, \"nowrap\" means do no wrapping at all.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"white-space: normal;\n"
				"white-space: nowrap;"
				"</pre>";
		}
		return "";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyWhiteSpace &rhs = (const CStylePropertyWhiteSpace &)other;
		return (m_bWrap == rhs.m_bWrap && m_bSet == rhs.m_bSet);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	bool m_bWrap;
	bool m_bSet;
};


//-----------------------------------------------------------------------------
// Purpose: text-overflow property
//-----------------------------------------------------------------------------
class CStylePropertyTextOverflow : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	CStylePropertyTextOverflow() : CStyleProperty( CStylePropertyTextOverflow::symbol )
	{
		m_bSet = false;
		m_bEllipsis = true;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTextOverflow::MergeTo" );
			return;
		}

		CStylePropertyTextOverflow *p = (CStylePropertyTextOverflow *)pTarget;
		if( !p->m_bSet )
		{
			p->m_bEllipsis = m_bEllipsis;
			p->m_bSet = m_bSet;
		}
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		pchString = CSSHelpers::SkipSpaces( pchString );
		if( V_strnicmp( pchString, "clip", V_ARRAYSIZE( "clip" ) ) == 0 )
		{
			m_bEllipsis = false;
			m_bSet = true;
			return true;
		}
		else if( V_strnicmp( pchString, "ellipsis", V_ARRAYSIZE( "ellipsis" ) ) == 0 )
		{
			m_bEllipsis = true;
			m_bSet = true;
			return true;
		}

		return false;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		if( m_bEllipsis )
			pfmtBuffer->Append( "ellipsis" );
		else
			pfmtBuffer->Append( "clip" );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		if( symProperty == symbol )
		{
			return "Controls truncation of text that doesn't fit in a panel.  \"clip\" means to simply truncate (on char boundaries), \"ellipsis\" means to end with '...'.\n"
				"We default to ellipsis, which is contrary to the normal CSS spec.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"text-overflow: ellipsis;\n"
				"text-overflow: clip;"
				"</pre>";
		}
		return "";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyTextOverflow &rhs = (const CStylePropertyTextOverflow &)other;
		return (m_bEllipsis == rhs.m_bEllipsis && m_bSet == rhs.m_bSet);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	bool m_bEllipsis;
	bool m_bSet;
};



//-----------------------------------------------------------------------------
// Purpose: Width property
//-----------------------------------------------------------------------------
class CStylePropertyWidth : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	CStylePropertyWidth() : CStyleProperty( CStylePropertyWidth::symbol )
	{
		m_Length.SetFitChildren();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyWidth::MergeTo" );
			return;
		}

		CStylePropertyWidth *p = (CStylePropertyWidth *)pTarget;
		p->m_Length = m_Length;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		CStylePropertyWidth *p = (CStylePropertyWidth *)&target;

		float flParentWidth, flParentHeight, flParentPerspective;
		GetParentSizeAvailable( pPanel, flParentWidth, flParentHeight, flParentPerspective );

		m_Length = LerpUILength( flProgress, m_Length, p->m_Length, flParentWidth );
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		bool bSuccess = CSSHelpers::BParseIntoUILengthForSizing( &m_Length, pchString, NULL );

		if ( m_Length.IsWidthPercentage() )
			return false;

		return bSuccess;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_Length.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, m_Length );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return	"Sets the width for this panel. "
			"Possible values:<br>"
			"\"fit-children\" - Panel size is set to the required size of all children (default)<br>"
			"&lt;pixels&gt; - Any fixed pixel value (ex: \"100px\")<br>"
			"&lt;percentage&gt; - Percentage of parent width (ex: \"100%\")<br>"
			"\"fill-parent-flow( &lt;weight&gt; )\" - Fills to remaining parent width. If multiple children are set to this value, weight is used to determine final width. For example, if three children are set to fill-parent-flow of 1.0 and the parent is 300px wide, each child will be 100px wide. (ex: \"fill-parent-flow( 1.0 )\" )<br>"
			"\"height-percentage( &lt;percentage&gt; )\" - Percentage of the panel's height, which allows you to enforce a particular aspect ratio.  The height cannot also be width-percentage.";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyWidth &rhs = (const CStylePropertyWidth &)other;
		return (m_Length == rhs.m_Length);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	CUILength m_Length;
};


//-----------------------------------------------------------------------------
// Purpose: Height property
//-----------------------------------------------------------------------------
class CStylePropertyHeight : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	CStylePropertyHeight() : CStyleProperty( CStylePropertyHeight::symbol )
	{
		m_Height.SetFitChildren();
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyHeight::MergeTo" );
			return;
		}

		CStylePropertyHeight *p = (CStylePropertyHeight *)pTarget;
		p->m_Height = m_Height;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		CStylePropertyHeight *p = (CStylePropertyHeight *)&target;

		float flParentWidth, flParentHeight, flParentPerspective;
		GetParentSizeAvailable( pPanel, flParentWidth, flParentHeight, flParentPerspective );

		m_Height = LerpUILength( flProgress, m_Height, p->m_Height, flParentHeight );
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		bool bSuccess = CSSHelpers::BParseIntoUILengthForSizing( &m_Height, pchString, NULL );

		if ( m_Height.IsHeightPercentage() )
			return false;

		return bSuccess;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_Height.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, m_Height );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return	"Sets the height for this panel. "
			"Possible values:<br>"
			"\"fit-children\" - Panel size is set to the required size of all children (default)<br>"
			"&lt;pixels&gt; - Any fixed pixel value (ex: \"100px\")<br>"
			"&lt;percentage&gt; - Percentage of parent height (ex: \"100%\")<br>"
			"\"fill-parent-flow( &lt;weight&gt; )\" - Fills to remaining parent width. If multiple children are set to this value, weight is used to determine final height. For example, if three children are set to fill-parent-flow of 1.0 and the parent is 300px tall, each child will be 100px tall. (ex: \"fill-parent-flow( 1.0 )\" )<br>"
			"\"width-percentage( &lt;percentage&gt; )\" - Percentage of the panel's width, which allows you to enforce a particular aspect ratio.  The width cannot also be height-percentage.";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyHeight &rhs = (const CStylePropertyHeight &)other;
		return (m_Height == rhs.m_Height);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	CUILength m_Height;
};


//-----------------------------------------------------------------------------
// Purpose: min-width property
//-----------------------------------------------------------------------------
class CStylePropertyMinWidth : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyMinWidth() : CStyleProperty( CStylePropertyMinWidth::symbol )
	{
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyMinWidth::MergeTo" );
			return;
		}

		CStylePropertyMinWidth *p = (CStylePropertyMinWidth *)pTarget;
		p->m_minWidth = m_minWidth;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )  {}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		bool bSuccess = CSSHelpers::BParseIntoUILengthForSizing( &m_minWidth, pchString, NULL );
		return bSuccess;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_minWidth.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, m_minWidth );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyMinWidth &rhs = (const CStylePropertyMinWidth &)other;
		return (m_minWidth == rhs.m_minWidth);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	CUILength m_minWidth;
};


//-----------------------------------------------------------------------------
// Purpose: min-height property
//-----------------------------------------------------------------------------
class CStylePropertyMinHeight : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyMinHeight() : CStyleProperty( CStylePropertyMinHeight::symbol )
	{
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyMinHeight::MergeTo" );
			return;
		}

		CStylePropertyMinHeight *p = (CStylePropertyMinHeight *)pTarget;
		p->m_minHeight = m_minHeight;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )  {}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		bool bSuccess = CSSHelpers::BParseIntoUILengthForSizing( &m_minHeight, pchString, NULL );
		return bSuccess;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_minHeight.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, m_minHeight );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyMinHeight &rhs = (const CStylePropertyMinHeight &)other;
		return (m_minHeight == rhs.m_minHeight);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	CUILength m_minHeight;
};


//-----------------------------------------------------------------------------
// Purpose: max-width property
//-----------------------------------------------------------------------------
class CStylePropertyMaxWidth : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyMaxWidth() : CStyleProperty( CStylePropertyMaxWidth::symbol )
	{
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyMaxWidth::MergeTo" );
			return;
		}

		CStylePropertyMaxWidth *p = (CStylePropertyMaxWidth *)pTarget;
		p->m_maxWidth = m_maxWidth;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		float flParentWidth, flParentHeight, flParentPerspective;
		GetParentSizeAvailable( pPanel, flParentWidth, flParentHeight, flParentPerspective );

		const CStylePropertyMaxWidth *p = (const CStylePropertyMaxWidth*)&target;
		m_maxWidth = LerpUILength( flProgress, m_maxWidth, p->m_maxWidth, flParentWidth );
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		bool bSuccess = CSSHelpers::BParseIntoUILengthForSizing( &m_maxWidth, pchString, NULL );
		return bSuccess;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_maxWidth.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, m_maxWidth );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyMaxWidth &rhs = (const CStylePropertyMaxWidth &)other;
		return (m_maxWidth == rhs.m_maxWidth);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	CUILength m_maxWidth;
};


//-----------------------------------------------------------------------------
// Purpose: max-height property
//-----------------------------------------------------------------------------
class CStylePropertyMaxHeight : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyMaxHeight() : CStyleProperty( CStylePropertyMaxHeight::symbol )
	{
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyMaxHeight::MergeTo" );
			return;
		}

		CStylePropertyMaxHeight *p = (CStylePropertyMaxHeight*)pTarget;
		p->m_maxHeight = m_maxHeight;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return true; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
		float flParentWidth, flParentHeight, flParentPerspective;
		GetParentSizeAvailable( pPanel, flParentWidth, flParentHeight, flParentPerspective );

		const CStylePropertyMaxHeight *p = (const CStylePropertyMaxHeight *)&target;
		m_maxHeight = LerpUILength( flProgress, m_maxHeight, p->m_maxHeight, flParentHeight );
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		bool bSuccess = CSSHelpers::BParseIntoUILengthForSizing( &m_maxHeight, pchString, NULL );
		return bSuccess;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_maxHeight.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, m_maxHeight );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyMaxHeight &rhs = (const CStylePropertyMaxHeight &)other;
		return (m_maxHeight == rhs.m_maxHeight);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	CUILength m_maxHeight;
};


//-----------------------------------------------------------------------------
// Purpose: Width property
//-----------------------------------------------------------------------------
class CStylePropertyVisible : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	CStylePropertyVisible() : CStyleProperty( CStylePropertyVisible::symbol )
	{
		m_bVisible = true;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyHeight::MergeTo" );
			return;
		}

		CStylePropertyVisible *p = (CStylePropertyVisible *)pTarget;
		p->m_bVisible = m_bVisible;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )  {}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( V_stricmp( "visible", pchString ) == 0 )
		{
			m_bVisible = true;
			return true;
		}
		else if( V_stricmp( "collapse", pchString ) == 0 )
		{
			m_bVisible = false;
			return true;
		}

		return false;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		pfmtBuffer->Append( m_bVisible ? "visible" : "collapse" );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return	"Controls if the panel is visible and is included in panel layout. "
			"Possible values:<br>"
			"\"visible\" - panel is visible and included in layout (default)<br>"
			"\"collapse\" - panel is invisible and not included in layout";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyVisible &rhs = (const CStylePropertyVisible&)other;
		return (m_bVisible == rhs.m_bVisible);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty )
		{
			if( !m_bVisible )
				return k_EStyleInvalidateLayoutSizeAndPosition;
			else
				return k_EStyleInvalidateLayoutNone;
		}

		if( !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	bool m_bVisible;
};


//-----------------------------------------------------------------------------
// Purpose: Flow property
//-----------------------------------------------------------------------------
class CStylePropertyFlowChildren : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	CStylePropertyFlowChildren() : CStyleProperty( CStylePropertyFlowChildren::symbol )
	{
		m_eFlowDirection = k_EFlowNone;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyFlow::MergeTo" );
			return;
		}

		CStylePropertyFlowChildren *p = (CStylePropertyFlowChildren *)pTarget;
		p->m_eFlowDirection = m_eFlowDirection;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )  {}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( V_stricmp( "none", pchString ) == 0 )
			m_eFlowDirection = k_EFlowNone;
		else if( V_stricmp( "down", pchString ) == 0 )
			m_eFlowDirection = k_EFlowDown;
		else if( V_stricmp( "right", pchString ) == 0 )
			m_eFlowDirection = k_EFlowRight;
		else if ( V_stricmp( "down-wrap", pchString ) == 0 )
			m_eFlowDirection = k_EFlowDownWrap;
		else if ( V_stricmp( "right-wrap", pchString ) == 0 )
			m_eFlowDirection = k_EFlowRightWrap;
		else
			return false;

		return true;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		if( m_eFlowDirection == k_EFlowNone )
			pfmtBuffer->Append( "none" );
		else if( m_eFlowDirection == k_EFlowDown )
			pfmtBuffer->Append( "down" );
		else if( m_eFlowDirection == k_EFlowRight )
			pfmtBuffer->Append( "right" );
		else if ( m_eFlowDirection == k_EFlowDownWrap )
			pfmtBuffer->Append( "down-wrap" );
		else if ( m_eFlowDirection == k_EFlowRightWrap )
			pfmtBuffer->Append( "right-wrap" );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyFlowChildren &rhs = (const CStylePropertyFlowChildren&)other;
		return (m_eFlowDirection == rhs.m_eFlowDirection);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	EFlowDirection m_eFlowDirection;
};


//-----------------------------------------------------------------------------
// Purpose: base class for margin/padding
//-----------------------------------------------------------------------------
template < class T >
class CStylePropertyDimensionsBase : public CStyleProperty
{
public:
	CStylePropertyDimensionsBase() : CStyleProperty( T::symbol )
	{
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyDimensionsBase::MergeTo" );
			return;
		}

		T *p = (T *)pTarget;
		if( !p->m_left.IsSet() )
			p->m_left = m_left;

		if( !p->m_top.IsSet() )
			p->m_top = m_top;

		if( !p->m_right.IsSet() )
			p->m_right = m_right;

		if( !p->m_bottom.IsSet() )
			p->m_bottom = m_bottom;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )  {}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( symParsedName == T::symbol )
		{
			// we support 1 to 4 values specified.
			if( CSSHelpers::BParseIntoUILength( &m_top, pchString, &pchString ) )
			{
				if( CSSHelpers::BParseIntoUILength( &m_right, pchString, &pchString ) )
				{
					if( CSSHelpers::BParseIntoUILength( &m_bottom, pchString, &pchString ) )
					{
						if( CSSHelpers::BParseIntoUILength( &m_left, pchString, &pchString ) )
						{
							// got all 4
						}
						else
						{
							m_left = m_right;
						}
					}
					else
					{
						m_left = m_right;
						m_bottom = m_top;
					}
				}
				else
				{
					m_right = m_top;
					m_left = m_right;
					m_bottom = m_top;
				}

				return true;
			}
			return false;
		}
		else if( symParsedName == T::symbolLeft )
		{
			return CSSHelpers::BParseIntoUILength( &m_left, pchString, NULL );
		}
		else if( symParsedName == T::symbolTop )
		{
			return CSSHelpers::BParseIntoUILength( &m_top, pchString, NULL );
		}
		else if( symParsedName == T::symbolBottom )
		{
			return CSSHelpers::BParseIntoUILength( &m_bottom, pchString, NULL );
		}
		else if( symParsedName == T::symbolRight )
		{
			return CSSHelpers::BParseIntoUILength( &m_right, pchString, NULL );
		}

		return false;
	}

	// called when we are ready to apply any scaling factor to the values
	virtual void ApplyUIScaleFactor( float flScaleFactor )
	{
		m_left.ScaleLengthValue( flScaleFactor );
		m_top.ScaleLengthValue( flScaleFactor );
		m_bottom.ScaleLengthValue( flScaleFactor );
		m_right.ScaleLengthValue( flScaleFactor );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, m_top );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_right );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_bottom );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_left );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		return (m_left.IsSet() && m_top.IsSet() && m_bottom.IsSet() && m_right.IsSet());
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		if( !m_left.IsSet() )
			m_left.SetLength( 0 );

		if( !m_top.IsSet() )
			m_top.SetLength( 0 );

		if( !m_right.IsSet() )
			m_right.SetLength( 0 );

		if( !m_bottom.IsSet() )
			m_bottom.SetLength( 0 );
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyDimensionsBase<T> &rhs = (const CStylePropertyDimensionsBase<T> &)other;
		return (m_left == rhs.m_left && m_top == rhs.m_top && m_right == rhs.m_right && m_bottom == rhs.m_bottom);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutSizeAndPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	CUILength m_left;
	CUILength m_top;
	CUILength m_right;
	CUILength m_bottom;
};


//-----------------------------------------------------------------------------
// Purpose: Animation properties
//-----------------------------------------------------------------------------
const float k_flFloatInfiniteIteration = FLT_MAX;
class CStylePropertyAnimationProperties : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	static const CStyleSymbol symbolName;
	static const CStyleSymbol symbolDuration;
	static const CStyleSymbol symbolTiming;
	static const CStyleSymbol symbolIteration;
	static const CStyleSymbol symbolDirection;
	static const CStyleSymbol symbolDelay;

	CStylePropertyAnimationProperties() : CStyleProperty( CStylePropertyAnimationProperties::symbol )
	{
		m_bNone = false;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyAnimationProperties::MergeTo" );
			return;
		}

		CStylePropertyAnimationProperties *p = (CStylePropertyAnimationProperties *)pTarget;

		// if the other property is already set to none, don't merge
		if( p->m_bNone )
			return;

		// if other does not yet have names set, and we are none, set other to none
		if( m_bNone && (p->m_vecAnimationProperties.Count() == 0 || !p->m_vecAnimationProperties[0].m_symName.IsValid()) )
		{
			p->m_bNone = true;
			p->m_vecAnimationProperties.RemoveAll();
			return;
		}

		// bugbug cboyd - need to think about what to do when property counts dont match. currently, we do not change the number of properties in the
		// target when a property name has been set as that count should override ours
		bool bPropertiesSet = (p->m_vecAnimationProperties.Count() > 0) ? p->m_vecAnimationProperties[0].m_symName.IsValid() : false;

		// add properties to target to match our count
		if( !bPropertiesSet )
		{
			p->m_vecAnimationProperties.EnsureCapacity( m_vecAnimationProperties.Count() );
			for( int i = p->m_vecAnimationProperties.Count(); i < m_vecAnimationProperties.Count(); i++ )
				p->AddNewProperty();
		}

		// set all unset properties on target
		FOR_EACH_VEC( m_vecAnimationProperties, i )
		{
			// we have more properties than the other guy. Stop processing.
			if( bPropertiesSet && p->m_vecAnimationProperties.Count() <= i )
				break;

			AnimationProperty_t &animOther = p->m_vecAnimationProperties[i];
			const AnimationProperty_t &animUs = m_vecAnimationProperties[i];

			if( !animOther.m_symName.IsValid() )
				animOther.m_symName = animUs.m_symName;

			if( animOther.m_flDuration == k_flFloatNotSet )
				animOther.m_flDuration = animUs.m_flDuration;

			if( animOther.m_eTimingFunction == k_EAnimationUnset )
			{
				animOther.m_eTimingFunction = animUs.m_eTimingFunction;
				animOther.m_CubicBezier = animUs.m_CubicBezier;
			}

			if( animOther.m_flDelay == k_flFloatNotSet )
				animOther.m_flDelay = animUs.m_flDelay;

			if( animOther.m_eAnimationDirection == k_EAnimationDirectionUnset )
				animOther.m_eAnimationDirection = animUs.m_eAnimationDirection;

			if( animOther.m_flIteration == k_flFloatNotSet )
				animOther.m_flIteration = animUs.m_flIteration;
		}
	}

	virtual bool BCanTransition() { return false; }
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ ) {}

	void AddNewProperty()
	{
		int iVec = m_vecAnimationProperties.AddToTail();
		AnimationProperty_t &transition = m_vecAnimationProperties[iVec];

		// copy other set properties forward
		if( iVec == 0 )
		{
			transition.m_flDuration = k_flFloatNotSet;
			transition.m_eTimingFunction = k_EAnimationUnset;

			Vector2D vec[4];
			panorama::GetAnimationCurveControlPoints( transition.m_eTimingFunction, vec );
			transition.m_CubicBezier.SetControlPoints( vec );

			transition.m_flIteration = k_flFloatNotSet;
			transition.m_eAnimationDirection = k_EAnimationDirectionUnset;
			transition.m_flDelay = k_flFloatNotSet;
		}
		else
		{
			transition = m_vecAnimationProperties[iVec - 1];
			transition.m_symName = UTL_INVAL_SYMBOL;
		}
	}

	void SetAnimation( const char *pchAnimationName, float flDuration, float flDelay, EAnimationTimingFunction eTimingFunc, CCubicBezierCurve< Vector2D > cubicBezier, EAnimationDirection eDirection, float flIterations )
	{
		m_bNone = false;
		m_vecAnimationProperties.RemoveAll();

		AnimationProperty_t animation;
		animation.m_symName = pchAnimationName;
		animation.m_eTimingFunction = eTimingFunc;
		animation.m_CubicBezier = cubicBezier;
		animation.m_eAnimationDirection = eDirection;
		animation.m_flDuration = flDuration;
		animation.m_flDelay = flDelay;
		animation.m_flIteration = flIterations;

		m_vecAnimationProperties.AddToTail( animation );
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		static CPanoramaSymbol k_symNone = "none";

		if( symParsedName == symbol )
		{
			m_vecAnimationProperties.RemoveAll();

			// transition property format: name duration timing-function delay iteration-count animation-direction [,*]
			m_bNone = false;
			while( *pchString != '\0' )
			{
				AnimationProperty_t animation;

				// get the animation name
				char rgchName[k_nCSSPropertyNameMax];
				if( !CSSHelpers::BParseIdent( rgchName, V_ARRAYSIZE( rgchName ), pchString, &pchString ) )
					return false;

				animation.m_symName = CPanoramaSymbol( rgchName );
				if( animation.m_symName == k_symNone )
					return false;

				// get rest
				if( !CSSHelpers::BParseTime( &animation.m_flDuration, pchString, &pchString ) ||
					!CSSHelpers::BParseTimingFunction( &animation.m_eTimingFunction, &animation.m_CubicBezier, pchString, &pchString ) ||
					!CSSHelpers::BParseTime( &animation.m_flDelay, pchString, &pchString ) ||
					!BParseIterationCount( &animation.m_flIteration, pchString, &pchString ) ||
					!CSSHelpers::BParseAnimationDirectionFunction( &animation.m_eAnimationDirection, pchString, &pchString ) )
				{
					return false;
				}

				m_vecAnimationProperties.AddToTail( animation );

				// see if there is another transition property
				if( !CSSHelpers::BSkipComma( pchString, &pchString ) )
				{
					// end of list.. should be an empty string
					pchString = CSSHelpers::SkipSpaces( pchString );
					return (pchString[0] == '\0');
				}
			}

			return true;
		}
		else if( symParsedName == symbolName )
		{
			// comma separated list of keyframe names
			CUtlVector< CPanoramaSymbol > vecNames;
			if( !CSSHelpers::BParseCommaSepList( &vecNames, CSSHelpers::BParseIdentToSymbol, pchString ) )
				return false;

			// we also support 'none'
			if( vecNames.Count() == 1 && vecNames[0] == k_symNone )
			{
				m_bNone = true;
				return true;
			}

			// if we have less properties specified than other params, error
			if( m_vecAnimationProperties.Count() > vecNames.Count() )
				return false;

			m_vecAnimationProperties.EnsureCapacity( vecNames.Count() );
			FOR_EACH_VEC( vecNames, i )
			{
				if( vecNames[i] == k_symNone )
					return false;

				// new property?
				if( m_vecAnimationProperties.Count() <= i )
					AddNewProperty();

				m_vecAnimationProperties[i].m_symName = vecNames[i];
			}

			return true;
		}
		else if( symParsedName == symbolDuration )
		{
			return BParseAndAddProperty( &AnimationProperty_t::m_flDuration, CSSHelpers::BParseTime, pchString );
		}
		else if( symParsedName == symbolDelay )
		{
			return BParseAndAddProperty( &AnimationProperty_t::m_flDelay, CSSHelpers::BParseTime, pchString );
		}
		else if( symParsedName == symbolTiming )
		{
			return BParseAndAddTimingFunction( CSSHelpers::BParseTimingFunction, pchString );
		}
		else if( symParsedName == symbolIteration )
		{
			return BParseAndAddProperty( &AnimationProperty_t::m_flIteration, BParseIterationCount, pchString );
		}
		else if( symParsedName == symbolDirection )
		{
			return BParseAndAddProperty( &AnimationProperty_t::m_eAnimationDirection, CSSHelpers::BParseAnimationDirectionFunction, pchString );
		}

		return false;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		if( m_vecAnimationProperties.Count() == 0 || m_bNone )
		{
			pfmtBuffer->Append( "none" );
			return;
		}

		FOR_EACH_VEC( m_vecAnimationProperties, i )
		{
			if( i > 0 )
				pfmtBuffer->Append( ",\n" );

			const AnimationProperty_t &prop = m_vecAnimationProperties[i];
			pfmtBuffer->Append( prop.m_symName.String() );
			pfmtBuffer->Append( " " );
			CSSHelpers::AppendTime( pfmtBuffer, prop.m_flDuration );
			if( prop.m_eTimingFunction != k_EAnimationCustomBezier )
				pfmtBuffer->AppendFormat( " %s ", PchNameFromEAnimationTimingFunction( prop.m_eTimingFunction ) );
			else
				pfmtBuffer->AppendFormat( " cubic-bezier( %1.3f, %1.3f, %1.3f, %1.3f ) ",
				prop.m_CubicBezier.ControlPoint( 1 ).x,
				prop.m_CubicBezier.ControlPoint( 1 ).y,
				prop.m_CubicBezier.ControlPoint( 2 ).x,
				prop.m_CubicBezier.ControlPoint( 2 ).y );

			CSSHelpers::AppendTime( pfmtBuffer, prop.m_flDelay );
			pfmtBuffer->Append( " " );

			// iteration count
			if( prop.m_flIteration == k_flFloatInfiniteIteration )
				pfmtBuffer->Append( "infinite" );
			else
				CSSHelpers::AppendFloat( pfmtBuffer, prop.m_flIteration );

			// direction func
			pfmtBuffer->Append( " " );
			if( prop.m_eAnimationDirection == k_EAnimationDirectionNormal )
				pfmtBuffer->Append( "normal" );
			else if( prop.m_eAnimationDirection == k_EAnimationDirectionAlternate )
				pfmtBuffer->Append( "alternate" );
			else if ( prop.m_eAnimationDirection == k_EAnimationDirectionAlternateReverse )
				pfmtBuffer->Append( "alternate-reverse" );
			else if( prop.m_eAnimationDirection == k_EAnimationDirectionReverse )
				pfmtBuffer->Append( "reverse" );
			else if( prop.m_eAnimationDirection == k_EAnimationDirectionUnset )
				pfmtBuffer->Append( "unset" );
			else
				AssertMsg( false, "Unknown animation direction" );
		}
	}

	template < typename T >
	bool BParseAndAddProperty( T AnimationProperty_t::*pProp, bool( *func )(T*, const char *, const char **), const char *pchString )
	{
		// parse comma separated list
		CUtlVector< T > vec;
		if( !CSSHelpers::BParseCommaSepList( &vec, func, pchString ) || vec.Count() == 0 )
			return false;

		FOR_EACH_VEC( vec, i )
		{
			// new property?
			if( m_vecAnimationProperties.Count() <= i )
				AddNewProperty();

			m_vecAnimationProperties[i].*pProp = vec[i];
		}

		// if provided list is less than the total number of properties, fill in rest with last value
		for( int i = vec.Count(); i < m_vecAnimationProperties.Count(); i++ )
			m_vecAnimationProperties[i].*pProp = m_vecAnimationProperties[i - 1].*pProp;

		return true;
	}

	bool BParseAndAddTimingFunction( bool( *func )(EAnimationTimingFunction *, CCubicBezierCurve<Vector2D> *, const char *, const char **), const char *pchString )
	{
		// parse comma separated list
		CUtlVector< EAnimationTimingFunction > vec;
		CUtlVector< CCubicBezierCurve< Vector2D > > vec2;
		if( !CSSHelpers::BParseCommaSepList( &vec, &vec2, func, pchString ) || vec.Count() == 0 )
			return false;

		FOR_EACH_VEC( vec, i )
		{
			// new property?
			if( m_vecAnimationProperties.Count() <= i )
				AddNewProperty();

			m_vecAnimationProperties[i].m_eTimingFunction = vec[i];
			m_vecAnimationProperties[i].m_CubicBezier = vec2[i];
		}

		// if provided list is less than the total number of properties, fill in rest with last value
		for( int i = vec.Count(); i < m_vecAnimationProperties.Count(); i++ )
		{
			m_vecAnimationProperties[i].m_eTimingFunction = m_vecAnimationProperties[i - 1].m_eTimingFunction;
			m_vecAnimationProperties[i].m_CubicBezier = m_vecAnimationProperties[i - 1].m_CubicBezier;
		}

		return true;
	}

	static bool BParseIterationCount( float *pflValue, const char *pchString, const char **pchAfterParse )
	{
		pchString = CSSHelpers::SkipSpaces( pchString );

		// special case handling for infinite
		if( V_strnicmp( pchString, "infinite", V_strlen( "infinite" ) ) == 0 )
		{
			*pflValue = k_flFloatInfiniteIteration;
			if( pchAfterParse )
				*pchAfterParse = pchString + V_strlen( "infinite" );

			return true;
		}

		// should be a number
		return CSSHelpers::BParseNumber( pflValue, pchString, pchAfterParse );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found by looking at lower weight styles
	virtual bool BFullySet()
	{
		if( m_bNone )
			return true;

		if( m_vecAnimationProperties.Count() < 1 )
			return false;

		// if the first transition property is fully set, all other transition properties should be set
		AnimationProperty_t &animationProperty = m_vecAnimationProperties[0];
		return (animationProperty.m_symName.IsValid() &&
			animationProperty.m_flDuration != k_flFloatNotSet &&
			animationProperty.m_eTimingFunction != k_EAnimationUnset &&
			animationProperty.m_flIteration != k_flFloatNotSet &&
			animationProperty.m_eAnimationDirection != k_EAnimationDirectionUnset &&
			animationProperty.m_flDelay != k_flFloatNotSet);
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		if( m_vecAnimationProperties.Count() < 1 )
			return;

		// if property names aren't set or transition time isn't set (would default to 0), remove all and exit
		AnimationProperty_t &firstProperty = m_vecAnimationProperties[0];
		if( !firstProperty.m_symName.IsValid() || firstProperty.m_flDuration == k_flFloatNotSet )
		{
			m_vecAnimationProperties.RemoveAll();
			return;
		}

		// set defaults
		FOR_EACH_VEC( m_vecAnimationProperties, i )
		{
			AnimationProperty_t &animationProperty = m_vecAnimationProperties[i];

			if( animationProperty.m_eTimingFunction == k_EAnimationUnset )
			{
				animationProperty.m_eTimingFunction = k_EAnimationEase;

				Vector2D vec[4];
				panorama::GetAnimationCurveControlPoints( animationProperty.m_eTimingFunction, vec );
				animationProperty.m_CubicBezier.SetControlPoints( vec );
			}

			if( animationProperty.m_flIteration == k_flFloatNotSet )
				animationProperty.m_flIteration = 1;

			if( animationProperty.m_eAnimationDirection == k_EAnimationDirectionUnset )
				animationProperty.m_eAnimationDirection = k_EAnimationDirectionNormal;

			if( animationProperty.m_flDelay == k_flFloatNotSet )
				animationProperty.m_flDelay = 0;
		}
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyAnimationProperties &rhs = (const CStylePropertyAnimationProperties &)other;
		if( m_bNone != rhs.m_bNone )
			return false;

		if( m_vecAnimationProperties.Count() != rhs.m_vecAnimationProperties.Count() )
			return false;

		FOR_EACH_VEC( m_vecAnimationProperties, i )
		{
			if( m_vecAnimationProperties[i] != rhs.m_vecAnimationProperties[i] )
				return false;
		}

		return true;
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

#ifdef DBGFLAG_VALIDATE
	virtual void Validate( CValidator &validator, const tchar *pchName )
	{
		VALIDATE_SCOPE();
		ValidateObj( m_vecAnimationProperties );
		CStyleProperty::Validate( validator, pchName );
	}
#endif

	CUtlVector< AnimationProperty_t > m_vecAnimationProperties;
	bool m_bNone;
};


//-----------------------------------------------------------------------------
// Purpose: Padding property
//-----------------------------------------------------------------------------
class CStylePropertyPadding : public CStylePropertyDimensionsBase < CStylePropertyPadding >
{
public:
	static const CStyleSymbol symbol;
	static const CStyleSymbol symbolLeft;
	static const CStyleSymbol symbolTop;
	static const CStyleSymbol symbolBottom;
	static const CStyleSymbol symbolRight;

	CStylePropertyPadding() : CStylePropertyDimensionsBase<CStylePropertyPadding>() {}
};


//-----------------------------------------------------------------------------
// Purpose: Margin property
//-----------------------------------------------------------------------------
class CStylePropertyMargin : public CStylePropertyDimensionsBase < CStylePropertyMargin >
{
public:
	static const CStyleSymbol symbol;
	static const CStyleSymbol symbolLeft;
	static const CStyleSymbol symbolTop;
	static const CStyleSymbol symbolBottom;
	static const CStyleSymbol symbolRight;

	CStylePropertyMargin() : CStylePropertyDimensionsBase<CStylePropertyMargin>() {}
};


//-----------------------------------------------------------------------------
// Purpose: -s2-mix-blend-mode property
//-----------------------------------------------------------------------------
class CStylePropertyMixBlendMode : public CStyleProperty
{
public:


	static const CStyleSymbol symbol;
	CStylePropertyMixBlendMode() : CStyleProperty( CStylePropertyMixBlendMode::symbol )
	{
		m_bSet = false;
		m_eMixBlendMode = k_EMixBlendModeNormal;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyMixBlendMode::MergeTo" );
			return;
		}

		CStylePropertyMixBlendMode *p = (CStylePropertyMixBlendMode *)pTarget;
		if( !p->m_bSet )
		{
			p->m_eMixBlendMode = m_eMixBlendMode;
			p->m_bSet = m_bSet;
		}
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		pchString = CSSHelpers::SkipSpaces( pchString );
		if( V_strnicmp( pchString, "normal", V_ARRAYSIZE( "normal" ) ) == 0 )
		{
			m_eMixBlendMode = k_EMixBlendModeNormal;
			m_bSet = true;
			return true;
		}
		else if( V_strnicmp( pchString, "multiply", V_ARRAYSIZE( "multiply" ) ) == 0 )
		{
			m_eMixBlendMode = k_EMixBlendModeMultiply;
			m_bSet = true;
			return true;
		}
		else if( V_strnicmp( pchString, "screen", V_ARRAYSIZE( "screen" ) ) == 0 )
		{
			m_eMixBlendMode = k_EMixBlendModeScreen;
			m_bSet = true;
			return true;
		}

		return false;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		if( m_eMixBlendMode == k_EMixBlendModeNormal )
			pfmtBuffer->Append( "normal" );
		else if( m_eMixBlendMode == k_EMixBlendModeMultiply )
			pfmtBuffer->Append( "multiply" );
		else if( m_eMixBlendMode == k_EMixBlendModeScreen )
			pfmtBuffer->Append( "screen" );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		if( symProperty == symbol )
		{
			return "Controls blending mode for the panel.  See CSS mix-blend-mode docs on web, except normal for us is with alpha blending.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"-s2-mix-blend-mode: normal;\n"
				"-s2-mix-blend-mode: multiply;\n"
				"-s2-mix-blend-mode: screen;"
				"</pre>";
		}
		return "";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyMixBlendMode &rhs = (const CStylePropertyMixBlendMode &)other;
		return (m_eMixBlendMode == rhs.m_eMixBlendMode && m_bSet == rhs.m_bSet);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	EMixBlendMode m_eMixBlendMode;
	bool m_bSet;
};


//-----------------------------------------------------------------------------
// Purpose: texture-sampling
//-----------------------------------------------------------------------------
class CStylePropertyTextureSampleMode : public CStyleProperty
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyTextureSampleMode() : CStyleProperty( CStylePropertyTextureSampleMode::symbol )
	{
		m_bSet = false;
		m_eTextureSampleMode = k_ETextureSampleModeNormal;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if ( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTextureSampleMode::MergeTo" );
			return;
		}

		CStylePropertyTextureSampleMode *p = (CStylePropertyTextureSampleMode *)pTarget;
		if ( !p->m_bSet )
		{
			p->m_eTextureSampleMode = m_eTextureSampleMode;
			p->m_bSet = m_bSet;
		}
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )
	{
	}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		pchString = CSSHelpers::SkipSpaces( pchString );
		if ( V_strnicmp( pchString, "normal", V_ARRAYSIZE( "normal" ) ) == 0 )
		{
			m_eTextureSampleMode = k_ETextureSampleModeNormal;
			m_bSet = true;
			return true;
		}
		else if ( V_strnicmp( pchString, "alpha-only", V_ARRAYSIZE( "alpha-only" ) ) == 0 )
		{
			m_eTextureSampleMode = k_ETextureSampleModeAlphaOnly;
			m_bSet = true;
			return true;
		}

		return false;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		if ( m_eTextureSampleMode == k_ETextureSampleModeNormal )
			pfmtBuffer->Append( "normal" );
		else if ( m_eTextureSampleMode == k_ETextureSampleModeAlphaOnly )
			pfmtBuffer->Append( "alpha-only" );
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		if ( symProperty == symbol )
		{
			return "Controls texture sampling mode for the panel. Set to alpha-only to use the textures alpha data across all 3 color channels.<br><br>"
				"<b>Examples:</b>"
				"<pre>"
				"texture-sampling: normal;\n"
				"texture-sampling: alpha-only;"
				"</pre>";
		}
		return "";
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if ( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyTextureSampleMode &rhs = (const CStylePropertyTextureSampleMode &)other;
		return (m_eTextureSampleMode == rhs.m_eTextureSampleMode && m_bSet == rhs.m_bSet);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	ETextureSampleMode m_eTextureSampleMode;
	bool m_bSet;
};


//-----------------------------------------------------------------------------
// Purpose: Align property
//-----------------------------------------------------------------------------
class CStylePropertyAlign : public CStyleProperty
{
public:

	static const CStyleSymbol symbol;
	static const CStyleSymbol symbolHorizontal;
	static const CStyleSymbol symbolVertical;
	CStylePropertyAlign() : CStyleProperty( CStylePropertyAlign::symbol )
	{
		m_eHorizontalAlignment = k_EHorizontalAlignmentUnset;
		m_eVerticalAlignment = k_EVerticalAlignmentUnset;
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyAlign::MergeTo" );
			return;
		}

		CStylePropertyAlign *p = (CStylePropertyAlign *)pTarget;
		if( p->m_eHorizontalAlignment == k_EHorizontalAlignmentUnset )
			p->m_eHorizontalAlignment = m_eHorizontalAlignment;

		if( p->m_eVerticalAlignment == k_EVerticalAlignmentUnset )
			p->m_eVerticalAlignment = m_eVerticalAlignment;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )  {}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		if( symParsedName == symbol )
		{
			return CSSHelpers::BParseHorizontalAlignment( &m_eHorizontalAlignment, pchString, &pchString ) && CSSHelpers::BParseVerticalAlignment( &m_eVerticalAlignment, pchString, &pchString );
		}
		else if( symParsedName == symbolHorizontal )
		{
			return CSSHelpers::BParseHorizontalAlignment( &m_eHorizontalAlignment, pchString, &pchString );
		}
		else if( symParsedName == symbolVertical )
		{
			return CSSHelpers::BParseVerticalAlignment( &m_eVerticalAlignment, pchString, &pchString );
		}

		return false;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		pfmtBuffer->AppendFormat( "%s %s", PchNameFromEHorizontalAlignment( m_eHorizontalAlignment ), PchNameFromEVerticalAlignment( m_eVerticalAlignment ) );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		return (m_eHorizontalAlignment != k_EHorizontalAlignmentUnset && m_eVerticalAlignment != k_EVerticalAlignmentUnset);
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		if( m_eHorizontalAlignment == k_EHorizontalAlignmentUnset )
			m_eHorizontalAlignment = k_EHorizontalAlignmentLeft;

		if( m_eVerticalAlignment == k_EVerticalAlignmentUnset )
			m_eVerticalAlignment = k_EVerticalAlignmentTop;
	}

	// Comparison function
	virtual bool operator==(const CStyleProperty &other) const
	{
		if( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyAlign &rhs = (const CStylePropertyAlign &)other;
		return (m_eHorizontalAlignment == rhs.m_eHorizontalAlignment && m_eVerticalAlignment == rhs.m_eVerticalAlignment);
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		if( !pCompareProperty || !(*this == *pCompareProperty) )
			return k_EStyleInvalidateLayoutPosition;

		return k_EStyleInvalidateLayoutNone;
	}

	EHorizontalAlignment m_eHorizontalAlignment;
	EVerticalAlignment m_eVerticalAlignment;
};


//-----------------------------------------------------------------------------
// Purpose: Context UI Position property base class
//-----------------------------------------------------------------------------
class CStylePropertyContextUIPosition : public CStyleProperty
{
public:
	CStylePropertyContextUIPosition( CStyleSymbol symPropertyName ) : CStyleProperty( symPropertyName )
	{
		for ( EContextUIPosition &ePosition : m_ePositions )
		{
			ePosition = k_EContextUIPositionUnset;
		}
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if ( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTooltipPosition::MergeTo" );
			return;
		}

		CStylePropertyContextUIPosition *p = ( CStylePropertyContextUIPosition * )pTarget;
		for ( int i = 0; i < V_ARRAYSIZE( m_ePositions ); ++i )
		{
			if ( p->m_ePositions[ i ] == k_EContextUIPositionUnset )
				p->m_ePositions[ i ] = m_ePositions[ i ];
		}
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )  {}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		char rgchPosition[ 128 ];

		int i = 0;
		while ( *pchString != '\0' )
		{
			if ( !CSSHelpers::BParseIdent( rgchPosition, V_ARRAYSIZE( rgchPosition ), pchString, &pchString ) )
				return i != 0;

			if ( i >= V_ARRAYSIZE( m_ePositions ) )
				return false;

			if ( V_stricmp( rgchPosition, "left" ) == 0 )
				m_ePositions[ i ] = k_EContextUIPositionLeft;
			else if ( V_stricmp( rgchPosition, "top" ) == 0 )
				m_ePositions[ i ] = k_EContextUIPositionTop;
			else if ( V_stricmp( rgchPosition, "right" ) == 0 )
				m_ePositions[ i ] = k_EContextUIPositionRight;
			else if ( V_stricmp( rgchPosition, "bottom" ) == 0 )
				m_ePositions[ i ] = k_EContextUIPositionBottom;
			else
				return false;

			++i;
		}

		return true;
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		bool bFirst = true;
		for ( EContextUIPosition ePosition : m_ePositions )
		{
			if ( bFirst )
			{
				bFirst = false;
			}
			else
			{
				pfmtBuffer->Append( " " );
			}

			pfmtBuffer->AppendFormat( "%s", PchNameFromEContextUIPosition( ePosition ) );
		}
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		for ( EContextUIPosition ePosition : m_ePositions )
		{
			if ( ePosition == k_EContextUIPositionUnset )
				return false;
		}

		return true;
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		ResolveDefaultPositions( m_ePositions );
	}

	static void ResolveDefaultPositions( EContextUIPosition( &ePositions )[ 4 ] )
	{
		// By default, pick "right" if it's not set
		if ( ePositions[ 0 ] == k_EContextUIPositionUnset )
		{
			ePositions[ 0 ] = k_EContextUIPositionRight;
		}

		// By default, just swap which side the tooltip is on for the second option
		if ( ePositions[ 1 ] == k_EContextUIPositionUnset || ePositions[ 1 ] == ePositions[ 0 ] )
		{
			if ( ePositions[ 0 ] == k_EContextUIPositionLeft )
				ePositions[ 1 ] = k_EContextUIPositionRight;
			else if ( ePositions[ 0 ] == k_EContextUIPositionRight )
				ePositions[ 1 ] = k_EContextUIPositionLeft;
			else if ( ePositions[ 0 ] == k_EContextUIPositionTop )
				ePositions[ 1 ] = k_EContextUIPositionBottom;
			else // eTooltipPositions[ 0 ] == k_EContextUIPositionBottom
				ePositions[ 1 ] = k_EContextUIPositionTop;
		}

		// The third spot is normally the opposite of the first spot, unless those are both already used.  Then it's just right or bottom.
		if ( ePositions[ 2 ] == k_EContextUIPositionUnset || ( ePositions[ 2 ] == ePositions[ 0 ] || ePositions[ 2 ] == ePositions[ 1 ] ) )
		{
			if ( ( ePositions[ 0 ] == k_EContextUIPositionLeft && ePositions[ 1 ] == k_EContextUIPositionRight ) ||
				( ePositions[ 0 ] == k_EContextUIPositionRight && ePositions[ 1 ] == k_EContextUIPositionLeft ) )
			{
				ePositions[ 2 ] = k_EContextUIPositionBottom;
			}
			else if ( ( ePositions[ 0 ] == k_EContextUIPositionTop && ePositions[ 1 ] == k_EContextUIPositionBottom ) ||
				( ePositions[ 0 ] == k_EContextUIPositionBottom && ePositions[ 1 ] == k_EContextUIPositionTop ) )
			{
				ePositions[ 2 ] = k_EContextUIPositionRight;
			}
			else
			{
				if ( ePositions[ 0 ] == k_EContextUIPositionLeft )
					ePositions[ 2 ] = k_EContextUIPositionRight;
				else if ( ePositions[ 0 ] == k_EContextUIPositionRight )
					ePositions[ 2 ] = k_EContextUIPositionLeft;
				else if ( ePositions[ 0 ] == k_EContextUIPositionTop )
					ePositions[ 2 ] = k_EContextUIPositionBottom;
				else // eTooltipPositions[ 0 ] == k_EContextUIPositionBottom
					ePositions[ 2 ] = k_EContextUIPositionTop;
			}
		}

		// The last spot is just whatever is still leftover
		if ( ePositions[ 3 ] == k_EContextUIPositionUnset || ( ePositions[ 3 ] == ePositions[ 0 ] || ePositions[ 3 ] == ePositions[ 1 ] || ePositions[ 3 ] == ePositions[ 2 ] ) )
		{
			CUtlVector< EContextUIPosition > vecPositions( 0, 4 );
			const EContextUIPosition arrAllPositions[] = { k_EContextUIPositionLeft, k_EContextUIPositionTop, k_EContextUIPositionRight, k_EContextUIPositionBottom };
			vecPositions.CopyArray( arrAllPositions, V_ARRAYSIZE( arrAllPositions ) );
			for ( int i = 0; i < 3; ++i )
			{
				vecPositions.FindAndFastRemove( ePositions[ i ] );
			}
			ePositions[ 3 ] = vecPositions[ 0 ];
		}
	}

	// Comparison function
	virtual bool operator==( const CStyleProperty &other ) const
	{
		if ( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyContextUIPosition &rhs = ( const CStylePropertyContextUIPosition & )other;

		for ( int i = 0; i < V_ARRAYSIZE( m_ePositions ); ++i )
		{
			if ( m_ePositions[ i ] != rhs.m_ePositions[ i ] )
				return false;
		}

		return true;
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	EContextUIPosition m_ePositions[ 4 ];
};


//-----------------------------------------------------------------------------
// Purpose: Tooltip Position property
//-----------------------------------------------------------------------------
class CStylePropertyTooltipPosition : public CStylePropertyContextUIPosition
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyTooltipPosition() : CStylePropertyContextUIPosition( CStylePropertyTooltipPosition::symbol )
	{
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies where to position a tooltip relative to this panel. Valid options include 'left', 'top', 'right', and 'bottom'. "
			"List up to 4 positions to determine the order that positions are tried if the tooltip doesn't fully fit on screen. Default "
			"is 'right left bottom top'. If less than 4 positions are specified, the tooltip first tries the opposite of the specified "
			"position along the same axis before switching to the other axis.<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"tooltip-position: bottom;\n"
			"tooltip-position: left bottom;"
			"</pre>";
	}
};


//-----------------------------------------------------------------------------
// Purpose: Context Menu Position property
//-----------------------------------------------------------------------------
class CStylePropertyContextMenuPosition : public CStylePropertyContextUIPosition
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyContextMenuPosition() : CStylePropertyContextUIPosition( CStylePropertyContextMenuPosition::symbol )
	{
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies where to position a context menu relative to this panel. Valid options include 'left', 'top', 'right', and 'bottom'. "
			"List up to 4 positions to determine the order that positions are tried if the context menu doesn't fully fit on screen. Default "
			"is 'right left bottom top'. If less than 4 positions are specified, the context menu first tries the opposite of the specified "
			"position along the same axis before switching to the other axis.<br><br>"
			"<b>Examples:</b>"
			"<pre>"
			"context-menu-position: bottom;\n"
			"context-menu-position: left bottom;"
			"</pre>";
	}
};



//-----------------------------------------------------------------------------
// Purpose: Base class for ContextUI Component Position properties
//-----------------------------------------------------------------------------
class CStylePropertyContextUIComponentPosition : public CStyleProperty
{
public:
	CStylePropertyContextUIComponentPosition( CStyleSymbol symPropertyName ) : CStyleProperty( symPropertyName )
	{
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if ( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertyTooltipComponentPosition::MergeTo" );
			return;
		}

		CStylePropertyContextUIComponentPosition *p = ( CStylePropertyContextUIComponentPosition * )pTarget;
		if ( !p->m_HorizontalPosition.IsSet() )
		{
			p->m_HorizontalPosition = m_HorizontalPosition;
		}
		if ( !p->m_VerticalPosition.IsSet() )
		{
			p->m_VerticalPosition = m_VerticalPosition;
		}
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )  {}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		return CSSHelpers::BParseIntoTwoUILengths( &m_HorizontalPosition, &m_VerticalPosition, pchString, &pchString );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		CSSHelpers::AppendUILength( pfmtBuffer, m_HorizontalPosition );
		pfmtBuffer->Append( " " );
		CSSHelpers::AppendUILength( pfmtBuffer, m_VerticalPosition );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		return m_HorizontalPosition.IsSet() && m_VerticalPosition.IsSet();
	}

	// Comparison function
	virtual bool operator==( const CStyleProperty &other ) const
	{
		if ( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertyContextUIComponentPosition &rhs = ( const CStylePropertyContextUIComponentPosition & )other;
		return m_HorizontalPosition == rhs.m_HorizontalPosition && m_VerticalPosition == rhs.m_VerticalPosition;
	}

	// Layout pieces that can be invalidated when the property is applied to a panel
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const { return k_EStyleInvalidateLayoutNone; }

	CUILength m_HorizontalPosition;
	CUILength m_VerticalPosition;
};


//-----------------------------------------------------------------------------
// Purpose: ContextUI Body Position property base class
//-----------------------------------------------------------------------------
class CStylePropertyContextUIBodyPosition : public CStylePropertyContextUIComponentPosition
{
public:
	CStylePropertyContextUIBodyPosition( CStyleSymbol symPropertyName ) : CStylePropertyContextUIComponentPosition( symPropertyName )
	{
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		ResolveDefaultBodyPosition( m_HorizontalPosition, m_VerticalPosition );
	}

	static void ResolveDefaultBodyPosition( CUILength &horizontalPosition, CUILength &verticalPosition )
	{
		if ( !horizontalPosition.IsSet() )
		{
			horizontalPosition.SetPercent( 0.0f );
		}

		if ( !verticalPosition.IsSet() )
		{
			verticalPosition.SetPercent( 0.0f );
		}
	}
};


//-----------------------------------------------------------------------------
// Purpose: Tooltip Body Position property
//-----------------------------------------------------------------------------
class CStylePropertyTooltipBodyPosition : public CStylePropertyContextUIBodyPosition
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyTooltipBodyPosition() : CStylePropertyContextUIBodyPosition( CStylePropertyTooltipBodyPosition::symbol )
	{
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies where to position the body of a tooltip relative to this panel. The first value controls how the body is "
			"aligned horizontally when the tooltip is to the top or bottom of the panel, and the second value controls how the body is "
			"aligned vertically when the tooltip is to the left or right of the panel. 0% means left/top aligned, 50% means "
			"center/middle aligned, and 100% means right/bottom aligned. Default is '0% 0%'.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"tooltip-body-position: 50% 100%;"
			"</pre>";
	}
};


//-----------------------------------------------------------------------------
// Purpose: Context Menu Body Position property
//-----------------------------------------------------------------------------
class CStylePropertyContextMenuBodyPosition : public CStylePropertyContextUIBodyPosition
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyContextMenuBodyPosition() : CStylePropertyContextUIBodyPosition( CStylePropertyContextMenuBodyPosition::symbol )
	{
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies where to position the body of a context menu relative to this panel. The first value controls how the body is "
			"aligned horizontally when the context menu is to the top or bottom of the panel, and the second value controls how the body is "
			"aligned vertically when the context menu is to the left or right of the panel. 0% means left/top aligned, 50% means "
			"center/middle aligned, and 100% means right/bottom aligned. Default is '0% 0%'.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"context-menu-body-position: 50% 100%;"
			"</pre>";
	}
};


//-----------------------------------------------------------------------------
// Purpose: ContextUI Arrow Position property base class
//-----------------------------------------------------------------------------
class CStylePropertyContextUIArrowPosition : public CStylePropertyContextUIComponentPosition
{
public:
	CStylePropertyContextUIArrowPosition( CStyleSymbol symPropertyName ) : CStylePropertyContextUIComponentPosition( symPropertyName )
	{
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
		ResolveDefaultArrowPosition( m_HorizontalPosition, m_VerticalPosition );
	}

	static void ResolveDefaultArrowPosition( CUILength &horizontalPosition, CUILength &verticalPosition )
	{
		if ( !horizontalPosition.IsSet() )
		{
			horizontalPosition.SetPercent( 50.0f );
		}

		if ( !verticalPosition.IsSet() )
		{
			verticalPosition.SetPercent( 50.0f );
		}
	}
};


//-----------------------------------------------------------------------------
// Purpose: Tooltip Arrow Position property
//-----------------------------------------------------------------------------
class CStylePropertyTooltipArrowPosition : public CStylePropertyContextUIArrowPosition
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyTooltipArrowPosition() : CStylePropertyContextUIArrowPosition( CStylePropertyTooltipArrowPosition::symbol )
	{
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies where to point the arrow of a tooltip at on this panel. The first value controls how the arrow is "
			"positioned horizontally when the tooltip is to the top or bottom of the panel, and the second value controls how the arrow is "
			"positioned vertically when the tooltip is to the left or right of the panel. Default is '50% 50%'.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"tooltip-arrow-position: 25% 50%;"
			"</pre>";
	}
};


//-----------------------------------------------------------------------------
// Purpose: Context Menu Arrow Position property
//-----------------------------------------------------------------------------
class CStylePropertyContextMenuArrowPosition : public CStylePropertyContextUIArrowPosition
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyContextMenuArrowPosition() : CStylePropertyContextUIArrowPosition( CStylePropertyContextMenuArrowPosition::symbol )
	{
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies where to point the arrow of a context menu at on this panel. The first value controls how the arrow is "
			"positioned horizontally when the context menu is to the top or bottom of the panel, and the second value controls how the arrow is "
			"positioned vertically when the context menu is to the left or right of the panel. Default is '50% 50%'.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"context-menu-arrow-position: 25% 50%;"
			"</pre>";
	}
};


//-----------------------------------------------------------------------------
// Purpose: Sound property base class
//-----------------------------------------------------------------------------
class CStylePropertySound : public CStyleProperty
{
public:
	CStylePropertySound( CStyleSymbol symPropertyName ) : CStyleProperty( symPropertyName )
	{
	}

	virtual void MergeTo( CStyleProperty *pTarget ) const
	{
		if ( pTarget->GetPropertySymbol() != GetPropertySymbol() )
		{
			AssertMsg( false, "Mismatched types to CStylePropertySound::MergeTo" );
			return;
		}

		CStylePropertySound *p = ( CStylePropertySound * )pTarget;
		if ( p->m_strSoundName.IsEmpty() )
			p->m_strSoundName = m_strSoundName;
	}

	// Can this property support animation?
	virtual bool BCanTransition() { return false; }

	// Interpolation func for animation of this property
	virtual void Interpolate( IUIPanel *pPanel, const CStyleProperty &target, float flProgress /* 0.0->1.0 */ )  {}

	// Parses string and sets value
	virtual bool BSetFromString( CStyleSymbol symParsedName, const char *pchString )
	{
		return CSSHelpers::BParseQuotedString( m_strSoundName, pchString );
	}

	// Gets string representation of property
	virtual void ToString( CFmtStr1024 *pfmtBuffer ) const
	{
		pfmtBuffer->AppendFormat( "\"%s\"", m_strSoundName.Get() );
	}

	// When applying styles to an element, used to determine if all data for this property has been set or if more fields should be found
	// by looking at lower weight styles
	virtual bool BFullySet()
	{
		return !m_strSoundName.IsEmpty();
	}

	// called when applied to a panel. Gives the style an opportunity to change any unset values to defaults
	virtual void ResolveDefaultValues()
	{
	}

	// Comparison function
	virtual bool operator==( const CStyleProperty &other ) const
	{
		if ( GetPropertySymbol() != other.GetPropertySymbol() )
			return false;

		const CStylePropertySound &rhs = ( const CStylePropertySound & )other;
		return m_strSoundName == rhs.m_strSoundName;
	}

	// Layout pieces that can be invalidated when the property is applied to a panel	
	virtual EStyleInvalidateLayout GetInvalidateLayout( CStyleProperty *pCompareProperty ) const
	{
		return k_EStyleInvalidateLayoutNone;
	}

	CUtlString m_strSoundName;
};


//-----------------------------------------------------------------------------
// Purpose: Entrance sound property
//-----------------------------------------------------------------------------
class CStylePropertyEntranceSound : public CStylePropertySound
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyEntranceSound() : CStylePropertySound( CStylePropertyEntranceSound::symbol )
	{
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies a sound name to play when this selector is applied.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"sound: \"whoosh_in\";"
			"</pre>";
	}
};


//-----------------------------------------------------------------------------
// Purpose: Exit sound property
//-----------------------------------------------------------------------------
class CStylePropertyExitSound : public CStylePropertySound
{
public:
	static const CStyleSymbol symbol;
	CStylePropertyExitSound() : CStylePropertySound( CStylePropertyExitSound::symbol )
	{
	}

	// Return a description for this property which will be shown in the debugger
	virtual const char *GetDescription( CStyleSymbol symProperty )
	{
		return "Specifies a sound name to play when this selector is removed.<br><br>"
			"<b>Example:</b>"
			"<pre>"
			"sound-out: \"whoosh_out\";"
			"</pre>";
	}
};


} // namespace panorama

#endif // STYLEPROPERTIES_H