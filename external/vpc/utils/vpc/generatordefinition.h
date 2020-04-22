//===================== Copyright (c) Valve Corporation. All Rights Reserved. ======================
//
//
//==================================================================================================

#ifndef GENERATORDEFINITION_H
#define GENERATORDEFINITION_H
#ifdef _WIN32
#pragma once
#endif

struct PropertyName_t
{
	int				m_nPropertyId;
	const char		*m_pPrefixName;
	const char		*m_pPropertyName;
};

enum configKeyword_e
{
	KEYWORD_UNKNOWN = -1,
	KEYWORD_GENERAL,
	KEYWORD_DEBUGGING,
	KEYWORD_COMPILER,
	KEYWORD_PS3_SNCCOMPILER,
	KEYWORD_PS3_GCCCOMPILER,
	KEYWORD_LIBRARIAN,
	KEYWORD_LINKER,
	KEYWORD_PS3_SNCLINKER,
	KEYWORD_PS3_GCCLINKER,
	KEYWORD_MANIFEST,
	KEYWORD_XMLDOCGEN,
	KEYWORD_BROWSEINFO,
	KEYWORD_RESOURCES,
	KEYWORD_PREBUILDEVENT,
	KEYWORD_PRELINKEVENT,
	KEYWORD_POSTBUILDEVENT,
	KEYWORD_CUSTOMBUILDSTEP,
	KEYWORD_XBOXIMAGE,
	KEYWORD_XBOXDEPLOYMENT,
	KEYWORD_MAX,
};

enum PropertyType_e
{
	PT_UNKNOWN = 0,
	PT_BOOLEAN,
	PT_STRING,
	PT_INTEGER,
	PT_LIST,
	PT_IGNORE,
	PT_DEPRECATED,
};

struct PropertyOrdinal_t
{
	CUtlString	m_ParseString;
	CUtlString	m_ValueString;
};

struct ToolProperty_t
{
	ToolProperty_t()
	{
		m_nPropertyId = -1;
		m_nType = PT_UNKNOWN;
		m_bFixSlashes = false;
		m_bEmitAsGlobalProperty = false;
		m_bInvertOutput = false;
		m_bAppendSlash = false;
		m_bPreferSemicolonNoComma = false;
		m_bPreferSemicolonNoSpace = false;
	}

	CUtlString						m_ParseString;
	CUtlString						m_AliasString;
	CUtlString						m_LegacyString;
	CUtlString						m_OutputString;
	CUtlVector< PropertyOrdinal_t >	m_Ordinals;

	int								m_nPropertyId;
	PropertyType_e					m_nType;
	bool							m_bFixSlashes;
	bool							m_bEmitAsGlobalProperty;
	bool							m_bInvertOutput;
	bool							m_bAppendSlash;
	bool							m_bPreferSemicolonNoComma;
	bool							m_bPreferSemicolonNoSpace;
};

struct GeneratorTool_t
{
	GeneratorTool_t()
	{
		m_nKeyword = KEYWORD_UNKNOWN;
	}

	CUtlString						m_ParseString;
	CUtlVector< ToolProperty_t >	m_Properties;
	configKeyword_e					m_nKeyword;
};

class CGeneratorDefinition
{
public:
	CGeneratorDefinition();

	void			LoadDefinition( const char *pDefinitionName, PropertyName_t *pPropertyNames );
	ToolProperty_t	*GetProperty( configKeyword_e keyword, const char *pPropertyName );

	const char		*GetScriptName( CRC32_t *pCRC );

private:
	void	AssignIdentifiers();
	void	IterateToolKey( KeyValues *pToolKV );
	void	IteratePropertyKey( GeneratorTool_t *pTool, KeyValues *pPropertyKV );
	void	IterateAttributesKey( ToolProperty_t *pProperty, KeyValues *pAttributesKV );
	void	Clear();

	PropertyName_t					*m_pPropertyNames;
	CUtlString						m_ScriptName;
	CUtlString						m_NameString;
	CUtlString						m_VersionString;
	CUtlVector< GeneratorTool_t >	m_Tools;
	CRC32_t							m_ScriptCRC;
};



#endif // GENERATORDEFINITION_H
