//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef HARDWAREMATRIXSTATE_H
#define HARDWAREMATRIXSTATE_H
#pragma once

// This emulates the hardware matrix palette and keeps up with 
// matrix usage, LRU's matrices, etc.
class CHardwareMatrixState
{
public:
	CHardwareMatrixState();

	void Init( int numHardwareMatrices );
	
	// return false if there is no slot for this matrix.
	bool AllocateMatrix( int globalMatrixID );

	// deallocate the least recently used matrix
	void DeallocateLRU( void );
	void DeallocateLRU( int n );

	// return true if a matrix is allocate.
	bool IsMatrixAllocated( int globalMatrixID ) const;
	
	// flush usage flags - signifies that none of the matrices are being used in the current strip
	// do this when starting a new strip.
	void SetAllUnused();

	void DeallocateAll();

	// save the complete state of the hardware matrices
	void SaveState();

	// restore the complete state of the hardware matrices
	void RestoreState();

	// Returns the number of free + unsed matrices
	int AllocatedMatrixCount() const;
	int	FreeMatrixCount() const;

	int GetNthBoneGlobalID( int n ) const;

	void DumpState( void );

private:
	int FindHardwareMatrix( int globalMatrixID );
	int FindLocalLRUIndex( void );

	// Increment and return LRU counter.
	struct MatrixState_t
	{
		bool allocated;
		int lastUsageID;
		int globalMatrixID;
	};

	int m_LRUCounter;
	int m_NumMatrices;
	int m_AllocatedMatrices;

	MatrixState_t *m_matrixState;
	MatrixState_t *m_savedMatrixState;
};

#endif // HARDWAREMATRIXSTATE_H
