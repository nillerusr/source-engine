//========= Copyright Valve Corporation, All rights reserved. ================================== //
//
// Purpose: 
//
//============================================================================================== //

#include "pch_materialsystem.h"
#include "ctexturecompositor.h"

#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/combineoperations.h"
#include "texturemanager.h"

#define MATSYS_INTERNAL // Naughty!
#include "cmaterialsystem.h"

#include "tier0/memdbgon.h"

#ifndef _WINDOWS
#define sscanf_s sscanf
#endif

// If this is 0 or unset, we won't use the caching functionality.
#define WITH_TEX_COMPOSITE_CACHE 1

#ifdef STAGING_ONLY // Always should remain staging only.
	ConVar r_texcomp_dump( "r_texcomp_dump", "0", FCVAR_NONE, "Whether we should dump the textures to disk or not. 1: Save all; 2: Save Final; 3: Save Final with name suitable for scripting; 4: Save Final and skip saving workshop icons." );
#endif

const int cMaxSelectors = 16;

// Ugh, this is annoying and matches TF's enums. That's lame. We should workaround this.
enum { Neutral = 0, Red = 2, Blue = 3 };

static int s_nDumpCount = 0;
static CInterlockedInt s_nCompositeCount = 0;

void ComputeTextureMatrixFromRectangle( VMatrix* pOutMat, const Vector2D& bl, const Vector2D& tl, const Vector2D& tr );
bool HasCycle( CTextureCompositorTemplate* pStartTempl );
CTextureCompositorTemplate* Advance( CTextureCompositorTemplate* pTmpl, int nSteps );
void PrintMinimumCycle( CTextureCompositorTemplate* pStartTempl );

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
struct CTCStageResult_t
{
	ITexture* m_pTexture;
	ITexture* m_pRenderTarget;

	float m_fAdjustBlackPoint;
	float m_fAdjustWhitePoint;
	float m_fAdjustGamma;

	matrix3x4_t m_mUvAdjust;

	inline CTCStageResult_t() 
	: m_pTexture(NULL)
	, m_pRenderTarget(NULL)
	, m_fAdjustBlackPoint(0.0f)
	, m_fAdjustWhitePoint(1.0f)
	, m_fAdjustGamma(1.0f)
	{
		SetIdentityMatrix( m_mUvAdjust );
	}

	inline void Cleanup( CTextureCompositor* _comp )
	{
		if ( m_pRenderTarget )
			_comp->ReleaseCompositorRenderTarget( m_pRenderTarget );

		m_pTexture = NULL;
		m_pRenderTarget = NULL;
	}
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
class CTCStage : public IAsyncTextureOperationReceiver
{
public:
	CTCStage();

protected:
	// Called by Release()
	virtual ~CTCStage();

public:
	// IAsyncTextureOperationReceiver
	virtual int AddRef() OVERRIDE;
	virtual int Release() OVERRIDE;
	virtual int GetRefCount() const OVERRIDE { return m_nReferenceCount; }
	virtual void OnAsyncCreateComplete( ITexture* pTex, void* pExtraArgs ) OVERRIDE { } 
	virtual void OnAsyncFindComplete( ITexture* pTex, void* pExtraArgs ) OVERRIDE { }
	virtual void OnAsyncMapComplete( ITexture* pTex, void* pExtraArgs, void* pMemory, int pPitch ) OVERRIDE { }
	virtual void OnAsyncReadbackBegin( ITexture* pDst, ITexture* pSrc, void* pExtraArgs ) OVERRIDE { }


	// Our stuff.
	void Resolve( bool bFirstTime, CTextureCompositor* _comp );
	inline ECompositeResolveStatus GetResolveStatus() const { return m_ResolveStatus; }
	inline const CTCStageResult_t& GetResult() const { Assert( GetResolveStatus() == ECRS_Complete ); return m_Result; }

	bool HasTeamSpecifics() const;
	void ComputeRandomValues( int* pCurIndex, CUniformRandomStream* pRNGs, int nRNGCount );

	inline void SetFirstChild( CTCStage* _stage ) { m_pFirstChild = _stage; }
	inline void SetNextSibling( CTCStage* _stage ) { m_pNextSibling = _stage; }

	inline CTCStage* GetFirstChild() { return m_pFirstChild; }
	inline CTCStage* GetNextSibling() { return m_pNextSibling; }

	inline const CTCStage* GetFirstChild() const { return m_pFirstChild; }
	inline const CTCStage* GetNextSibling() const { return m_pNextSibling; }

	void AppendChildren( const CUtlVector< CTCStage* >& _children )
	{
		// Do these in reverse order, they will wind up in the right order 
		FOR_EACH_VEC_BACK( _children, i )
		{
			CTCStage* childStage = _children[i];
			childStage->SetNextSibling( GetFirstChild() );
			SetFirstChild( childStage );
		}
	}

	void CleanupChildResults( CTextureCompositor* _comp );

	// Render a quad with _mat using _inputs to _destRT
	void Render( ITexture* _destRT, IMaterial* _mat, const CUtlVector<CTCStageResult_t>& _inputs, CTextureCompositor* _comp, bool bClear ); 

	void Cleanup( CTextureCompositor* _comp );

	// Does this stage target a render target or a texture? 
	virtual bool DoesTargetRenderTarget() const = 0;

	inline void SetResult( const CTCStageResult_t& _result )
	{
		Assert( m_ResolveStatus != ECRS_Complete );
		m_Result = _result;
		m_ResolveStatus = ECRS_Complete;
	}

protected:

	inline void SetResolveStatus( ECompositeResolveStatus _status )
	{
		m_ResolveStatus = _status;
	}

	// This function is called only once during the first ResolveTraversal, and is
	// for the compositor to request its textures. Textures should not be requested
	// before this or they can be held waaaay too long.
	virtual void RequestTextures() = 0;

	// This function will be called during Resolve traversal. At the point when this is called,
	// all of this node's children will have had their resolve completed. Our siblings will
	// not have resolved yet.
	virtual void ResolveThis( CTextureCompositor* _comp ) = 0;

	// This function is called during HasTeamSpecifics traversal. 
	virtual bool HasTeamSpecificsThis() const = 0;

	virtual bool ComputeRandomValuesThis( CUniformRandomStream* pRNG ) = 0;

private:
	CInterlockedInt m_nReferenceCount;

	CTCStage* m_pFirstChild;
	CTCStage* m_pNextSibling;

	CTCStageResult_t m_Result;
	ECompositeResolveStatus m_ResolveStatus;
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
typedef void ( *ParseSingleKV )( KeyValues* _kv, void* _dest );
struct ParseTableEntry
{
	const char* keyName;
	ParseSingleKV parseFunc;
	size_t structOffset;
};

// ------------------------------------------------------------------------------------------------
struct Range
{
	float low;
	float high;

	Range( )
	: low( 0 )
	, high( 0 )
	{ } 

	Range( float _l, float _h )
	: low( _l )
	, high( _h )
	{ } 
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void ParseBoolFromKV( KeyValues* _kv, void* _pDest )
{
	bool* realDest = ( bool* ) _pDest;
	( *realDest ) = _kv->GetBool();
}

// ------------------------------------------------------------------------------------------------
template<int N>
void ParseIntVectorFromKV( KeyValues* _kv, void* _pDest )
{
	CCopyableUtlVector<int>* realDest = ( CCopyableUtlVector<int>* ) _pDest;
	const int parsedValue = _kv->GetInt();
	if ( realDest->Size() < N )
	{
		realDest->AddToTail( parsedValue );
	}
	else
	{
		DevWarning( "Too many numbers (>%d), ignoring the value '%d'.\n", N, parsedValue );
	}
}

// ------------------------------------------------------------------------------------------------
template< class T >
CUtlString AsStringT( const T& _val )
{
#ifdef _WIN32
	// Not sure why linux is unhappy here. Error messages unhelpful. Thanks, GCC.
	static_assert( false, "Must add specialization for typename T" );
#endif
	return CUtlString( "" );
}

// ------------------------------------------------------------------------------------------------
template<>
CUtlString AsStringT< int >( const int& _val )
{
	char buffer[ 12 ];
	V_sprintf_safe( buffer, "%d", _val );
	return CUtlString( buffer );
}

// ------------------------------------------------------------------------------------------------
template< class T >
void ParseTFromKV( KeyValues* _kv, void* _pDest )
{
#ifdef _WIN32
	// Not sure why linux is unhappy here. Error messages unhelpful. Thanks, GCC.
	static_assert( false, "Must add specialization for typename T" );
#endif
}

// ------------------------------------------------------------------------------------------------
template<>
void ParseTFromKV< int >( KeyValues* _kv, void* _pDest )
{
	int* realDest = ( int* ) _pDest;
	( *realDest ) = _kv->GetInt();
}

// ------------------------------------------------------------------------------------------------
template<>
void ParseTFromKV< Vector2D >( KeyValues* _kv, void* _pDest )
{
	Vector2D* realDest = ( Vector2D* ) _pDest;
	Vector2D tmpDest;
	int count = sscanf_s( _kv->GetString(), "%f %f", &tmpDest.x, &tmpDest.y );
	if  ( count != 2 )
	{
		Error( "Expected exactly two values, %d were provided.\n", count );
		return;
	}

	*realDest = tmpDest;
}

// ------------------------------------------------------------------------------------------------
template< class T, int N = INT_MAX >
void ParseVectorFromKV( KeyValues* _kv, void* _pDest )
{
	CCopyableUtlVector< T >* realDest = ( CCopyableUtlVector< T >* ) _pDest;
	
	T parsedValue = T();
	ParseTFromKV<T>( _kv, &parsedValue );

	if ( realDest->Size() < N )
	{
		realDest->AddToTail( parsedValue );
	}
	else
	{
		DevWarning( "Too many entries (>%d), ignoring the value '%s'.\n", N, AsStringT( parsedValue ).Get() );
	}
}

// ------------------------------------------------------------------------------------------------
void ParseRangeFromKV( KeyValues* _kv, void* _pDest )
{
	Range* realDest = ( Range* ) _pDest;
	Range tmpDest;

	int count = sscanf_s( _kv->GetString(), "%f %f", &tmpDest.low, &tmpDest.high );
	switch (count)
	{
	case 1:
		// If we parse one, use the same value for low and high.
		( *realDest ).low = tmpDest.low;
		( *realDest ).high = tmpDest.low;
		break;
	case 2:
		// If we parse two, they're both correct.
		( *realDest ).low = tmpDest.low;
		( *realDest ).high = tmpDest.high;
		break;

		// error cases
	case EOF:
	case 0:
	default:
		Error( "Incorrect number of numbers while parsing, using defaults. This error message should be improved\n" );
	};
}

// ------------------------------------------------------------------------------------------------
void ParseInverseRangeFromKV( KeyValues* _kv, void* _pDest )
{
	const float kSubstValue = 0.00001;
	ParseRangeFromKV( _kv, _pDest );
	Range* realDest = ( Range* ) _pDest;

	if ( realDest->low != 0.0f )
	{
		( *realDest ).low = 1.0f / realDest->low;
	}
	else
	{
		Error( "Specified 0.0 for low value, that is illegal in this field. Substituting %.5f\n", kSubstValue );
		( *realDest ).low = kSubstValue;
	}

	if ( realDest->high != 0.0f )
	{
		( *realDest ).high = 1.0f / realDest->high;
	}
	else
	{
		Error( "Specified 0.0 for high value, that is illegal in this field. Substituting %.5f\n", kSubstValue );
		( *realDest ).high = kSubstValue;
	}
}

// ------------------------------------------------------------------------------------------------
template < int Div >
void ParseRangeThenDivideBy( KeyValues *_kv, void* _pDest )
{
	static_assert( Div != 0, "Cannot specify a divisor of 0." );
	float fDiv = (float) Div;

	ParseRangeFromKV( _kv, _pDest );
	Range* realDest = ( Range* ) _pDest;

	( *realDest ).low  = ( *realDest ).low  / fDiv;
	( *realDest ).high = ( *realDest ).high / fDiv;
}

// ------------------------------------------------------------------------------------------------
void ParseStringFromKV( KeyValues* _kv, void* _pDest )
{
	CUtlString* realDest = ( CUtlString* ) _pDest;
	(*realDest) = _kv->GetString();
}

// ------------------------------------------------------------------------------------------------
struct TextureStageParameters
{
	CUtlString m_pTexFilename;
	CUtlString m_pTexRedFilename;
	CUtlString m_pTexBlueFilename;
	Range m_AdjustBlack;
	Range m_AdjustOffset;
	Range m_AdjustGamma;

	Range m_Rotation;
	Range m_TranslateU;
	Range m_TranslateV;
	Range m_ScaleUV;
	bool m_AllowFlipU;
	bool m_AllowFlipV;
	bool m_Evaluate;

