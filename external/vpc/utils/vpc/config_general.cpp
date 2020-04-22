//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

bool VPC_Config_General_AdditionalOutputFiles( const char *pPropertyName )
{
	// Ignore this. We only care about it when looking at dependencies,
	// and baseprojectdatacollector will get it in that case.
	char buff[MAX_SYSTOKENCHARS];
	ParsePropertyValue( &g_pScriptData, g_pScriptLine, NULL, buff, sizeof( buff ) );
	return true;
}

bool VPC_Config_General_OutputDirectory( const char *pPropertyName )
{
	SET_STRING_PROPERTY( pPropertyName, g_spConfig, get_OutputDirectory, put_OutputDirectory );
}

bool VPC_Config_General_IntermediateDirectory( const char *pPropertyName )
{
	SET_STRING_PROPERTY( pPropertyName, g_spConfig, get_IntermediateDirectory, put_IntermediateDirectory );
}

bool VPC_Config_General_ExtensionsToDeleteOnClean( const char *pPropertyName )
{
	SET_STRING_PROPERTY( pPropertyName, g_spConfig, get_DeleteExtensionsOnClean, put_DeleteExtensionsOnClean );
}

bool VPC_Config_General_BuildLogFile( const char *pPropertyName )
{
	SET_STRING_PROPERTY( pPropertyName, g_spConfig, get_BuildLogFile, put_BuildLogFile );
}

bool VPC_Config_General_InheritedProjectPropertySheets( const char *pPropertyName )
{
	SET_STRING_PROPERTY( pPropertyName, g_spConfig, get_InheritedPropertySheets, put_InheritedPropertySheets );
}

bool VPC_Config_General_ConfigurationType( const char *pPropertyName )
{
	char	buff[MAX_SYSTOKENCHARS];

	if ( !ParsePropertyValue( &g_pScriptData, g_pScriptLine, NULL, buff, sizeof( buff ) ) )
		return true;

	ConfigurationTypes	option = typeUnknown;
	if ( !V_stricmp( buff, "Utility" ) )
		option = typeUnknown;
	else if ( !V_stricmp( buff, "Application (.exe)" ) || !V_stricmp( buff, "Title (.xex)" ) )
		option = typeApplication;
	else if ( !V_stricmp( buff, "Dynamic Library (.dll)" ) || !V_stricmp( buff, "Dynamic Library (.xex)" ) )
		option = typeDynamicLibrary;
	else if ( !V_stricmp( buff, "Static Library (.lib)" ) )
		option = typeStaticLibrary;
	else
		VPC_SyntaxError();

	SET_LIST_PROPERTY( pPropertyName, g_spConfig, get_ConfigurationType, put_ConfigurationType, ConfigurationTypes, option );
}

bool VPC_Config_General_UseOfMFC( const char *pPropertyName )
{
	char	buff[MAX_SYSTOKENCHARS];

	if ( !ParsePropertyValue( &g_pScriptData, g_pScriptLine, NULL, buff, sizeof( buff ) ) )
		return true;

	useOfMfc option = useMfcStdWin;
	if ( !V_stricmp( buff, "Use Standard Windows Libraries" ) )
		option = useMfcStdWin;
	else if ( !V_stricmp( buff, "Use MFC in a Static Library" ) )
		option = useMfcStatic;
	else if ( !V_stricmp( buff, "Use MFC in a Shared DLL" ) )
		option = useMfcDynamic;
	else
		VPC_SyntaxError();

	SET_LIST_PROPERTY( pPropertyName, g_spConfig, get_useOfMfc, put_useOfMfc, useOfMfc, option );
}

bool VPC_Config_General_UseOfATL( const char *pPropertyName )
{
	char	buff[MAX_SYSTOKENCHARS];

	if ( !ParsePropertyValue( &g_pScriptData, g_pScriptLine, NULL, buff, sizeof( buff ) ) )
		return true;

	useOfATL option = useATLNotSet;
	if ( !V_stricmp( buff, "Not Using ATL" ) )
		option = useATLNotSet;
	else if ( !V_stricmp( buff, "Static Link to ATL" ) )
		option = useATLStatic;
	else if ( !V_stricmp( buff, "Dynamic Link to ATL" ) )
		option = useATLDynamic;
	else
		VPC_SyntaxError();

	SET_LIST_PROPERTY( pPropertyName, g_spConfig, get_useOfATL, put_useOfATL, useOfATL, option );
}

