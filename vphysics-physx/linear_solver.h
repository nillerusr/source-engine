//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef LINEAR_SOLVER_H
#define LINEAR_SOLVER_H
#ifdef _WIN32
#pragma once
#endif


// Take the determinant of a matrix.  
// NOTE: ONLY SUPPORTS 2x2, 3x3, and 4x4
extern float Det( float *matrix, int rows );

// solve a system of linear equations using Cramer's rule (only supports up to 4 variables due to limits on Det())
extern float *SolveCramer( const float *matrix, int rows, int columns );

#endif // LINEAR_SOLVER_H