	TextureStageParameters()
	: m_AdjustBlack( 0, 0 )
	, m_AdjustOffset( 1, 1 )
	, m_AdjustGamma( 1, 1 )
	, m_Rotation( 0 , 0 )
	, m_TranslateU( 0, 0 )
	, m_TranslateV( 0, 0 )
	, m_ScaleUV( 1, 1 )
	, m_AllowFlipU( false )
	, m_AllowFlipV( false )
	, m_Evaluate( true )
	{ }
};

// ------------------------------------------------------------------------------------------------
const ParseTableEntry cTextureStageParametersParseTable[] = 
{
	{ "texture",			ParseStringFromKV,				offsetof( TextureStageParameters, m_pTexFilename ) },
	{ "texture_red",		ParseStringFromKV,				offsetof( TextureStageParameters, m_pTexRedFilename ) },
	{ "texture_blue",		ParseStringFromKV,				offsetof( TextureStageParameters, m_pTexBlueFilename ) },
	{ "adjust_black",		ParseRangeThenDivideBy<255>,	offsetof( TextureStageParameters, m_AdjustBlack ) },
	{ "adjust_offset",		ParseRangeThenDivideBy<255>,	offsetof( TextureStageParameters, m_AdjustOffset ) },
	{ "adjust_gamma",		ParseInverseRangeFromKV,		offsetof( TextureStageParameters, m_AdjustGamma ) },
	{ "rotation",			ParseRangeFromKV,				offsetof( TextureStageParameters, m_Rotation ) },
	{ "translate_u",		ParseRangeFromKV,				offsetof( TextureStageParameters, m_TranslateU ) },
	{ "translate_v",		ParseRangeFromKV,				offsetof( TextureStageParameters, m_TranslateV ) },
	{ "scale_uv",			ParseRangeFromKV,				offsetof( TextureStageParameters, m_ScaleUV ) },
	{ "flip_u",				ParseBoolFromKV,				offsetof( TextureStageParameters, m_AllowFlipU ) },
	{ "flip_v",				ParseBoolFromKV,				offsetof( TextureStageParameters, m_AllowFlipV ) },
	{ "evaluate?", 			ParseBoolFromKV,				offsetof( TextureStageParameters, m_Evaluate ) },

	{ 0, 0 }
};

 // ------------------------------------------------------------------------------------------------
 // ------------------------------------------------------------------------------------------------
 // ------------------------------------------------------------------------------------------------
class CTCTextureStage : public CTCStage
{
public:
	CTCTextureStage( const TextureStageParameters& _tsp, uint32 nTexCompositeCreateFlags ) 
	: m_Parameters( _tsp ) 
	, m_pTex( NULL )
	, m_pTexRed( NULL )
	, m_pTexBlue( NULL )
	{ 
	}

	virtual ~CTCTextureStage()
	{
		SafeRelease( &m_pTex );	
		SafeRelease( &m_pTexBlue );
		SafeRelease( &m_pTexRed );
	}

	virtual void OnAsyncFindComplete( ITexture* pTex, void* pExtraArgs ) 
	{ 
		switch ( ( int ) pExtraArgs )
		{
		case Neutral:
			SafeAssign( &m_pTex, pTex ); 
			break;
		case Red:
			SafeAssign( &m_pTexRed, pTex );
			break;
		case Blue:
			SafeAssign( &m_pTexBlue, pTex );
			break;
		default:
			Assert( !"Unexpected value passed to OnAsyncFindComplete" );
			break;
		};
	}

	virtual bool DoesTargetRenderTarget() const { return false; }

protected:
	bool AreTexturesLoaded() const
	{
		if ( !m_Parameters.m_pTexFilename.IsEmpty() && !m_pTex ) 
			return false;

		if ( !m_Parameters.m_pTexRedFilename.IsEmpty() && !m_pTexRed )
			return false;

		if ( !m_Parameters.m_pTexBlueFilename.IsEmpty() && !m_pTexBlue )
			return false;

		return true;
	}

	ITexture* GetTeamSpecificTexture( int nTeam )
	{
		if ( nTeam == Red && m_pTexRed )
			return m_pTexRed;

		if ( nTeam == Blue && m_pTexBlue )
			return m_pTexBlue;

		return m_pTex;
	}

	virtual void RequestTextures()
	{
		if ( !m_Parameters.m_pTexFilename.IsEmpty() )
			materials->AsyncFindTexture( m_Parameters.m_pTexFilename.Get(), TEXTURE_GROUP_RUNTIME_COMPOSITE, this, ( void* ) Neutral, false, TEXTUREFLAGS_IMMEDIATE_CLEANUP );
		if ( !m_Parameters.m_pTexRedFilename.IsEmpty() )
			materials->AsyncFindTexture( m_Parameters.m_pTexRedFilename.Get(), TEXTURE_GROUP_RUNTIME_COMPOSITE, this, ( void* ) Red, false, TEXTUREFLAGS_IMMEDIATE_CLEANUP );
		if ( !m_Parameters.m_pTexBlueFilename.IsEmpty() )
			materials->AsyncFindTexture( m_Parameters.m_pTexBlueFilename.Get(), TEXTURE_GROUP_RUNTIME_COMPOSITE, this, ( void* ) Blue, false, TEXTUREFLAGS_IMMEDIATE_CLEANUP );	
	}

	virtual void ResolveThis( CTextureCompositor* _comp )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		// We shouldn't have any children, we're going to ignore them anyways.
		Assert( GetFirstChild() == NULL );

		ECompositeResolveStatus resolveStatus = GetResolveStatus();
		// If we're done, we're done.
		if ( resolveStatus == ECRS_Complete || resolveStatus == ECRS_Error )
			return;

		if ( resolveStatus == ECRS_Scheduled )
			SetResolveStatus( ECRS_PendingTextureLoads );

		// Someone is misusing this node if this assert fires.
		Assert( GetResolveStatus() == ECRS_PendingTextureLoads );

		// When the texture has finished loading, this will be set to the texture we should use.
		if ( !AreTexturesLoaded() )
			return;

		if ( !m_pTex && !m_pTexRed && !m_pTexBlue )
		{
			_comp->Error( false, "Invalid texture_lookup node, must specify at least texture (or texture_red and texture_blue) or all of them.\n" );
			return;
		}

		if ( m_pTex && m_pTex->IsError() )
		{
			_comp->Error( false, "Failed to load texture '%s', this is non-recoverable.\n", m_Parameters.m_pTexFilename.Get() );
			return;
		}

		if ( m_pTexRed && m_pTexRed->IsError() )
		{
			_comp->Error( false, "Failed to load texture_red '%s', this is non-recoverable.\n", m_Parameters.m_pTexRedFilename.Get() );
			return;
		}

		if ( m_pTexBlue && m_pTexBlue->IsError() )
		{
			_comp->Error( false, "Failed to load texture_blue '%s', this is non-recoverable.\n", m_Parameters.m_pTexBlueFilename.Get() );
			return;
		}

		CTCStageResult_t res;
		res.m_pTexture = GetTeamSpecificTexture( _comp->GetTeamNumber() );
		res.m_fAdjustBlackPoint = m_fAdjustBlack;
		res.m_fAdjustWhitePoint = m_fAdjustWhite;
		res.m_fAdjustGamma      = m_fAdjustGamma;
		// Store the matrix into the uv adjustment matrix
		m_mTextureAdjust.Set3x4( res.m_mUvAdjust );

		SetResult( res );

		CleanupChildResults( _comp );
		tmMessage( TELEMETRY_LEVEL0, TMMF_ICON_NOTE, "Completed: %s", __FUNCTION__ );
	}

	virtual bool HasTeamSpecificsThis() const OVERRIDE
	{
		return !m_Parameters.m_pTexBlueFilename.IsEmpty();
	}

	virtual bool ComputeRandomValuesThis( CUniformRandomStream* pRNG ) OVERRIDE
	{
		// If you change the order of these random numbers being generated, or add new ones, you will
		// change the look of existing players' weapons! Don't do that.
		const bool shouldFlipU = m_Parameters.m_AllowFlipU ? pRNG->RandomInt( 0, 1 ) != 0 : false;
		const bool shouldFlipV = m_Parameters.m_AllowFlipV ? pRNG->RandomInt( 0, 1 ) != 0 : false;
		const float translateU = pRNG->RandomFloat( m_Parameters.m_TranslateU.low, m_Parameters.m_TranslateU.high );
		const float translateV = pRNG->RandomFloat( m_Parameters.m_TranslateV.low, m_Parameters.m_TranslateV.high );
		const float rotation = pRNG->RandomFloat( m_Parameters.m_Rotation.low, m_Parameters.m_Rotation.high );
		const float scaleUV = pRNG->RandomFloat( m_Parameters.m_ScaleUV.low, m_Parameters.m_ScaleUV.high );

		const float adjustBlack = pRNG->RandomFloat( m_Parameters.m_AdjustBlack.low, m_Parameters.m_AdjustBlack.high );
		const float adjustOffset = pRNG->RandomFloat( m_Parameters.m_AdjustOffset.low, m_Parameters.m_AdjustOffset.high );
		const float adjustGamma = pRNG->RandomFloat( m_Parameters.m_AdjustGamma.low, m_Parameters.m_AdjustGamma.high );
		const float adjustWhite = adjustBlack + adjustOffset;

		m_fAdjustBlack = adjustBlack;
		m_fAdjustWhite = adjustWhite;
		m_fAdjustGamma = adjustGamma;

		const float finalScaleU = scaleUV * ( shouldFlipU ? -1.0f : 1.0f );
		const float finalScaleV = scaleUV * ( shouldFlipV ? -1.0f : 1.0f );

		MatrixBuildRotateZ( m_mTextureAdjust, rotation );
		m_mTextureAdjust = m_mTextureAdjust.Scale( Vector( finalScaleU, finalScaleV, 1.0f ) );
		MatrixTranslate( m_mTextureAdjust, Vector( translateU, translateV, 0 ) );
		// Copy W into Z because we're doing a texture matrix.
		m_mTextureAdjust[ 0 ][ 2 ] = m_mTextureAdjust[ 0 ][ 3 ];
		m_mTextureAdjust[ 1 ][ 2 ] = m_mTextureAdjust[ 1 ][ 3 ];
		m_mTextureAdjust[ 2 ][ 2 ] = 1.0f;

		return true;
	}


private:
	TextureStageParameters m_Parameters;
	ITexture* m_pTex;
	ITexture* m_pTexRed;
	ITexture* m_pTexBlue;

	// Random values here
	float m_fAdjustBlack;
	float m_fAdjustWhite;
	float m_fAdjustGamma;
	VMatrix m_mTextureAdjust;
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------

// Keep in sync with CombineOperation
const char* cCombineMaterialName[] =
{
	"dev/CompositorMultiply",
	"dev/CompositorAdd",
	"dev/CompositorLerp",

	"dev/CompositorSelect",

	"\0 ECO_Legacy_Lerp_FirstPass", // Procedural; starting with \0 will skip precaching
	"\0 ECO_Legacy_Lerp_SecondPass", // Procedural; starting with \0 will skip precaching

	"dev/CompositorBlend",

	"\0 ECO_LastPrecacheMaterial", // 

	"CompositorError",

	NULL
};

static_assert( ARRAYSIZE( cCombineMaterialName ) == ECO_COUNT + 1, "cCombineMaterialName and ECombineOperation are out of sync." );

// ------------------------------------------------------------------------------------------------
struct CombineStageParameters
{
	ECombineOperation m_CombineOp;
	Range m_AdjustBlack;
	Range m_AdjustOffset;
	Range m_AdjustGamma;

	Range m_Rotation;
	Range m_TranslateU;
	Range m_TranslateV;
	Range m_ScaleUV;

	bool m_AllowFlipU;
	bool m_AllowFlipV;
	bool m_Evaluate;

