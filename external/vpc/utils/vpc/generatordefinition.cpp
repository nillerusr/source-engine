//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
//
//=====================================================================================//

#include "vpc.h"

CGeneratorDefinition::CGeneratorDefinition()
{
	Clear();
}

void CGeneratorDefinition::Clear()
{
	m_pPropertyNames = NULL;
	m_ScriptName.Clear();
	m_NameString.Clear();
	m_VersionString.Clear();
	m_Tools.Purge();
	m_ScriptCRC = 0;
}

void CGeneratorDefinition::IterateAttributesKey( ToolProperty_t *pProperty, KeyValues *pAttributesKV )
{
	const char *pAttributeName = pAttributesKV->GetName();
	const char *pValue = pAttributesKV->GetString( "" );

	//Msg( "Attribute name: %s\n", pAttributeName );

	if ( !V_stricmp( pAttributeName, "type" ) )
	{
		if ( !V_stricmp( pValue, "bool" ) || !V_stricmp( pValue, "boolean" ) )
		{
			pProperty->m_nType = PT_BOOLEAN;
		}
		else if ( !V_stricmp( pValue, "string" ) )
		{
			pProperty->m_nType = PT_STRING;
		}
		else if ( !V_stricmp( pValue, "list" ) )
		{
			pProperty->m_nType = PT_LIST;
		}
		else if (  !V_stricmp( pValue, "int" ) || !V_stricmp( pValue, "integer" ) )
		{
			pProperty->m_nType = PT_INTEGER;
		}
		else if ( !V_stricmp( pValue, "ignore" ) || !V_stricmp( pValue, "none" ) )
		{
			pProperty->m_nType = PT_IGNORE;
		}
		else if ( !V_stricmp( pValue, "deprecated" ) || !V_stricmp( pValue, "donotuse" ) )
		{
			pProperty->m_nType = PT_DEPRECATED;
		}
		else 
		{
			// unknown
			g_pVPC->VPCError( "Unknown type '%s' in '%s'", pValue, pProperty->m_ParseString.Get() );
		}
	}
	else if ( !V_stricmp( pAttributeName, "alias" ) )
	{
		pProperty->m_AliasString = pValue;
	}
	else if ( !V_stricmp( pAttributeName, "legacy" ) )
	{
		pProperty->m_LegacyString = pValue;
	}
	else if ( !V_stricmp( pAttributeName, "InvertOutput" ) )
	{
		pProperty->m_bInvertOutput = pAttributesKV->GetBool();
	}
	else if ( !V_stricmp( pAttributeName, "output" ) )
	{
		pProperty->m_OutputString = pValue;
	}
	else if ( !V_stricmp( pAttributeName, "fixslashes" ) )
	{
		pProperty->m_bFixSlashes = pAttributesKV->GetBool();
	}
	else if ( !V_stricmp( pAttributeName, "PreferSemicolonNoComma" ) )
	{
		pProperty->m_bPreferSemicolonNoComma = pAttributesKV->GetBool();
	}
	else if ( !V_stricmp( pAttributeName, "PreferSemicolonNoSpace" ) )
	{
		pProperty->m_bPreferSemicolonNoSpace = pAttributesKV->GetBool();
	}
	else if ( !V_stricmp( pAttributeName, "AppendSlash" ) )
	{
		pProperty->m_bAppendSlash = pAttributesKV->GetBool();
	}
	else if ( !V_stricmp( pAttributeName, "GlobalProperty" ) )
	{
		pProperty->m_bEmitAsGlobalProperty = pAttributesKV->GetBool();
	}
	else if ( !V_stricmp( pAttributeName, "ordinals" ) )
	{
		if ( pProperty->m_nType == PT_UNKNOWN )
		{
			pProperty->m_nType = PT_LIST;
		}

		for ( KeyValues *pKV = pAttributesKV->GetFirstSubKey(); pKV; pKV = pKV->GetNextKey() )
		{
			const char *pOrdinalName = pKV->GetName();
			const char *pOrdinalValue = pKV->GetString();
			if ( !pOrdinalValue[0] )
			{
				g_pVPC->VPCError( "Unknown ordinal value for name '%s' in '%s'", pOrdinalName, pProperty->m_ParseString.Get() );
			}

			int iIndex = pProperty->m_Ordinals.AddToTail();
			pProperty->m_Ordinals[iIndex].m_ParseString = pOrdinalName;
			pProperty->m_Ordinals[iIndex].m_ValueString = pOrdinalValue;
		}
	}
	else
	{
		g_pVPC->VPCError( "Unknown attribute '%s' in '%s'", pAttributeName, pProperty->m_ParseString.Get() );
	}
}

