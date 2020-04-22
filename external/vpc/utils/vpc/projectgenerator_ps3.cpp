//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

#undef PROPERTYNAME
#define PROPERTYNAME( X, Y ) { X##_##Y, #X, #Y },
static PropertyName_t s_PS3PropertyNames[] =
{
	#include "projectgenerator_ps3.inc"
	{ -1, NULL, NULL }
};

IBaseProjectGenerator* GetPS3ProjectGenerator()
{
	static CProjectGenerator_PS3 *s_pProjectGenerator = NULL;
	if ( !s_pProjectGenerator )
	{
		s_pProjectGenerator = new CProjectGenerator_PS3();
	}

	return s_pProjectGenerator->GetProjectGenerator();
}

CProjectGenerator_PS3::CProjectGenerator_PS3()
{
	m_pVCProjGenerator = new CVCProjGenerator();
	m_pVCProjGenerator->SetupGeneratorDefinition( this, "ps3.def", s_PS3PropertyNames );
}

bool CProjectGenerator_PS3::WriteFile( CProjectFile *pFile )
{
	m_XMLWriter.PushNode( "File" );
	m_XMLWriter.Write( CFmtStrMax( "RelativePath=\"%s\"", pFile->m_Name.Get() ) );
	m_XMLWriter.Write( ">" );

	for ( int i = 0; i < pFile->m_Configs.Count(); i++ )
	{
		if ( !WriteConfiguration( pFile->m_Configs[i] ) )
			return false;
	}

	m_XMLWriter.PopNode( true );
	
	return true;
}

bool CProjectGenerator_PS3::WriteFolder( CProjectFolder *pFolder )
{
	m_XMLWriter.PushNode( "Filter" );
	m_XMLWriter.Write( CFmtStrMax( "Name=\"%s\"", m_XMLWriter.FixupXMLString( pFolder->m_Name.Get() ) ) );
	m_XMLWriter.Write( ">" );

	for ( int iIndex = pFolder->m_Folders.Head(); iIndex != pFolder->m_Folders.InvalidIndex(); iIndex = pFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolder( pFolder->m_Folders[iIndex] ) )
			return false;
	}

	for ( int iIndex = pFolder->m_Files.Head(); iIndex != pFolder->m_Files.InvalidIndex(); iIndex = pFolder->m_Files.Next( iIndex ) )
	{
		if ( !WriteFile( pFolder->m_Files[iIndex] ) )
			return false;
	}

	m_XMLWriter.PopNode( true );
	
	return true;
}

