//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: See notes below
//
//=============================================================================

#include "mdlobjects/mdlobjects.h"
#include "movieobjects/movieobjects.h"

// YOU MUST INCLUDE THIS FILE INTO ANY PROJECT WHICH USES THE mdlobjects.lib FILE
// This hack causes the class factories for the element types to be imported into the compiled code...

// MDL types
USING_ELEMENT_FACTORY( DmeHitbox );
USING_ELEMENT_FACTORY( DmeHitboxSet );
USING_ELEMENT_FACTORY( DmeBodyPart );
USING_ELEMENT_FACTORY( DmeBlankBodyPart );
USING_ELEMENT_FACTORY( DmeLOD );
USING_ELEMENT_FACTORY( DmeLODList );
USING_ELEMENT_FACTORY( DmeCollisionModel );
USING_ELEMENT_FACTORY( DmeBodyGroup );
USING_ELEMENT_FACTORY( DmeBodyGroupList );
USING_ELEMENT_FACTORY( DmeMdlList );
USING_ELEMENT_FACTORY( DmeBoneWeight );
USING_ELEMENT_FACTORY( DmeBoneMask );
USING_ELEMENT_FACTORY( DmeBoneMaskList );
USING_ELEMENT_FACTORY( DmeSequence );
USING_ELEMENT_FACTORY( DmeSequenceList );
USING_ELEMENT_FACTORY( DmeBoneFlexDriverControl );
USING_ELEMENT_FACTORY( DmeBoneFlexDriver );
USING_ELEMENT_FACTORY( DmeBoneFlexDriverList );
