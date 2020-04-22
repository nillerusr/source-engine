//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "tier0/platform.h"
#include "linear_solver.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// BUGBUG: Remove/tune this number somehow!!!
#define DET_EPSILON		1e-6f
	

// assumes square matrix!
// ONLY SUPPORTS 2x2, 3x3, and 4x4
float Det( float *matrix, int rows )
{
	if ( rows == 2 )
	{
		return matrix[0]*matrix[3] - matrix[1]*matrix[2];
	}
	if ( rows == 3 )
	{
		return matrix[0]*matrix[4]*matrix[8] - matrix[0]*matrix[7]*matrix[5] - matrix[1]*matrix[3]*matrix[8] +
				matrix[1]*matrix[5]*matrix[6] + matrix[2]*matrix[3]*matrix[7] - matrix[2]*matrix[4]*matrix[6];
	}

	// ERROR no more than 4x4
	if ( rows != 4 )
		return 0;

	// UNDONE: Generalize this to NxN
	float tmp[9];
	float det = 0.f;
	// do 4 3x3 dets
	for ( int i = 0; i < 4; i++ )
	{
		// develop on row 0
		int out = 0;
		for ( int j = 1; j < 4; j++ )
		{
			// iterate each column and 
			for ( int k = 0; k < 4; k++ )
			{
				if ( k == i )
					continue;
				tmp[out] = matrix[(j*rows)+k];
				out++;
			}
		}
		if ( i & 1 )
		{
			det -= matrix[i]*Det(tmp,3);
		}
		else
		{
			det += matrix[i]*Det(tmp,3);
		}
	}

	return det;
}

float *SolveCramer( const float *matrix, int rows, int columns )
{
	// max 4 equations, 4 unknowns (until determinant routine is more general)
	float tmpMain[16*16], tmpSub[16*16];
	static float solution[16];

	int i, j;

	if ( rows > 4 || columns > 5 )
	{
		return NULL;
	}


	int outCol = columns - 1;
	// copy out the square matrix
	for ( i = 0; i < rows; i++ )
	{
		memcpy( tmpMain + (i*outCol), matrix + i*columns, sizeof(float)*outCol );
	}

	float detMain = Det( tmpMain, rows );

	// probably degenerate!
	if ( fabs(detMain) < DET_EPSILON )
	{
		return NULL;
	}

	for ( i = 0; i < rows; i++ )
	{
		// copy the square matrix
		memcpy( tmpSub, tmpMain, sizeof(float)*rows*rows );
		
		// copy the column of constants over the row
		for ( j = 0; j < rows; j++ )
		{
			tmpSub[i+j*outCol] = matrix[j*columns+columns-1];
		}
		float det = Det( tmpSub, rows );
		solution[i] = det / detMain;
	}

	return solution;
}
