//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "imageloadwrapper.h"

bool ConvertImageToRGB( const byte *pubImageData, int cubImageData, CUtlBuffer &bufOutput, int &width, int &height )
{
	bool bSuccess = false;
	if ( GetJpegDimensions( pubImageData, cubImageData, (uint32&)width, (uint32&)height ) )
	{
		bSuccess = ConvertJpegToRGB( pubImageData, cubImageData, bufOutput, width, height );
	}
	else if ( GetPNGDimensions( pubImageData, cubImageData, (uint32&)width, (uint32&)height ) )
	{
		if ( ConvertPNGToRGBA( pubImageData, cubImageData, bufOutput, width, height ) )
			bSuccess = BConvertRGBAToRGB( bufOutput, width, height );
	}
	else if ( GetTGADimensions( cubImageData, (char*)pubImageData, &width, &height ) )
	{
		byte *pubRGB = NULL;
		int cubRGB = 0;
		bSuccess = LoadTGA( cubImageData, (char*)pubImageData, &pubRGB, &cubRGB, &width, &height );
		if ( bSuccess )
		{
			bufOutput.CopyBuffer( pubRGB, cubRGB );
			delete [] pubRGB;
			bSuccess = BConvertRGBAToRGB( bufOutput, width, height );
		}
	}
	return bSuccess;
}

bool ConvertImageToRGBA( const byte *pubImageData, int cubImageData, CUtlBuffer &bufOutput, int &width, int &height )
{
	bool bSuccess = false;
	if ( GetJpegDimensions( pubImageData, cubImageData, (uint32&)width, (uint32&)height ) )
	{
		bSuccess = ConvertJpegToRGBA( pubImageData, cubImageData, bufOutput, width, height );
	}
	else if ( GetPNGDimensions( pubImageData, cubImageData, (uint32&)width, (uint32&)height ) )
	{
		bSuccess = ConvertPNGToRGBA( pubImageData, cubImageData, bufOutput, width, height );
	}
	else if ( GetTGADimensions( cubImageData, (char*)pubImageData, &width, &height ) )
	{
		byte *pubRGB = NULL;
		int cubRGB = 0;
		bSuccess = LoadTGA( cubImageData, (char*)pubImageData, &pubRGB, &cubRGB, &width, &height );
		if ( bSuccess )
		{
			bufOutput.CopyBuffer( pubRGB, cubRGB );
			delete [] pubRGB;
		}
	}
	return bSuccess;
}

