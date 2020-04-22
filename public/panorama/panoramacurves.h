//=========== Copyright Valve Corporation, All rights reserved. ===============//
//
// Purpose: Public header for panorama UI framework
//
//
//=============================================================================//

#ifndef PANORAMA_PANORAMACURVES_H
#define PANORAMA_PANORAMACURVES_H
#pragma once

#include "panoramatypes.h"

namespace panorama
{

void GetAnimationCurveControlPoints( EAnimationTimingFunction eAnimation, Vector2D points[4] );

};

#endif

