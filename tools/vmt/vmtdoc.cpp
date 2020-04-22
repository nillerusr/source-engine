//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "vmtdoc.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "datamodel/dmelement.h"
#include "vmttool.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/ishader.h"
#include "toolutils/enginetools_int.h"
#include "filesystem.h"


//-----------------------------------------------------------------------------
// Standard properties
//-----------------------------------------------------------------------------
struct StandardParam_t
{
	const char *m_pParamName;
	ShaderParamType_t m_ParamType;
	const char *m_pDefaultValue;
	const char *m_pWidgetType;
	const char *m_pTextType;
};

// NOTE: All entries in here must have all-lowercase param names!
static StandardParam_t g_pStandardParams[] = 
{
	{ "$surfaceprop", SHADER_PARAM_TYPE_STRING, "default", "surfacepropertypicker", "surfacePropertyName" },
	{ "%detailtype", SHADER_PARAM_TYPE_STRING, "", "detailtypepicker", "detailTypeName" },
	{ "%compilesky", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compilehint", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compileskip", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compileorigin", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compileclip", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%playerclip", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compilenpcclip", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compilenochop", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compiletrigger", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compilenolight", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compileplayercontrolclip", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compileladder", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compilewet", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compilenodraw", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compileinvisible", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compilenonsolid", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compiledetail", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compilewater", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ "%compileslime", SHADER_PARAM_TYPE_BOOL, "0", NULL, NULL },
	{ NULL, SHADER_PARAM_TYPE_BOOL, NULL, NULL, NULL }
};


//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CVMTDoc::CVMTDoc( IVMTDocCallback *pCallback ) : m_pCallback( pCallback )
{
	m_hRoot = NULL;
	m_pFileName[0] = 0;
	m_bDirty = false;
	m_pCurrentIShader = NULL;

	KeyValues *pKeyValues = new KeyValues( "Wireframe" );
	m_pScratchMaterial.Init( "VMT Preview", pKeyValues );
	g_pDataModel->InstallNotificationCallback( this );
}

CVMTDoc::~CVMTDoc()
{
	if ( m_hRoot.Get() )
	{
		RemoveAllShaderParams( m_hRoot );
	}
	g_pDataModel->RemoveNotificationCallback( this );
}


//-----------------------------------------------------------------------------
// Inherited from INotifyUI
//-----------------------------------------------------------------------------
void CVMTDoc::NotifyDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	OnDataChanged( pReason, nNotifySource, nNotifyFlags );
}

	
//-----------------------------------------------------------------------------
// Gets the file name
//-----------------------------------------------------------------------------
const char *CVMTDoc::GetFileName()
{
	return m_pFileName;
}

void CVMTDoc::SetFileName( const char *pFileName )
{
	Q_strncpy( m_pFileName, pFileName, sizeof( m_pFileName ) );
	Q_FixSlashes( m_pFileName );
	SetDirty( true );
}


//-----------------------------------------------------------------------------
// Dirty bits
//-----------------------------------------------------------------------------
void CVMTDoc::SetDirty( bool bDirty )
{
	m_bDirty = bDirty;
}

bool CVMTDoc::IsDirty() const
{
	return m_bDirty;
}


