//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "materialsystem/imaterialproxy.h"
#include "materialsystem/imaterialvar.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/itexture.h"
#include "toolframework_client.h"
#include "portalrenderable_flatbasic.h"
#include <KeyValues.h>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CPortalPickAlphaMaskProxy : public IMaterialProxy
{
private:
	IMaterialVar *m_AlphaMaskTextureOutput;
	IMaterialVar *m_AlphaMaskTextureFrame;
	ITexture *m_pOpeningTexture;
	ITexture *m_pIdleTexture;

public:
	CPortalPickAlphaMaskProxy( void );
	~CPortalPickAlphaMaskProxy( void );
	virtual bool Init( IMaterial *pMaterial, KeyValues *pKeyValues );
	virtual void OnBind( void *pBind );
	virtual void Release( void ) { delete this; }
	virtual IMaterial *	GetMaterial() 
	{ 
		if ( m_AlphaMaskTextureOutput )
			return m_AlphaMaskTextureOutput->GetOwningMaterial();
		if ( m_AlphaMaskTextureFrame )
			return m_AlphaMaskTextureFrame->GetOwningMaterial();
		return NULL; 
	}
};

CPortalPickAlphaMaskProxy::CPortalPickAlphaMaskProxy( void )
: m_pOpeningTexture( NULL ), m_pIdleTexture( NULL )
{

}

CPortalPickAlphaMaskProxy::~CPortalPickAlphaMaskProxy( void )
{
	if( m_pOpeningTexture )
	{
		m_pOpeningTexture->DecrementReferenceCount();
		m_pOpeningTexture = NULL;
	}

	if( m_pIdleTexture )
	{
		m_pIdleTexture->DecrementReferenceCount();
		m_pIdleTexture = NULL;
	}
}

bool CPortalPickAlphaMaskProxy::Init( IMaterial *pMaterial, KeyValues *pKeyValues )
{
	char const* pszResultVar = pKeyValues->GetString( "maskTextureVar" );
	if( !pszResultVar )
		return false;

	char const* pszFrameVar = pKeyValues->GetString( "maskFrameVar" );
	if( !pszFrameVar )
		return false;

	char const* pszOpeningTextureInput = pKeyValues->GetString( "openingTexture" );
	if( !pszOpeningTextureInput )
		return false;

	char const* pszIdleTextureInput = pKeyValues->GetString( "idleTexture" );
	if( !pszIdleTextureInput )
		return false;

	bool foundVar;
	m_AlphaMaskTextureOutput = pMaterial->FindVar( pszResultVar, &foundVar, false );
	if( !foundVar )
		return false;

	m_AlphaMaskTextureFrame = pMaterial->FindVar( pszFrameVar, &foundVar, false );
	if( !foundVar )
		return false;


	m_pOpeningTexture = materials->FindTexture( pszOpeningTextureInput, TEXTURE_GROUP_CLIENT_EFFECTS );
	m_pOpeningTexture->IncrementReferenceCount();
	m_pIdleTexture = materials->FindTexture( pszIdleTextureInput, TEXTURE_GROUP_CLIENT_EFFECTS );	
	m_pIdleTexture->IncrementReferenceCount();
	return true;
}

void CPortalPickAlphaMaskProxy::OnBind( void *pBind )
{
	if( pBind == NULL )
		return;

	IClientRenderable *pRenderable = (IClientRenderable*)( pBind );

	CPortalRenderable_FlatBasic *pFlatBasic = dynamic_cast<CPortalRenderable_FlatBasic*>( pRenderable );

	if ( !pFlatBasic )
		return;

	if( pFlatBasic->m_fOpenAmount == 1.0f )
	{
		m_AlphaMaskTextureOutput->SetTextureValue( m_pIdleTexture );
	}
	else
	{
		m_AlphaMaskTextureOutput->SetTextureValue( m_pOpeningTexture );
		m_AlphaMaskTextureFrame->SetIntValue( pFlatBasic->m_fOpenAmount * m_pOpeningTexture->GetNumAnimationFrames() );
	}

	if ( ToolsEnabled() )
	{
		ToolFramework_RecordMaterialParams( GetMaterial() );
	}
}

EXPOSE_INTERFACE( CPortalPickAlphaMaskProxy, IMaterialProxy, "PortalPickAlphaMask" IMATERIAL_PROXY_INTERFACE_VERSION );