	CombineStageParameters()
	: m_CombineOp( ECO_Error )
	, m_AdjustBlack( 0, 0 )
	, m_AdjustOffset( 1, 1 )
	, m_AdjustGamma( 1, 1 )
	, m_Rotation( 0 , 0 )
	, m_TranslateU( 0, 0 )
	, m_TranslateV( 0, 0 )
	, m_ScaleUV( 1, 1 )
	, m_AllowFlipU( false )
	, m_AllowFlipV( false )
	, m_Evaluate( true )
	{ }

};

// ------------------------------------------------------------------------------------------------
void ParseOperationFromKV( KeyValues* _kv, void* _pDest )
{
	ECombineOperation* realDest = ( ECombineOperation* ) _pDest;
	const char* opStr = _kv->GetString();

	if ( V_stricmp( "multiply", opStr ) == 0 )
		(*realDest) = ECO_Multiply;
	else if ( V_stricmp( "add", opStr ) == 0 )
		(*realDest) = ECO_Add;
	else if ( V_stricmp( "lerp", opStr) == 0 )
		(*realDest) = ECO_Lerp;
	else
		(*realDest) = ECO_Error;
}

// ------------------------------------------------------------------------------------------------
const ParseTableEntry cCombineStageParametersParseTable[] = 
{
	{ "adjust_black",	ParseRangeThenDivideBy<255>,	offsetof( CombineStageParameters, m_AdjustBlack ) },
	{ "adjust_offset",	ParseRangeThenDivideBy<255>,	offsetof( CombineStageParameters, m_AdjustOffset ) },
	{ "adjust_gamma",	ParseInverseRangeFromKV,		offsetof( CombineStageParameters, m_AdjustGamma ) },
	{ "rotation",		ParseRangeFromKV,				offsetof( CombineStageParameters, m_Rotation ) },
	{ "translate_u",	ParseRangeFromKV,				offsetof( CombineStageParameters, m_TranslateU ) },
	{ "translate_v",	ParseRangeFromKV,				offsetof( CombineStageParameters, m_TranslateV ) },
	{ "scale_uv",		ParseRangeFromKV,				offsetof( CombineStageParameters, m_ScaleUV ) },
	{ "flip_u",			ParseBoolFromKV,				offsetof( CombineStageParameters, m_AllowFlipU ) },
	{ "flip_v",			ParseBoolFromKV,				offsetof( CombineStageParameters, m_AllowFlipV ) },
	{ "evaluate?", 		ParseBoolFromKV,				offsetof( CombineStageParameters, m_Evaluate ) },

	{ 0, 0 }
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
class CTCCombineStage : public CTCStage 
{
public:
	CTCCombineStage( const CombineStageParameters& _csp, uint32 nTexCompositeCreateFlags )
	: m_Parameters( _csp ) 
	, m_pMaterial( NULL )
	{ 
		Assert( m_Parameters.m_CombineOp >= 0 && m_Parameters.m_CombineOp < ECO_COUNT );

		SafeAssign( &m_pMaterial, materials->FindMaterial( cCombineMaterialName[ m_Parameters.m_CombineOp ], TEXTURE_GROUP_RUNTIME_COMPOSITE ) );
	} 

	virtual ~CTCCombineStage() 
	{ 
		SafeRelease( &m_pMaterial );
	} 

	virtual bool DoesTargetRenderTarget() const { return true; }


protected:
	virtual void RequestTextures() { /* No textures here */ }

	virtual void ResolveThis( CTextureCompositor* _comp )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		ECompositeResolveStatus resolveStatus = GetResolveStatus();
		// If we're done, we're done.
		if ( resolveStatus == ECRS_Complete || resolveStatus == ECRS_Error )
			return;

		if ( resolveStatus == ECRS_Scheduled )
			SetResolveStatus( ECRS_PendingTextureLoads );

		// Someone is misusing this node if this assert fires.
		Assert( GetResolveStatus() == ECRS_PendingTextureLoads );

		for ( CTCStage* child = GetFirstChild(); child; child = child->GetNextSibling() )
		{
			// If any child isn't ready to go, we're not ready to go.
			if ( child->GetResolveStatus() != ECRS_Complete )
				return;
		}
		
		ITexture* pRenderTarget = _comp->AllocateCompositorRenderTarget();

		CUtlVector<CTCStageResult_t> results;
		uint childCount = 0;
		for ( CTCStage* child = GetFirstChild(); child; child = child->GetNextSibling() )
		{
			results.AddToTail( child->GetResult() );
			++childCount;
		}

		// TODO: If there are more than 8 children, need to split them into multiple groups here. Skip it for now.

		Render( pRenderTarget, m_pMaterial, results, _comp, true );

		CTCStageResult_t res;
		res.m_pRenderTarget = pRenderTarget;
		res.m_fAdjustBlackPoint = m_fAdjustBlack;
		res.m_fAdjustWhitePoint = m_fAdjustWhite;
		res.m_fAdjustGamma      = m_fAdjustGamma;

		SetResult( res );

		// As soon as we have scheduled the read of a child render target, we can release that 
		// texture back to the pool for use by another stage. Everything is pipelined, so this just
		// works.
		CleanupChildResults( _comp );
		tmMessage( TELEMETRY_LEVEL0, TMMF_ICON_NOTE, "Completed: %s", __FUNCTION__ );
	}

	virtual bool HasTeamSpecificsThis() const OVERRIDE{ return false; }

	virtual bool ComputeRandomValuesThis( CUniformRandomStream* pRNG ) OVERRIDE
	{
		const float adjustBlack = pRNG->RandomFloat( m_Parameters.m_AdjustBlack.low, m_Parameters.m_AdjustBlack.high );
		const float adjustOffset = pRNG->RandomFloat( m_Parameters.m_AdjustOffset.low, m_Parameters.m_AdjustOffset.high );
		const float adjustGamma = pRNG->RandomFloat( m_Parameters.m_AdjustGamma.low, m_Parameters.m_AdjustGamma.high );
		const float adjustWhite = adjustBlack + adjustOffset;

		m_fAdjustBlack = adjustBlack;
		m_fAdjustWhite = adjustWhite;
		m_fAdjustGamma = adjustGamma;

		return true;
	}

private:
	CombineStageParameters m_Parameters;
	IMaterial* m_pMaterial;

	float m_fAdjustBlack;
	float m_fAdjustWhite;
	float m_fAdjustGamma;
};

// ------------------------------------------------------------------------------------------------
struct SelectStageParameters
{
	CUtlString m_pTexFilename;
	CCopyableUtlVector<int> m_Select;
	bool m_Evaluate;

	SelectStageParameters()
	: m_Evaluate( true )
	{ 
	}
};

// ------------------------------------------------------------------------------------------------
const ParseTableEntry cSelectStageParametersParseTable[] = 
{
	{ "groups",		ParseStringFromKV,							offsetof( SelectStageParameters, m_pTexFilename ) },
	{ "select",		ParseVectorFromKV< int, cMaxSelectors >,	offsetof( SelectStageParameters, m_Select ) },
	{ "evaluate?", 	ParseBoolFromKV,							offsetof( SelectStageParameters, m_Evaluate ) },

	{ 0, 0 }
};

 // ------------------------------------------------------------------------------------------------
 // ------------------------------------------------------------------------------------------------
 // ------------------------------------------------------------------------------------------------
class CTCSelectStage : public CTCStage
{
public:
	CTCSelectStage( const SelectStageParameters& _ssp, uint32 nTexCompositeCreateFlags ) 
	: m_Parameters( _ssp ) 
	, m_pMaterial( NULL ) 
	, m_pTex( NULL )
	{ 
		SafeAssign( &m_pMaterial, materials->FindMaterial( cCombineMaterialName[ ECO_Select ], TEXTURE_GROUP_RUNTIME_COMPOSITE ) );
	}
	virtual ~CTCSelectStage() 
	{ 
		SafeRelease( &m_pMaterial );
		SafeRelease( &m_pTex );
	}

	virtual void OnAsyncFindComplete( ITexture* pTex, void* pExtraArgs ) { SafeAssign( &m_pTex, pTex ); }

	virtual bool DoesTargetRenderTarget() const { return true; }


protected:
	virtual void RequestTextures() 
	{
		materials->AsyncFindTexture( m_Parameters.m_pTexFilename.Get(), TEXTURE_GROUP_RUNTIME_COMPOSITE, this, NULL, false, TEXTUREFLAGS_IMMEDIATE_CLEANUP );
	}

	virtual void ResolveThis( CTextureCompositor* _comp )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		// We shouldn't have any children, we're going to ignore them anyways.
		Assert( GetFirstChild() == NULL );

		ECompositeResolveStatus resolveStatus = GetResolveStatus();
		// If we're done, we're done.
		if ( resolveStatus == ECRS_Complete || resolveStatus == ECRS_Error )
			return;

		if ( resolveStatus == ECRS_Scheduled )
			SetResolveStatus( ECRS_PendingTextureLoads );

		// Someone is misusing this node if this assert fires.
		Assert( GetResolveStatus() == ECRS_PendingTextureLoads );

		// When the texture has finished loading, this will be set to the texture we should use.
		if ( m_pTex == NULL )
			return;

		if ( m_pTex->IsError() )
		{
			_comp->Error( false, "Failed to load texture %s, this is non-recoverable.\n", m_Parameters.m_pTexFilename.Get() );
			return;
		}

		ITexture* pRenderTarget = _comp->AllocateCompositorRenderTarget();

		char buffer[128];
		for ( int i = 0; i < cMaxSelectors; ++i )
		{
			bool bFound = false;

			V_snprintf( buffer, ARRAYSIZE( buffer ), "$selector%d", i );
			IMaterialVar* pVar = m_pMaterial->FindVar( buffer, &bFound );
			Assert(bFound);
			if ( i < m_Parameters.m_Select.Size() )
				pVar->SetIntValue( m_Parameters.m_Select[i] );
			else
				pVar->SetIntValue( 0 );
		}

		CTCStageResult_t inRes;
		inRes.m_pTexture = m_pTex;
		CUtlVector<CTCStageResult_t> fakeResults;
		fakeResults.AddToTail( inRes );
		Render( pRenderTarget, m_pMaterial, fakeResults, _comp, true );

		CTCStageResult_t outRes;
		outRes.m_pRenderTarget = pRenderTarget;
		SetResult( outRes );

		CleanupChildResults( _comp );
		tmMessage( TELEMETRY_LEVEL0, TMMF_ICON_NOTE, "Completed: %s", __FUNCTION__ );
	}

	virtual bool HasTeamSpecificsThis() const OVERRIDE { return false; }

	virtual bool ComputeRandomValuesThis( CUniformRandomStream* pRNG ) OVERRIDE
	{
		// No RNG here.
		return false;
	}

private:
	SelectStageParameters m_Parameters;
	IMaterial* m_pMaterial;
	ITexture* m_pTex;
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
struct Sticker_t
{
	float m_fWeight;				// Random likelihood this one is to be selected
	CUtlString m_baseFilename;	// Name of the base file for the sticker (the albedo). 
	CUtlString m_specFilename;	// Name of the specular file for the sticker, or if blank we will assume it is baseFilename + _spec + baseExtension
	
	Sticker_t()
	: m_fWeight( 1.0 )
	{ }
};

// ------------------------------------------------------------------------------------------------
template<>
void ParseTFromKV< Sticker_t >( KeyValues* _kv, void* _pDest )
{
	Sticker_t* realDest = ( Sticker_t* ) _pDest;
	Sticker_t tmpDest;

	tmpDest.m_fWeight = _kv->GetFloat( "weight", 1.0 );
	tmpDest.m_baseFilename = _kv->GetString( "base" );
	KeyValues* pSpec = _kv->FindKey( "spec" );
	if ( pSpec ) 
		tmpDest.m_specFilename = pSpec->GetString();	
	else
	{
		CUtlString specPath = tmpDest.m_baseFilename.StripExtension() 
							+ "_s" 
							+ tmpDest.m_baseFilename.GetExtension();

		tmpDest.m_specFilename = specPath;
	}

	*realDest = tmpDest;
}

// ------------------------------------------------------------------------------------------------
template <>
CUtlString AsStringT< Sticker_t >( const Sticker_t& _val )
{
	char buffer[ 80 ];
	V_sprintf_safe( buffer, "[ weight %.2f; base \"%s\"; spec \"%s\" ]", _val.m_fWeight, _val.m_baseFilename.Get(), _val.m_specFilename.Get() );
	return CUtlString( buffer );
}

// ------------------------------------------------------------------------------------------------
template< class T >
struct Settable_t
{
	T m_val;
	bool m_bSet;

	Settable_t()
	: m_val( T() )
	, m_bSet( false )
	{ }
};

// ------------------------------------------------------------------------------------------------
template < class T >
void ParseSettable( KeyValues *_kv, void* _pDest )
{
	Settable_t<T> *pSettable = ( Settable_t<T>* )_pDest;

	ParseTFromKV<T>( _kv, &pSettable->m_val );
	( *pSettable ).m_bSet = true;
}

// ------------------------------------------------------------------------------------------------
struct ApplyStickerStageParameters
{
	CCopyableUtlVector< Sticker_t > m_possibleStickers; 

	Settable_t< Vector2D > m_vDestBL;
	Settable_t< Vector2D > m_vDestTL;
	Settable_t< Vector2D > m_vDestTR;

	Range m_AdjustBlack;
	Range m_AdjustOffset;
	Range m_AdjustGamma;
	bool m_Evaluate;

