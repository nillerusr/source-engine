//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "importkeyvaluebase.h"
#include "dmserializers.h"
#include "datamodel/idatamodel.h"
#include "datamodel/dmelement.h"
#include "tier1/KeyValues.h"
#include "tier1/utlbuffer.h"
#include "datamodel/dmattribute.h"
#include "filesystem.h"
#include "tier2/tier2.h"


//-----------------------------------------------------------------------------
// Serialization class for Key Values
//-----------------------------------------------------------------------------
class CImportVMT : public CImportKeyValueBase
{
public:
	virtual const char *GetName() const { return "vmt"; }
	virtual const char *GetDescription() const { return "Valve Material File"; }
	virtual int GetCurrentVersion() const { return 0; } // doesn't store a version

	bool Serialize( CUtlBuffer &outBuf, CDmElement *pRoot );
	CDmElement* UnserializeFromKeyValues( KeyValues *pKeyValues );

private:
	// Unserialize fallbacks
	bool UnserializeFallbacks( CDmElement *pRoot, KeyValues *pFallbackKeyValues );

	// Unserialize proxies
	bool UnserializeProxies( CDmElement *pRoot, KeyValues *pKeyValues );

	// Creates a shader parameter from a key value
	bool UnserializeShaderParam( CDmElement *pRoot, KeyValues* pKeyValue );

	// Creates a matrix material var
	bool CreateMatrixMaterialVarFromKeyValue( CDmElement *pRoot, const char *pParamName, const char *pString );

	// Creates a vector shader parameter
	bool CreateVectorMaterialVarFromKeyValue( CDmElement *pRoot, const char *pParamName, const char *pString );

	// Writes out a single shader parameter
	bool SerializeShaderParameter( CUtlBuffer &buf, CDmAttribute *pAttribute );

	// Writes out all shader parameters
	bool SerializeShaderParameters( CUtlBuffer &buf, CDmElement *pRoot );

	// Writes out all shader fallbacks
	bool SerializeFallbacks( CUtlBuffer &buf, CDmElement *pRoot );

	// Writes out all material proxies
	bool SerializeProxies( CUtlBuffer &buf, CDmElement *pRoot );

	// Handle patch files
	void ExpandPatchFile( KeyValues *pKeyValues );
};


//-----------------------------------------------------------------------------
// Singleton instance
//-----------------------------------------------------------------------------
static CImportVMT s_ImportVMT;

void InstallVMTImporter( IDataModel *pFactory )
{
	pFactory->AddSerializer( &s_ImportVMT );
}


