//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef P4SLN_H
#define P4SLN_H
#ifdef _WIN32
#pragma once
#endif


void GenerateSolutionForPerforceChangelist( CProjectDependencyGraph &dependencyGraph, CUtlVector<int> &changelists, IBaseSolutionGenerator *pGenerator, const char *pSolutionFilename );


#endif // P4SLN_H