	ApplyStickerStageParameters()
	: m_AdjustBlack( 0, 0 )
	, m_AdjustOffset( 1, 1 )
	, m_AdjustGamma( 1, 1 )
	, m_Evaluate( true )
	{ }
};

// ------------------------------------------------------------------------------------------------
const ParseTableEntry cApplyStickerStageParametersParseTable[] = 
{
	{ "sticker",			ParseVectorFromKV< Sticker_t >, offsetof( ApplyStickerStageParameters, m_possibleStickers ) },
	{ "dest_bl",			ParseSettable< Vector2D >,		offsetof( ApplyStickerStageParameters, m_vDestBL ) },
	{ "dest_tl",			ParseSettable< Vector2D >,		offsetof( ApplyStickerStageParameters, m_vDestTL ) },
	{ "dest_tr",			ParseSettable< Vector2D >,		offsetof( ApplyStickerStageParameters, m_vDestTR ) },
	{ "adjust_black",		ParseRangeThenDivideBy< 255 >,	offsetof( ApplyStickerStageParameters, m_AdjustBlack ) },
	{ "adjust_offset",		ParseRangeThenDivideBy< 255 >,	offsetof( ApplyStickerStageParameters, m_AdjustOffset ) },
	{ "adjust_gamma",		ParseInverseRangeFromKV,		offsetof( ApplyStickerStageParameters, m_AdjustGamma ) },
	{ "evaluate?", 			ParseBoolFromKV,				offsetof( ApplyStickerStageParameters, m_Evaluate ) },

	{ 0, 0 }
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
class CTCApplyStickerStage : public CTCStage 
{
	enum { Albedo = 0, Specular = 1 };

public:
	CTCApplyStickerStage( const ApplyStickerStageParameters& _assp, uint32 nTexCompositeCreateFlags )
	: m_Parameters( _assp ) 
	, m_pMaterial( NULL )
	, m_pTex( NULL )
	, m_pTexSpecular( NULL )
	, m_nChoice( 0 )
	{ 
		SafeAssign( &m_pMaterial, materials->FindMaterial( cCombineMaterialName[ ECO_Blend ], TEXTURE_GROUP_RUNTIME_COMPOSITE ) );
	} 

	virtual ~CTCApplyStickerStage() 
	{ 
		SafeRelease( &m_pTex );
		SafeRelease( &m_pTexSpecular );
		SafeRelease( &m_pMaterial );
	} 

	virtual bool DoesTargetRenderTarget() const { return true; }

protected:
	bool AreTexturesLoaded() const
	{
		if ( !m_Parameters.m_possibleStickers[ m_nChoice ].m_baseFilename.IsEmpty() && !m_pTex )
			return false;

		if ( !m_Parameters.m_possibleStickers[ m_nChoice ].m_specFilename.IsEmpty() && !m_pTexSpecular )
			return false;

		return true;
	}

	virtual void RequestTextures()
	{
		if ( !m_Parameters.m_possibleStickers[ m_nChoice ].m_baseFilename.IsEmpty() )
			materials->AsyncFindTexture( m_Parameters.m_possibleStickers[ m_nChoice ].m_baseFilename.Get(), TEXTURE_GROUP_RUNTIME_COMPOSITE, this, ( void* ) Albedo, false, TEXTUREFLAGS_IMMEDIATE_CLEANUP );

		if ( !m_Parameters.m_possibleStickers[ m_nChoice ].m_specFilename.IsEmpty() )
			materials->AsyncFindTexture( m_Parameters.m_possibleStickers[ m_nChoice ].m_specFilename.Get(), TEXTURE_GROUP_RUNTIME_COMPOSITE, this, ( void* ) Specular, false, TEXTUREFLAGS_IMMEDIATE_CLEANUP );	
	}

	virtual void ResolveThis( CTextureCompositor* _comp )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		ECompositeResolveStatus resolveStatus = GetResolveStatus();
		// If we're done, we're done.
		if ( resolveStatus == ECRS_Complete || resolveStatus == ECRS_Error )
			return;

		if ( resolveStatus == ECRS_Scheduled )
			SetResolveStatus( ECRS_PendingTextureLoads );

		// Someone is misusing this node if this assert fires.
		Assert( GetResolveStatus() == ECRS_PendingTextureLoads );

		CTCStage* pChild = GetFirstChild();
		if ( pChild != NULL && pChild->GetResolveStatus() != ECRS_Complete )
			return;

		if ( !AreTexturesLoaded() )
			return;

		// Ensure we only have zero or one direct children.
		Assert( !pChild || pChild->GetNextSibling() == NULL );

		// We expect exactly one or zero children. If we have a child, use its render target to render to, otherwise 
		// Get one and use that.
		ITexture* pRenderTarget = _comp->AllocateCompositorRenderTarget();
		
		CUtlVector<CTCStageResult_t> results;

		// If we have a child, great! Use it. If not, 
		if ( pChild )
			results.AddToTail( pChild->GetResult() );
		else
		{
			CTCStageResult_t fakeRes;					
			fakeRes.m_pTexture = materials->FindTexture( "black", TEXTURE_GROUP_RUNTIME_COMPOSITE );
		}

		CTCStageResult_t baseTex, specTex;
		baseTex.m_pTexture = m_pTex;
		m_mTextureAdjust.Set3x4( baseTex.m_mUvAdjust );
		results.AddToTail( baseTex );

		specTex.m_pTexture = m_pTexSpecular;
		m_mTextureAdjust.Set3x4( specTex.m_mUvAdjust );
		results.AddToTail( specTex );

		Render( pRenderTarget, m_pMaterial, results, _comp, pChild == NULL );

		CTCStageResult_t res;
		res.m_pRenderTarget = pRenderTarget;
		res.m_fAdjustBlackPoint = m_fAdjustBlack;
		res.m_fAdjustWhitePoint = m_fAdjustWhite;
		res.m_fAdjustGamma      = m_fAdjustGamma;

		SetResult( res );

		// As soon as we have scheduled the read of a child render target, we can release that 
		// texture back to the pool for use by another stage. Everything is pipelined, so this just
		// works.
		CleanupChildResults( _comp );
		tmMessage( TELEMETRY_LEVEL0, TMMF_ICON_NOTE, "Completed: %s", __FUNCTION__ );
	}

	virtual bool HasTeamSpecificsThis() const OVERRIDE{ return false; }

	virtual bool ComputeRandomValuesThis( CUniformRandomStream* pRNG ) OVERRIDE
	{
		float m_fTotalWeight = 0;
		FOR_EACH_VEC( m_Parameters.m_possibleStickers, i )
		{
			m_fTotalWeight += m_Parameters.m_possibleStickers[ i ].m_fWeight;		
		}

		float fWeight = pRNG->RandomFloat( 0.0f, m_fTotalWeight );
		FOR_EACH_VEC( m_Parameters.m_possibleStickers, i )
		{
			const float thisWeight = m_Parameters.m_possibleStickers[ i ].m_fWeight;
			if ( fWeight < thisWeight )
			{
				m_nChoice = i;
				break;
			}
			else
			{
				fWeight -= thisWeight;
			}
		}
		
		const float adjustBlack = pRNG->RandomFloat( m_Parameters.m_AdjustBlack.low, m_Parameters.m_AdjustBlack.high );
		const float adjustOffset = pRNG->RandomFloat( m_Parameters.m_AdjustOffset.low, m_Parameters.m_AdjustOffset.high );
		const float adjustGamma = pRNG->RandomFloat( m_Parameters.m_AdjustGamma.low, m_Parameters.m_AdjustGamma.high );
		const float adjustWhite = adjustBlack + adjustOffset;

		m_fAdjustBlack = adjustBlack;
		m_fAdjustWhite = adjustWhite;
		m_fAdjustGamma = adjustGamma;
		
		ComputeTextureMatrixFromRectangle( &m_mTextureAdjust, m_Parameters.m_vDestBL.m_val, m_Parameters.m_vDestTL.m_val, m_Parameters.m_vDestTR.m_val );
		return true;
	}

	virtual void OnAsyncFindComplete( ITexture* pTex, void* pExtraArgs )
	{
		switch ( ( int ) pExtraArgs )
		{
		case Albedo:
			SafeAssign( &m_pTex, pTex );
			break;
		case Specular:
			// It's okay if this is the case, we just need to substitute with the black texture.
			if ( pTex->IsError() )
			{
				pTex = materials->FindTexture( "black", TEXTURE_GROUP_RUNTIME_COMPOSITE );
			}
			SafeAssign( &m_pTexSpecular, pTex );
			break;
		default:
			Assert( !"Unexpected value passed to OnAsyncFindComplete" );
			break;
		};
	}

private:
	ApplyStickerStageParameters m_Parameters;
	IMaterial* m_pMaterial;
	ITexture* m_pTex;
	ITexture* m_pTexSpecular;
	int m_nChoice;

	float m_fAdjustBlack;
	float m_fAdjustWhite;
	float m_fAdjustGamma;
	VMatrix m_mTextureAdjust;
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// This is a procedural stage we use to copy the results of a composite into a texture so we can 
// release the render targets back to a pool to be used later.
class CTCCopyStage : public CTCStage
{
public:
	CTCCopyStage()
	: m_pTex( NULL )
	{
	
	}

	~CTCCopyStage()
	{
		SafeRelease( &m_pTex );
	}

	virtual void OnAsyncCreateComplete( ITexture* pTex, void* pExtraArgs ) 
	{ 
		SafeAssign( &m_pTex, pTex ); 
		tmMessage( TELEMETRY_LEVEL0, TMMF_ICON_NOTE, "Completed: %s", __FUNCTION__ );
	}

	virtual bool DoesTargetRenderTarget() const { return false; }

private:
	virtual void RequestTextures() { /* No input textures */ }

	virtual void ResolveThis( CTextureCompositor* _comp )
	{
		tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

		ECompositeResolveStatus resolveStatus = GetResolveStatus();

		// If we're done, we're done.
		if ( resolveStatus == ECRS_Complete || resolveStatus == ECRS_Error )
			return;

		if ( resolveStatus == ECRS_Scheduled )
			SetResolveStatus( ECRS_PendingTextureLoads );

		Assert( GetFirstChild() != NULL );

		// Can't move forward until the child is done.
		if ( GetFirstChild()->GetResolveStatus() != ECRS_Complete )
			return;

		// Compositing has completed!
		if ( m_pTex ) 
		{
			if ( m_pTex->IsError() )
			{
				_comp->Error( false, "Error occurred copying render target to texture. This is fatal." );
				return;
			}

			CTCStageResult_t res;
			res.m_pTexture = m_pTex;

#ifdef STAGING_ONLY
			if ( r_texcomp_dump.GetInt() == 2 )
			{
				char buffer[128];
				V_snprintf( buffer, ARRAYSIZE(buffer), "composite_%s_result_%02d.tga", _comp->GetName().Get(), s_nDumpCount++ );
				GetFirstChild()->GetResult().m_pRenderTarget->SaveToFile( buffer );
			}
#endif

			SetResult( res );
			return;
		}

		if ( resolveStatus == ECRS_PendingComposites )
			return;

		ImageFormat fmt = IMAGE_FORMAT_DXT5_RUNTIME;

		if ( _comp->GetCreateFlags() & TEX_COMPOSITE_CREATE_FLAGS_NO_COMPRESSION ) 
			fmt = IMAGE_FORMAT_RGBA8888;

		bool bGenMipmaps = !( _comp->GetCreateFlags() & TEX_COMPOSITE_CREATE_FLAGS_NO_MIPMAPS );

		// We want to do this once only.
		char buffer[_MAX_PATH];
		_comp->GetTextureName( buffer, ARRAYSIZE( buffer ) );

		int nCreateFlags = TEXTUREFLAGS_IMMEDIATE_CLEANUP 
					     | TEXTUREFLAGS_TRILINEAR
						 | TEXTUREFLAGS_ANISOTROPIC;

#if defined( STAGING_ONLY )
		#if WITH_TEX_COMPOSITE_CACHE
			if ( r_texcomp_dump.GetInt() == 0 && ( _comp->GetCreateFlags() & TEX_COMPOSITE_CREATE_FLAGS_FORCE ) == 0 )
				nCreateFlags = 0;
		#endif
#endif

		CMatRenderContextPtr pRenderContext( materials );
		pRenderContext->AsyncCreateTextureFromRenderTarget( GetFirstChild()->GetResult().m_pRenderTarget, buffer, fmt, bGenMipmaps, nCreateFlags, this, NULL );

		SetResolveStatus( ECRS_PendingComposites );
		// Don't clean up here just yet, we'll get cleaned up when the composite is totally complete.
		tmMessage( TELEMETRY_LEVEL0, TMMF_ICON_NOTE, "Begun: %s", __FUNCTION__ );
	}

	virtual bool HasTeamSpecificsThis() const OVERRIDE { return false; }

	virtual bool ComputeRandomValuesThis( CUniformRandomStream* pRNG ) OVERRIDE
	{
		// No RNG here.
		return false;
	}

	ITexture* m_pTex;
	CUtlString m_FinalTextureName;
	uint32 m_nTexCompositeCreateFlags;
};

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
CTextureCompositor::CTextureCompositor( int _width, int _height, int nTeam, const char* pCompositeName, uint64 nRandomSeed, uint32 nTexCompositeCreateFlags )
: m_nReferenceCount( 0 )
, m_nWidth( _width )
, m_nHeight( _height )
, m_nTeam( nTeam )
, m_nRandomSeed( nRandomSeed )
, m_pRootStage( NULL )
, m_ResolveStatus( ECRS_Idle )
, m_bError( false )
, m_bFatal( false )
, m_nRenderTargetsAllocated( 0 )
, m_CompositeName( pCompositeName )
, m_nTexCompositeCreateFlags( nTexCompositeCreateFlags )
, m_bHasTeamSpecifics( false )
, m_nCompositePaintKitId( 0 )
{

}

// ------------------------------------------------------------------------------------------------
CTextureCompositor::~CTextureCompositor()
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );
	Assert ( m_nReferenceCount == 0 );

