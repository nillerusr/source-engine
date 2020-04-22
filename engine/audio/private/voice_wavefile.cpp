//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#undef fopen
#include <stdio.h>
#include "voice_wavefile.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static unsigned long ReadDWord(FILE * fp) 
{
	unsigned long ret;  
	fread( &ret, 4, 1, fp );
	return ret;
}

static unsigned short ReadWord(FILE * fp) 
{
	unsigned short ret; 
	fread( &ret, 2, 1, fp );
	return ret;
}

static void WriteDWord(FILE * fp, unsigned long val) 
{
	fwrite( &val, 4, 1, fp );
}

static void WriteWord(FILE * fp, unsigned short val) 
{
	fwrite( &val, 2, 1, fp );
}



bool ReadWaveFile(
	const char *pFilename,
	char *&pData,
	int &nDataBytes,
	int &wBitsPerSample,
	int &nChannels,
	int &nSamplesPerSec)
{
	FILE * fp = fopen(pFilename, "rb");
	if(!fp)
		return false;

	fseek( fp, 22, SEEK_SET );
	
	nChannels = ReadWord(fp);
	nSamplesPerSec = ReadDWord(fp);

	fseek(fp, 34, SEEK_SET);
	wBitsPerSample = ReadWord(fp);

	fseek(fp, 40, SEEK_SET);
	nDataBytes = ReadDWord(fp);
	ReadDWord(fp);
	pData = new char[nDataBytes];
	if(!pData)
	{
		fclose(fp);
		return false;
	}
	fread(pData, nDataBytes, 1, fp);
	fclose( fp );
	return true;
}

bool WriteWaveFile(
	const char *pFilename, 
	const char *pData, 
	int nBytes, 
	int wBitsPerSample, 
	int nChannels, 
	int nSamplesPerSec)
{
	FILE * fp = fopen(pFilename, "wb");
	if(!fp)
		return false;

	// Write the RIFF chunk.
	fwrite("RIFF", 4, 1, fp);
	WriteDWord(fp, 0);
	fwrite("WAVE", 4, 1, fp);
	

	// Write the FORMAT chunk.
	fwrite("fmt ", 4, 1, fp);
	
	WriteDWord(fp, 0x10);
	WriteWord(fp, 1);	// WAVE_FORMAT_PCM
	WriteWord(fp, (unsigned short)nChannels);	
	WriteDWord(fp, (unsigned long)nSamplesPerSec);
	WriteDWord(fp, (unsigned long)((wBitsPerSample / 8) * nChannels * nSamplesPerSec));
	WriteWord(fp, (unsigned short)((wBitsPerSample / 8) * nChannels));
	WriteWord(fp, (unsigned short)wBitsPerSample);

	// Write the DATA chunk.
	fwrite("data", 4, 1, fp);
	WriteDWord(fp, (unsigned long)nBytes);
	fwrite(pData, nBytes, 1, fp);


	// Go back and write the length of the riff file.
	unsigned long dwVal = ftell(fp) - 8;
	fseek( fp, 4, SEEK_SET );
	WriteDWord(fp, dwVal);

	fclose(fp);
	return true;
}


