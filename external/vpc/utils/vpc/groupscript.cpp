//========= Copyright © 1996-2006, Valve Corporation, All rights reserved. ============//
//
// Purpose: VPC
//
//=====================================================================================//

#include "vpc.h"

// This keyword works in both group and project scripts
extern void VPC_SharedKeyword_Conditional();


//-----------------------------------------------------------------------------
//	VPC_Group_FindOrCreateProject
//
//-----------------------------------------------------------------------------
projectIndex_t VPC_Group_FindOrCreateProject( const char *pName, bool bCreate )
{
	for ( int i = 0; i < g_pVPC->m_Projects.Count(); i++ )
	{
		if ( !V_stricmp( pName, g_pVPC->m_Projects[i].name.String() ) )
		{
			return i;
		}
	}

	if ( !bCreate )
		return INVALID_INDEX;

	int index = g_pVPC->m_Projects.AddToTail();
	g_pVPC->m_Projects[index].name = pName;

	return index;
}

//-----------------------------------------------------------------------------
//	VPC_Group_CreateGroup
//
//-----------------------------------------------------------------------------
groupIndex_t VPC_Group_CreateGroup()
{
	groupIndex_t index = g_pVPC->m_Groups.AddToTail();
	return index;
}

//-----------------------------------------------------------------------------
//	VPC_Group_FindOrCreateGroupTag
//
//-----------------------------------------------------------------------------
groupTagIndex_t VPC_Group_FindOrCreateGroupTag( const char *pName, bool bCreate )
{
	for (int i=0; i<g_pVPC->m_GroupTags.Count(); i++)
	{
		if ( !V_stricmp( pName, g_pVPC->m_GroupTags[i].name.String() ) )
			return i;
	}

	if ( !bCreate )
		return INVALID_INDEX;

	groupTagIndex_t index = g_pVPC->m_GroupTags.AddToTail();
	g_pVPC->m_GroupTags[index].name = pName;

	return index;
}

//-----------------------------------------------------------------------------
//	VPC_GroupKeyword_Games
//
//-----------------------------------------------------------------------------
void VPC_GroupKeyword_Games()
{
	const char	*pToken;

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
		g_pVPC->VPCSyntaxError();

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
			g_pVPC->VPCSyntaxError();

		if ( !V_stricmp( pToken, "}" ) )
		{
			// end of section
			break;
		}
		else
		{
			g_pVPC->FindOrCreateConditional( pToken, true, CONDITIONAL_GAME );
		}
	}
}

