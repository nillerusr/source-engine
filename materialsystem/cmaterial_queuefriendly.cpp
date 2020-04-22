//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "cmaterial_queuefriendly.h"
#include "tier1/callqueue.h"
#include "materialsystem_global.h"


#define USE_QUEUED_MATERIAL_CALLS //uncomment to queue up material changing calls. Comment out to always use instant calls.


#ifdef USE_QUEUED_MATERIAL_CALLS

#define QUEUE_MATERIAL_CALL( FuncName, ... )														\
	{																								\
		ICallQueue *pCallQueue = materials->GetRenderContext()->GetCallQueue();						\
		if ( !pCallQueue )																			\
		{																							\
			m_pRealTimeVersion->FuncName( __VA_ARGS__ );											\
		}																							\
		else																						\
		{																							\
			pCallQueue->QueueCall( m_pRealTimeVersion, &IMaterialInternal::FuncName, ##__VA_ARGS__ );	\
		}																							\
	}

#else

#define QUEUE_MATERIAL_CALL( FuncName, ... ) m_pRealTimeVersion->FuncName( __VA_ARGS__ );

#endif


const char *CMaterial_QueueFriendly::GetName() const
{
	return m_pRealTimeVersion->GetName();
}

const char *CMaterial_QueueFriendly::GetTextureGroupName() const
{
	return m_pRealTimeVersion->GetTextureGroupName();
}

PreviewImageRetVal_t CMaterial_QueueFriendly::GetPreviewImageProperties( int *width, int *height, ImageFormat *imageFormat, bool* isTranslucent ) const
{
	return m_pRealTimeVersion->GetPreviewImageProperties( width, height, imageFormat, isTranslucent );
}

PreviewImageRetVal_t CMaterial_QueueFriendly::GetPreviewImage( unsigned char *data, int width, int height, ImageFormat imageFormat ) const
{
	return m_pRealTimeVersion->GetPreviewImage( data, width, height, imageFormat );
}

int CMaterial_QueueFriendly::GetMappingWidth( )
{
	return m_pRealTimeVersion->GetMappingWidth();
}

int CMaterial_QueueFriendly::GetMappingHeight( )
{
	return m_pRealTimeVersion->GetMappingHeight();
}

int CMaterial_QueueFriendly::GetNumAnimationFrames( )
{
	return m_pRealTimeVersion->GetNumAnimationFrames();
}

bool CMaterial_QueueFriendly::InMaterialPage( void )
{
	return m_pRealTimeVersion->InMaterialPage();
}

void CMaterial_QueueFriendly::GetMaterialOffset( float *pOffset )
{
	m_pRealTimeVersion->GetMaterialOffset( pOffset );
}

void CMaterial_QueueFriendly::GetMaterialScale( float *pScale )
{
	m_pRealTimeVersion->GetMaterialScale( pScale );
}

IMaterial *CMaterial_QueueFriendly::GetMaterialPage( void )
{
	return m_pRealTimeVersion->GetMaterialPage();
}

void CMaterial_QueueFriendly::IncrementReferenceCount( void )
{
	Assert( ThreadInMainThread() );
	++m_nReferenceCount;
	m_pRealTimeVersion->IncrementReferenceCount();
}

int CMaterial_QueueFriendly::GetEnumerationID( void ) const
{
	return m_pRealTimeVersion->GetEnumerationID();
}

bool CMaterial_QueueFriendly::HasProxy( void ) const
{
	return m_pRealTimeVersion->HasProxy();
}

void CMaterial_QueueFriendly::GetReflectivity( Vector& reflect )
{
	m_pRealTimeVersion->GetReflectivity( reflect );
}

bool CMaterial_QueueFriendly::GetPropertyFlag( MaterialPropertyTypes_t type )
{
	return m_pRealTimeVersion->GetPropertyFlag( type );
}

bool CMaterial_QueueFriendly::IsTwoSided()
{
	return m_pRealTimeVersion->IsTwoSided();
}

int CMaterial_QueueFriendly::ShaderParamCount() const
{
	return m_pRealTimeVersion->ShaderParamCount();
}

bool CMaterial_QueueFriendly::IsErrorMaterial() const
{
	return m_pRealTimeVersion->IsErrorMaterial();
}

bool CMaterial_QueueFriendly::IsSpriteCard()
{
	return m_pRealTimeVersion->IsSpriteCard();
}



//TODO: Investigate if these are likely to change at all when setting vars/flags
bool CMaterial_QueueFriendly::IsAlphaTested()
{
	return m_pRealTimeVersion->IsAlphaTested();
}

bool CMaterial_QueueFriendly::IsVertexLit()
{
	return m_pRealTimeVersion->IsVertexLit();
}

VertexFormat_t CMaterial_QueueFriendly::GetVertexFormat() const
{
	return m_pRealTimeVersion->GetVertexFormat();
}

bool CMaterial_QueueFriendly::UsesEnvCubemap( void )
{
	return m_pRealTimeVersion->UsesEnvCubemap();
}

bool CMaterial_QueueFriendly::NeedsTangentSpace( void )
{
	return m_pRealTimeVersion->NeedsTangentSpace();
}

bool CMaterial_QueueFriendly::NeedsSoftwareSkinning( void )
{
	return m_pRealTimeVersion->NeedsSoftwareSkinning();
}

int CMaterial_QueueFriendly::GetNumPasses( void )
{
	return m_pRealTimeVersion->GetNumPasses();
}

int CMaterial_QueueFriendly::GetTextureMemoryBytes( void )
{
	return m_pRealTimeVersion->GetTextureMemoryBytes();
}

bool CMaterial_QueueFriendly::NeedsLightmapBlendAlpha( void )
{
	return m_pRealTimeVersion->NeedsLightmapBlendAlpha();
}

bool CMaterial_QueueFriendly::NeedsSoftwareLighting( void )
{
	return m_pRealTimeVersion->NeedsSoftwareLighting();
}

MorphFormat_t CMaterial_QueueFriendly::GetMorphFormat() const
{
	return m_pRealTimeVersion->GetMorphFormat();
}

void CMaterial_QueueFriendly::GetLowResColorSample( float s, float t, float *color ) const
{
	if ( m_pRealTimeVersion )
		m_pRealTimeVersion->GetLowResColorSample( s, t, color );
	else
		color[ 0 ] = color[ 1 ] = color[ 2 ] = 0.0f;
}



IMaterialVar *CMaterial_QueueFriendly::FindVar( const char *varName, bool *found, bool complain )
{
	//TODO: return a queue friendly variable that can be get/set
	return m_pRealTimeVersion->FindVar( varName, found, complain );
}

IMaterialVar *CMaterial_QueueFriendly::FindVarFast( char const *pVarName, unsigned int *pToken )
{
	//TODO: return a queue friendly variable that can be get/set
	return m_pRealTimeVersion->FindVarFast( pVarName, pToken );
}

IMaterialVar **CMaterial_QueueFriendly::GetShaderParams( void )
{
	//TODO: return queue friendly variables that can be get/set
	return m_pRealTimeVersion->GetShaderParams();
}

void CMaterial_QueueFriendly::DecrementReferenceCount( void )
{
	Assert( ThreadInMainThread() );
	--m_nReferenceCount;
	QUEUE_MATERIAL_CALL( DecrementReferenceCount );
}

void CMaterial_QueueFriendly::DeleteIfUnreferenced()
{
	Assert( ThreadInMainThread() );
	if ( m_nReferenceCount > 0 )
		return;
		
	MaterialSystem()->RemoveMaterial( GetRealTimeVersion() );
	QUEUE_MATERIAL_CALL( DeleteIfUnreferenced );
}

void CMaterial_QueueFriendly::RecomputeStateSnapshots()
{
	QUEUE_MATERIAL_CALL( RecomputeStateSnapshots );
}


bool CMaterial_QueueFriendly::IsTranslucent()
{
	//TODO: need to base this as if the queued state is 100% up to date
	return m_pRealTimeVersion->IsTranslucentInternal( GetMaterialVarFlag( MATERIAL_VAR_IGNORE_ALPHA_MODULATION ) ? 1.0f : m_fAlphaModulationOnQueueCompletion );
}

bool CMaterial_QueueFriendly::NeedsPowerOfTwoFrameBufferTexture( bool bCheckSpecificToThisFrame )
{
	//bCheckSpecificToThisFrame scares me a bit.
	return m_pRealTimeVersion->NeedsPowerOfTwoFrameBufferTexture( bCheckSpecificToThisFrame );
}

bool CMaterial_QueueFriendly::NeedsFullFrameBufferTexture( bool bCheckSpecificToThisFrame )
{
	//bCheckSpecificToThisFrame scares me a bit.
	return m_pRealTimeVersion->NeedsFullFrameBufferTexture( bCheckSpecificToThisFrame );
}

void CMaterial_QueueFriendly::AlphaModulate( float alpha )
{
	QUEUE_MATERIAL_CALL( AlphaModulate, alpha );
	m_fAlphaModulationOnQueueCompletion = alpha;
}

void CMaterial_QueueFriendly::ColorModulate( float r, float g, float b )
{
	QUEUE_MATERIAL_CALL( ColorModulate, r, g, b );
	m_vColorModulationOnQueueCompletion.Init( r, g, b );
}

void CMaterial_QueueFriendly::SetMaterialVarFlag( MaterialVarFlags_t flag, bool on )
{
	QUEUE_MATERIAL_CALL( SetMaterialVarFlag, flag, on );
}
bool CMaterial_QueueFriendly::GetMaterialVarFlag( MaterialVarFlags_t flag ) const
{
	//TODO: somehow mix both queued and real time states
	return m_pRealTimeVersion->GetMaterialVarFlag( flag );
}

void CMaterial_QueueFriendly::SetShader( const char *pShaderName )
{
	//TODO: queue it and investigate, seems like a grenade.
	m_pRealTimeVersion->SetShader( pShaderName );
}

void CMaterial_QueueFriendly::SetShaderAndParams( KeyValues *pKeyValues )
{
	//TODO: queue it and investigate, seems like a grenade.
	m_pRealTimeVersion->SetShaderAndParams( pKeyValues );
}

const char *CMaterial_QueueFriendly::GetShaderName() const
{
	//TODO: return as if the queue is up to date. Someone could have set the shader very recently
	return m_pRealTimeVersion->GetShaderName();
}

void CMaterial_QueueFriendly::Refresh()
{
	//TODO: Investigate, this one seems like a grenade.
	m_pRealTimeVersion->Refresh();
	//QUEUE_MATERIAL_CALL( Refresh );
}

void CMaterial_QueueFriendly::RefreshPreservingMaterialVars()
{
	//TODO: Investigate, this one seems like a grenade.
	m_pRealTimeVersion->RefreshPreservingMaterialVars();
	//QUEUE_MATERIAL_CALL( RefreshPreservingMaterialVars );
}

void CMaterial_QueueFriendly::SetUseFixedFunctionBakedLighting( bool bEnable )
{
	QUEUE_MATERIAL_CALL( SetUseFixedFunctionBakedLighting, bEnable );
}

float CMaterial_QueueFriendly::GetAlphaModulation()
{
#ifdef USE_QUEUED_MATERIAL_CALLS
	return m_fAlphaModulationOnQueueCompletion;
#else
	return m_pRealTimeVersion->GetAlphaModulation();
#endif
}

void CMaterial_QueueFriendly::GetColorModulation( float *r, float *g, float *b )
{
#ifdef USE_QUEUED_MATERIAL_CALLS
	*r = m_vColorModulationOnQueueCompletion.x;
	*g = m_vColorModulationOnQueueCompletion.y;
	*b = m_vColorModulationOnQueueCompletion.z;
#else
	m_pRealTimeVersion->GetColorModulation( r, g, b );
#endif
}

void CMaterial_QueueFriendly::CallBindProxy( void *proxyData )
{
	//TODO: queue it? Investigate.
	return m_pRealTimeVersion->CallBindProxy( proxyData );
}

IMaterial *CMaterial_QueueFriendly::CheckProxyReplacement( void *proxyData )
{
	return m_pRealTimeVersion->CheckProxyReplacement( proxyData );
}

void CMaterial_QueueFriendly::PrecacheMappingDimensions()
{
	return m_pRealTimeVersion->PrecacheMappingDimensions();
}

void CMaterial_QueueFriendly::FindRepresentativeTexture()
{
	return m_pRealTimeVersion->FindRepresentativeTexture();
}

bool CMaterial_QueueFriendly::IsRealTimeVersion( void ) const
{
	return false;
}

IMaterialInternal *CMaterial_QueueFriendly::GetRealTimeVersion( void )
{
	return m_pRealTimeVersion;
}

IMaterialInternal *CMaterial_QueueFriendly::GetQueueFriendlyVersion( void )
{
	return this;
}


void CMaterial_QueueFriendly::UpdateToRealTime( void )
{
	m_fAlphaModulationOnQueueCompletion = m_pRealTimeVersion->GetAlphaModulation();
	m_pRealTimeVersion->GetColorModulation( &m_vColorModulationOnQueueCompletion.x,
											&m_vColorModulationOnQueueCompletion.y,
											&m_vColorModulationOnQueueCompletion.z );
	
	m_nReferenceCount = m_pRealTimeVersion->GetReferenceCount();
}


















