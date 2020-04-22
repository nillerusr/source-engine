//====== Copyright © 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: 
//
//=============================================================================

#ifndef IBASESOLUTIONGENERATOR_H
#define IBASESOLUTIONGENERATOR_H
#ifdef _WIN32
#pragma once
#endif


#include "dependencies.h"


class IBaseSolutionGenerator
{
public:
	virtual void GenerateSolutionFile( const char *pSolutionFilename, CUtlVector<CDependency_Project*> &projects ) = 0;	
};


#endif // IBASESOLUTIONGENERATOR_H
