//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================

#include "jpegloader.h"
#include "tier0/dbg.h"
#include "tier1/utlvector.h"
#include "tier0/vprof.h"
#include "jpeglib/jpeglib.h"

//-----------------------------------------------------------------------------
// Purpose: Takes a RGBA image buffer and resizes it using linear interpolation.
//
// Params: bufRGBA should contain the current image, nWidth and nHeight should describe
// the current images dimensions.  nNewWidth and nNewHeight should be the new target size,
// one, but not both, of these may be -1 which will indicate to preserve aspect ratio
// and size that dimension to match the aspect ratio adjustment applied to the other
// dimension which must then be specified explicitly.  nNewWidth and nNewHeight may
// be modified by the function and will specify the final width/height after the 
// function returns succesfully.  If both nNewWidth and nNewHeight are specified
// the content will be scaled without changing the aspect ratio and black letterboxing
// will be added if appropriate
//-----------------------------------------------------------------------------
bool BResizeImageInternal( CUtlBuffer &bufImage, int nWidth, int nHeight, int &nNewWidth, int &nNewHeight, bool bIsRGBA = true )
{
	VPROF_BUDGET( "BResizeImageRGBA", VPROF_BUDGETGROUP_OTHER_VGUI );
	CUtlBuffer bufImageOut;

	if ( nWidth == 0 || nHeight == 0 )
		return false;

	// Must specify at least one, then we'll compute the other to preserve aspect ratio if it's set to -1
	if ( nNewWidth == - 1 && nNewHeight == -1 )
		return false;

	if ( nNewHeight == -1 )
	{
		float flAspect = (float)nNewWidth/(float)nWidth;
		nNewHeight = (int)(flAspect*nHeight);
	}
	else if ( nNewWidth == -1 )
	{
		float flAspect = (float)nNewHeight/(float)nHeight;
		nNewWidth =  (int)(flAspect*nWidth);
	}

	if ( nNewWidth == 0 || nNewHeight == 0 )
		return false;

	int nNewContentHeight = nNewHeight;
	int nNewContentWidth = nNewWidth;

	// Calculate the width/height of the actual content
	if ( nWidth/(float)nHeight > nNewContentWidth/(float)nNewContentHeight )
		nNewContentHeight = ( nNewContentWidth * nHeight )/nWidth;
	else
		nNewContentWidth = ( nNewContentHeight * nWidth )/nHeight;

	int bytesPerPixel = bIsRGBA ? 4 : 3;

	bufImageOut.EnsureCapacity( nNewWidth*nNewHeight*bytesPerPixel );
	bufImageOut.SeekPut( CUtlBuffer::SEEK_HEAD, nNewWidth*nNewHeight*bytesPerPixel );


	// Letterboxing
	int nPaddingTop = (nNewHeight - nNewContentHeight)/2;
	int nPaddingBottom = nNewHeight - nNewContentHeight - nPaddingTop;
	int nPaddingLeft = (nNewWidth - nNewContentWidth)/2;
	int nPaddingRight = nNewWidth - nNewContentWidth - nPaddingLeft;

	Assert( nPaddingTop + nPaddingBottom + nNewContentHeight == nNewHeight );
	Assert( nPaddingLeft + nPaddingRight + nNewContentWidth == nNewWidth );

	if ( nPaddingLeft > 0 ||
		nPaddingRight > 0 ||
		nPaddingTop > 0 ||
		nPaddingBottom > 0 )
	{
		Q_memset( bufImageOut.Base(), 0, nNewWidth*nNewHeight*bytesPerPixel );
	}

	byte *pBits = (byte*)bufImageOut.Base();

	int nOriginalStride = nWidth;
	int nTargetStride = nNewWidth;

	float flXRatio = (float)(nWidth-1)/(float)nNewContentWidth;
	float flYRatio = (float)(nHeight-1)/(float)nNewContentHeight;

	byte *pSrcBits = (byte*)bufImage.Base();
	for( int yNew=0; yNew<nNewContentHeight; ++yNew )
	{
		int y = (int)(flYRatio * yNew);
		float yDiff = (flYRatio * yNew) - y;

		for ( int xNew=0; xNew<nNewContentWidth; ++xNew )
		{
			int x = (int)(flXRatio * xNew);
			float xDiff = (flXRatio * xNew) - x;

			int aOffset = (x+(y*nOriginalStride))*bytesPerPixel;
			int bOffset = aOffset+bytesPerPixel;
			int cOffset = aOffset + (nOriginalStride*bytesPerPixel);
			int dOffset = cOffset+bytesPerPixel;

			byte red = (byte)(
				pSrcBits[aOffset]*(1-xDiff)*(1-yDiff)
				+pSrcBits[bOffset]*(xDiff)*(1-yDiff)
				+pSrcBits[cOffset]*(yDiff)*(1-xDiff)
				+pSrcBits[dOffset]*(xDiff)*(yDiff)
				);

			byte green = (byte)(
				pSrcBits[aOffset+1]*(1-xDiff)*(1-yDiff)
				+pSrcBits[bOffset+1]*(xDiff)*(1-yDiff)
				+pSrcBits[cOffset+1]*(yDiff)*(1-xDiff)
				+pSrcBits[dOffset+1]*(xDiff)*(yDiff)
				);

			byte blue = (byte)(
				pSrcBits[aOffset+2]*(1-xDiff)*(1-yDiff)
				+pSrcBits[bOffset+2]*(xDiff)*(1-yDiff)
				+pSrcBits[cOffset+2]*(yDiff)*(1-xDiff)
				+pSrcBits[dOffset+2]*(xDiff)*(yDiff)
				);

			byte alpha = 0;
			if ( bytesPerPixel == 4 )
			{
				alpha = (byte)(
					pSrcBits[aOffset+3]*(1-xDiff)*(1-yDiff)
					+pSrcBits[bOffset+3]*(xDiff)*(1-yDiff)
					+pSrcBits[cOffset+3]*(yDiff)*(1-xDiff)
					+pSrcBits[dOffset+3]*(xDiff)*(yDiff)
					);
			}

			int targetOffset = (nPaddingLeft+xNew+((nPaddingTop+yNew)*nTargetStride))*bytesPerPixel;

			pBits[targetOffset] = red;
			pBits[targetOffset+1] = green;
			pBits[targetOffset+2] = blue;
			if ( bytesPerPixel == 4 )
				pBits[targetOffset+3] = alpha;
		}
	}
	bufImage.Swap( bufImageOut );
	return true;
}