	// Have to clean up the stages before cleaning up the render target pool, because cleanup up
	// stages will throw things back to the render target pool.
	SafeRelease( &m_pRootStage );

	FOR_EACH_VEC( m_RenderTargetPool, i )
	{
		RenderTarget_t& rt = m_RenderTargetPool[ i ];
		SafeRelease( &rt.m_pRT );
	}
}

// ------------------------------------------------------------------------------------------------
void CTextureCompositor::Restart()
{
	Assert(!"TODO! Need to clone the root node, then cleanup the old root and start the new work.");

	// CTCStage* clone = m_pRootStage->Clone();
	SafeRelease( &m_pRootStage );
	// m_pRootStage = clone;

	m_ResolveStatus = ECRS_Scheduled;

	// Kick it off again
	m_pRootStage->Resolve( true, this );
	m_ResolveStatus = ECRS_PendingTextureLoads;
}

// ------------------------------------------------------------------------------------------------
void CTextureCompositor::Shutdown()
{
	// If this thing is a template, then it's a faker and doesn't have an m_pRootStage. This is 
	// only true during startup when we're just verifying that the templates look sane--later
	// they should have real data.
	if ( m_pRootStage )
		m_pRootStage->Cleanup( this );

	// These should match now.
	Assert( m_nRenderTargetsAllocated == m_RenderTargetPool.Count() );
}

// ------------------------------------------------------------------------------------------------
int CTextureCompositor::AddRef()
{
	return ++m_nReferenceCount;
}

// ------------------------------------------------------------------------------------------------
int CTextureCompositor::Release()
{
	int retVal = --m_nReferenceCount;
	Assert( retVal >= 0 ); 
	if ( retVal == 0 ) 
	{
		Shutdown();
		delete this;	
	}

	return retVal;
}

// ------------------------------------------------------------------------------------------------
void CTextureCompositor::Update()
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	Assert( m_pRootStage );

	if ( m_bError )
	{
		if ( !m_bFatal )
		{
			m_bError = false;
			Restart();
		}
		else
			m_ResolveStatus = ECRS_Error;		
		return;	
	}

	if ( m_pRootStage->GetResolveStatus() != ECRS_Complete )
		m_pRootStage->Resolve( false, this );

	if ( m_pRootStage->GetResolveStatus() == ECRS_Complete )
	{
		#ifdef STAGING_ONLY
			// One time, go ahead and dump out the texture if we're supposed to right here, at completion time.
			if ( ( r_texcomp_dump.GetInt() == 3 || r_texcomp_dump.GetInt() == 4 ) && m_ResolveStatus != ECRS_Complete )
			{
				char filename[_MAX_PATH];
				V_sprintf_safe( filename, "%s.tga", m_CompositeName.Get() );
				m_pRootStage->GetResult().m_pTexture->SaveToFile( filename );
			}
		#endif

		m_ResolveStatus = ECRS_Complete;

#ifdef RAD_TELEMETRY_ENABLED
		char buffer[ 256 ];
		GetTextureName( buffer, ARRAYSIZE( buffer ) );
		tmEndTimeSpan( TELEMETRY_LEVEL0, m_nCompositePaintKitId, 0, "Composite: %s", tmDynamicString( TELEMETRY_LEVEL0, buffer ) );
#endif
	}
}

// ------------------------------------------------------------------------------------------------
ITexture* CTextureCompositor::GetResultTexture() const
{
	Assert( m_pRootStage && m_pRootStage->GetResolveStatus() == ECRS_Complete );
	Assert( m_pRootStage->GetResult().m_pTexture );
	return m_pRootStage->GetResult().m_pTexture;
}

// ------------------------------------------------------------------------------------------------
ECompositeResolveStatus CTextureCompositor::GetResolveStatus() const
{
	return m_ResolveStatus;
}

// ------------------------------------------------------------------------------------------------
void CTextureCompositor::ScheduleResolve( )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	Assert( m_pRootStage );
	Assert( m_ResolveStatus == ECRS_Idle );

	#if WITH_TEX_COMPOSITE_CACHE
		if ( ( GetCreateFlags() & TEX_COMPOSITE_CREATE_FLAGS_FORCE ) == 0)
		{
			char buffer[ _MAX_PATH ];
			GetTextureName( buffer, ARRAYSIZE( buffer ) );

			// I think there's a race condition here, add a flag to FindTexture that says only if loaded, and bumps ref?
			if ( materials->IsTextureLoaded( buffer ) )
			{
				ITexture* resTexture = materials->FindTexture( buffer, TEXTURE_GROUP_RUNTIME_COMPOSITE, false, 0 );
				if ( resTexture && resTexture->IsError() == false )
				{
					m_pRootStage->OnAsyncCreateComplete( resTexture, NULL );
					CTCStageResult_t res;
					res.m_pTexture = resTexture;
					m_pRootStage->SetResult( res );

					m_ResolveStatus = ECRS_Complete;
					return;
				}
			}
		}
	#endif

	#ifdef RAD_TELEMETRY_ENABLED
		m_nCompositePaintKitId = ++s_nCompositeCount;
		char buffer[256];
		GetTextureName( buffer, ARRAYSIZE( buffer ) );
		tmBeginTimeSpan( TELEMETRY_LEVEL0, m_nCompositePaintKitId, 0, "Composite: %s", tmDynamicString( TELEMETRY_LEVEL0, buffer ) );
	#endif

	m_ResolveStatus = ECRS_Scheduled;

	// Naughty.
	extern CMaterialSystem g_MaterialSystem;
	g_MaterialSystem.ScheduleTextureComposite( this );
}

// ------------------------------------------------------------------------------------------------
void CTextureCompositor::Resolve()
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	// We can actually get in multiply times for the same one because of the way EconItemView works.
	// So if that's the case, bail.
	if ( m_ResolveStatus != ECRS_Scheduled )
		return;

	m_pRootStage->Resolve( true, this );

	// Update our resolve status
	m_ResolveStatus = ECRS_PendingTextureLoads;
}

// ------------------------------------------------------------------------------------------------
void CTextureCompositor::Error( bool _retry, const char* _debugDevMsg, ... )
{
	m_bError = true;
	m_bFatal = !_retry;

	va_list args;
	va_start( args, _debugDevMsg );
	WarningV( _debugDevMsg, args );
	va_end( args );
}

// ------------------------------------------------------------------------------------------------
void CTextureCompositor::SetRootStage( CTCStage* rootStage )
{
	SafeAssign( &m_pRootStage, rootStage );

	// After we set a root, compute everyone's RNG values. Do this once, early, to ensure the values are stable.
	uint32 seedhi = 0;
	uint32 seedlo = 0;
	GetSeed( &seedhi, &seedlo );

	CUniformRandomStream streams[2];
	streams[0].SetSeed( seedhi );
	streams[1].SetSeed( seedlo );
	
	int currentIndex = 0;

	m_pRootStage->ComputeRandomValues( &currentIndex, streams, ARRAYSIZE( streams ) );
}

// ------------------------------------------------------------------------------------------------
// TODO: Need to accept format and depth status
ITexture* CTextureCompositor::AllocateCompositorRenderTarget( )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	FOR_EACH_VEC( m_RenderTargetPool, i )
	{
		const RenderTarget_t& rt = m_RenderTargetPool[ i ];
		if ( rt.m_nWidth == m_nWidth && rt.m_nHeight == m_nHeight )
		{
			ITexture* retVal = rt.m_pRT;
			m_RenderTargetPool.Remove( i );
			return retVal;
		}
	}

	// Lie to the material system that we are asking for this allocation way back at the beginning of time.
	// This used to matter to GPUs for perf, but hasn't in a long time.
	materials->OverrideRenderTargetAllocation( true );
	ITexture* retVal = materials->CreateNamedRenderTargetTextureEx( "", m_nWidth, m_nHeight, RT_SIZE_LITERAL_PICMIP, IMAGE_FORMAT_RGBA8888, MATERIAL_RT_DEPTH_NONE, TEXTUREFLAGS_IMMEDIATE_CLEANUP );
	Assert( retVal );
	materials->OverrideRenderTargetAllocation( false );

	// Used to count how many we actually allocated so we can verify we cleaned them all up at 
	// shutdown
	++m_nRenderTargetsAllocated;
	return retVal;
}

// ------------------------------------------------------------------------------------------------
void CTextureCompositor::ReleaseCompositorRenderTarget( ITexture* _tex )
{
	Assert( _tex );
	int w = _tex->GetMappingWidth();
	int h = _tex->GetMappingHeight();

	RenderTarget_t rt = { w, h, _tex };
	m_RenderTargetPool.AddToTail( rt );
}

// ------------------------------------------------------------------------------------------------
void CTextureCompositor::GetTextureName( char* pOutBuffer, int nBufferLen ) const
{
	uint32 seedhi = 0;
	uint32 seedlo = 0;
	GetSeed( &seedhi, &seedlo );

	Assert( m_pRootStage != NULL );
	if ( m_pRootStage->HasTeamSpecifics() )
		V_snprintf( pOutBuffer, nBufferLen, "proc/texcomp/%s_flags%08x_seedhi%08x_seedlo%08x_team%d_w%d_h%d", GetName().Get(), GetCreateFlags(), seedhi, seedlo, m_nTeam, m_nWidth, m_nHeight	);
	else
		V_snprintf( pOutBuffer, nBufferLen, "proc/texcomp/%s_flags%08x_seedhi%08x_seedlo%08x_w%d_h%d", GetName().Get(), GetCreateFlags(), seedhi, seedlo, m_nWidth, m_nHeight	);
}

