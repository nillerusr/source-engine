//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

//*********** (C) Copyright 2000 Valve, L.L.C. All rights reserved. ***********
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//
//*****************************************************************************
//
// Contents:
//
//		
//
// Authors:
//
// Target restrictions:
//
// Tool restrictions:
//
// Things to do:
//
//		
//
//*****************************************************************************

#ifndef INCLUDED_STEAM_BOOTSTRAPPER_H
#define INCLUDED_STEAM_BOOTSTRAPPER_H

#if defined(_MSC_VER) && (_MSC_VER > 1000)
#pragma once
#endif


//*****************************************************************************
//
// 'Local' build control section.
//
//*****************************************************************************

#if  (BUILD_MODE == BUILD_MODE_RELEASE_NORMAL)  ||  (BUILD_MODE == BUILD_MODE_RELEASE_TEST)


#elif BUILD_MODE == BUILD_MODE_DEBUG_NORMAL


#else
	// 'Safe' default settings.  This allows new build modes to be added to the 
	// project without requiring the manual updating of all 'local build control' 
	// sections in every module and header file.

#endif



//*****************************************************************************
//
// Include files required by this header.
//
// Note: Do NOT place any 'using' directives or declarations in header files - 
// put them at the top of the source files that require them.  
// Use fully-qualified names in header files. 
//
//*****************************************************************************




//*****************************************************************************
//
// Exported constants and macros.
// - Wrap these definitions in a namespace whenever possible
//
//*****************************************************************************


namespace 
{
	// constant definitions here
}

#define szSteamBootStrapperIconIdEnvVar  "__STEAM_BOOTSTRAPPER_ICON_ID__"

//*****************************************************************************
//
// Exported scalar type and enumerated type definitions.
// - Wrap these definitions in a namespace whenever possible
//
//*****************************************************************************


namespace 
{
// scalar and enumerated type definitions here
}



//*****************************************************************************
//
// Exported class, structure, and complex type definitions.
// - Wrap these definitions in a namespace whenever possible
//
//*****************************************************************************


namespace 
{
	// class, structure, and complex type definitions here
}



//*****************************************************************************
//
// Exported function prototypes
// - Wrap these definitions in a namespace whenever possible
// - declared extern here, and defined without storage class in the source file.
//
//*****************************************************************************


namespace 
{
	// function prototypes here
}



//*****************************************************************************
//
// Exported variable and data declarations
// - Wrap these definitions in a namespace whenever possible
// - declared extern here, and defined without storage class in the source file.
//
//*****************************************************************************


namespace 
{
	// variable and data declarations here
}



//*****************************************************************************
//
// Inline function definitions.
//
//*****************************************************************************




#endif