// Resize an RGB image using linear interpolation
bool BResizeImageRGB( CUtlBuffer &bufRGB, int nWidth, int nHeight, int &nNewWidth, int &nNewHeight )
{
	return BResizeImageInternal( bufRGB, nWidth, nHeight, nNewWidth, nNewHeight, false );
}

// Resize an RGBA image using linear interpolation
bool BResizeImageRGBA( CUtlBuffer &bufRGB, int nWidth, int nHeight, int &nNewWidth, int &nNewHeight )
{
	return BResizeImageInternal( bufRGB, nWidth, nHeight, nNewWidth, nNewHeight, true );
}

// Convert an RGB image to RGBA with 100% opacity
bool BConvertRGBToRGBA( CUtlBuffer &bufRGB, int nWidth, int nHeight )
{
	CUtlBuffer bufRGBA;
	bufRGBA.EnsureCapacity( nWidth*nHeight*4 );
	byte *pBits = (byte*)bufRGB.Base();
	byte *pBitsOut = (byte*)bufRGBA.Base();

	for( int y=0; y<nHeight; ++y )
	{
		for( int x=0; x<nWidth; ++x )
		{
			*pBitsOut = *pBits;
			*(pBitsOut+1) = *(pBits+1);
			*(pBitsOut+2) = *(pBits+2);
			*(pBitsOut+3) = 255;

			pBitsOut += 4;
			pBits += 3;
		}
	}
	bufRGB.Swap( bufRGBA );
	return true;
}

// Convert an RGBA image to RGB using opacity against a given solid background
bool BConvertRGBAToRGB( CUtlBuffer &bufRGBA, int nWidth, int nHeight, Color colorBG )
{
	CUtlBuffer bufRGB;
	bufRGB.EnsureCapacity( nWidth*nHeight*3 );
	byte *pBits = (byte*)bufRGBA.Base();
	byte *pBitsOut = (byte*)bufRGB.Base();

	for( int y=0; y<nHeight; ++y )
	{
		for( int x=0; x<nWidth; ++x )
		{
			float fOpacity = *(pBits+3) / 255.0;
			*pBitsOut = *pBits * fOpacity + (1.0 - fOpacity) * colorBG.r();
			*(pBitsOut+1) = *(pBits+1) * fOpacity + (1.0 - fOpacity) * colorBG.g();
			*(pBitsOut+2) = *(pBits+2) * fOpacity + (1.0 - fOpacity) * colorBG.b();

			pBitsOut += 3;
			pBits += 4;
		}
	}
	bufRGB.Swap( bufRGBA );
	return true;
}