// ------------------------------------------------------------------------------------------------
void CTextureCompositor::GetSeed( uint32* pOutHi, uint32* pOutLo ) const
{
	tmZone( TELEMETRY_LEVEL2, TMZF_NONE, "%s", __FUNCTION__ );

	Assert( pOutHi && pOutLo );
	( *pOutHi ) = 0;
	( *pOutLo ) = 0;

	// This is most definitely not the most efficient way to do this.
	for ( int i = 0; i < 32; ++i ) 
	{
		( *pOutHi ) |= (uint32)( ( m_nRandomSeed & ( uint64( 1 ) << ( ( 2 * i ) + 0 ) ) ) >> i );
		( *pOutLo ) |= (uint32)( ( m_nRandomSeed & ( uint64( 1 ) << ( ( 2 * i ) + 1 ) ) ) >> ( i + 1 ) );
	}
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
CTCStage::CTCStage() 
: m_nReferenceCount( 1 ) // This is 1 because the common case is to assign these as children, and we don't want to play with refs there.
, m_pFirstChild( NULL )
, m_pNextSibling( NULL )
, m_ResolveStatus( ECRS_Idle )
{ }

// ------------------------------------------------------------------------------------------------
CTCStage::~CTCStage() 
{ 
	Assert ( m_nReferenceCount == 0 );
	SafeRelease( &m_pFirstChild ); 
	SafeRelease( &m_pNextSibling );
}

// ------------------------------------------------------------------------------------------------
int CTCStage::AddRef() 
{ 
	return ++m_nReferenceCount; 
}

// ------------------------------------------------------------------------------------------------
int CTCStage::Release() 
{ 
	int retVal = --m_nReferenceCount;
	if ( retVal == 0 )
		delete this; 
	return retVal;
}

// ------------------------------------------------------------------------------------------------
void CTCStage::Resolve( bool bFirstTime, CTextureCompositor* _comp )
{

	if ( m_pFirstChild )
		m_pFirstChild->Resolve( bFirstTime, _comp );

	// Update our status, which may be updated below. Only do this the first time through.
	if ( bFirstTime ) 
	{
		m_ResolveStatus = ECRS_Scheduled;
		// Request textures here. We used to request in the constructor, but it caused us
		// to potentially hold all paintkitted textures for all time. That's bad for Mac,
		// where we are super memory constrained.
		RequestTextures();
	}
	
	ResolveThis( _comp );

	if ( m_pNextSibling )
		m_pNextSibling->Resolve( bFirstTime, _comp );
}

// ------------------------------------------------------------------------------------------------
bool CTCStage::HasTeamSpecifics( ) const
{
	if ( m_pFirstChild && m_pFirstChild->HasTeamSpecifics() )
		return true;

	if ( HasTeamSpecificsThis() )
		return true;

	return m_pNextSibling && m_pNextSibling->HasTeamSpecifics();
}

// ------------------------------------------------------------------------------------------------
void CTCStage::ComputeRandomValues( int* pCurIndex, CUniformRandomStream* pRNGs, int nRNGCount )
{
	Assert( pCurIndex != NULL );
	Assert( pRNGs != NULL );
	Assert( nRNGCount != 0 );

	// We do a depth-first traversal here, but we hit ourselves first.
	if ( ComputeRandomValuesThis( &pRNGs[*pCurIndex] ) )
	{
		// Switch which RNG the next person will use.
		( *pCurIndex ) = ( ( *pCurIndex ) + 1 ) % nRNGCount;
	}

	if ( m_pFirstChild )
		m_pFirstChild->ComputeRandomValues( pCurIndex, pRNGs, nRNGCount );

	if ( m_pNextSibling )
		m_pNextSibling->ComputeRandomValues( pCurIndex, pRNGs, nRNGCount );
}

// ------------------------------------------------------------------------------------------------
void CTCStage::CleanupChildResults( CTextureCompositor* _comp )
{
	// This does not recurse. We call it as we move through the tree to clean up our 
	// first-generation children.
	for ( CTCStage* child = GetFirstChild(); child; child = child->GetNextSibling() )
	{
		child->m_Result.Cleanup( _comp );
		child->m_Result = CTCStageResult_t();
	}
}

// ------------------------------------------------------------------------------------------------
void CTCStage::Render( ITexture* _destRT, IMaterial* _mat, const CUtlVector<CTCStageResult_t>& _inputs, CTextureCompositor* _comp, bool bClear )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	CUtlVector< IMaterialVar* > varsToClean;
	bool bFound = false;
	char buffer[128];
	FOR_EACH_VEC( _inputs, i )
	{
		const CTCStageResult_t& stageParams = _inputs[ i ];

		Assert( stageParams.m_pTexture || stageParams.m_pRenderTarget );
		ITexture* inTex = stageParams.m_pTexture 
			            ? stageParams.m_pTexture 
						: stageParams.m_pRenderTarget;

		V_snprintf( buffer, ARRAYSIZE( buffer ), "$srctexture%d", i );

		// Set the texture
		IMaterialVar* var = _mat->FindVar( buffer, &bFound );
		Assert( bFound );
		var->SetTextureValue( inTex );
		varsToClean.AddToTail( var );

		// And the levels parameters
		V_snprintf( buffer, ARRAYSIZE(buffer), "$texadjustlevels%d", i );
		var = _mat->FindVar( buffer, &bFound );
		Assert(bFound);
		var->SetVecValue( stageParams.m_fAdjustBlackPoint, stageParams.m_fAdjustWhitePoint, stageParams.m_fAdjustGamma );

		// And the expected transform
		V_snprintf( buffer, ARRAYSIZE(buffer), "$textransform%d", i );
		var = _mat->FindVar( buffer, &bFound );
		Assert(bFound);
		var->SetMatrixValue( stageParams.m_mUvAdjust );
	}

	IMaterialVar* var = _mat->FindVar( "$textureinputcount", &bFound );
	Assert( bFound );
	var->SetIntValue( _inputs.Count() );

	CMatRenderContextPtr pRenderContext( materials );

	int w = _destRT->GetActualWidth();
	int h = _destRT->GetActualHeight();

	pRenderContext->PushRenderTargetAndViewport( _destRT, 0, 0, w, h );
 
	if ( bClear )
	{
		pRenderContext->ClearColor4ub( 0, 0, 0, 255 );
		pRenderContext->ClearBuffers( true, false, false );
	}

	// Perform the render!
	pRenderContext->DrawScreenSpaceQuad( _mat );

#ifdef STAGING_ONLY
	if (r_texcomp_dump.GetInt() == 1)
	{
		FOR_EACH_VEC(_inputs, i)
		{
			if (_inputs[i].m_pTexture)
			{
				V_snprintf(buffer, ARRAYSIZE(buffer), "composite_%s_input_%02d_in%01d_%08x.tga", _comp->GetName().Get(), s_nDumpCount, i, (int) this);
				_inputs[i].m_pTexture->SaveToFile(buffer);
			}
		}

		V_snprintf(buffer, ARRAYSIZE(buffer), "composite_%s_result_%02d_%08x.tga", _comp->GetName().Get(), s_nDumpCount++, (int) this);
		_destRT->SaveToFile(buffer);
	}
#endif

	// Restore previous state
	pRenderContext->PopRenderTargetAndViewport();

	// After rendering, clean up the leftover texture references or they will be there for a long
	// time.
	FOR_EACH_VEC( varsToClean, i )
	{
		varsToClean[ i ]->SetUndefined();
	}
}

