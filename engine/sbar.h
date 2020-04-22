//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
#ifndef SBAR_H
#define SBAR_H
#pragma once

// the status bar is only redrawn if something has changed, but if anything
// does, the entire thing will be redrawn for the next vid.numpages frames.
void Sbar_Draw (void);
// called every frame by screen

#endif // SBAR_H