//-----------------------------------------------------------------------------
// Creates the root element
//-----------------------------------------------------------------------------
bool CVMTDoc::CreateRootElement()
{
	Assert( !m_hRoot.Get() );

	DmFileId_t fileid = g_pDataModel->FindOrCreateFileId( GetFileName() );

	// Create the main element
	m_hRoot = g_pDataModel->CreateElement( "DmElement", GetFileName(), fileid );
	if ( m_hRoot == DMELEMENT_HANDLE_INVALID )
		return false;

	g_pDataModel->SetFileRoot( fileid, m_hRoot );

	// Each VMT list needs to have an editortype associated with it so it displays nicely in editors
	m_hRoot->SetValue( "editorType", "vmt" );
	m_hRoot->AddAttribute( "proxies", AT_ELEMENT_ARRAY );
	m_hRoot->AddAttribute( "fallbacks", AT_ELEMENT_ARRAY );

	m_pCallback->RemoveAllToolParameters();

	// Add standard parameters
	for ( int i = 0; g_pStandardParams[i].m_pParamName; ++i )
	{
		AddNewShaderParam( m_hRoot, g_pStandardParams[i].m_pParamName, g_pStandardParams[i].m_ParamType, 
			g_pStandardParams[i].m_pDefaultValue );

		if ( g_pStandardParams[i].m_pParamName[0] == '%' )
		{
			m_pCallback->AddToolParameter( g_pStandardParams[i].m_pParamName, g_pStandardParams[i].m_pWidgetType, g_pStandardParams[i].m_pTextType );
		}
		else if ( g_pStandardParams[i].m_pWidgetType || g_pStandardParams[i].m_pTextType )
		{
			m_pCallback->RemoveShaderParameter( g_pStandardParams[i].m_pParamName ); 
			m_pCallback->AddShaderParameter( g_pStandardParams[i].m_pParamName, g_pStandardParams[i].m_pWidgetType, g_pStandardParams[i].m_pTextType );
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Creates a new VMT
//-----------------------------------------------------------------------------
void CVMTDoc::CreateNew()
{
	Assert( !m_hRoot.Get() );

	// This is not undoable
	CAppDisableUndoScopeGuard guard( "CVMTDoc::CreateNew", NOTIFY_CHANGE_OTHER );

	Q_strncpy( m_pFileName, "untitled", sizeof( m_pFileName ) );

	// Create the main element
	if ( !CreateRootElement() )
		return;

	m_pPreviewMaterial.Init( m_pScratchMaterial );

	SetShader( "wireframe" );
	SetDirty( false );
}


//-----------------------------------------------------------------------------
// Copies VMT parameters into the root
//-----------------------------------------------------------------------------
void CVMTDoc::CopyParamsFromVMT( CDmElement *pVMT )
{
	// First, set the shader parameters
	SetShader( pVMT->GetValueString( "shader" ) );

	// Now, copy the shader parameters over
	CDmAttribute* pSrc;
	CDmAttribute* pDst;
	for ( pSrc = pVMT->FirstAttribute(); pSrc; pSrc = pSrc->NextAttribute() )
	{
		// Only copy shader parameters
		if ( !IsShaderParam( pSrc ) )
			continue;

		// Adds the attribute if it doesn't exist
		const char *pSrcName = pSrc->GetName();
		if ( !m_hRoot->HasAttribute( pSrcName ) )
		{
			m_hRoot->AddAttribute( pSrcName, pSrc->GetType() );
		}
		pDst = m_hRoot->GetAttribute( pSrcName );
		pDst->AddFlag( FATTRIB_USERDEFINED );

		DmAttributeType_t srcType = pSrc->GetType();
		DmAttributeType_t dstType = pDst->GetType();
		if ( dstType == srcType )
		{
			pDst->SetValue( pSrc );
			continue;
		}

		// Certain type conversions are allowed
		switch( dstType )
		{
		case AT_BOOL:
			if ( srcType == AT_INT )
			{
				pDst->SetValue( pSrc );
			}
			break;

		case AT_INT:
			if ( srcType == AT_BOOL )
			{
				pDst->SetValue( pSrc );
			}
			break;

		case AT_COLOR:
			if ( srcType == AT_VECTOR3 )
			{
				Color c;
				int r, g, b;
				Vector v = pSrc->GetValue<Vector>( );
				v *= 255.0f;
				r = clamp( v[0], 0, 255 );
				g = clamp( v[1], 0, 255 );
				b = clamp( v[2], 0, 255 );
				c.SetColor( r, g, b, 255 );
				pDst->SetValue( c );
			}
			break;
		}
	}

	// Any shader parameter that isn't in the VMT make undefined
	for ( pDst = m_hRoot->FirstAttribute(); pDst; pDst = pDst->NextAttribute() )
	{
		if ( !IsShaderParam( pDst ) )
			continue;

		if ( !pVMT->HasAttribute( pDst->GetName() ) )
		{
			// Special hack for alpha + colors
			if ( !Q_stricmp( pDst->GetName(), "$alpha" ) )
			{
				pDst->SetValue( 1.0f );
			}
			else if ( pDst->GetType() == AT_COLOR )
			{
				Color c( 255, 255, 255, 255 );
				pDst->SetValue( c );
			}
			else
			{
				pDst->SetToDefaultValue();
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Hooks the preview to an existing material, if there is one
//-----------------------------------------------------------------------------
void CVMTDoc::SetupPreviewMaterial( )
{
	// Extract a material name from the material
	char pLocalName[MAX_PATH];

	// relative paths can be passed in for in-game material picking
	if ( !g_pFileSystem->FullPathToRelativePath( m_pFileName, pLocalName, sizeof(pLocalName) ) )
	{
		Q_strcpy( pLocalName, m_pFileName );
	}

	if ( Q_strnicmp( pLocalName, "materials", 9 ) )
		goto noMaterialConnection;

	// Skip the '/' also
	char pMaterialName[MAX_PATH];
	Q_StripExtension( pLocalName + 10, pMaterialName, sizeof(pMaterialName) );
	IMaterial *pMaterial = g_pMaterialSystem->FindMaterial( pMaterialName, "Editable material", false );
	if ( !pMaterial || pMaterial->IsErrorMaterial() )
		goto noMaterialConnection;

	m_pPreviewMaterial.Init( pMaterial );
	return;

noMaterialConnection:
	m_pPreviewMaterial.Init( m_pScratchMaterial );
}


//-----------------------------------------------------------------------------
// Saves/loads from file
//-----------------------------------------------------------------------------
bool CVMTDoc::LoadFromFile( const char *pFileName )
{
	Assert( !m_hRoot.Get() );

	SetDirty( false );

	Q_strncpy( m_pFileName, pFileName, sizeof( m_pFileName ) );
	if ( !m_pFileName[0] )
		return false;

	// This is not undoable
	CAppDisableUndoScopeGuard guard( "CVMTDoc::LoadFromFile", NOTIFY_CHANGE_OTHER );

	// Create the main element
	if ( !CreateRootElement() )
		return false;

	// change the filename of all the elements under the root, so we can unload the imported elements later
	DmFileId_t rootFileId = g_pDataModel->GetFileId( m_pFileName );
	g_pDataModel->SetFileName( rootFileId, "<temp>" );

	// This will allow us to edit in context!
	SetupPreviewMaterial( );

	CDmElement *pIVMT = NULL;
	g_pDataModel->RestoreFromFile( m_pFileName, NULL, "vmt", &pIVMT );
	CDmElement *pVMT = CastElement< CDmElement >( pIVMT );
	if ( !pVMT )
		return false;

	// FIXME: This is necessary so that all shader parameters appear in
	// the same order, with the same type, as what you'd get using File->New.
	// If we added a dependency to the material system into dmserializers,
	// we could avoid this work here (I think!).
	CopyParamsFromVMT( pVMT );

	// unload the imported elements and change the root's filename back
	DmFileId_t vmtFileId = g_pDataModel->GetFileId( m_pFileName );
	g_pDataModel->RemoveFileId( vmtFileId );
	g_pDataModel->SetFileName( rootFileId, m_pFileName );

	SetDirty( false );
	return true;
}

//-----------------------------------------------------------------------------
// Prior to saving to disk, extract all shader parameters which == the default
//-----------------------------------------------------------------------------
CDmElement* CVMTDoc::ExtractDefaultParameters( )
{					   
	CDmElement *pMaterial = m_hRoot->Copy( );

	CDmAttribute* pAttribute = pMaterial->FirstAttribute();
	CDmAttribute* pNextAttribute = NULL;
	for ( ; pAttribute; pAttribute = pNextAttribute )
	{
		pNextAttribute = pAttribute->NextAttribute();

		const char *pShaderParam = pAttribute->GetName();

		// Check for standard params
		int i;
		for ( i = 0; g_pStandardParams[i].m_pParamName != NULL; ++i )
		{
			if ( !Q_stricmp( g_pStandardParams[i].m_pParamName, pShaderParam ) )
			{
				char temp[512];
				CUtlBuffer buf( temp, sizeof(temp), CUtlBuffer::TEXT_BUFFER | CUtlBuffer::EXTERNAL_GROWABLE );
				pAttribute->Serialize( buf );

				if ( !Q_stricmp( (char*)buf.Base(), g_pStandardParams[i].m_pDefaultValue ) )
				{
					// Buffers match! Therefore it's still using the default parameter
					pMaterial->RemoveAttributeByPtr( pAttribute );
				}
				break;
			}
		}

		// Standard attribute found, continue
		if ( g_pStandardParams[i].m_pParamName )
			continue;

		// Only remove shader parameters
		if ( !IsShaderParam( pAttribute ) )
			continue;

		// Remove flags whose value is 0
		int nCount = g_pMaterialSystem->ShaderFlagCount();
		for ( i = 0; i < nCount; ++i )
		{
			const char *pFlagName = g_pMaterialSystem->ShaderFlagName( i );
			if ( !Q_stricmp( pShaderParam, pFlagName ) )
				break;
		}

		// It's a flag! Remove the attribute if its value is 0
		if ( i != nCount )
		{
			if ( pAttribute->GetValue<bool>( ) == 0 )
			{
				pMaterial->RemoveAttributeByPtr( pAttribute );
			}
			continue;
		}

		// FIXME: We can't do this.. the defaults in the strings need to be changed to 
		// make it so they actually match the true defaults
		continue;

		// Remove parameters which match the default value
		nCount = m_pCurrentIShader->GetNumParams();
		for ( i = 0; i < nCount; ++i )
		{
			// FIXME: Check type matches
			if ( Q_stricmp( pShaderParam, m_pCurrentIShader->GetParamName( i ) ) )
				continue;

			// NOTE: This isn't particularly efficient. Too bad!
			// It's hard to do efficiently owing to all the import conversion
			char temp[512];
			char temp2[512];
			CUtlBuffer buf( temp, sizeof(temp), CUtlBuffer::TEXT_BUFFER | CUtlBuffer::EXTERNAL_GROWABLE );
			CUtlBuffer buf2( temp2, sizeof(temp2), CUtlBuffer::TEXT_BUFFER | CUtlBuffer::EXTERNAL_GROWABLE );
			pAttribute->Serialize( buf );
			SetAttributeValueFromDefault( pMaterial, pAttribute, m_pCurrentIShader->GetParamDefault( i ) );
			pAttribute->Serialize( buf2 );

			if ( ( buf.TellMaxPut() == buf2.TellMaxPut() ) && !memcmp( buf.Base(), buf2.Base(), buf.TellMaxPut() ) )
			{
				// Buffers match! Therefore it's still using the default parameter
				pMaterial->RemoveAttributeByPtr( pAttribute );
			}
			else
			{
				// Restore the actual value
				pAttribute->Unserialize( buf );
			}
			break;
		}
	}

	return pMaterial;
}


//-----------------------------------------------------------------------------
// Saves to disk
//-----------------------------------------------------------------------------
bool CVMTDoc::SaveToFile( )
{
	if ( m_hRoot.Get() && m_pFileName && m_pFileName[0] )
	{
		CDisableUndoScopeGuard guard;
		CDmElement *pSaveRoot = ExtractDefaultParameters();
		bool bOk = g_pDataModel->SaveToFile( m_pFileName, NULL, "keyvalues", "vmt", pSaveRoot );
		DestroyElement( pSaveRoot, TD_DEEP );
		if ( !bOk )
			return false;
	}

	SetDirty( false );
	return true;
}


//-----------------------------------------------------------------------------
// Finds a shader
//-----------------------------------------------------------------------------
IShader *CVMTDoc::FindShader( const char *pShaderName )
{
	int nCount = g_pMaterialSystem->ShaderCount();
	IShader **ppShaderList = (IShader**)_alloca( nCount * sizeof(IShader*) );
	g_pMaterialSystem->GetShaders( 0, nCount, ppShaderList );
	for ( int i = 0; i < nCount; ++i )
	{
		if ( !Q_stricmp( pShaderName, ppShaderList[i]->GetName() ) )
			return ppShaderList[i];
	}
	return NULL;
}


//-----------------------------------------------------------------------------
// Is this attribute a shader parameter?
//-----------------------------------------------------------------------------
bool CVMTDoc::IsShaderParam( CDmAttribute* pAttribute )
{
	const char *pName = pAttribute->GetName();

	// Shader params start with a $ or %
	if ( pName[0] != '$' && pName[0] != '%' )
		return false;

	// Don't remove name, type, or id
	if ( pAttribute->IsFlagSet( FATTRIB_STANDARD ) )
		return false;

	// All shader params have USERDEFINED set
	if ( !pAttribute->IsFlagSet( FATTRIB_USERDEFINED ) )
		return false;

	// Don't remove arrays... those aren't shader parameters
	if ( pAttribute->GetType() == AT_ELEMENT_ARRAY )
		return false;

	// Standard params aren't counted here
	for ( int i = 0; g_pStandardParams[i].m_pParamName; ++i )
	{
		if ( !Q_stricmp( g_pStandardParams[i].m_pParamName, pName ) )
			return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Remove all shader parameters 
//-----------------------------------------------------------------------------
void CVMTDoc::RemoveAllShaderParams( CDmElement *pMaterial )
{
	CDmAttribute* pAttribute;
	CDmAttribute* pNextAttribute = NULL;
	for ( pAttribute = pMaterial->FirstAttribute(); pAttribute; pAttribute = pNextAttribute )
	{
		pNextAttribute = pAttribute->NextAttribute();

		// Only remove shader parameters
		if ( !IsShaderParam( pAttribute ) )
			continue;

		m_pCallback->RemoveShaderParameter( pAttribute->GetName() );
		pMaterial->RemoveAttributeByPtr( pAttribute );
	}
}
	

//-----------------------------------------------------------------------------
// Remove all shader parameters that don't exist in the new shader
//-----------------------------------------------------------------------------
void CVMTDoc::RemoveUnusedShaderParams( CDmElement *pMaterial, IShader *pShader, IShader *pOldShader )
{
	CDmAttribute* pAttribute = pMaterial->FirstAttribute();
	CDmAttribute* pNextAttribute = NULL;
	for ( ; pAttribute; pAttribute = pNextAttribute )
	{
		pNextAttribute = pAttribute->NextAttribute();

		// Only remove shader parameters
		if ( !IsShaderParam( pAttribute ) )
			continue;

		// Don't remove flags
		int nCount = g_pMaterialSystem->ShaderFlagCount();
		int i;
		for ( i = 0; i < nCount; ++i )
		{
			const char *pFlagName = g_pMaterialSystem->ShaderFlagName( i );
			if ( !Q_stricmp( pAttribute->GetName(), pFlagName ) )
				break;
		}

		if ( i != nCount )
			continue;

		const char *pShaderParam = pAttribute->GetName();

		// Remove parameters we've currently got but which don't exist in the new shader
		nCount = pShader->GetNumParams();
		for ( i = 0; i < nCount; ++i )
		{
			// FIXME: Check type matches
			if ( !Q_stricmp( pShaderParam, pShader->GetParamName( i ) ) )
				break;
		}

		// No match? Remove it!
		if ( i == nCount )
		{
			m_pCallback->RemoveShaderParameter( pAttribute->GetName() );
			pMaterial->RemoveAttributeByPtr( pAttribute );
			continue;
		}

		// Remove parameters from the old shader which match the default value
		// This will make the default values update to the new shader's defaults
		if ( pOldShader )
		{
			nCount = pOldShader->GetNumParams();
			for ( i = 0; i < nCount; ++i )
			{
				// FIXME: Check type matches
				if ( Q_stricmp( pShaderParam, pOldShader->GetParamName( i ) ) )
					continue;

				// NOTE: This isn't particularly efficient. Too bad!
				// It's hard to do efficiently owing to all the import conversion
				char temp1[512];
				char temp2[512];
				CUtlBuffer buf1( temp1, sizeof(temp1), CUtlBuffer::TEXT_BUFFER | CUtlBuffer::EXTERNAL_GROWABLE );
				CUtlBuffer buf2( temp2, sizeof(temp2), CUtlBuffer::TEXT_BUFFER | CUtlBuffer::EXTERNAL_GROWABLE );
				pAttribute->Serialize( buf1 );
				SetAttributeValueFromDefault( pMaterial, pAttribute, pOldShader->GetParamDefault( i ) );
				pAttribute->Serialize( buf2 );

				if ( ( buf1.TellMaxPut() == buf2.TellMaxPut() ) && !memcmp( buf1.Base(), buf2.Base(), buf1.TellMaxPut() ) )
				{
					// Buffers match! Therefore it's still using the default parameter
					m_pCallback->RemoveShaderParameter( pAttribute->GetName() );
					pMaterial->RemoveAttributeByPtr( pAttribute );
				}
				else
				{
					pAttribute->Unserialize( buf1 );
				}
				break;
			}
		}
	}
}


//-----------------------------------------------------------------------------
// Add attribute for shader parameter
//-----------------------------------------------------------------------------
CDmAttribute* CVMTDoc::AddAttributeForShaderParameter( CDmElement *pMaterial, const char *pParamName, ShaderParamType_t paramType )
{
	CDmAttribute *pAttribute = NULL;
	switch ( paramType )
	{
	case SHADER_PARAM_TYPE_INTEGER:
		pAttribute = pMaterial->AddAttribute( pParamName, AT_INT );
		break;

	case SHADER_PARAM_TYPE_BOOL:
		pAttribute = pMaterial->AddAttribute( pParamName, AT_BOOL );
		break;

 	case SHADER_PARAM_TYPE_FLOAT:
		pAttribute = pMaterial->AddAttribute( pParamName, AT_FLOAT );
		break;

	case SHADER_PARAM_TYPE_STRING:
		pAttribute = pMaterial->AddAttribute( pParamName, AT_STRING );
		break;

	case SHADER_PARAM_TYPE_COLOR:
		pAttribute = pMaterial->AddAttribute( pParamName, AT_COLOR );
		break;

	case SHADER_PARAM_TYPE_VEC2:
		pAttribute = pMaterial->AddAttribute( pParamName, AT_VECTOR2 );
		break;

	case SHADER_PARAM_TYPE_VEC3:
		pAttribute = pMaterial->AddAttribute( pParamName, AT_VECTOR3 );
		break;

	case SHADER_PARAM_TYPE_VEC4:
		pAttribute = pMaterial->AddAttribute( pParamName, AT_VECTOR4 );
		break;

	case SHADER_PARAM_TYPE_FOURCC:
		Assert( 0 );
		break;

	case SHADER_PARAM_TYPE_MATRIX:
		pAttribute = pMaterial->AddAttribute( pParamName, AT_VMATRIX );
		break;

	case SHADER_PARAM_TYPE_TEXTURE:
		pAttribute = pMaterial->AddAttribute( pParamName, AT_STRING );
		m_pCallback->AddShaderParameter( pParamName, "vtfpicker", "vtfName" );
		break;

	case SHADER_PARAM_TYPE_MATERIAL:
		pAttribute = pMaterial->AddAttribute( pParamName, AT_STRING );
		m_pCallback->AddShaderParameter( pParamName, "vmtpicker", "vmtName" );
		break;

	default:
		break;
	}

	if ( pAttribute )
	{
		pAttribute->AddFlag( FATTRIB_USERDEFINED );
	}

	return pAttribute;
}


//-----------------------------------------------------------------------------
// A couple methods to set vmatrix param values from strings (OLD METHOD!)
//-----------------------------------------------------------------------------
bool CVMTDoc::SetVMatrixParamValue( CDmAttribute *pAttribute, const char *pValue )
{
	// FIXME: Change default strings to match DME?
	// Then we could remove this crap
	VMatrix mat;
	int count = sscanf( pValue, " [ %f %f %f %f  %f %f %f %f  %f %f %f %f  %f %f %f %f ]",
		&mat.m[0][0], &mat.m[0][1], &mat.m[0][2], &mat.m[0][3],
		&mat.m[1][0], &mat.m[1][1], &mat.m[1][2], &mat.m[1][3],
		&mat.m[2][0], &mat.m[2][1], &mat.m[2][2], &mat.m[2][3],
		&mat.m[3][0], &mat.m[3][1], &mat.m[3][2], &mat.m[3][3] );
	if (count == 16)
	{
		pAttribute->SetValue( mat );
		return true;
	}

	Vector2D scale, center;
	float angle;
	Vector2D translation;
	count = sscanf( pValue, " center %f %f scale %f %f rotate %f translate %f %f",
		&center.x, &center.y, &scale.x, &scale.y, &angle, &translation.x, &translation.y );
	if (count != 7)
		return false;

	VMatrix temp;
	MatrixBuildTranslation( mat, -center.x, -center.y, 0.0f );
	MatrixBuildScale( temp, scale.x, scale.y, 1.0f );
	MatrixMultiply( temp, mat, mat );
	MatrixBuildRotateZ( temp, angle );
	MatrixMultiply( temp, mat, mat );
	MatrixBuildTranslation( temp, center.x + translation.x, center.y + translation.y, 0.0f );
	MatrixMultiply( temp, mat, mat );
	pAttribute->SetValue( mat );
	return true;
}


//-----------------------------------------------------------------------------
// A couple methods to set vmatrix param values from strings (OLD METHOD!)
//-----------------------------------------------------------------------------
bool CVMTDoc::SetVector2DParamValue( CDmAttribute *pAttribute, const char *pValue )
{
	Vector2D vec;
	int count = sscanf( pValue, " [ %f %f ]", &vec[0], &vec[1] );
	if ( count == 2 )
	{
		pAttribute->SetValue( vec );
		return true;
	}

	count = sscanf( pValue, " { %f %f }", &vec[0], &vec[1] );
	if ( count == 2 )
	{
		vec /= 255.0f;
		pAttribute->SetValue( vec );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// A couple methods to set vmatrix param values from strings (OLD METHOD!)
//-----------------------------------------------------------------------------
bool CVMTDoc::SetVector3DParamValue( CDmAttribute *pAttribute, const char *pValue )
{
	Vector vec;
	int count = sscanf( pValue, " [ %f %f %f ]", &vec[0], &vec[1], &vec[2] );
	if ( count == 3 )
	{
		pAttribute->SetValue( vec );
		return true;
	}

	count = sscanf( pValue, " { %f %f %f }", &vec[0], &vec[1], &vec[2] );
	if ( count == 3 )
	{
		vec /= 255.0f;
		pAttribute->SetValue( vec );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// A couple methods to set vmatrix param values from strings (OLD METHOD!)
//-----------------------------------------------------------------------------
bool CVMTDoc::SetVector4DParamValue( CDmAttribute *pAttribute, const char *pValue )
{
	Vector4D vec;
	int count = sscanf( pValue, " [ %f %f %f %f ]", &vec[0], &vec[1], &vec[2], &vec[3] );
	if ( count == 4 )
	{
		pAttribute->SetValue( vec );
		return true;
	}

	count = sscanf( pValue, " { %f %f %f %f }", &vec[0], &vec[1], &vec[2], &vec[3] );
	if ( count == 4 )
	{
		vec /= 255.0f;
		pAttribute->SetValue( vec );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// A couple methods to set vmatrix param values from strings (OLD METHOD!)
//-----------------------------------------------------------------------------
bool CVMTDoc::SetColorParamValue( CDmAttribute *pAttribute, const char *pValue )
{
	Color c;
	int r, g, b;
	Vector vec;
	int count = sscanf( pValue, " [ %f %f %f ]", &vec[0], &vec[1], &vec[2] );
	if ( count == 3 )
	{
		vec *= 255.0f;
		r = clamp( vec[0], 0, 255 );
		g = clamp( vec[1], 0, 255 );
		b = clamp( vec[2], 0, 255 );
		c.SetColor( r, g, b, 255 );
		pAttribute->SetValue( c );
		return true;
	}

	count = sscanf( pValue, " { %d %d %d }", &r, &g, &b );
	if ( count == 3 )
	{
		c.SetColor( r, g, b, 255 );
		pAttribute->SetValue( c );
		return true;
	}

	return false;
}


//-----------------------------------------------------------------------------
// Sets an attribute value from the shader param default
//-----------------------------------------------------------------------------
void CVMTDoc::SetAttributeValueFromDefault( CDmElement *pMaterial, CDmAttribute *pAttribute, const char *pValue )
{
	// FIXME: Change default strings to match DME?
	// Then we could remove this crap
	switch ( pAttribute->GetType() )
	{
	case AT_VMATRIX:
		if ( SetVMatrixParamValue( pAttribute, pValue ) )
			return;
		break;
	case AT_COLOR:
		if ( SetColorParamValue( pAttribute, pValue ) )
			return;
		break;
	case AT_VECTOR2:
		if ( SetVector2DParamValue( pAttribute, pValue ) )
			return;
		break;
	case AT_VECTOR3:
		if ( SetVector3DParamValue( pAttribute, pValue ) )
			return;
		break;
	case AT_VECTOR4:
		if ( SetVector4DParamValue( pAttribute, pValue ) )
			return;
		break;
	}

	pMaterial->SetValueFromString( pAttribute->GetName(), pValue );
}


//-----------------------------------------------------------------------------
// Add a single shader parameter if it doesn't exist
//-----------------------------------------------------------------------------
void CVMTDoc::AddNewShaderParam( CDmElement *pMaterial, const char *pParamName, ShaderParamType_t paramType, const char *pValue )
{
	char temp[512];
	Q_strncpy( temp, pParamName, sizeof(temp) );
	Q_strlower( temp );
	pParamName = temp;

	CDmAttribute* pAttribute = NULL;
	for ( pAttribute = pMaterial->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		// Don't bother testing against name, type, or id
		if ( pAttribute->IsFlagSet( FATTRIB_STANDARD ) )
			continue;

		const char *pAttributeName = pAttribute->GetName();
		if ( !Q_stricmp( pAttributeName, pParamName ) )
			return;
	}

	// No match? Add it!
	pAttribute = AddAttributeForShaderParameter( pMaterial, pParamName, paramType );
	if ( pAttribute )
	{
		SetAttributeValueFromDefault( pMaterial, pAttribute, pValue );
	}
}


//-----------------------------------------------------------------------------
// Add all shader parameters that don't currently exist
//-----------------------------------------------------------------------------
void CVMTDoc::AddNewShaderParams( CDmElement *pMaterial, IShader *pShader )
{
	// First add all flags
	m_pCallback->RemoveAllFlagParameters();
	int nCount = g_pMaterialSystem->ShaderFlagCount();
 	int i;
	for ( i = 0; i < nCount; ++i )
	{
		const char *pParamName = g_pMaterialSystem->ShaderFlagName( i );
		AddNewShaderParam( pMaterial, pParamName, SHADER_PARAM_TYPE_BOOL, "0" );
		m_pCallback->AddFlagParameter( pParamName );
	}

	// Next add all shader-specific parameters
	nCount = pShader->GetNumParams();
	for ( i = 0; i < nCount; ++i )
	{
		const char *pParamName = pShader->GetParamName( i );

		// Don't add parameters that don't want to be editable
		if ( pShader->GetParamFlags( i ) & SHADER_PARAM_NOT_EDITABLE )
			continue;

		ShaderParamType_t paramType = pShader->GetParamType( i );
		const char *pDefault = pShader->GetParamDefault( i );
		AddNewShaderParam( pMaterial, pParamName, paramType, pDefault );
	}
}


//-----------------------------------------------------------------------------
// Sets shader parameters to the default for that shader
//-----------------------------------------------------------------------------
void CVMTDoc::SetParamsToDefault()
{
	// This is undoable
	CAppUndoScopeGuard guard( 0, "Set Params to Default", "Set Params to Default" );

	// Next add all shader-specific parameters
	int nCount = m_pCurrentIShader->GetNumParams();
	for ( int i = 0; i < nCount; ++i )
	{
		const char *pParamName = m_pCurrentIShader->GetParamName( i );

		// Don't set parameters that don't want to be editable
		if ( m_pCurrentIShader->GetParamFlags( i ) & SHADER_PARAM_NOT_EDITABLE )
			continue;

		char pAttributeName[512];
		Q_strncpy( pAttributeName, pParamName, sizeof(pAttributeName) );
		Q_strlower( pAttributeName );

		if ( !m_hRoot->HasAttribute( pAttributeName ) )
			continue;

		CDmAttribute *pAttribute = m_hRoot->GetAttribute( pAttributeName );
		const char *pDefault = m_pCurrentIShader->GetParamDefault( i );
		SetAttributeValueFromDefault( m_hRoot, pAttribute, pDefault );
	}
}

	
//-----------------------------------------------------------------------------
// Sets the shader in the material
//-----------------------------------------------------------------------------
void CVMTDoc::SetShader( const char *pShaderName )
{
	// No change? don't bother
	if ( !Q_stricmp( m_CurrentShader, pShaderName ) )
		return;

	m_CurrentShader = pShaderName;

	// This is undoable
	CAppUndoScopeGuard guard( NOTIFY_SETDIRTYFLAG, "Set Shader", "Set Shader" );

	char pActualShaderName[512];
	g_pMaterialSystem->GetShaderFallback( pShaderName, pActualShaderName, sizeof(pActualShaderName) );

	m_hRoot->SetValue( "shader", pShaderName );

	// First, find the shader
	IShader *pShader = FindShader( pActualShaderName );

	// Remove all shader parameters that don't exist in the new shader
	// And also remove shader parameters that do match the default value
	RemoveUnusedShaderParams( m_hRoot, pShader, m_pCurrentIShader );

	// Add all shader parameters that don't currently exist
	AddNewShaderParams( m_hRoot, pShader );

	m_pCurrentIShader = pShader;
}


//-----------------------------------------------------------------------------
// Gets the preview material
//-----------------------------------------------------------------------------
IMaterial *CVMTDoc::GetPreviewMaterial()
{
	return m_pPreviewMaterial;
}

	
//-----------------------------------------------------------------------------
// Updates the preview material
//-----------------------------------------------------------------------------
void CVMTDoc::UpdatePreviewMaterial()
{
	if ( !m_hRoot.Get() )
		return;

	// Update all shader parameters
	SetShader( m_hRoot->GetValueString( "shader" ) );

	// Use the file conversion to write to a text format
	char buf[1024];
	CUtlBuffer vmtBuf( buf, sizeof(buf), CUtlBuffer::TEXT_BUFFER | CUtlBuffer::EXTERNAL_GROWABLE );
	g_pDataModel->Serialize( vmtBuf, "vmt", "vmt", m_hRoot );

	// Now use the text format to create a keyvalues
	KeyValues *pVMTKeyValues = new KeyValues( "ShaderName" );
	pVMTKeyValues->LoadFromBuffer( "VMT Preview", vmtBuf, g_pFileSystem, "GAME" );

	// Finally, hook the keyvalues into the material.
	m_pPreviewMaterial->SetShaderAndParams( pVMTKeyValues );
	pVMTKeyValues->deleteThis();
}

	
//-----------------------------------------------------------------------------
// Returns the root object
//-----------------------------------------------------------------------------
CDmElement *CVMTDoc::GetRootObject()
{
	return m_hRoot;
}

	
//-----------------------------------------------------------------------------
// Called when data changes
//-----------------------------------------------------------------------------
void CVMTDoc::OnDataChanged( const char *pReason, int nNotifySource, int nNotifyFlags )
{
	SetDirty( nNotifyFlags & NOTIFY_SETDIRTYFLAG ? true : false );
	UpdatePreviewMaterial();
	m_pCallback->OnDocChanged( pReason, nNotifySource, nNotifyFlags );
}