void CGeneratorDefinition::IteratePropertyKey( GeneratorTool_t *pTool, KeyValues *pPropertyKV )
{
	//Msg( "Property Key name: %s\n", pPropertyKV->GetName() );

	int iIndex = pTool->m_Properties.AddToTail();
	ToolProperty_t *pProperty = &pTool->m_Properties[iIndex];

	pProperty->m_ParseString = pPropertyKV->GetName();

	KeyValues *pKV = pPropertyKV->GetFirstSubKey();
	if ( !pKV )
		return;

	for ( ;pKV; pKV = pKV->GetNextKey() )
	{		
		IterateAttributesKey( pProperty, pKV );
	}
}

void CGeneratorDefinition::IterateToolKey( KeyValues *pToolKV )
{
	//Msg( "Tool Key name: %s\n", pToolKV->GetName() );

	// find or create
	GeneratorTool_t *pTool = NULL;
	for ( int i = 0; i < m_Tools.Count(); i++ )
	{
		if ( !V_stricmp( pToolKV->GetName(), m_Tools[i].m_ParseString ) )
		{
			pTool = &m_Tools[i];
			break;
		}
	}
	if ( !pTool )
	{
		int iIndex = m_Tools.AddToTail();
		pTool = &m_Tools[iIndex];
	}

	pTool->m_ParseString = pToolKV->GetName();

	KeyValues *pKV = pToolKV->GetFirstSubKey();
	if ( !pKV )
		return;

	for ( ;pKV; pKV = pKV->GetNextKey() )
	{		
		IteratePropertyKey( pTool, pKV );
	}
}

void CGeneratorDefinition::AssignIdentifiers()
{
	CUtlVector< bool > usedPropertyNames;
	int nTotalPropertyNames = 0;
	while ( m_pPropertyNames[nTotalPropertyNames].m_nPropertyId >= 0 )
	{
		nTotalPropertyNames++;
	}
	usedPropertyNames.SetCount( nTotalPropertyNames );

	// assign property identifiers
	for ( int i = 0; i < m_Tools.Count(); i++ )
	{
		GeneratorTool_t *pTool = &m_Tools[i];

		// assign the tool keyword
		configKeyword_e keyword = g_pVPC->NameToKeyword( pTool->m_ParseString.Get() );
		if ( keyword == KEYWORD_UNKNOWN )
		{
			g_pVPC->VPCError( "Unknown Tool Keyword '%s' in '%s'", pTool->m_ParseString.Get(), m_ScriptName.Get() );
		}
		pTool->m_nKeyword = keyword;

		const char *pPrefix = m_NameString.Get();
		const char *pToolName = pTool->m_ParseString.Get();
		if ( pToolName[0] == '$' )
		{
			pToolName++;
		}
		
		for ( int j = 0; j < pTool->m_Properties.Count(); j++ )
		{
			ToolProperty_t *pProperty = &pTool->m_Properties[j];

			if ( pProperty->m_nType == PT_IGNORE || pProperty->m_nType == PT_DEPRECATED )
			{
				continue;
			}

			const char *pPropertyName = pProperty->m_AliasString.Get();
			if ( !pPropertyName[0] )
			{
				pPropertyName = pProperty->m_ParseString.Get();
			}
			if ( pPropertyName[0] == '$' )
			{
				pPropertyName++;
			}

			CUtlString prefixString = CFmtStr( "%s_%s", pPrefix, pToolName );

			bool bFound = false;
			for ( int k = 0; k < nTotalPropertyNames && !bFound; k++ )
			{
				if ( !V_stricmp( prefixString.Get(), m_pPropertyNames[k].m_pPrefixName ) )
				{
					if ( !V_stricmp( pPropertyName, m_pPropertyNames[k].m_pPropertyName ) )
					{
						pProperty->m_nPropertyId = m_pPropertyNames[k].m_nPropertyId;
						bFound = true;
						usedPropertyNames[k] = true;
					}
				}
			}
			if ( !bFound )
			{
				g_pVPC->VPCError( "Could not find PROPERTYNAME( %s, %s ) for %s", prefixString.Get(), pPropertyName, m_ScriptName.Get() );
			}
		}
	}

	if ( g_pVPC->IsVerbose() )
	{
		for ( int i = 0; i < usedPropertyNames.Count(); i++ )
		{
			if ( !usedPropertyNames[i] )
			{
				g_pVPC->VPCWarning( "Unused PROPERTYNAME( %s, %s ) in %s", m_pPropertyNames[i].m_pPrefixName, m_pPropertyNames[i].m_pPropertyName, m_ScriptName.Get() );
			}
		}
	}
}