//-----------------------------------------------------------------------------
// Writes out a single shader parameter
//-----------------------------------------------------------------------------
bool CImportVMT::SerializeShaderParameter( CUtlBuffer &buf, CDmAttribute *pAttribute )
{
	// We have a shader parameter at this point.
	switch ( pAttribute->GetType() )
	{
	case AT_INT:
		buf.Printf( "\"%s\" \"%d\"\n", pAttribute->GetName(), pAttribute->GetValue<int>( ) );
		break;

	case AT_BOOL:
		buf.Printf( "\"%s\" \"%d\"\n", pAttribute->GetName(), pAttribute->GetValue<bool>( ) );
		break;

	case AT_FLOAT:
		buf.Printf( "\"%s\" \"%f\"\n", pAttribute->GetName(), pAttribute->GetValue<float>( ) );
		break;

	case AT_STRING:
		buf.Printf( "\"%s\" \"%s\"\n", pAttribute->GetName(), pAttribute->GetValue<CUtlString>( ).Get() );
		break;

	case AT_VECTOR2:
		{
			const Vector2D &vec = pAttribute->GetValue<Vector2D>( );
			buf.Printf( "\"%s\" \"[ %f %f ]\"\n", pAttribute->GetName(), vec.x, vec.y );
		}
		break;

	case AT_VECTOR3:
		{
			const Vector &vec = pAttribute->GetValue<Vector>( );
			buf.Printf( "\"%s\" \"[ %f %f %f ]\"\n", pAttribute->GetName(), vec.x, vec.y, vec.z );
		}
		break;

	case AT_VECTOR4:
		{
			const Vector4D &vec = pAttribute->GetValue<Vector4D>( );
			buf.Printf( "\"%s\" \"[ %f %f %f %f ]\"\n", pAttribute->GetName(), vec.x, vec.y, vec.z, vec.w );
		}
		break;

	case AT_COLOR:
		{
			// NOTE: VMTs only support 3 component color (no alpha)
			const Color &color = pAttribute->GetValue<Color>( );
			buf.Printf( "\"%s\" \"{ %d %d %d }\"\n", pAttribute->GetName(), color.r(), color.g(), color.b() );
		}
		break;

	case AT_VMATRIX:
		{
			const VMatrix &mat = pAttribute->GetValue<VMatrix>( );
			buf.Printf( "\"%s\" \"[ %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f ]\"\n", pAttribute->GetName(), 
				mat[0][0], mat[0][1], mat[0][2], mat[0][3],
				mat[1][0], mat[1][1], mat[1][2], mat[1][3],
				mat[2][0], mat[2][1], mat[2][2], mat[2][3],
				mat[3][0], mat[3][1], mat[3][2], mat[3][3] );
		}
		break;

	default:
		Warning( "Attempted to serialize an unsupported shader parameter type %s (%s)\n", 
			pAttribute->GetName(), g_pDataModel->GetAttributeNameForType( pAttribute->GetType() ) );
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
// Writes out all shader parameters
//-----------------------------------------------------------------------------
bool CImportVMT::SerializeShaderParameters( CUtlBuffer &buf, CDmElement *pRoot )
{
	for ( CDmAttribute *pAttribute = pRoot->FirstAttribute(); pAttribute; pAttribute = pAttribute->NextAttribute() )
	{
		// Skip the standard attributes
		if ( pAttribute->IsFlagSet( FATTRIB_STANDARD ) )
			continue;

		// Skip the shader name
		const char *pName = pAttribute->GetName();
		if ( !Q_stricmp( pAttribute->GetName(), "shader" ) )
			continue;

		// Names that don't start with a $ or a % are not shader parameters
		if ( pName[0] != '$' && pName[0] != '%' )
			continue;

		// Skip element array children; we'll handle them separately.
		if ( pAttribute->GetType() == AT_ELEMENT_ARRAY )
			continue;

		// Write out the shader parameter
		if ( !SerializeShaderParameter( buf, pAttribute ) )
			return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
// Writes out all shader fallbacks
//-----------------------------------------------------------------------------
bool CImportVMT::SerializeFallbacks( CUtlBuffer &buf, CDmElement *pRoot )
{
	if ( !pRoot->HasAttribute( "fallbacks" ) )
		return true;

	CDmAttribute *pFallbacks = pRoot->GetAttribute( "fallbacks" );
	if ( pFallbacks->GetType() != AT_ELEMENT_ARRAY )
		return false;

	CDmrElementArray<> array( pFallbacks );
	int nCount = array.Count();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pFallback = array[i];
		Assert( pFallback );

		PrintStringAttribute( pFallback, buf, "shader", false, true );
		buf.Printf( "{\n" );
		buf.PushTab();
		if ( !SerializeShaderParameters( buf, pFallback ) )
			return false;
		buf.PopTab();
		buf.Printf( "}\n" );
	}
	return true;
}


//-----------------------------------------------------------------------------
// Writes out all material proxies
//-----------------------------------------------------------------------------
bool CImportVMT::SerializeProxies( CUtlBuffer &buf, CDmElement *pRoot )
{
	if ( !pRoot->HasAttribute( "proxies" ) )
		return true;

	CDmAttribute *pProxies = pRoot->GetAttribute( "proxies" );
	if ( pProxies->GetType() != AT_ELEMENT_ARRAY )
		return false;

	CDmrElementArray<> array( pProxies );
	int nCount = array.Count();
	if ( nCount == 0 )
		return true;

	buf.Printf( "\"Proxies\"\n" );
	buf.Printf( "{\n" );
	buf.PushTab();
	for ( int i = 0; i < nCount; ++i )
	{
		CDmElement *pProxy = array[i];
		Assert( pProxy );

		PrintStringAttribute( pProxy, buf, "proxyType", false, true );
		buf.Printf( "{\n" );
		buf.PushTab();
		if ( !SerializeShaderParameters( buf, pProxy ) )
			return false;
		buf.PopTab();
		buf.Printf( "}\n" );
	}
	buf.PopTab();
	buf.Printf( "}\n" );
	return true;
}


//-----------------------------------------------------------------------------
// Writes out a new vmt file
//-----------------------------------------------------------------------------
bool CImportVMT::Serialize( CUtlBuffer &buf, CDmElement *pRoot )
{
	PrintStringAttribute( pRoot, buf, "shader", false, true );
	buf.Printf( "{\n" );
	buf.PushTab();

	if ( !SerializeShaderParameters( buf, pRoot ) )
		return false;

	if ( !SerializeFallbacks( buf, pRoot ) )
		return false;

	if ( !SerializeProxies( buf, pRoot ) )
		return false;

	buf.PopTab();
	buf.Printf( "}\n" );
	return true;
}


//-----------------------------------------------------------------------------
// Parser utilities
//-----------------------------------------------------------------------------
static inline bool IsWhitespace( char c )
{
	return c == ' ' || c == '\t';
}

static inline bool IsEndline( char c )
{
	return c == '\n' || c == '\0';
}

static inline bool IsVector( char const* v )
{
	while (IsWhitespace(*v))
	{
		++v;
		if (IsEndline(*v))
			return false;
	}
	return *v == '[' || *v == '{';
}


//-----------------------------------------------------------------------------
// Creates a vector material var
//-----------------------------------------------------------------------------
int ParseVectorFromKeyValueString( const char *pParamName, const char* pScan, const char *pMaterialName, float vecVal[4] )
{
	bool divideBy255 = false;

	// skip whitespace
	while( IsWhitespace(*pScan) )
	{
		++pScan;
	}

	if( *pScan == '{' )
	{
		divideBy255 = true;
	}
	else
	{
		Assert( *pScan == '[' );
	}
	
	// skip the '['
	++pScan;
	int i;
	for( i = 0; i < 4; i++ )
	{
		// skip whitespace
		while( IsWhitespace(*pScan) )
		{
			++pScan;
		}

		if( IsEndline(*pScan) || *pScan == ']' || *pScan == '}' )
		{
			if (*pScan != ']' && *pScan != '}')
			{
				Warning( "Warning in .VMT file (%s): no ']' or '}' found in vector key \"%s\".\n"
					"Did you forget to surround the vector with \"s?\n", pMaterialName, pParamName );
			}

			// allow for vec2's, etc.
			vecVal[i] = 0.0f;
			break;
		}

		char* pEnd;

		vecVal[i] = strtod( pScan, &pEnd );
		if (pScan == pEnd)
		{
			Warning( "Error in .VMT file: error parsing vector element \"%s\" in \"%s\"\n", pParamName, pMaterialName );
			return 0;
		}

		pScan = pEnd;
	}

	if( divideBy255 )
	{
		vecVal[0] *= ( 1.0f / 255.0f );
		vecVal[1] *= ( 1.0f / 255.0f );
		vecVal[2] *= ( 1.0f / 255.0f );
		vecVal[3] *= ( 1.0f / 255.0f );
	}

	return i;
}


//-----------------------------------------------------------------------------
// Sets shader parameter attributes
//-----------------------------------------------------------------------------
template< class T >
inline bool SetShaderParamAttribute( CDmElement *pElement, const char *pAttributeName, const T &value )
{
	if ( !pElement )
		return false;

	if ( !pElement->SetValue( pAttributeName, value ) )
		return false;

	CDmAttribute *pAttribute = pElement->GetAttribute( pAttributeName );
	pAttribute->AddFlag( FATTRIB_USERDEFINED );
	return true;
}

inline bool SetShaderParamAttribute( CDmElement *pElement, const char *pAttributeName, const char *value )
{
	if ( !pElement )
		return false;

	if ( !pElement->SetValue( pAttributeName, value ) )
		return false;

	CDmAttribute *pAttribute = pElement->GetAttribute( pAttributeName );
	pAttribute->AddFlag( FATTRIB_USERDEFINED );
	return true;
}


//-----------------------------------------------------------------------------
// Creates a vector shader parameter
//-----------------------------------------------------------------------------
bool CImportVMT::CreateVectorMaterialVarFromKeyValue( CDmElement *pElement, const char *pParamName, const char *pString )
{
	Vector4D vecVal;
	int nDim = ParseVectorFromKeyValueString( pParamName, pString, FileName(), vecVal.Base() );
	if ( nDim == 0 )
		return false;

	// Create the variable!
	switch ( nDim )
	{
	case 1:
		return SetShaderParamAttribute( pElement, pParamName, vecVal[0] );
	case 2:
		return SetShaderParamAttribute( pElement, pParamName, vecVal.AsVector2D() );
	case 3:
		return SetShaderParamAttribute( pElement, pParamName, vecVal.AsVector3D() );
	case 4:
		return SetShaderParamAttribute( pElement, pParamName, vecVal );
	}

	return false;
}


//-----------------------------------------------------------------------------
// Creates a matrix shader parameter
//-----------------------------------------------------------------------------
bool CImportVMT::CreateMatrixMaterialVarFromKeyValue( CDmElement *pElement, const char *pParamName, const char *pScan )
{
	// Matrices can be specified one of two ways:
	// [ # # # #  # # # #  # # # #  # # # # ]
	// or
	// center # # scale # # rotate # translate # #

	VMatrix mat;
	int count = sscanf( pScan, " [ %f %f %f %f  %f %f %f %f  %f %f %f %f  %f %f %f %f ]",
		&mat.m[0][0], &mat.m[0][1], &mat.m[0][2], &mat.m[0][3],
		&mat.m[1][0], &mat.m[1][1], &mat.m[1][2], &mat.m[1][3],
		&mat.m[2][0], &mat.m[2][1], &mat.m[2][2], &mat.m[2][3],
		&mat.m[3][0], &mat.m[3][1], &mat.m[3][2], &mat.m[3][3] );
	if (count == 16)
	{
		return SetShaderParamAttribute( pElement, pParamName, mat );
	}

	Vector2D scale, center;
	float angle;
	Vector2D translation;
	count = sscanf( pScan, " center %f %f scale %f %f rotate %f translate %f %f",
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

	// Create the variable!
	return SetShaderParamAttribute( pElement, pParamName, mat );
}


//-----------------------------------------------------------------------------
// Creates a shader parameter from a key value
//-----------------------------------------------------------------------------
bool CImportVMT::UnserializeShaderParam( CDmElement *pRoot, KeyValues* pKeyValues )
{
	char pParamName[512];
	Q_strncpy( pParamName, pKeyValues->GetName(), sizeof(pParamName) );
	Q_strlower( pParamName );

	switch( pKeyValues->GetDataType() )
	{
	case KeyValues::TYPE_INT:
		return SetShaderParamAttribute( pRoot, pParamName, pKeyValues->GetInt() );

	case KeyValues::TYPE_FLOAT:
		return SetShaderParamAttribute( pRoot, pParamName, pKeyValues->GetFloat() );

	case KeyValues::TYPE_STRING:
		{
			char const* pString = pKeyValues->GetString();

			// Only valid if it's a texture attribute
			if ( !pString || !pString[0] )
				return SetShaderParamAttribute( pRoot, pParamName, pString );

			// Look for matrices
			if ( CreateMatrixMaterialVarFromKeyValue( pRoot, pParamName, pString ) )
				return true;

			// Look for vectors
			if ( !IsVector( pString ) )
				return SetShaderParamAttribute( pRoot, pParamName, pString );

			// Parse the string as a vector...
			return CreateVectorMaterialVarFromKeyValue( pRoot, pParamName, pString );
		}
	}

	return false;
}


//-----------------------------------------------------------------------------
// Unserialize proxies
//-----------------------------------------------------------------------------
bool CImportVMT::UnserializeProxies( CDmElement *pElement, KeyValues *pKeyValues )
{
	// Create a child element array to contain all material proxies
	CDmAttribute *pProxies = pElement->AddAttribute( "proxies", AT_ELEMENT_ARRAY );
	if ( !pProxies )
		return false;

	CDmrElementArray<> array( pProxies );

	// Proxies are a list of sub-keys, the name is the proxy name, subkeys are values
	for ( KeyValues *pProxy = pKeyValues->GetFirstTrueSubKey(); pProxy != NULL; pProxy = pProxy->GetNextTrueSubKey() )
	{
		CDmElement *pProxyElement = CreateDmElement( "DmElement", pProxy->GetName(), NULL ); 
		array.AddToTail( pProxyElement );
		pProxyElement->SetValue( "proxyType", pKeyValues->GetName() );
		pProxyElement->SetValue( "editorType", "vmtProxy" );

		// Normal keys are proxy parameters
		for ( KeyValues *pProxyParam = pProxy->GetFirstValue(); pProxyParam != NULL; pProxyParam = pProxyParam->GetNextValue() )
		{
			switch( pProxyParam->GetDataType() )
			{
			case KeyValues::TYPE_INT:
				pProxyElement->SetValue( pProxyParam->GetName(), pProxyParam->GetInt() );
				return true;

			case KeyValues::TYPE_FLOAT:
				pProxyElement->SetValue( pProxyParam->GetName(), pProxyParam->GetFloat() );
				return true;

			case KeyValues::TYPE_STRING:
				pProxyElement->SetValue( pProxyParam->GetName(), pProxyParam->GetString() );
				return true;

			default:
				Warning( "Unhandled proxy keyvalues type (proxy %s var %s)\n", pProxy->GetName(), pProxyParam->GetName() );
				return false;
			}
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// Unserialize fallbacks
//-----------------------------------------------------------------------------
bool CImportVMT::UnserializeFallbacks( CDmElement *pElement, KeyValues *pFallbackKeyValues )
{
	// Create a child element array to contain all material proxies
	CDmAttribute *pFallbacks = pElement->AddAttribute( "fallbacks", AT_ELEMENT_ARRAY );
	if ( !pFallbacks )
		return false;

	CDmrElementArray<> array( pFallbacks );

	CDmElement *pFallback = CreateDmElement( "DmElement", pFallbackKeyValues->GetName(), NULL ); 
	array.AddToTail( pFallback );
	pFallback->SetValue( "editorType", "vmtFallback" );

	// Normal keys are shader parameters
	for ( KeyValues *pShaderParam = pFallbackKeyValues->GetFirstValue(); pShaderParam != NULL; pShaderParam = pShaderParam->GetNextValue() )
	{
		if ( !UnserializeShaderParam( pFallback, pShaderParam ) )
		{
			Warning( "Error importing vmt shader parameter %s\n", pShaderParam->GetName() );
			return NULL;
		}
	}

	return true;
}


//-----------------------------------------------------------------------------
// VMT parser
//-----------------------------------------------------------------------------
void InsertKeyValues( KeyValues& dst, KeyValues& src, bool bCheckForExistence )
{
	KeyValues *pSrcVar = src.GetFirstSubKey();
	while( pSrcVar )
	{
		if ( !bCheckForExistence || dst.FindKey( pSrcVar->GetName() ) )
		{
			switch( pSrcVar->GetDataType() )
			{
			case KeyValues::TYPE_STRING:
				dst.SetString( pSrcVar->GetName(), pSrcVar->GetString() );
				break;
			case KeyValues::TYPE_INT:
				dst.SetInt( pSrcVar->GetName(), pSrcVar->GetInt() );
				break;
			case KeyValues::TYPE_FLOAT:
				dst.SetFloat( pSrcVar->GetName(), pSrcVar->GetFloat() );
				break;
			case KeyValues::TYPE_PTR:
				dst.SetPtr( pSrcVar->GetName(), pSrcVar->GetPtr() );
				break;
			}
		}
		pSrcVar = pSrcVar->GetNextKey();
	}

	if( bCheckForExistence )
	{
		for( KeyValues *pScan = dst.GetFirstTrueSubKey(); pScan; pScan = pScan->GetNextTrueSubKey() )
		{
			KeyValues *pTmp = src.FindKey( pScan->GetName() );
			if( !pTmp )
				continue;
			// make sure that this is a subkey.
			if( pTmp->GetDataType() != KeyValues::TYPE_NONE )
				continue;
			InsertKeyValues( *pScan, *pTmp, bCheckForExistence );
		}
	}
}


//-----------------------------------------------------------------------------
// Handle patch files
//-----------------------------------------------------------------------------
void CImportVMT::ExpandPatchFile( KeyValues *pKeyValues )
{
	int count = 0;
	while( count < 10 && stricmp( pKeyValues->GetName(), "patch" ) == 0 )
	{
//		WriteKeyValuesToFile( "patch.txt", keyValues );
		const char *pIncludeFileName = pKeyValues->GetString( "include" );
		if( pIncludeFileName )
		{
			KeyValues * includeKeyValues = new KeyValues( "vmt" );
			bool success = includeKeyValues->LoadFromFile( g_pFullFileSystem, pIncludeFileName, IsX360() ? "GAME" : NULL );
			if( success )
			{
				KeyValues *pInsertSection = pKeyValues->FindKey( "insert" );
				if( pInsertSection )
				{
					InsertKeyValues( *includeKeyValues, *pInsertSection, false );
				}
				
				KeyValues *pReplaceSection = pKeyValues->FindKey( "replace" );
				if( pReplaceSection )
				{
					InsertKeyValues( *includeKeyValues, *pReplaceSection, true );
				}

				*pKeyValues = *includeKeyValues;
				includeKeyValues->deleteThis();
				// Could add other commands here, like "delete", "rename", etc.
			}
			else
			{
				includeKeyValues->deleteThis();
				return;
			}
		}
		else
		{
			return;
		}
		count++;
	}
	if( count >= 10 )
	{
		Warning( "Infinite recursion in patch file?\n" );
	}
}


//-----------------------------------------------------------------------------
// Main entry point for the unserialization
//-----------------------------------------------------------------------------
CDmElement* CImportVMT::UnserializeFromKeyValues( KeyValues *pKeyValues )
{
	ExpandPatchFile( pKeyValues );

	// Create the main element
	CDmElement *pRoot = CreateDmElement( "DmElement", "VMT", NULL );
	if ( !pRoot )
		return NULL;

	// Each material needs to have an editortype associated with it so it displays nicely in editors
	pRoot->SetValue( "editorType", "vmt" );

	// Each material needs a proxy list and a fallback list
	if ( !pRoot->AddAttribute( "proxies", AT_ELEMENT_ARRAY ) )
		return NULL;
	if ( !pRoot->AddAttribute( "fallbacks", AT_ELEMENT_ARRAY ) )
		return NULL;

	// The keyvalues name is the shader name
	pRoot->SetValue( "shader", pKeyValues->GetName() );

	// Normal keys are shader parameters
	for ( KeyValues *pShaderParam = pKeyValues->GetFirstValue(); pShaderParam != NULL; pShaderParam = pShaderParam->GetNextValue() )
	{
		if ( !UnserializeShaderParam( pRoot, pShaderParam ) )
		{
			Warning( "Error importing vmt shader parameter %s\n", pShaderParam->GetName() );
			return NULL;
		}
	}

	// Subkeys are either proxies or fallbacks
	for ( KeyValues *pSubKey = pKeyValues->GetFirstTrueSubKey(); pSubKey != NULL; pSubKey = pSubKey->GetNextTrueSubKey() )
	{
		if ( !Q_stricmp( pSubKey->GetName(), "Proxies" ) )
		{
			UnserializeProxies( pRoot, pSubKey );
		}
		else
		{
			UnserializeFallbacks( pRoot, pSubKey );
		}
	}

	// Resolve all element references recursively
	RecursivelyResolveElement( pRoot );

	return pRoot;
}