//-----------------------------------------------------------------------------
//	VPC_GroupKeyword_Group
//
//-----------------------------------------------------------------------------
void VPC_GroupKeyword_Group()
{
	const char			*pToken;
	bool			bFirstToken = true;
	groupIndex_t	groupIndex;
	projectIndex_t	projectIndex;

	groupIndex = VPC_Group_CreateGroup();

	while ( 1 )
	{
		if ( !bFirstToken )
		{
			pToken = g_pVPC->GetScript().PeekNextToken( false );
			if ( !pToken || !pToken[0] )
				break;
		}
		else
		{
			bFirstToken = false;
		}

		pToken = g_pVPC->GetScript().GetToken( false );
		if ( !pToken || !pToken[0] )
			g_pVPC->VPCSyntaxError();

		// specified tag now builds this group
		groupTagIndex_t groupTagIndex = VPC_Group_FindOrCreateGroupTag( pToken, true );
		g_pVPC->m_GroupTags[groupTagIndex].groups.AddToTail( groupIndex );
	}

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
		g_pVPC->VPCSyntaxError();

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
			g_pVPC->VPCSyntaxError();

		if ( !V_stricmp( pToken, "}" ) )
		{
			// end of section
			break;
		}
		else
		{
			projectIndex = VPC_Group_FindOrCreateProject( pToken, false );
			if ( projectIndex != INVALID_INDEX )
			{
				int index = g_pVPC->m_Groups[groupIndex].projects.AddToTail();
				g_pVPC->m_Groups[groupIndex].projects[index] = projectIndex;
			}
			else
			{
				g_pVPC->VPCWarning( "No Project %s defined, ignoring.", pToken );
				continue;
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	VPC_GroupKeyword_Project
//
//-----------------------------------------------------------------------------
void VPC_GroupKeyword_Project()
{
	const char *pToken;

	pToken = g_pVPC->GetScript().GetToken( false );
	if ( !pToken || !pToken[0] )
		g_pVPC->VPCSyntaxError();

	if ( VPC_Group_FindOrCreateProject( pToken, false ) != INVALID_INDEX )
	{
		// already defined
		g_pVPC->VPCWarning( "project %s already defined", pToken );
		g_pVPC->VPCSyntaxError();
	}

	projectIndex_t projectIndex = VPC_Group_FindOrCreateProject( pToken, true );

	// create a default group that contains just this project
	groupIndex_t groupIndex = VPC_Group_CreateGroup();
	g_pVPC->m_Groups[groupIndex].projects.AddToTail( projectIndex );

	// create a default tag that matches the project name
	groupTagIndex_t groupTagIndex = VPC_Group_FindOrCreateGroupTag( pToken, true );
	g_pVPC->m_GroupTags[groupTagIndex].groups.AddToTail( groupIndex );
	g_pVPC->m_GroupTags[groupTagIndex].bSameAsProject = true;

	pToken = g_pVPC->GetScript().GetToken( true );
	if ( !pToken || !pToken[0] || V_stricmp( pToken, "{" ) )
		g_pVPC->VPCSyntaxError();

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
			g_pVPC->VPCSyntaxError();

		if ( !V_stricmp( pToken, "}" ) )
		{
			// end of section
			break;
		}
		else
		{
			scriptIndex_t scriptIndex = g_pVPC->m_Projects[projectIndex].scripts.AddToTail();
			g_pVPC->m_Projects[projectIndex].scripts[scriptIndex].name = pToken;

			pToken = g_pVPC->GetScript().PeekNextToken( false );
			if ( pToken && pToken[0] )
			{
				pToken = g_pVPC->GetScript().GetToken( false );
				g_pVPC->m_Projects[projectIndex].scripts[scriptIndex].m_condition = pToken;
			}
		}
	}
}


//-----------------------------------------------------------------------------
//	VPC_ParseGroupScript
//
//-----------------------------------------------------------------------------
void VPC_ParseGroupScript( const char *pScriptName )
{
	char	szScriptName[MAX_PATH];
	const char	*pToken;

	// caller's pointer is aliased
	strcpy( szScriptName, pScriptName );
	V_FixSlashes( szScriptName );

	g_pVPC->VPCStatus( false, "Parsing: %s", szScriptName );
	g_pVPC->GetScript().PushScript( szScriptName );

	while ( 1 )
	{
		pToken = g_pVPC->GetScript().GetToken( true );
		if ( !pToken || !pToken[0] )
		{
			// end of file
			break;
		}

		if ( !V_stricmp( pToken, "$include" ) )
		{
			pToken = g_pVPC->GetScript().GetToken( false );
			if ( !pToken || !pToken[0] )
			{
				// end of file
				g_pVPC->VPCSyntaxError();
			}

			// recurse into and run
			VPC_ParseGroupScript( pToken );
		}
		else if ( !V_stricmp( pToken, "$games" ) )
		{
			VPC_GroupKeyword_Games();
		}
		else if ( !V_stricmp( pToken, "$group" ) )
		{
			VPC_GroupKeyword_Group();
		}
		else if ( !V_stricmp( pToken, "$project" ) )
		{
			VPC_GroupKeyword_Project();
		}
		else if ( !V_stricmp( pToken, "$Conditional" ) )
		{
			VPC_SharedKeyword_Conditional();
		}
		else
		{
			g_pVPC->VPCSyntaxError();
		}
	}

	g_pVPC->GetScript().PopScript();
}

//-----------------------------------------------------------------------------
//	Collect all the +XXX, remove all the -XXX
//	This allows removal to be the expected trumping operation.
//-----------------------------------------------------------------------------
void CVPC::GenerateBuildSet( CProjectDependencyGraph &dependencyGraph )
{
	// process +XXX commands
	for ( int i = 0; i < m_BuildCommands.Count(); i++ )
	{
		const char *pCommand = m_BuildCommands[i].Get();
		if ( pCommand[0] == '-' )
			continue;

		groupTagIndex_t groupTagIndex = VPC_Group_FindOrCreateGroupTag( pCommand+1, false );
		if ( groupTagIndex == INVALID_INDEX )
			continue;
		groupTag_t *pGroupTag = &g_pVPC->m_GroupTags[groupTagIndex];

		CUtlVector<projectIndex_t> projectsToAdd;

		for ( int j=0; j<pGroupTag->groups.Count(); j++ )
		{
			group_t *pGroup = &g_pVPC->m_Groups[pGroupTag->groups[j]];
			for ( int k=0; k<pGroup->projects.Count(); k++ )
			{
				projectIndex_t targetProject = pGroup->projects[k];
				if ( pCommand[0] == '*' )
				{
					// Add this project and any projects that depend on it.
					if ( !dependencyGraph.HasGeneratedDependencies() )
						dependencyGraph.BuildProjectDependencies( BUILDPROJDEPS_CHECK_ALL_PROJECTS, m_pPhase1Projects );

					dependencyGraph.GetProjectDependencyTree( targetProject, projectsToAdd, false );
				}
				else if ( pCommand[0] == '@' )
				{
					// Add this project and any projects that it depends on.
					if ( !dependencyGraph.HasGeneratedDependencies() )
						dependencyGraph.BuildProjectDependencies( BUILDPROJDEPS_CHECK_ALL_PROJECTS, m_pPhase1Projects );

					dependencyGraph.GetProjectDependencyTree( targetProject, projectsToAdd, true );
				}
				else
				{
					projectsToAdd.AddToTail( targetProject );
				}
			}
		}

		// Add all the projects in the list.
		for ( int j=0; j < projectsToAdd.Count(); j++ )
		{
			projectIndex_t targetProject = projectsToAdd[j];

			if ( g_pVPC->m_TargetProjects.Find( targetProject ) == -1 )
			{
				g_pVPC->m_TargetProjects.AddToTail( targetProject );
			}
		}
	}

	// process -XXX commands, explicitly remove tagge projects
	for ( int i=0; i<m_BuildCommands.Count(); i++ )
	{
		const char *pCommand = m_BuildCommands[i].Get();
		if ( pCommand[0] == '+' || pCommand[0] == '*' || pCommand[0] == '@' )
			continue;

		groupTagIndex_t groupTagIndex = VPC_Group_FindOrCreateGroupTag( pCommand+1, false );
		if ( groupTagIndex == INVALID_INDEX )
			continue;
		groupTag_t *pGroupTag = &g_pVPC->m_GroupTags[groupTagIndex];

		for ( int j=0; j<pGroupTag->groups.Count(); j++ )
		{
			group_t *pGroup = &g_pVPC->m_Groups[pGroupTag->groups[j]];
			for ( int k=0; k<pGroup->projects.Count(); k++ )
			{
				g_pVPC->m_TargetProjects.FindAndRemove( pGroup->projects[k] );
			}
		}
	}
}