bool CProjectGenerator_PS3::WritePreBuildEventTool( CPreBuildEventTool *pPreBuildEventTool )
{
	if ( !pPreBuildEventTool )
	{
		// not an error, some tools n/a for aconfig
		return true;
	}

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( "Name=\"VCPreBuildEventTool\"" );

	for ( int i = 0; i < pPreBuildEventTool->m_PropertyStates.m_Properties.Count(); i++ )
	{
		switch ( pPreBuildEventTool->m_PropertyStates.m_Properties[i].m_pToolProperty->m_nPropertyId )
		{
		case PS3_PREBUILDEVENT_CommandLine:
			m_XMLWriter.Write( CFmtStrMax( "CommandLine=\"%s\"", m_XMLWriter.FixupXMLString( pPreBuildEventTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;

		case PS3_PREBUILDEVENT_Description:
			m_XMLWriter.Write( CFmtStrMax( "Description=\"%s\"", pPreBuildEventTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_PREBUILDEVENT_ExcludedFromBuild:
			m_XMLWriter.Write( CFmtStrMax( "ExcludedFromBuild=\"%s\"", BoolStringToTrueFalseString( pPreBuildEventTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;
		}
	}

	m_XMLWriter.PopNode( false );

	return true;
}	

bool CProjectGenerator_PS3::WriteCustomBuildTool( CCustomBuildTool *pCustomBuildTool )
{
	if ( !pCustomBuildTool )
	{
		// not an error, some tools n/a for aconfig
		return true;
	}

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( "Name=\"VCCustomBuildTool\"" );

	for ( int i = 0; i < pCustomBuildTool->m_PropertyStates.m_Properties.Count(); i++ )
	{
		switch ( pCustomBuildTool->m_PropertyStates.m_Properties[i].m_pToolProperty->m_nPropertyId )
		{
		case PS3_CUSTOMBUILDSTEP_CommandLine:
			m_XMLWriter.Write( CFmtStrMax( "CommandLine=\"%s\"", m_XMLWriter.FixupXMLString( pCustomBuildTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;

		case PS3_CUSTOMBUILDSTEP_Description:
			m_XMLWriter.Write( CFmtStrMax( "Description=\"%s\"", pCustomBuildTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_CUSTOMBUILDSTEP_Outputs:
			m_XMLWriter.Write( CFmtStrMax( "Outputs=\"%s\"", pCustomBuildTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_CUSTOMBUILDSTEP_AdditionalDependencies:
			m_XMLWriter.Write( CFmtStrMax( "AdditionalDependencies=\"%s\"", pCustomBuildTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;
		}
	}

	m_XMLWriter.PopNode( false );

	return true;
}	

bool CProjectGenerator_PS3::WriteSNCCompilerTool( CCompilerTool *pCompilerTool )
{
	if ( !pCompilerTool )
	{
		// not an error, some tools n/a for a config
		return true;
	}

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( "Name=\"VCCLCompilerTool\"" );

	// aggregates or purges state as needed
	CUtlString additionalOptions = "";

	for ( int i = 0; i < pCompilerTool->m_PropertyStates.m_Properties.Count(); i++ )
	{
		int nOrdinalValue = atoi( pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() );

		switch ( pCompilerTool->m_PropertyStates.m_Properties[i].m_pToolProperty->m_nPropertyId )
		{
		case PS3_SNCCOMPILER_AdditionalIncludeDirectories:
			m_XMLWriter.Write( CFmtStrMax( "AdditionalIncludeDirectories=\"%s\"", pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_SNCCOMPILER_PreprocessorDefinitions:
			m_XMLWriter.Write( CFmtStrMax( "PreprocessorDefinitions=\"%s\"", pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_SNCCOMPILER_ForceIncludes:
			m_XMLWriter.Write( CFmtStrMax( "ForcedIncludeFiles=\"%s\"", pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_SNCCOMPILER_GenerateDebugInformation:
			if ( nOrdinalValue )
			{
				additionalOptions += "-g ";
			}
			break;

		case PS3_SNCCOMPILER_Warnings:
			if ( nOrdinalValue )
			{
				additionalOptions += CFmtStrMax( "-Xdiag=%d ", nOrdinalValue );
			}
			break;

		case PS3_SNCCOMPILER_TreatMessagesAsErrors:
			if ( nOrdinalValue )
			{
				additionalOptions += CFmtStrMax( "-Xquit=%d ", nOrdinalValue );
			}
			break;

		case PS3_SNCCOMPILER_DisableSpecificWarnings:
			m_XMLWriter.Write( CFmtStrMax( "DisableSpecificWarnings=\"%s\"", pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_SNCCOMPILER_ObjectFileName:
			m_XMLWriter.Write( CFmtStrMax( "ObjectFile=\"%s\"", pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_SNCCOMPILER_CallprofHierarchicalProfiling:
			if ( nOrdinalValue )
			{
				additionalOptions += CFmtStrMax( "-Xcallprof=%d ", nOrdinalValue );
			}
			break;

		case PS3_SNCCOMPILER_OptimizationLevel:
			if ( nOrdinalValue == 0 )
			{
				// lack of any -0<x> means -O0
				additionalOptions += " ";
			}
			else if ( nOrdinalValue == 1 )
			{
				additionalOptions += "-O1 ";
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += "-O2 ";
			}
			else if ( nOrdinalValue == 3 )
			{
				additionalOptions += "-Os ";
			}
			else if ( nOrdinalValue == 4 )
			{
				additionalOptions += "-Od ";
			}
			break;

		case PS3_SNCCOMPILER_FastMath:
			if ( nOrdinalValue )
			{
				additionalOptions += CFmtStrMax( "-Xfastmath=%d ", nOrdinalValue );
			}
			break;

		case PS3_SNCCOMPILER_RelaxAliasChecking:
			if ( nOrdinalValue >= 0 )
			{
				additionalOptions += CFmtStrMax( "-Xrelaxalias=%d ", nOrdinalValue );
			}
			break;

		case PS3_SNCCOMPILER_BranchlessCompares:
			if ( nOrdinalValue )
			{
				additionalOptions += CFmtStrMax( "-Xbranchless=%d ", nOrdinalValue );
			}
			break;

		case PS3_SNCCOMPILER_UnrollLoops:
			if ( nOrdinalValue )
			{
				additionalOptions += CFmtStrMax( "-Xunrollssa=%d ", nOrdinalValue );
			}
			break;

		case PS3_SNCCOMPILER_AssumeAlignedPointers:
			if ( nOrdinalValue )
			{
				additionalOptions += "-Xassumecorrectalignment=1 ";
			}
			break;

		case PS3_SNCCOMPILER_AssumeCorrectSign:
			if ( nOrdinalValue )
			{
				additionalOptions += "-Xassumecorrectsign=1 ";
			}
			break;

		case PS3_SNCCOMPILER_TOCPointerPreservation:
			if ( nOrdinalValue )
			{
				additionalOptions += CFmtStrMax( "-Xnotocrestore=%d ", nOrdinalValue );
			}
			break;

		case PS3_SNCCOMPILER_InitializedDataPlacement:
			additionalOptions += CFmtStrMax( "-Xbss=%d ", nOrdinalValue );
			break;

		case PS3_SNCCOMPILER_PromoteFPConstantsToDoubles:
			if ( nOrdinalValue )
			{
				additionalOptions += "-Xfltconst=8 ";
			}
			break;

		case PS3_SNCCOMPILER_CCPPDialect:
			if ( nOrdinalValue )
			{
				if ( nOrdinalValue == 1 )
				{
					additionalOptions += "-Xc=ansi ";
				}
				else if ( nOrdinalValue == 2 )
				{
					additionalOptions += "-Xc=arm ";
				}
				else if ( nOrdinalValue == 3 )
				{
					additionalOptions += "-Xc=cp ";
				}
				else if ( nOrdinalValue == 4 )
				{
					additionalOptions += "-Xc=cfront ";
				}
				else if ( nOrdinalValue == 5 )
				{
					additionalOptions += "-Xc=knr ";
				}
			}
			break;

		case PS3_SNCCOMPILER_CPPExceptionsAndRTTIUsage:
			if ( nOrdinalValue == 0 )
			{
				additionalOptions += "-Xc-=rtti -Xc-=exceptions ";
			}
			else if ( nOrdinalValue == 1 )
			{
				additionalOptions += "-Xc+=rtti -Xc-=exceptions ";
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += "-Xc+=rtti -Xc+=exceptions ";
			}
			break;

		case PS3_SNCCOMPILER_DefaultCharUnsigned:
			if ( nOrdinalValue )
			{
				additionalOptions += "-Xchar=unsigned ";
			}
			break;

		case PS3_SNCCOMPILER_DefaultFPConstantsAsTypeFloat:
			if ( nOrdinalValue )
			{
				additionalOptions += "-Xsingleconst=1 ";
			}
			break;

		case PS3_SNCCOMPILER_BuiltInDefinitionForWCHAR_TType:
			if ( nOrdinalValue == 0 )	
			{
				additionalOptions += "-Xwchart=uint ";
			}
			else if ( nOrdinalValue == 1 )
			{
				additionalOptions += "-Xwchart=ulong ";
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += "-Xwchart=ushort ";
			}
			else if ( nOrdinalValue == 3 )
			{
				additionalOptions += "-Xwchart=uchar ";
			}
			else if ( nOrdinalValue == 4 )
			{
				additionalOptions += "-Xwchart=int ";
			}
			else if ( nOrdinalValue == 5 )
			{
				additionalOptions += "-Xwchart=long ";
			}
			else if ( nOrdinalValue == 6 )
			{
				additionalOptions += "-Xwchart=short ";
			}
			else if ( nOrdinalValue == 7 )
			{
				additionalOptions += "-Xwchart=char ";
			}
			else if ( nOrdinalValue == 8 )
			{
				additionalOptions += "-Xwchart=schar ";
			}
			break;

		case PS3_SNCCOMPILER_CreateUsePrecompiledHeader:
			if ( nOrdinalValue == 1 )
			{
				additionalOptions += CFmtStrMax( "--create_pch=&quot;%s&quot; ", pCompilerTool->m_PropertyStates.GetProperty( PS3_SNCCOMPILER_PrecompiledHeaderFile )->m_StringValue );
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += "--pch --pch_dir=&quot;$(IntDir)&quot; ";
			}
			else if ( nOrdinalValue == 3 )
			{
				additionalOptions += CFmtStrMax( "--use_pch=&quot;%s&quot; ", pCompilerTool->m_PropertyStates.GetProperty( PS3_SNCCOMPILER_PrecompiledHeaderFile )->m_StringValue );
			}
			break;

		case PS3_SNCCOMPILER_PrecompiledHeaderFile:
			// already accounted for
			break;

		case PS3_SNCCOMPILER_AdditionalOptions:
			if ( !pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.IsEmpty() )
			{
				additionalOptions += pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue;
				additionalOptions += " ";
			}
			break;
		}
	}

	if ( !additionalOptions.IsEmpty() )
	{
		m_XMLWriter.Write( CFmtStrMax( "AdditionalOptions=\"%s\"", additionalOptions.Get() ) );
	}

	m_XMLWriter.PopNode( false );

	return true;
}	

bool CProjectGenerator_PS3::WriteGCCCompilerTool( CCompilerTool *pCompilerTool )
{
	if ( !pCompilerTool )
	{
		// not an error, some tools n/a for a config
		return true;
	}

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( "Name=\"VCCLCompilerTool\"" );

	// aggregates or purges state as needed
	CUtlString additionalOptions = "";

	for ( int i = 0; i < pCompilerTool->m_PropertyStates.m_Properties.Count(); i++ )
	{
		int nOrdinalValue = atoi( pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() );

		switch ( pCompilerTool->m_PropertyStates.m_Properties[i].m_pToolProperty->m_nPropertyId )
		{
		case PS3_GCCCOMPILER_AdditionalIncludeDirectories:
			m_XMLWriter.Write( CFmtStrMax( "AdditionalIncludeDirectories=\"%s\"", m_XMLWriter.FixupXMLString( pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;

		case PS3_GCCCOMPILER_PreprocessorDefinitions:
			m_XMLWriter.Write( CFmtStrMax( "PreprocessorDefinitions=\"%s\"", pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GCCCOMPILER_ForceIncludes:
			m_XMLWriter.Write( CFmtStrMax( "ForcedIncludeFiles=\"%s\"", pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GCCCOMPILER_GenerateDebugInformation:
			if ( nOrdinalValue )
			{
				additionalOptions += "-g ";
			}
			break;

		case PS3_GCCCOMPILER_Warnings:
			if ( nOrdinalValue == 0 )	
			{
				additionalOptions += "-w ";
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += "-Wall ";
			}
			break;

		case PS3_GCCCOMPILER_ExtraWarnings:
			if ( nOrdinalValue )
			{
				additionalOptions += "-Wextra ";
			}
			break;

		case PS3_GCCCOMPILER_WarnLoadHitStores:
			break;

		case PS3_GCCCOMPILER_WarnMicrocodedInstruction:
			break;

		case PS3_GCCCOMPILER_TreatWarningsAsErrors:
			if ( nOrdinalValue )
			{
				additionalOptions += "-Werror ";
			}
			break;

		case PS3_GCCCOMPILER_ObjectFileName:
			m_XMLWriter.Write( CFmtStrMax( "ObjectFile=\"%s\"", pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GCCCOMPILER_CallprofHierarchicalProfiling:
			break;

		case PS3_GCCCOMPILER_SPURSUsage:
			if ( nOrdinalValue == 1 )	
			{
				additionalOptions += "-mspurs-job-initialize ";
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += "-mspurs-job ";
			}
			else if ( nOrdinalValue == 3 )
			{
				additionalOptions += "-mspurs-task ";
			}
			break;

		case PS3_GCCCOMPILER_OptimizationLevel:
			if ( nOrdinalValue == 0 )	
			{
				additionalOptions += "-O0 ";
			}
			else if ( nOrdinalValue == 1 )	
			{
				additionalOptions += "-O1 ";
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += "-O2 ";
			}
			else if ( nOrdinalValue == 3 )
			{
				additionalOptions += "-O3 ";
			}
			else if ( nOrdinalValue == 4 )
			{
				additionalOptions += "-Os ";
			}
			break;

		case PS3_GCCCOMPILER_FastMath:
			if ( nOrdinalValue )
			{
				additionalOptions += "-ffast-math ";
			}
			break;

		case PS3_GCCCOMPILER_NoStrictAliasing:
			if ( nOrdinalValue )
			{
				additionalOptions += "-fno-strict-aliasing ";
			}
			break;

		case PS3_GCCCOMPILER_UnrollLoops:
			if ( nOrdinalValue )
			{
				additionalOptions += "-funroll-loops ";
			}
			break;

		case PS3_GCCCOMPILER_InlineFunctionSizeLimit:
			if ( nOrdinalValue )
			{
				additionalOptions += CFmtStrMax( "-finline-limit=%d ", nOrdinalValue );
			}
			break;

		case PS3_GCCCOMPILER_TOCUsage:
			break;

		case PS3_GCCCOMPILER_SaveRestoreFunctions:
			break;

		case PS3_GCCCOMPILER_GenerateMicrocodedInstructions:
			break;

		case PS3_GCCCOMPILER_PositionIndependentCode:
			if ( nOrdinalValue )
			{
				additionalOptions += "-fpic ";
			}
			break;

		case PS3_GCCCOMPILER_FunctionSections:
			if ( nOrdinalValue )
			{
				additionalOptions += "-ffunction-sections ";
			}
			break;

		case PS3_GCCCOMPILER_DataSections:
			if ( nOrdinalValue )
			{
				additionalOptions += "-fdata-sections ";
			}
			break;

		case PS3_GCCCOMPILER_StackCheck:
			if ( nOrdinalValue )
			{
				additionalOptions += "-fstack-check ";
			}
			break;

		case PS3_GCCCOMPILER_CPPExceptionsAndRTTIUsage:
			if ( nOrdinalValue == 0 )
			{
				additionalOptions += "-fno-exceptions -fno-rtti ";
			}
			break;

		case PS3_GCCCOMPILER_CheckANSICompliance:
			if ( nOrdinalValue )
			{
				additionalOptions += "-ansi ";
			}
			break;

		case PS3_GCCCOMPILER_DefaultCharSigned:
			if ( nOrdinalValue )
			{
				additionalOptions += "-fsigned-char ";
			}
			break;

		case PS3_GCCCOMPILER_Permissive:
			if ( nOrdinalValue )
			{
				additionalOptions += "-fpermissive ";
			}
			break;

		case PS3_GCCCOMPILER_EnableMSExtensions:
			break;

		case PS3_GCCCOMPILER_RelaxCPPCompliance:
			if ( nOrdinalValue )
			{
				additionalOptions += "-fsource-402 ";
			}
			break;

		case PS3_GCCCOMPILER_AdditionalOptions:
			if ( !pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue.IsEmpty() )
			{
				additionalOptions += pCompilerTool->m_PropertyStates.m_Properties[i].m_StringValue;
				additionalOptions += " ";
			}
			break;
		}
	}

	if ( !additionalOptions.IsEmpty() )
	{
		m_XMLWriter.Write( CFmtStrMax( "AdditionalOptions=\"%s\"", additionalOptions.Get() ) );
	}

	m_XMLWriter.PopNode( false );

	return true;
}

bool CProjectGenerator_PS3::WritePreLinkEventTool( CPreLinkEventTool *pPreLinkEventTool )
{
	if ( !pPreLinkEventTool )
	{
		// not an error, some tools n/a for aconfig
		return true;
	}

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( "Name=\"VCPreLinkEventTool\"" );

	for ( int i = 0; i < pPreLinkEventTool->m_PropertyStates.m_Properties.Count(); i++ )
	{
		switch ( pPreLinkEventTool->m_PropertyStates.m_Properties[i].m_pToolProperty->m_nPropertyId )
		{
		case PS3_PRELINKEVENT_CommandLine:
			m_XMLWriter.Write( CFmtStrMax( "CommandLine=\"%s\"", m_XMLWriter.FixupXMLString( pPreLinkEventTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;

		case PS3_PRELINKEVENT_Description:
			m_XMLWriter.Write( CFmtStrMax( "Description=\"%s\"", pPreLinkEventTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_PRELINKEVENT_ExcludedFromBuild:
			m_XMLWriter.Write( CFmtStrMax( "ExcludedFromBuild=\"%s\"", BoolStringToTrueFalseString( pPreLinkEventTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;
		}
	}

	m_XMLWriter.PopNode( false );

	return true;
}

bool CProjectGenerator_PS3::WriteSNCLinkerTool( CLinkerTool *pLinkerTool )
{
	if ( !pLinkerTool )
	{
		// not an error, some tools n/a for a config
		return true;
	}

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( "Name=\"VCLinkerTool\"" );

	// aggregates or purges state as needed
	CUtlString additionalOptions = "";

	for ( int i = 0; i < pLinkerTool->m_PropertyStates.m_Properties.Count(); i++ )
	{
		int nOrdinalValue = atoi( pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() );

		switch ( pLinkerTool->m_PropertyStates.m_Properties[i].m_pToolProperty->m_nPropertyId )
		{
		case PS3_SNCLINKER_OutputFile:
			m_XMLWriter.Write( CFmtStrMax( "OutputFile=\"%s\"", pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_SNCLINKER_OutputFormat:
			if ( nOrdinalValue == 1 )
			{
				additionalOptions += "-oformat=fself ";
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += "-oformat=fself_npdrm ";
			}
			else if ( nOrdinalValue == 3 )
			{
				additionalOptions += "-oformat=prx -prx-with-runtime ";
			}
			else if ( nOrdinalValue == 4 )
			{
				additionalOptions += "-oformat=fsprx -prx-with-runtime ";
			}
			break;

		case PS3_SNCLINKER_AdditionalDependencies:
			m_XMLWriter.Write( CFmtStrMax( "AdditionalDependencies=\"%s\"", pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_SNCLINKER_AdditionalLibraryDirectories:
			m_XMLWriter.Write( CFmtStrMax( "AdditionalLibraryDirectories=\"%s\"", pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_SNCLINKER_IgnoreAllDefaultLibraries:
			if ( nOrdinalValue )
			{
				m_XMLWriter.Write( "IgnoreAllDefaultLibraries=\"true\"" );
			}
			break;

		case PS3_SNCLINKER_UsingExceptionHandling:
			if ( nOrdinalValue )
			{
				additionalOptions += "--exceptions ";
			}
			break;

		case PS3_SNCLINKER_TOCPointerElimination:
			if ( nOrdinalValue )
			{
				additionalOptions += "--notocrestore ";
			}
			break;

		case PS3_SNCLINKER_ForceSymbolReferences:
			m_XMLWriter.Write( CFmtStrMax( "ForceSymbolReferences=\"%s\"", pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_SNCLINKER_CallprofHierarchicalProfiling:
			if ( nOrdinalValue )
			{
				additionalOptions += "--callprof ";
			}
			break;

		case PS3_SNCLINKER_DebugInfoAndSymbolStripping:
			if ( nOrdinalValue == 1 )
			{
				additionalOptions += "-S ";
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += "-s ";
			}
			break;

		case PS3_SNCLINKER_UnusedFunctionAndDataStripping:
			if ( nOrdinalValue == 1 )
			{
				additionalOptions += "-strip-unused ";
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += "-strip-unused-data ";
			}
			break;

		case PS3_SNCLINKER_ImportLibrary:
			m_XMLWriter.Write( CFmtStrMax( "ImportLibrary=\"%s\"", pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_SNCLINKER_GenerateMapFile:
			if ( nOrdinalValue == 1 )
			{
				additionalOptions += CFmtStrMax( "-Map=&quot;%s&quot; ", pLinkerTool->m_PropertyStates.GetProperty( PS3_SNCLINKER_MapFileName )->m_StringValue );
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += CFmtStrMax( "-Map=&quot;%s&quot; -sn-full-map ", pLinkerTool->m_PropertyStates.GetProperty( PS3_SNCLINKER_MapFileName )->m_StringValue );
			}
			break;

		case PS3_SNCLINKER_MapFileName:
			m_XMLWriter.Write( CFmtStrMax( "MapFileName=\"%s\"", pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_SNCLINKER_LinkLibraryDependencies:
			m_XMLWriter.Write( CFmtStrMax( "LinkLibraryDependencies=\"%s\"", BoolStringToTrueFalseString( pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;

		case PS3_SNCLINKER_AdditionalOptions:
			if ( !pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.IsEmpty() )
			{
				additionalOptions += pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue;
				additionalOptions += " ";
			}
			break;
		}
	}

	if ( !additionalOptions.IsEmpty() )
	{
		m_XMLWriter.Write( CFmtStrMax( "AdditionalOptions=\"%s\"", additionalOptions.Get() ) );
	}

	m_XMLWriter.PopNode( false );

	return true;
}

bool CProjectGenerator_PS3::WriteGCCLinkerTool( CLinkerTool *pLinkerTool )
{
	if ( !pLinkerTool )
	{
		// not an error, some tools n/a for a config
		return true;
	}
	
	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( "Name=\"VCLinkerTool\"" );

	// aggregates or purges state as needed
	CUtlString additionalOptions = "";

	for ( int i = 0; i < pLinkerTool->m_PropertyStates.m_Properties.Count(); i++ )
	{
		int nOrdinalValue = atoi( pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() );

		switch ( pLinkerTool->m_PropertyStates.m_Properties[i].m_pToolProperty->m_nPropertyId )
		{
		case PS3_GCCLINKER_OutputFile:
			m_XMLWriter.Write( CFmtStrMax( "OutputFile=\"%s\"", pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GCCLINKER_AdditionalDependencies:
			m_XMLWriter.Write( CFmtStrMax( "AdditionalDependencies=\"%s\"", pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GCCLINKER_AdditionalLibraryDirectories:
			m_XMLWriter.Write( CFmtStrMax( "AdditionalLibraryDirectories=\"%s\"", pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GCCLINKER_ImportLibrary:
			m_XMLWriter.Write( CFmtStrMax( "ImportLibrary=\"%s\"", pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GCCLINKER_SPURSUsage:
			if ( nOrdinalValue == 1 )	
			{
				additionalOptions += "-mspurs-job-initialize ";
			}
			else if ( nOrdinalValue == 2 )
			{
				additionalOptions += "-mspurs-job ";
			}
			else if ( nOrdinalValue == 3 )
			{
				additionalOptions += "-mspurs-task ";
			}
			break;

		case PS3_GCCLINKER_PositionIndependentCode:
			if ( nOrdinalValue )	
			{
				additionalOptions += "-fpic ";
			}
			break;

		case PS3_GCCLINKER_EmitRelocations:
			if ( nOrdinalValue )	
			{
				additionalOptions += "-Wl,-q ";
			}
			break;

		case PS3_GCCLINKER_GarbageCollection:
			if ( nOrdinalValue )	
			{
				additionalOptions += "-Wl,--gc-sections ";
			}
			break;

		case PS3_GCCLINKER_GenerateMapFile:
			if ( nOrdinalValue == 1 )
			{
				additionalOptions += CFmtStrMax( "-Map=&quot;%s&quot; ", pLinkerTool->m_PropertyStates.GetProperty( PS3_GCCLINKER_MapFileName )->m_StringValue );
			}
			break;

		case PS3_GCCLINKER_MapFileName:
			m_XMLWriter.Write( CFmtStrMax( "MapFileName=\"%s\"", pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GCCLINKER_LinkLibraryDependencies:
			m_XMLWriter.Write( CFmtStrMax( "LinkLibraryDependencies=\"%s\"", BoolStringToTrueFalseString( pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;
		
		case PS3_SNCLINKER_AdditionalOptions:
			if ( !pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue.IsEmpty() )
			{
				additionalOptions += pLinkerTool->m_PropertyStates.m_Properties[i].m_StringValue;
				additionalOptions += " ";
			}
			break;
		}
	}

	if ( !additionalOptions.IsEmpty() )
	{
		m_XMLWriter.Write( CFmtStrMax( "AdditionalOptions=\"%s\"", additionalOptions.Get() ) );
	}

	m_XMLWriter.PopNode( false );

	return true;
}

bool CProjectGenerator_PS3::WriteLibrarianTool( CLibrarianTool *pLibrarianTool )
{
	if ( !pLibrarianTool )
	{
		// not an error, some tools n/a for aconfig
		return true;
	}

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( "Name=\"VCLibrarianTool\"" );

	for ( int i = 0; i < pLibrarianTool->m_PropertyStates.m_Properties.Count(); i++ )
	{
		switch ( pLibrarianTool->m_PropertyStates.m_Properties[i].m_pToolProperty->m_nPropertyId )
		{
		case PS3_LIBRARIAN_OutputFile:
			m_XMLWriter.Write( CFmtStrMax( "OutputFile=\"%s\"", pLibrarianTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;
		case PS3_LIBRARIAN_AdditionalDependencies:
			m_XMLWriter.Write( CFmtStrMax( "AdditionalDependencies=\"%s\"", pLibrarianTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;
		case PS3_LIBRARIAN_WholeArchive:
			// can't decode, seems broken
			break;
		case PS3_LIBRARIAN_LinkLibraryDependencies:
			m_XMLWriter.Write( CFmtStrMax( "LinkLibraryDependencies=\"%s\"", BoolStringToTrueFalseString( pLibrarianTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;
		}
	}

	m_XMLWriter.PopNode( false );

	return true;
}	

bool CProjectGenerator_PS3::WritePostBuildEventTool( CPostBuildEventTool *pPostBuildEventTool )
{
	if ( !pPostBuildEventTool )
	{
		// not an error, some tools n/a for aconfig
		return true;
	}

	m_XMLWriter.PushNode( "Tool" );

	m_XMLWriter.Write( "Name=\"VCPostBuildEventTool\"" );

	for ( int i = 0; i < pPostBuildEventTool->m_PropertyStates.m_Properties.Count(); i++ )
	{
		switch ( pPostBuildEventTool->m_PropertyStates.m_Properties[i].m_pToolProperty->m_nPropertyId )
		{
		case PS3_POSTBUILDEVENT_CommandLine:
			m_XMLWriter.Write( CFmtStrMax( "CommandLine=\"%s\"", m_XMLWriter.FixupXMLString( pPostBuildEventTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;

		case PS3_POSTBUILDEVENT_Description:
			m_XMLWriter.Write( CFmtStrMax( "Description=\"%s\"", pPostBuildEventTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_POSTBUILDEVENT_ExcludedFromBuild:
			m_XMLWriter.Write( CFmtStrMax( "ExcludedFromBuild=\"%s\"", BoolStringToTrueFalseString( pPostBuildEventTool->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;
		}
	}

	m_XMLWriter.PopNode( false );
	
	return true;
}	

const char *CProjectGenerator_PS3::BoolStringToTrueFalseString( const char *pValue )
{
	return Sys_StringToBool( pValue ) ? "true" : "false";
}

bool CProjectGenerator_PS3::WriteConfiguration( CProjectConfiguration *pConfig )
{
	if ( pConfig->m_bIsFileConfig )
	{
		m_XMLWriter.PushNode( "FileConfiguration" );
	}
	else
	{
		m_XMLWriter.PushNode( "Configuration" );
	}

	const char *pOutputName = "???";
	if ( !V_stricmp( pConfig->m_Name.Get(), "debug" ) )
	{
		if ( m_pVCProjGenerator->GetVSIType() == PS3_VSI_TYPE_SNC )
		{
			pOutputName = "PS3SNCDebug|Win32";
		}
		else if ( m_pVCProjGenerator->GetVSIType() == PS3_VSI_TYPE_GCC )
		{
			pOutputName = "PS3GCCDebug|Win32";
		}
	}
	else if ( !V_stricmp( pConfig->m_Name.Get(), "release" ) )
	{
		if ( m_pVCProjGenerator->GetVSIType() == PS3_VSI_TYPE_SNC )
		{
			pOutputName = "PS3SNCRelease|Win32";
		}
		else if ( m_pVCProjGenerator->GetVSIType() == PS3_VSI_TYPE_GCC )
		{
			pOutputName = "PS3GCCRelease|Win32";
		}
	}
	else
	{
		return false;
	}

	m_XMLWriter.Write( CFmtStrMax( "Name=\"%s\"", pOutputName ) );

	// write configuration properties
	for ( int i = 0; i < pConfig->m_PropertyStates.m_Properties.Count(); i++ )
	{
		switch ( pConfig->m_PropertyStates.m_Properties[i].m_pToolProperty->m_nPropertyId )
		{
		case PS3_GENERAL_ConfigurationType:
			m_XMLWriter.Write( CFmtStrMax( "ConfigurationType=\"%s\"", pConfig->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GENERAL_ExcludedFromBuild:
			m_XMLWriter.Write( CFmtStrMax( "ExcludedFromBuild=\"%s\"", BoolStringToTrueFalseString( pConfig->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) ) );
			break;

		case PS3_GENERAL_OutputDirectory:
			m_XMLWriter.Write( CFmtStrMax( "OutputDirectory=\"%s\"", pConfig->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GENERAL_IntermediateDirectory:
			m_XMLWriter.Write( CFmtStrMax( "IntermediateDirectory=\"%s\"", pConfig->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GENERAL_ExtensionsToDeleteOnClean:
			m_XMLWriter.Write( CFmtStrMax( "DeleteExtensionsOnClean=\"%s\"", pConfig->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GENERAL_BuildLogFile:
			m_XMLWriter.Write( CFmtStrMax( "BuildLogFile=\"%s\"", pConfig->m_PropertyStates.m_Properties[i].m_StringValue.Get() ) );
			break;

		case PS3_GENERAL_SystemIncludeDependencies:
			// ignoring
			break;

		case PS3_GENERAL_SaveDebuggerPropertiesInProject:
			// ignoring
			break;
		}
	}
	
	m_XMLWriter.Write( ">" );

	if ( !WritePreBuildEventTool( pConfig->GetPreBuildEventTool() ) )
		return false;

	if ( !WriteCustomBuildTool( pConfig->GetCustomBuildTool() ) )
		return false;

	if ( m_pVCProjGenerator->GetVSIType() == PS3_VSI_TYPE_SNC )
	{
		if ( !WriteSNCCompilerTool( pConfig->GetCompilerTool() ) )
			return false;
	}
	else if ( m_pVCProjGenerator->GetVSIType() == PS3_VSI_TYPE_GCC )
	{
		if ( !WriteGCCCompilerTool( pConfig->GetCompilerTool() ) )
			return false;
	}

	if ( !WritePreLinkEventTool( pConfig->GetPreLinkEventTool() ) )
		return false;

	if ( m_pVCProjGenerator->GetVSIType() == PS3_VSI_TYPE_SNC )
	{
		if ( !WriteSNCLinkerTool( pConfig->GetLinkerTool() ) )
			return false;
	}
	else if ( m_pVCProjGenerator->GetVSIType() == PS3_VSI_TYPE_GCC )
	{
		if ( !WriteGCCLinkerTool( pConfig->GetLinkerTool() ) )
			return false;
	}

	if ( !WriteLibrarianTool( pConfig->GetLibrarianTool() ) )
		return false;

	if ( !WritePostBuildEventTool( pConfig->GetPostBuildEventTool() ) )
		return false;
	
	m_XMLWriter.PopNode( true );

	return true;
}

bool CProjectGenerator_PS3::WriteToXML()
{
	m_XMLWriter.PushNode( "VisualStudioProject" );
	m_XMLWriter.Write( "ProjectType=\"Visual C++\"" );
	m_XMLWriter.Write( "Version=\"8.00\"" );
	m_XMLWriter.Write( CFmtStrMax( "Name=\"%s\"", m_pVCProjGenerator->GetProjectName().Get() ) );
	m_XMLWriter.Write( CFmtStrMax( "ProjectGUID=\"%s\"", m_pVCProjGenerator->GetGUIDString().Get() ) );
	m_XMLWriter.Write( CFmtStrMax( "RootNamespace=\"%s\"", m_pVCProjGenerator->GetProjectName().Get() ) );
	if ( g_pVPC->BUseP4SCC() )
		m_XMLWriter.Write( "SccProjectName=\"Perforce Project\"\nSccLocalPath=\"..\"\nSccProvider=\"MSSCCI:Perforce SCM\"\n" );
	m_XMLWriter.Write( ">" );

	m_XMLWriter.PushNode( "Platforms" );
	m_XMLWriter.PushNode( "Platform" );
	m_XMLWriter.Write( "Name=\"win32\"" );
	m_XMLWriter.PopNode( false );
	m_XMLWriter.PopNode( true );

	m_XMLWriter.PushNode( "ToolFiles" );
	m_XMLWriter.PopNode( true );

	CUtlVector< CUtlString > configurationNames;
	m_pVCProjGenerator->GetAllConfigurationNames( configurationNames ); 

	// write the root configurations
	m_XMLWriter.PushNode( "Configurations" );
	for ( int i = 0; i < configurationNames.Count(); i++ )
	{
		CProjectConfiguration *pConfiguration = NULL;
		if ( m_pVCProjGenerator->GetRootConfiguration( configurationNames[i].Get(), &pConfiguration ) )
		{
			if ( !WriteConfiguration( pConfiguration ) )
				return false;
		}
	}
	m_XMLWriter.PopNode( true );

	m_XMLWriter.PushNode( "References" );
	m_XMLWriter.PopNode( true );

	m_XMLWriter.PushNode( "Files" );

	CProjectFolder *pRootFolder = m_pVCProjGenerator->GetRootFolder();
	for ( int iIndex = pRootFolder->m_Folders.Head(); iIndex != pRootFolder->m_Folders.InvalidIndex(); iIndex = pRootFolder->m_Folders.Next( iIndex ) )
	{
		if ( !WriteFolder( pRootFolder->m_Folders[iIndex] ) )
			return false;
	}

	for ( int iIndex = pRootFolder->m_Files.Head(); iIndex != pRootFolder->m_Files.InvalidIndex(); iIndex = pRootFolder->m_Files.Next( iIndex ) )
	{
		if ( !WriteFile( pRootFolder->m_Files[iIndex] ) )
			return false;
	}

	m_XMLWriter.PopNode( true );

	m_XMLWriter.PushNode( "Globals" );
	m_XMLWriter.PopNode( true );

	m_XMLWriter.PopNode( true );

	return true;
}

bool CProjectGenerator_PS3::Save( const char *pOutputFilename )
{
	if ( !m_XMLWriter.Open( pOutputFilename ) )
		return false;

	bool bValid = WriteToXML();

	m_XMLWriter.Close();

	if ( bValid )
	{
		// Not sure what this file does or why, but we emit it and
		// its part of a default SN project. The custom build steps
		// were copied are hosted in all the vpc ps3 base scripts.
		FILE *fp = fopen( "vsi.nul", "wt" );
		if ( fp )
		{
			fprintf( fp, "SN Visual Studio Integration\n" );
			fprintf( fp, "IMPORTANT: Do not remove the custom build step for this file\n" );
			fclose( fp );
		}
	}

	return bValid;
}



