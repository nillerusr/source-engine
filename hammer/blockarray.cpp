//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include <windows.h>
#include <stdio.h>

template <class T, int nBlockSize, int nMaxBlocks>
class BlockArray
{
public:
	BlockArray()
	{
		nCount = nBlocks = 0;
	}
	~BlockArray()
	{
		GetBlocks(0);
	}

	T& operator[] (int iIndex);
	
	void SetCount(int nObjects);
	int GetCount() { return nCount; }

private:
	T * Blocks[nMaxBlocks+1];
	short nCount;
	short nBlocks;
	void GetBlocks(int nNewBlocks);
};

/*
template <class T, int nBlockSize, int nMaxBlocks>
BlockArray<T,BlockSize,nMaxBlocks>::BlockArray()
{
	nCount = nBlocks = 0;
}

template <class T, int nBlockSize, int nMaxBlocks>
BlockArray<T,BlockSize,nMaxBlocks>::~BlockArray()
{
	GetBlocks(0);	// free blocks	
}
*/

template <class T, int nBlockSize, int nMaxBlocks>
void BlockArray<T,nBlockSize,nMaxBlocks>::
	GetBlocks(int nNewBlocks)
{
	for(int i = nBlocks; i < nNewBlocks; i++)
	{
		Blocks[i] = new T[nBlockSize];
	}
	for(i = nNewBlocks; i < nBlocks; i++)
	{
		delete[] Blocks[i];
	}

	nBlocks = nNewBlocks;
}

template <class T, int nBlockSize, int nMaxBlocks>
void BlockArray<T,nBlockSize,nMaxBlocks>::
	SetCount(int nObjects)
{
	if(nObjects == nCount)
		return;

	// find the number of blocks required by nObjects
	int nNewBlocks = (nObjects / nBlockSize) + 1;
	if(nNewBlocks != nBlocks)
		GetBlocks(nNewBlocks);
	nCount = nObjects;
}

template <class T, int nBlockSize, int nMaxBlocks>
T& BlockArray<T,nBlockSize,nMaxBlocks>::operator[] (int iIndex)
{
	if(iIndex >= nCount)
		SetCount(iIndex+1);
	return Blocks[iIndex / nBlockSize][iIndex % nBlockSize];
}

typedef struct
{
	char Name[128];
	int iValue;
} Buffy;

void main(void)
{
	BlockArray<Buffy, 16, 16> Buffies;

	for(int i = 0; i < 256; i++)
	{
		Buffies[i].iValue = i;
		strcpy(Buffies[i].Name, "Buk bUk buK");
	}

	for(i = 0; i < 256; i++)
	{
		printf("%d: %s\n", Buffies[i].iValue, Buffies[i].Name);
	}

	Buffies.SetCount(10);
}