void CGeneratorDefinition::LoadDefinition( const char *pDefnitionName, PropertyName_t *pPropertyNames )
{
	Clear();

	m_pPropertyNames = pPropertyNames;
	g_pVPC->GetScript().PushScript( CFmtStr( "vpc_scripts\\definitions\\%s", pDefnitionName ) );
	
	// project definitions are KV format
	KeyValues *pScriptKV = new KeyValues( g_pVPC->GetScript().GetName() );

	pScriptKV->LoadFromBuffer( g_pVPC->GetScript().GetName(), g_pVPC->GetScript().GetData() );

	m_ScriptName = g_pVPC->GetScript().GetName();
	m_ScriptCRC = CRC32_ProcessSingleBuffer( g_pVPC->GetScript().GetData(), strlen( g_pVPC->GetScript().GetData() ) );

	m_NameString = pScriptKV->GetName();

	KeyValues *pKV = pScriptKV->GetFirstSubKey();
	for ( ;pKV; pKV = pKV->GetNextKey() )
	{
		const char *pKeyName = pKV->GetName();
		if ( !V_stricmp( pKeyName, "version" ) )
		{
			m_VersionString = pKV->GetString();
		}
		else
		{
			IterateToolKey( pKV );
		}
	}

	g_pVPC->GetScript().PopScript();
	pScriptKV->deleteThis();

	g_pVPC->VPCStatus( false, "Definition: '%s' Version: %s", m_NameString.Get(), m_VersionString.Get() );

	AssignIdentifiers();
}

const char *CGeneratorDefinition::GetScriptName( CRC32_t *pCRC )
{
	if ( pCRC )
	{
		*pCRC = m_ScriptCRC;
	}

	return m_ScriptName.Get();
}

ToolProperty_t *CGeneratorDefinition::GetProperty( configKeyword_e keyword, const char *pPropertyName )
{
	for ( int i = 0; i < m_Tools.Count(); i++ )
	{
		GeneratorTool_t *pTool = &m_Tools[i];
		if ( pTool->m_nKeyword != keyword )
			continue;

		for ( int j = 0; j < pTool->m_Properties.Count(); j++ )
		{
			ToolProperty_t *pToolProperty = &pTool->m_Properties[j];
			if ( !V_stricmp( pToolProperty->m_ParseString.Get(), pPropertyName ) )
			{
				// found
				return pToolProperty;
			}
			if ( !pToolProperty->m_LegacyString.IsEmpty() && !V_stricmp( pToolProperty->m_LegacyString.Get(), pPropertyName ) )
			{
				// found
				return pToolProperty;
			}
		}
	}

	// not found
	return NULL;
}


















	