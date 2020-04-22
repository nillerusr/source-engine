//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//


#include <string.h>
#include "fileimage.h"

#include "winlite.h"
#include "vgui_internal.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// TGA header.
#pragma pack(1)
	class TGAFileHeader
	{
	public:
		unsigned char	m_IDLength;
		unsigned char	m_ColorMapType;
		unsigned char	m_ImageType;
		unsigned short	m_CMapStart;
		unsigned short	m_CMapLength;
		unsigned char	m_CMapDepth;
		unsigned short	m_XOffset;
		unsigned short	m_YOffset;
		unsigned short	m_Width;
		unsigned short	m_Height;
		unsigned char	m_PixelDepth;
		unsigned char	m_ImageDescriptor;
	};
#pragma pack()



// ---------------------------------------------------------------------------------------- //
// FileImageStream_Memory.
// ---------------------------------------------------------------------------------------- //
FileImageStream_Memory::FileImageStream_Memory(void *pData, int dataLen)
{
	m_pData = (unsigned char*)pData;
	m_DataLen = dataLen;
	m_CurPos = 0;
	m_bError = false;
}

void FileImageStream_Memory::Read(void *pData, int len)
{
	unsigned char *pOut;
	int i;

	pOut = (unsigned char*)pData;
	for(i=0; i < len; i++)
	{
		if(m_CurPos < m_DataLen)
		{
			pOut[i] = m_pData[m_CurPos];
			++m_CurPos;
		}
		else
		{
			pOut[i] = 0;
			m_bError = true;
		}
	}
}

bool FileImageStream_Memory::ErrorStatus()
{
	bool ret=m_bError;
	m_bError=false;
	return ret;
}



// ---------------------------------------------------------------------------------------- //
// Encode/decode functions.
// ---------------------------------------------------------------------------------------- //
static void WriteRun(unsigned char *pColor, FileHandle_t fp, int runLength)
{
	unsigned char runCount;

	runCount = runLength - 1;
	runCount |= (1 << 7);
	g_pFullFileSystem->Write( &runCount, 1, fp );
	g_pFullFileSystem->Write( pColor, 4, fp );
}


// Load in a 32-bit TGA file.
bool Load32BitTGA(
	FileImageStream *fp,
	FileImage *pImage)
{
	TGAFileHeader hdr;
	char dummyChar;
	int i, x, y;
	long color;
	int runLength, curOut;
	unsigned char *pLine;
	unsigned char packetHeader;


	pImage->Term();

	// Read and verify the header.
	fp->Read(&hdr, sizeof(hdr));
	if(hdr.m_PixelDepth != 32 || hdr.m_ImageType != 10)
		return false;

	// Skip the ID area..
	for(i=0; i < hdr.m_IDLength; i++)
		fp->Read(&dummyChar, 1);

	pImage->m_Width = hdr.m_Width;
	pImage->m_Height = hdr.m_Height;
	pImage->m_pData = new unsigned char[hdr.m_Width * hdr.m_Height * 4];
	if(!pImage->m_pData)
		return false;

	// Read in the data..
	for(y=pImage->m_Height-1; y >= 0; y--)
	{
		pLine = &pImage->m_pData[y*pImage->m_Width*4];

		curOut = 0;
		while(curOut < pImage->m_Width)
		{
			fp->Read(&packetHeader, 1);
			
			runLength = (int)(packetHeader & ~(1 << 7)) + 1;
			if(curOut + runLength > pImage->m_Width)
				return false;

			if(packetHeader & (1 << 7))
			{
				fp->Read(&color, 4);
				for(x=0; x < runLength; x++)
				{
					*((long*)pLine) = color;
					pLine += 4;
				}
			}
			else
			{
				for(x=0; x < runLength; x++)
				{
					fp->Read(&color, 4);
					*((long*)pLine) = color;
					pLine += 4;
				}
			}

			curOut += runLength;
		}
	}

	return true;
}


// Write a 32-bit TGA file.
void Save32BitTGA(
	FileHandle_t fp,
	FileImage *pImage)
{
	TGAFileHeader hdr;
	int y, runStart, x;
	unsigned char *pLine;


	memset(&hdr, 0, sizeof(hdr));
	hdr.m_PixelDepth = 32;
	hdr.m_ImageType = 10;	// Run-length encoded RGB.
	hdr.m_Width = pImage->m_Width;
	hdr.m_Height = pImage->m_Height;

	g_pFullFileSystem->Write(&hdr, sizeof(hdr), fp );
	
	// Lines are written bottom-up.
	for(y=pImage->m_Height-1; y >= 0; y--)
	{
		pLine = &pImage->m_pData[y*pImage->m_Width*4];
		
		runStart = 0;
		for(x=0; x < pImage->m_Width; x++)
		{
			if((x - runStart) >= 128 ||
				*((long*)&pLine[runStart*4]) != *((long*)&pLine[x*4]))
			{
				// Encode this  Run.
				WriteRun(&pLine[runStart*4], fp, x - runStart);
				runStart = x;
			}
		}

		// Encode the last Run.
		if(x - runStart > 0)
		{
			WriteRun(&pLine[runStart*4], fp, x - runStart);
		}
	}
}


