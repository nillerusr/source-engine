//========= Copyright Valve Corporation, All rights reserved. ============//

#include "platform.h"

#include <vector>
#include <math.h>
#include <assert.h>
#include <float.h>

// Prevent OpenCV and other Vortex modules from using D3D (they will want D3D11, and we're a D3D9 game)
#define NO_D3D