bool VPC_Config_General_MinimizeCRTUseInATL( const char *pPropertyName )
{
	SET_BOOL_PROPERTY( pPropertyName, g_spConfig, get_ATLMinimizesCRunTimeLibraryUsage, put_ATLMinimizesCRunTimeLibraryUsage );
}

bool VPC_Config_General_CharacterSet( const char *pPropertyName )
{
	char	buff[MAX_SYSTOKENCHARS];

	if ( !ParsePropertyValue( &g_pScriptData, g_pScriptLine, NULL, buff, sizeof( buff ) ) )
		return true;

	charSet option = charSetNotSet;
	if ( !V_stricmp( buff, "Not Set" ) )
		option = charSetNotSet;
	else if ( !V_stricmp( buff, "Use Unicode Character Set" ) )
		option = charSetUnicode;
	else if ( !V_stricmp( buff, "Use Multi-Byte Character Set" ) )
		option = charSetMBCS;
	else
		VPC_SyntaxError();

	SET_LIST_PROPERTY( pPropertyName, g_spConfig, get_CharacterSet, put_CharacterSet, charSet, option );
}

bool VPC_Config_General_CommonLanguageRuntimeSupport( const char *pPropertyName )
{
	VPC_Error( "Setting '%s' Not Implemented", pPropertyName );
	return false;
}

bool VPC_Config_General_WholeProgramOptimization( const char *pPropertyName )
{
	char	buff[MAX_SYSTOKENCHARS];

	if ( !ParsePropertyValue( &g_pScriptData, g_pScriptLine, NULL, buff, sizeof( buff ) ) )
		return true;

	WholeProgramOptimizationTypes option = WholeProgramOptimizationNone;
	if ( !V_stricmp( buff, "No Whole Program Optimization" ) )
		option = WholeProgramOptimizationNone;
	else if ( !V_stricmp( buff, "Use Link Time Code Generation" ) )
		option = WholeProgramOptimizationLinkTimeCodeGen;
	else if ( !V_stricmp( buff, "Profile Guided Optimization - Instrument" ) )
		option = WholeProgramOptimizationPGOInstrument;
	else if ( !V_stricmp( buff, "Profile Guided Optimization - Optimize" ) )
		option = WholeProgramOptimizationPGOOptimize;
	else if ( !V_stricmp( buff, "Profile Guided Optimization - Update" ) )
		option = WholeProgramOptimizationPGOUpdate;
	else
		VPC_SyntaxError();

	SET_LIST_PROPERTY( pPropertyName, g_spConfig, get_WholeProgramOptimization, put_WholeProgramOptimization, WholeProgramOptimizationTypes, option );
}

extern bool VPC_Config_IgnoreOption( const char *pPropertyName );

property_t g_generalProperties[] =
{
	{g_pOption_AdditionalProjectDependencies,	VPC_Config_IgnoreOption },
	{g_pOption_AdditionalOutputFiles,			VPC_Config_IgnoreOption },
	{"$GameOutputFile",					VPC_Config_IgnoreOption },
	{"$OutputDirectory",				VPC_Config_General_OutputDirectory },
	{"$IntermediateDirectory",			VPC_Config_General_IntermediateDirectory},
	{"$ExtensionsToDeleteOnClean",		VPC_Config_General_ExtensionsToDeleteOnClean},
	{"$BuildLogFile",					VPC_Config_General_BuildLogFile},
	{"$InheritedProjectPropertySheets",	VPC_Config_General_InheritedProjectPropertySheets},
	{"$ConfigurationType",				VPC_Config_General_ConfigurationType},
	{"$UseOfMFC",						VPC_Config_General_UseOfMFC, PLATFORM_WINDOWS},
	{"$UseOfATL",						VPC_Config_General_UseOfATL, PLATFORM_WINDOWS},
	{"$MinimizeCRTUseInATL",			VPC_Config_General_MinimizeCRTUseInATL, PLATFORM_WINDOWS},
	{"$CharacterSet",					VPC_Config_General_CharacterSet},
	{"$CommonLanguageRuntimeSupport",	VPC_Config_General_CommonLanguageRuntimeSupport, PLATFORM_WINDOWS},
	{"$WholeProgramOptimization",		VPC_Config_General_WholeProgramOptimization},
	{NULL}
};
