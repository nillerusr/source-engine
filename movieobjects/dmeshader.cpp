//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================
#include "movieobjects/dmeshader.h"
#include "datamodel/dmelementfactoryhelper.h"
#include "movieobjects_interfaces.h"

#include "materialsystem/IShader.h"
#include "materialsystem/imaterialsystem.h"


//-----------------------------------------------------------------------------
// Expose this class to the scene database 
//-----------------------------------------------------------------------------
IMPLEMENT_ELEMENT_FACTORY( DmeShader, CDmeShader );


//-----------------------------------------------------------------------------
// Constructor, destructor
//-----------------------------------------------------------------------------
void CDmeShader::OnConstruction()
{
	m_ShaderName.Init( this, "shaderName" );

	m_ShaderName = "wireframe";
	m_pShader = NULL;
}

void CDmeShader::OnDestruction()
{
}


//-----------------------------------------------------------------------------
// Shader name access
//-----------------------------------------------------------------------------
void CDmeShader::SetShaderName( const char *pShaderName )
{
	m_ShaderName = pShaderName;
}

const char *CDmeShader::GetShaderName() const
{
	return m_ShaderName;
}


//-----------------------------------------------------------------------------
// Finds a shader
//-----------------------------------------------------------------------------
IShader *CDmeShader::FindShader()
{
	int nCount = MaterialSystem()->ShaderCount();
	IShader **ppShaderList = (IShader**)_alloca( nCount * sizeof(IShader*) );
	MaterialSystem()->GetShaders( 0, nCount, ppShaderList );
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( m_ShaderName, ppShaderList[i]->GetName() ) )
			return ppShaderList[i];
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Remove all shader parameters that don't exist in the new shader
//-----------------------------------------------------------------------------
void CDmeShader::RemoveUnusedShaderParams( IShader *pShader )
{
	IDmAttribute* pAttribute = FirstAttribute();
	IDmAttribute* pNextAttribute = NULL;
	for ( ; pAttribute; pAttribute = pNextAttribute )
	{
		pNextAttribute = pAttribute->NextAttribute();

		// Don't remove name, type, or id
		if ( pAttribute->IsFlagSet( FATTRIB_STANDARD ) )
			continue;

		const char *pShaderParam = pAttribute->GetName();
		int nCount = pShader->GetNumParams();
		int i;
		for ( i = 0; i < nCount; ++i )
		{
			if ( !Q_stricmp( pShaderParam, pShader->GetParamName( i ) ) )
				break;
		}

		// No match? Remove it!
		if ( i == nCount )
		{
			RemoveAttributeByPtr( pAttribute );
		}
	}
}


//-----------------------------------------------------------------------------
// Add attribute for shader parameter
//-----------------------------------------------------------------------------
IDmAttribute* CDmeShader::AddAttributeForShaderParameter( IShader *pShader, int nIndex )
{
	ShaderParamType_t paramType = pShader->GetParamType( nIndex );
	const char *pParamName = pShader->GetParamName( nIndex );

	IDmAttribute *pAttribute = NULL;
	switch ( paramType )
	{
	case SHADER_PARAM_TYPE_INTEGER:
		pAttribute = AddAttributeTyped<int>( pParamName );
		break;

	case SHADER_PARAM_TYPE_BOOL:
		pAttribute = AddAttributeTyped<bool>( pParamName );
		break;

 	case SHADER_PARAM_TYPE_FLOAT:
		pAttribute = AddAttributeTyped<float>( pParamName );
		break;

	case SHADER_PARAM_TYPE_STRING:
		pAttribute = AddAttributeTyped<CUtlString>( pParamName );
		break;

	case SHADER_PARAM_TYPE_COLOR:
		pAttribute = AddAttributeTyped<Color>( pParamName );
		break;

	case SHADER_PARAM_TYPE_VEC2:
		pAttribute = AddAttributeTyped<Vector2D>( pParamName );
		break;

	case SHADER_PARAM_TYPE_VEC3:
		pAttribute = AddAttributeTyped<Vector>( pParamName );
		break;

	case SHADER_PARAM_TYPE_VEC4:
		pAttribute = AddAttributeTyped<Vector4D>( pParamName );
		break;

	case SHADER_PARAM_TYPE_FOURCC:
		Assert( 0 );
		break;

	case SHADER_PARAM_TYPE_MATRIX:
		pAttribute = AddAttributeTyped<VMatrix>( pParamName );
		break;

	case SHADER_PARAM_TYPE_TEXTURE:
		pAttribute = AddAttributeTyped<CDmElementRef>( pParamName );
		break;

	case SHADER_PARAM_TYPE_MATERIAL:
		pAttribute = AddAttributeTyped<CDmElementRef>( pParamName );
		break;

	default:
		break;
	}
	return pAttribute;
}


//-----------------------------------------------------------------------------
// Add all shader parameters that don't currently exist
//-----------------------------------------------------------------------------
void CDmeShader::AddNewShaderParams( IShader *pShader )
{
	int nCount = pShader->GetNumParams();
	int i;
	for ( i = 0; i < nCount; ++i )
	{
		const char *pParamName = pShader->GetParamName( i );

		IDmAttribute* pAttribute = NULL;
		for ( pAttribute = FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
		{
			// Don't remove name, type, or id
			if ( pAttribute->IsFlagSet( FATTRIB_STANDARD ) )
				continue;

			const char *pAttributeName = pAttribute->GetName();
			if ( !Q_stricmp( pAttributeName, pParamName ) )
				break;
		}

		// No match? Add it!
		if ( pAttribute != NULL )
			continue;

		pAttribute = AddAttributeForShaderParameter( pShader, i );
		if ( pAttribute )
		{
			const char *pDefault = pShader->GetParamDefault( i );

			SetAttributeValueFromString( pParamName, pDefault );
		}
	}
}


//-----------------------------------------------------------------------------
// resolve
//-----------------------------------------------------------------------------
void CDmeShader::Resolve()
{
	if ( !m_ShaderName.IsDirty() || !MaterialSystem() )
		return;

	// First, find the shader
	IShader *pShader = FindShader();

	// Remove all shader parameters that don't exist in the new shader
	RemoveUnusedShaderParams( pShader );

	// Add all shader parameters that don't currently exist
	AddNewShaderParams( pShader );
}


//-----------------------------------------------------------------------------
// Returns a procedural material to be associated with this shader
//-----------------------------------------------------------------------------
void CDmeShader::CreateMaterial( const char *pMaterialName )
{
	KeyValues *pVMTKeyValues = new KeyValues( GetShaderName() );

	IDmAttribute* pAttribute = FirstAttribute();
	IDmAttribute* pNextAttribute = NULL;
	for ( ; pAttribute; pAttribute = pNextAttribute )
	{
		pNextAttribute = pAttribute->NextAttribute();

		// Don't remove name, type, or id
		if ( pAttribute->IsFlagSet( FATTRIB_STANDARD ) )
			continue;

		const char *pShaderParam = pAttribute->GetName();
		int nCount = pShader->GetNumParams();
		int i;
		for ( i = 0; i < nCount; ++i )
		{
			if ( !Q_stricmp( pShaderParam, pShader->GetParamName( i ) ) )
				break;
		}

		// No match? Remove it!
		if ( i == nCount )
		{
			RemoveAttributeByPtr( pAttribute );
		}
	}

	pVMTKeyValues->SetInt( "$model", 1 );
	pVMTKeyValues->SetFloat( "$decalscale", 0.05f );
	pVMTKeyValues->SetString( "$basetexture", "error" );
	return MaterialSystem()->CreateMaterial( pMaterialName, pVMTKeyValues );
}