// ------------------------------------------------------------------------------------------------
void CTCStage::Cleanup( CTextureCompositor* _comp )
{
	if ( m_pFirstChild )
		m_pFirstChild->Cleanup( _comp );

	m_Result.Cleanup( _comp );

	if ( m_pNextSibling )
		m_pNextSibling->Cleanup( _comp );
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
typedef bool ( *TBuildNodeFromKVFunc )( CTCStage** ppOutStage, const char* _key, KeyValues* _kv, uint32 nTexCompositeCreateFlags );
bool TexStageFromKV( CTCStage** ppOutStage, const char* _key, KeyValues* _kv, uint32 nTexCompositeCreateFlags );
template<int Type> bool CombineStageFromKV( CTCStage** ppOutStage, const char* _key, KeyValues* _kv, uint32 nTexCompositeCreateFlags );
bool SelectStageFromKV( CTCStage** ppOutStage, const char* _key, KeyValues* _kv, uint32 nTexCompositeCreateFlags );
bool ApplyStickerStageFromKV( CTCStage** ppOutStage, const char* _key, KeyValues* _kv, uint32 nTexCompositeCreateFlags );

struct NodeDefinitionEntry
{
	const char* keyName;
	TBuildNodeFromKVFunc buildFunc;
};

NodeDefinitionEntry cNodeParseTable[] = 
{
	{ "texture_lookup",		TexStageFromKV },

	{ "combine_add",		CombineStageFromKV<ECO_Add> },
	{ "combine_lerp",		CombineStageFromKV<ECO_Lerp> },
	{ "combine_multiply",	CombineStageFromKV<ECO_Multiply> },

	{ "select",				SelectStageFromKV },

	{ "apply_sticker",		ApplyStickerStageFromKV },

	{ 0, 0 }
};

// ------------------------------------------------------------------------------------------------
template<typename S>
void ParseIntoStruct( S* _outStruct, CUtlVector< KeyValues *>* _leftovers, KeyValues* _kv, uint32 nTexCompositeCreateFlags, const ParseTableEntry* _entries )
{
	Assert( _leftovers );

	const char* keyName = _kv->GetName();
	keyName;

	FOR_EACH_SUBKEY( _kv, thisKey )
	{
		bool parsed = false;
		for ( int e = 0; _entries[e].keyName; ++e )
		{
			if ( V_stricmp( _entries[e].keyName, thisKey->GetName() ) == 0 )
			{
				// If we're instancing, go ahead and run the parse function. If we're just doing template verification
				// then the right hand side may still have variables that need to be expanded, so just verify that the
				// left hand side is sane.
				if ( ( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_TEMPLATE_ONLY ) == 0 )
				{
					void* pDest = ((unsigned char*)_outStruct) + _entries[e].structOffset;
					_entries[e].parseFunc( thisKey, pDest );
				}
				parsed = true;
				break;
			}
		}

		if ( !parsed )
		{
			( *_leftovers ).AddToTail( thisKey );		
		}
	}
}

// ------------------------------------------------------------------------------------------------
bool ParseNodes( CUtlVector< CTCStage* >* _outStages, const CUtlVector< KeyValues *>& _kvs, uint32 nTexCompositeCreateFlags )
{
	tmZone( TELEMETRY_LEVEL0, TMZF_NONE, "%s", __FUNCTION__ );

	bool anyFails = false;

	FOR_EACH_VEC( _kvs, thisKey )
	{
		KeyValues *thisKV = _kvs[ thisKey ];

		bool parsed = false;
		for ( int e = 0; cNodeParseTable[ e ].keyName; ++e )
		{			
			if ( V_stricmp( cNodeParseTable[ e ].keyName, thisKV->GetName() ) == 0 )
			{
				CTCStage* pNewStage = NULL;
				if ( !cNodeParseTable[ e ].buildFunc( &pNewStage, thisKV->GetName(), thisKV, nTexCompositeCreateFlags ) )
					anyFails = true;

				(*_outStages).AddToTail( pNewStage );
				parsed = true;
				break;
			}
		}

		if (!parsed)
		{
			DevWarning( "Compositor Error: Unexpected key '%s' while parsing definition.\n", thisKV->GetName() );
			anyFails = true;
		}
	}

	return !anyFails;
}

// ------------------------------------------------------------------------------------------------
bool TexStageFromKV( CTCStage** ppOutStage, const char* _key, KeyValues* _kv, uint32 nTexCompositeCreateFlags )
{
	Assert( ppOutStage != NULL );

	TextureStageParameters tsp;
	CUtlVector< KeyValues* > leftovers;
	CUtlVector< CTCStage* > childNodes;
	ParseIntoStruct( &tsp, &leftovers, _kv, nTexCompositeCreateFlags, cTextureStageParametersParseTable );
	if ( !ParseNodes( &childNodes, leftovers, nTexCompositeCreateFlags ) )
		return false;

	if ( !( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_SCHEMA_ONLY ) )
	{
		( *ppOutStage ) = new CTCTextureStage( tsp, nTexCompositeCreateFlags );
		( *ppOutStage )->AppendChildren( childNodes );
	}

	return true;
}

// ------------------------------------------------------------------------------------------------
template <int Type>
bool CombineStageFromKV( CTCStage** ppOutStage, const char* _key, KeyValues* _kv, uint32 nTexCompositeCreateFlags )
{
	Assert( ppOutStage != NULL );

	static_assert( Type >= 0 && Type < ECO_Error, "Invalid type, you need to update the enum." );
	CombineStageParameters csp;
	csp.m_CombineOp = (ECombineOperation) Type;

	CUtlVector< KeyValues* > leftovers;
	CUtlVector< CTCStage* > childNodes;
	ParseIntoStruct( &csp, &leftovers, _kv, nTexCompositeCreateFlags, cCombineStageParametersParseTable );
	if ( !ParseNodes( &childNodes, leftovers, nTexCompositeCreateFlags  ) )
		return false;

	if ( !( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_SCHEMA_ONLY ) )
	{
		( *ppOutStage ) = new CTCCombineStage( csp, nTexCompositeCreateFlags );
		( *ppOutStage )->AppendChildren( childNodes );
	}

	return true;
}

// ------------------------------------------------------------------------------------------------
bool SelectStageFromKV( CTCStage** ppOutStage, const char* _key, KeyValues* _kv, uint32 nTexCompositeCreateFlags )
{
	Assert( ppOutStage != NULL );

	SelectStageParameters ssp;
	CUtlVector< KeyValues* > leftovers;
	CUtlVector< CTCStage* > childNodes;
	ParseIntoStruct( &ssp, &leftovers, _kv, nTexCompositeCreateFlags, cSelectStageParametersParseTable );
	if ( !ParseNodes( &childNodes, leftovers, nTexCompositeCreateFlags  ) )
		return false;

	if ( !( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_SCHEMA_ONLY ) )
	{
		( *ppOutStage ) = new CTCSelectStage( ssp, nTexCompositeCreateFlags );
		( *ppOutStage )->AppendChildren( childNodes );
	}
	
	return true;
}

// ------------------------------------------------------------------------------------------------
bool ApplyStickerStageFromKV( CTCStage** ppOutStage, const char* _key, KeyValues* _kv, uint32 nTexCompositeCreateFlags )
{
	Assert( ppOutStage != NULL );

	ApplyStickerStageParameters assp;
	CUtlVector< KeyValues* > leftovers;
	CUtlVector< CTCStage* > childNodes;
	ParseIntoStruct( &assp, &leftovers, _kv, nTexCompositeCreateFlags, cApplyStickerStageParametersParseTable );
	if ( !ParseNodes( &childNodes, leftovers, nTexCompositeCreateFlags ) )
		return false;

	// These stages can have exactly one child. 
	if ( childNodes.Count() > 1 )
		return false;

	int setCount = 0;
	if ( assp.m_vDestBL.m_bSet ) ++setCount;
	if ( assp.m_vDestTL.m_bSet ) ++setCount;
	if ( assp.m_vDestTR.m_bSet ) ++setCount;
	if ( setCount != 3 )
		return false;

	if ( !( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_SCHEMA_ONLY ) )
	{
		( *ppOutStage ) = new CTCApplyStickerStage( assp, nTexCompositeCreateFlags );
		( *ppOutStage )->AppendChildren( childNodes );
	}
	
	return true;
}

// ------------------------------------------------------------------------------------------------
const char *GetCombinedMaterialName( ECombineOperation eMaterial )
{
	Assert( eMaterial >= ECO_FirstPrecacheMaterial && eMaterial < ECO_COUNT );
	return cCombineMaterialName[eMaterial];
}

// ------------------------------------------------------------------------------------------------
KeyValues* ResolveTemplate( const char* pRootName, KeyValues* pValues, uint32 nTexCompositeCreateFlags, bool *pInOutAllocdNew )
{
	Assert( pRootName != NULL && pValues != NULL && pInOutAllocdNew != NULL );

	const char* pTemplateName = NULL;
	bool bImplementsTemplate = false;
	bool bHasOtherNodes = false;

	// First, figure out if the tree is sensible.
	FOR_EACH_SUBKEY( pValues, pChild )
	{
		const char* pChildName = pChild->GetName();
		if ( V_stricmp( pChildName, "implements" ) == 0 )
		{
			if ( bImplementsTemplate )
			{
				Warning( "ERROR[%s]: implements field can only appear once, seen a second time as 'implements \"%s\"\n", pRootName, pChild->GetString() );
				return NULL;			
			}

			bImplementsTemplate = true;
			pTemplateName = pChild->GetString();
		}
		else if ( pChildName && pChildName[0] != '$' )
		{
			bHasOtherNodes = true;
		}
	}

	if ( bImplementsTemplate && bHasOtherNodes )
	{
		Warning( "ERROR[%s]: if using 'implements', can only have variable definitions--other fields not allowed.\n", pRootName );
		return NULL;
	}
	
	// If we're not doing templates, we're all finished. 
	if ( !bImplementsTemplate )
		return pValues;

	KeyValues* pNewKV = NULL;

	if ( ( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_TEMPLATE_ONLY ) == 0 )
	{
		CTextureCompositorTemplate* pTmpl = TextureManager()->FindTextureCompositorTemplate( pTemplateName );
		if ( !pTmpl )
		{
			Warning( "ERROR[%s]: Couldn't find template named '%s'.\n", pRootName, pTemplateName );
			return NULL;
		}

		Assert( pTmpl->GetKV() );

		// If the verify flag isn't set, we're instancing the template so do all the logic.
		if  ( pTmpl->ImplementsTemplate() )
		{
			pNewKV = ResolveTemplate( pRootName, pTmpl->GetKV(), nTexCompositeCreateFlags, pInOutAllocdNew );
		}
		else
		{
			// The root-most template will allocate the memory for all of us.
			pNewKV = pTmpl->GetKV()->MakeCopy();
			pNewKV->SetName( pRootName );
			( *pInOutAllocdNew ) = true;
		}
	}
	else
	{
		// Just return the original KV back to the caller, who just wants a success code here. 
		return pValues;	
	}

	// Now, copy any child var definitions from pValues into pNewKV. Because of the recursive call stack, 
	// this has the net effect that more concrete templates will write their values later than more remote templates.
	FOR_EACH_SUBKEY( pValues, pChild )
	{
		const char* pChildName = pChild->GetName();
		if ( pChildName && pChildName[0] == '$' )
		{
			pNewKV->AddSubKey( pChild->MakeCopy() );
		}
	}

	// Success!
	return pNewKV;
}

// ------------------------------------------------------------------------------------------------
typedef CUtlDict< const char* > VariableDefs_t;
KeyValues* ExtractVariableDefinitions( VariableDefs_t* pOutVarDefs, const char* pRootName, KeyValues* pKeyValues )
{
	Assert( pOutVarDefs );

	FOR_EACH_SUBKEY( pKeyValues, pChild )
	{
		const char* pChildName = pChild->GetName();
		if ( pChildName[0] == '$' )
		{
			if ( pChild->GetFirstTrueSubKey() )
			{
				Warning( "ERROR[%s]: All variable definitions must be simple strings, '%s' was a full subtree.\n", pRootName, pChildName );
				return NULL;
			}

			int ndx = ( *pOutVarDefs ).Find( pChildName + 1 );
			if ( pOutVarDefs->IsValidIndex( ndx ) )
				( *pOutVarDefs )[ ndx ] = pChild->GetString();
			else
				( *pOutVarDefs ).Insert( pChildName + 1, pChild->GetString() );
		}
	}

	return pKeyValues;
}

// ------------------------------------------------------------------------------------------------
CUtlString GetErrorTrail( CUtlVector< const char* >& errorStack )
{
	if ( errorStack.Count() == 0 )
		return CUtlString( "" );

	const int stackLength = errorStack.Count();
	const int stackLengthMinusOne = stackLength - 1;

	const char* cStageSep = " -> ";
	const int cStageSepStrLen = V_strlen( cStageSep );

	int totalStrLength = 0;

	for ( int i = 0; i < stackLength; ++i ) 
	{
		totalStrLength += V_strlen( errorStack[ i ] );
	}

	totalStrLength += stackLengthMinusOne * cStageSepStrLen;

	CUtlString retStr;
	retStr.SetLength( totalStrLength );

	char* pDstOrig = retStr.GetForModify(); pDstOrig;
	char* pDst = retStr.GetForModify();

	int destPos = 0;
	for ( int i = 0; i < stackLength; ++i )
	{
		// Copy the string
		const char* pSrc = errorStack[ i ];
		while ( ( *pDst++ = *pSrc++ ) != 0 ) 
			++destPos;
		--pDst;

		if ( i < stackLengthMinusOne )
		{
			// Now copy our separator
			pSrc = cStageSep;
			while ( ( *pDst++ = *pSrc++ ) != 0 )
				++destPos;
			--pDst;
		}
	}

	Assert( destPos == totalStrLength );
	Assert( pDst - retStr.Get() == totalStrLength );
	// SetLength above already included the +1 to length for the null terminator.
	*pDst = '\0';

	return retStr;
}

// ------------------------------------------------------------------------------------------------
enum ParseMode
{
	Copy,
	DetermineStringForReplace,
};

// ------------------------------------------------------------------------------------------------
// Returns the number of characters written into pOutBuffer or -1 if there was an error. 
int SubstituteVarsRecursive( char* pOutBuffer, int* pOutSubsts, CUtlVector< const char* >& errorStack, const char* pStr, uint32 nTexCompositeCreateFlags, const VariableDefs_t& varDefs )
{
	ParseMode mode = Copy;
	char* pCurVariable = NULL;

	char* pDst = pOutBuffer;

	int srcPos = 0;
	int dstPos = 0;
	while ( pStr[ srcPos ] != 0 )
	{
		const char* srcC = pStr + srcPos;

		switch ( mode )
		{
		case Copy:
			if ( srcC[ 0 ] == '$' && srcC[ 1 ] == '[' )
			{
				mode = DetermineStringForReplace;
				srcPos += 2;
				pCurVariable = const_cast< char* >( pStr + srcPos );
				continue;
			}
			else if ( pOutBuffer )
			{
				pDst[ dstPos++ ] = pStr[ srcPos++ ];
			}
			else
			{
				++dstPos;
				++srcPos;
			}

			break;

		case DetermineStringForReplace:
			if ( srcC[ 0 ] == ']' )
			{
				// Make a modification so we can just do the lookup from this buffer.
				pCurVariable[ srcC - pCurVariable ] = 0;

				// Lookup our substitution value.
				int ndx = varDefs.Find( pCurVariable );
				const char* pSubstText = NULL;

				if ( ndx != varDefs.InvalidIndex() )
				{
					pSubstText = varDefs[ ndx ];	
				}
				else if ( ( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_TEMPLATE_ONLY ) != 0 )
				{
					pSubstText = ""; // It's fine to run into these when verifying the template only.
				}
				else
				{
					Warning( "ERROR[%s]: Couldn't find variable named $%s that was requested to be substituted.\n", ( const char* ) GetErrorTrail( errorStack ), pCurVariable );

					// Restore the string first. 
					pCurVariable[ srcC - pCurVariable ] = ']';

					return -1;
				}

				// Put it back.
				pCurVariable[ srcC - pCurVariable ] = ']';

				int charsWritten = SubstituteVarsRecursive( pOutBuffer ? &pDst[ dstPos ] : NULL, pOutSubsts, errorStack, pSubstText, nTexCompositeCreateFlags, varDefs );
				if ( charsWritten < 0 )
					return -1;

				++( *pOutSubsts );
				dstPos += charsWritten;
				++srcPos;

				mode = Copy;
			}
			else
			{
				++srcPos;
			}

			break;
		}
	}

	if ( mode == DetermineStringForReplace )
	{
		Warning( "ERROR[%s]: Variable $[%s missing closing bracket ].\n", ( const char* ) GetErrorTrail( errorStack ), pCurVariable );
		return -1;
	}

	return dstPos;
}

// ------------------------------------------------------------------------------------------------
// Returns true if successful, false otherwise.
bool SubstituteVars( CUtlString* pOutStr, int* pOutSubsts, CUtlVector< const char* >& errorStack, const char* pStr, uint32 nTexCompositeCreateFlags, const VariableDefs_t& varDefs )
{
	Assert( pOutStr != NULL && pOutSubsts != NULL && pStr != NULL );

	( *pOutSubsts ) = 0;
	
	// Even though this involves a traversal, we're saving a malloc by walking this thing once looking for the start token. 
	const char* pFirstRepl = V_strstr( pStr, "$[" );

	// No substitutions, so bail out now.
	if ( pFirstRepl == NULL )
	{
		( *pOutStr ) = pStr;		
		return true;
	}

	// We could do this as we go, but we're trying to avoid re-mallocing memory repeatedly in here so process once
	// to find out what the size is. 
	int expectedLen = SubstituteVarsRecursive( NULL, pOutSubsts, errorStack, pStr, nTexCompositeCreateFlags, varDefs );
	if ( expectedLen < 0 )
		return false;

	// We don't need to actually write the string, and we shouldn't. If we're just verifying, exit now with success.
	if ( ( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_TEMPLATE_ONLY ) != 0 )
		return true;

	CUtlString& outStr = ( *pOutStr );
	outStr.SetLength( expectedLen ); // SetLength does +1 to the length for us.

	int finalLen = SubstituteVarsRecursive( outStr.GetForModify(), pOutSubsts, errorStack, pStr, nTexCompositeCreateFlags, varDefs );

	if ( finalLen < 0 )
		return false;

	// Otherwise things have gone horribly wrong.
	Assert( outStr.Length() == expectedLen );
	Assert( expectedLen == finalLen );

	// Success!
	return true;
}

// ------------------------------------------------------------------------------------------------
bool ResolveAllVariablesRecursive( CUtlVector< const char* >& errorStack, const VariableDefs_t& varDefs, KeyValues* pKeyValues, uint32 nTexCompositeCreateFlags, CUtlString& tmpStr )
{
	// hope for the best
	bool success = true;

	FOR_EACH_SUBKEY( pKeyValues, pChild )
	{
		if ( pChild->GetName()[ 0 ] == '$' )
			continue;

		errorStack.AddToTail( pChild->GetName() );

		if ( pChild->GetFirstSubKey() )
		{
			if ( !ResolveAllVariablesRecursive( errorStack, varDefs, pChild, nTexCompositeCreateFlags, tmpStr ) )
				success = false;
		}
		else 
		{
			int nSubsts = 0;
			if ( !SubstituteVars( &tmpStr, &nSubsts, errorStack, pChild->GetString(), nTexCompositeCreateFlags, varDefs ) )
				success = false;

			// Did we do any substitutions?
			if ( nSubsts > 0 && ( ( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_TEMPLATE_ONLY ) == 0 ) )
				pChild->SetStringValue( tmpStr );
		}

		errorStack.RemoveMultipleFromTail( 1 );
	}
	
	return success;
}

// ------------------------------------------------------------------------------------------------
KeyValues* ResolveAllVariables( const char* pRootName, const VariableDefs_t& varDefs, KeyValues* pKeyValues, uint32 nTexCompositeCreateFlags, bool *pInOutAllocdNew )
{
	KeyValuesAD kvad_onError( ( KeyValues* ) nullptr );

	// Let's just assume first that if we have any vars, we will need to substitute them.
	// But if we're just verifying the template, no need.
	if ( !( *pInOutAllocdNew ) && varDefs.Count() > 0 && ( ( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_TEMPLATE_ONLY ) == 0 ) )
	{
		pKeyValues = pKeyValues->MakeCopy();
		kvad_onError.Assign( pKeyValues );
		( *pInOutAllocdNew ) = true;
	}

	CUtlString str;

	CUtlVector< const char* > errorStack;
	errorStack.AddToHead( pRootName );

	if ( !ResolveAllVariablesRecursive( errorStack, varDefs, pKeyValues, nTexCompositeCreateFlags, str ) )
		return NULL;

	kvad_onError.Assign( NULL );
	return pKeyValues;
}

// ------------------------------------------------------------------------------------------------
// Perform all template expansion and variable substitution here. What should be output 
// should look like v1.0 paintkits without templates or variables. Return NULL
// if var substitution fails or if we can't resolve a template or something 
// (after outputting a meaningful error message, of course).
KeyValues* ParseTopLevelIntoKV( const char* pRootName, KeyValues* pValues, uint32 nTexCompositeCreateFlags, bool *pOutAllocdNew )
{
	Assert( pRootName != NULL );
	Assert( pOutAllocdNew != NULL );
	if ( !pValues )
		return NULL;

	bool bRequiresCleanup = false;
	KeyValues* pExpandedKV = NULL;
	KeyValuesAD autoCleanup_pExpandedKV( pExpandedKV );
	VariableDefs_t varDefs;

	pExpandedKV = ResolveTemplate( pRootName, pValues, nTexCompositeCreateFlags, &bRequiresCleanup );
	if ( pExpandedKV == NULL )
		return NULL;

	if ( bRequiresCleanup )
	{
		Assert( autoCleanup_pExpandedKV == nullptr || autoCleanup_pExpandedKV == pExpandedKV );
		autoCleanup_pExpandedKV.Assign( pExpandedKV );
	}

	pExpandedKV = ExtractVariableDefinitions( &varDefs, pRootName, pExpandedKV );
	if ( pExpandedKV == NULL )
		return NULL;

	// Only resolve the variables if we're instantiating. During verification time, we'll
	// just check that the keys are sensible and we can skip this.
	pExpandedKV = ResolveAllVariables( pRootName, varDefs, pExpandedKV, nTexCompositeCreateFlags, &bRequiresCleanup);
	if ( pExpandedKV == NULL )
		return NULL;

	Assert( bRequiresCleanup || varDefs.Count() == 0 || ( ( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_TEMPLATE_ONLY ) != 0 ) );
	varDefs.RemoveAll(); // These won't be valid after we cleanup the tree to remove variable definitions.

	if ( bRequiresCleanup )
	{
		KeyValues* pChild = pExpandedKV->GetFirstSubKey();

		while ( pChild )
		{
			const char* pChildName = pChild->GetName();
			if ( pChildName[ 0 ] == '$' )
			{
				KeyValues* pNext = pChild->GetNextKey();

				pExpandedKV->RemoveSubKey( pChild );
				pChild->deleteThis();
				pChild = pNext;
			}
			else
				pChild = pChild->GetNextKey();
		}
	}


	// We don't need to clean up the KeyValues we created, so clear the AD.
	autoCleanup_pExpandedKV.Assign( NULL );

	( *pOutAllocdNew ) = bRequiresCleanup;
	return pExpandedKV;	
}

// ------------------------------------------------------------------------------------------------
bool HasTemplateOrVariables( const char** ppOutTemplateName, KeyValues* pKV)
{
	Assert( ppOutTemplateName );

	bool retVal = false;
	( *ppOutTemplateName ) = NULL;

	FOR_EACH_SUBKEY( pKV, pChild )
	{
		const char* pName = pChild->GetName();
		if ( V_stricmp( pName, "implements" ) == 0 )	
		{
			( *ppOutTemplateName ) = pChild->GetString();
			retVal = true;
		}

		if ( pName[ 0 ] == '$' )
			retVal = true;
	}

	return retVal;
}

// ------------------------------------------------------------------------------------------------
CTextureCompositor* CreateTextureCompositor( int _w, int _h, const char* pCompositeName, int nTeamNum, uint64 nRandomSeed, KeyValues* _stageDesc, uint32 nTexCompositeCreateFlags )
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL0 );

	#ifdef STAGING_ONLY
		if ( r_texcomp_dump.GetInt() == 3 || r_texcomp_dump.GetInt() == 4 )
		{
			// Skip compression because it breaks saving render targets out
			// Also don't pollute the cache (or use it)
			nTexCompositeCreateFlags |= ( TEX_COMPOSITE_CREATE_FLAGS_NO_COMPRESSION | TEX_COMPOSITE_CREATE_FLAGS_FORCE );
		}
	#endif

	CUtlVector< CTCStage* > vecStage;
	CUtlVector< KeyValues* > kvs;

	KeyValuesAD kvAutoCleanup( (KeyValues*) nullptr );

	bool bRequiresCleanup = false;

	if ( ( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_LOG_NODES_ONLY ) != 0 )
	{
		DevMsg( 0, "%s\n{\n", pCompositeName );
		KeyValuesDumpAsDevMsg( _stageDesc, 1, 0 );
		DevMsg( 0, "}\n" );
	}

	_stageDesc = ParseTopLevelIntoKV( pCompositeName, _stageDesc, nTexCompositeCreateFlags, &bRequiresCleanup );
	if ( !_stageDesc ) 
	{
		if ( ( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_LOG_NODES_ONLY ) != 0 )
			Msg( "ERROR[%s]: Failed to create compositor, errors above.\n", pCompositeName );

		return NULL;
	}

	// Set ourselves up for future cleanup.
	if ( bRequiresCleanup )
		kvAutoCleanup.Assign( _stageDesc );

	if ( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_LOG_NODES_ONLY )
	{
		if ( bRequiresCleanup )
		{
			DevMsg( 0, "With expansion:\n%s\n{\n", pCompositeName );
			KeyValuesDumpAsDevMsg( _stageDesc, 1, 0 );
			DevMsg( 0, "}\n" );
		}
		return NULL;
	}

	const char* pTemplateName = NULL;
	// If we're just doing a template verification, and we still have keys or values that look like template stuff, bail out now. 
	if ( HasTemplateOrVariables( &pTemplateName, _stageDesc ) && ( ( nTexCompositeCreateFlags & TEX_COMPOSITE_CREATE_FLAGS_VERIFY_TEMPLATE_ONLY ) != 0 ) )
	{
		CTextureCompositor* pComp = new CTextureCompositor( _w, _h, nTeamNum, pCompositeName, nRandomSeed, nTexCompositeCreateFlags );
		if ( pTemplateName )
			pComp->SetTemplate( pTemplateName );
		return pComp;
	}
	
	KeyValues* kv = _stageDesc->GetFirstTrueSubKey();
	if ( !kv )
		return NULL;

	kvs.AddToTail( kv );
	
	if ( !ParseNodes( &vecStage, kvs, nTexCompositeCreateFlags  ) )
	{
		FOR_EACH_VEC( vecStage, i )
		{
			SafeRelease( &vecStage[ i ] );
		}
	
		return NULL;
	}

	// Should only get 1 here.
	Assert( vecStage.Count() == 1 );

	CTCStage* rootStage = vecStage[ 0 ];

	// Need to add a copy as the new root.
	CTCStage* copyStage = new CTCCopyStage;
	copyStage->SetFirstChild( rootStage );
	rootStage = copyStage;

	CTextureCompositor* texCompositor = new CTextureCompositor( _w, _h, nTeamNum, pCompositeName, nRandomSeed, nTexCompositeCreateFlags );
	if ( pTemplateName )
		texCompositor->SetTemplate( pTemplateName );

	texCompositor->SetRootStage( rootStage );

	SafeRelease( &rootStage );

	return texCompositor;
}

// ------------------------------------------------------------------------------------------------
CTextureCompositorTemplate* CTextureCompositorTemplate::Create( const char* pName, KeyValues* pTmplDesc )
{
	if ( !pName || !pTmplDesc )
		return NULL;

	CTextureCompositor* texCompositor = CreateTextureCompositor( 1, 1, pName, 2, 0, pTmplDesc, TEX_COMPOSITE_CREATE_FLAGS_VERIFY_SCHEMA_ONLY | TEX_COMPOSITE_CREATE_FLAGS_VERIFY_TEMPLATE_ONLY );

	if ( texCompositor )
	{
		CTextureCompositorTemplate* pTemplate = new CTextureCompositorTemplate( pName, pTmplDesc );
		if ( texCompositor->UsesTemplate() )
		{
			pTemplate->SetImplementsName( texCompositor->GetTemplateName() );
		}
		// Bump then release the ref.
		texCompositor->AddRef();
		texCompositor->Release();

		return pTemplate;
	}

	return NULL;
}

// ------------------------------------------------------------------------------------------------
CTextureCompositorTemplate::~CTextureCompositorTemplate()
{
	// We don't own the KV we were created with--don't delete it.
}

// ------------------------------------------------------------------------------------------------
bool CTextureCompositorTemplate::ResolveDependencies() const
{
	// If we don't reference another template, then our verification was validated at construction 
	// time.
	if ( m_ImplementsName.IsEmpty() )
		return true;

	CTextureCompositorTemplate* pImplementsTmpl = TextureManager()->FindTextureCompositorTemplate( m_ImplementsName );

	// If we couldn't find our child, then we are not okay.
	if ( pImplementsTmpl == NULL )
	{
		Warning( "ERROR[paintkit_template %s]: Couldn't find template '%s' which we claim to implement.\n", (const char*) m_Name, (const char*)m_ImplementsName );
		return false;
	}

	return true;
}

// ------------------------------------------------------------------------------------------------
bool CTextureCompositorTemplate::HasDependencyCycles()
{
	// Uses Floyd's algorithm to determine if there's a cycle. 
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL1 );

	if ( HasCycle( this ) )
	{
		// Print the cycle. This also marks the nodes as having been tested for cycles.
		PrintMinimumCycle( this );
		return true;
	}
	else
	{
		// Mark everything in this lineage as having been tested for cycles. 
		CTextureCompositorTemplate* pTmpl = this;
		while ( pTmpl != NULL )
		{
			if ( pTmpl->HasCheckedForCycles() )
				break;

			pTmpl->SetCheckedForCycles( true );
			pTmpl = Advance( pTmpl, 1 );
		}
	}

	return false;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
void ComputeTextureMatrixFromRectangle( VMatrix* pOutMat, const Vector2D& bl, const Vector2D& tl, const Vector2D& tr )
{
	Assert( pOutMat != NULL );

	Vector2D leftEdge = bl - tl;
	Vector2D topEdge = tr - tl;
	Vector2D topEdgePerpLeft( -topEdge.y, topEdge.x );

	float magLeftEdge = leftEdge.Length();
	float magTopEdge = topEdge.Length();

	float xScalar = ( topEdgePerpLeft.Dot( leftEdge ) > 0 ) ? 1 : -1;


	// Simplification of acos( ( A . L ) / ( mag( A ) * mag( L ) )
	// Because A is ( 0, 1), which means A . L is just L.y
	// and mag( A ) * mag( L ) is just mag( L )
	float rotationD = RAD2DEG( acos( leftEdge.y / magLeftEdge ) ) 
		            * ( leftEdge.x < 0 ? 1 : -1 );
	
	VMatrix tmpMat;
	tmpMat.Identity();
	MatrixTranslate( tmpMat, Vector( tl.x, tl.y, 0 ) );
	MatrixRotate( tmpMat, Vector( 0, 0, 1 ), rotationD );
	tmpMat = tmpMat.Scale( Vector( xScalar * magTopEdge, magLeftEdge, 1.0f ) );
	MatrixInverseGeneral( tmpMat, *pOutMat );
	
	// Copy W into Z because this is a 2-D matrix.
	( *pOutMat )[ 0 ][ 2 ] = ( *pOutMat )[ 0 ][ 3 ];
	( *pOutMat )[ 1 ][ 2 ] = ( *pOutMat )[ 1 ][ 3 ];
	( *pOutMat )[ 2 ][ 2 ] = 1.0f;
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
CTextureCompositorTemplate* Advance( CTextureCompositorTemplate* pTmpl, int nSteps )
{
	Assert( pTmpl != NULL );

	for ( int i = 0; i < nSteps; ++i ) 
	{
		if ( pTmpl->ImplementsTemplate() )
		{
			pTmpl = TextureManager()->FindTextureCompositorTemplate( pTmpl->GetImplementsName() );
		}
		else
			return NULL;
	}

	return pTmpl;
}

// ------------------------------------------------------------------------------------------------
bool HasCycle( CTextureCompositorTemplate* pStartTempl )
{
	Assert( pStartTempl != NULL );

	CTextureCompositorTemplate* pTortoise = pStartTempl;
	CTextureCompositorTemplate* pHare = Advance( pStartTempl, 1 );

	while ( pHare != NULL )
	{
		Assert( pTortoise != NULL ); // pTortoise should never be NULL unless pHare already is.

		if ( pTortoise == pHare )
			return true;

		// There may still actually be a cycle here, but we've already reported it if so,
		// so go ahead and bail out and say "no cycle found."
		if ( pTortoise->HasCheckedForCycles() || pHare->HasCheckedForCycles() )
			return false;

		pTortoise = Advance( pTortoise, 1 );
		pHare = Advance( pHare, 1 );
	}

	return false;
}

// ------------------------------------------------------------------------------------------------
void PrintMinimumCycle( CTextureCompositorTemplate* pTmpl )
{
	TM_ZONE_DEFAULT( TELEMETRY_LEVEL1 );

	const char* pFirstNodeName = pTmpl->GetName();
	// Also mark the nodes as having been cycle-tested to save execution of retesting the same templates.

	// Finding a minimum cycle is O( n log n ) using a map, but we only do this when there's an error.
	CUtlMap< CTextureCompositorTemplate*, int > cycles( DefLessFunc( CTextureCompositorTemplate* ) );
	CUtlLinkedList< const char* > cycleBuilder;

	while ( pTmpl != NULL)
	{
		// Add before we bail so that the first looping element is in the list twice.
		cycleBuilder.AddToTail( pTmpl->GetName() );

		if ( cycles.IsValidIndex( cycles.Find( pTmpl ) ) )
			break;

		pTmpl->SetCheckedForCycles( true );
		cycles.Insert( pTmpl );
		pTmpl = Advance( pTmpl, 1 );
	}

	// If this hits, we didn't actually have a cycle. What?
	Assert( pTmpl );

	Warning( "ERROR[paintkit_template %s]: Detected cycle in paintkit template dependency chain: ", pFirstNodeName );
	FOR_EACH_LL( cycleBuilder, i )
	{
		Warning( "%s -> ", cycleBuilder[ i ] );
	}

	Warning( "...\n" );
}

