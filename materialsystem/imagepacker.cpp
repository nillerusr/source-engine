//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=====================================================================================//

#include "imagepacker.h"
#include "materialsystem_global.h"
#include "IHardwareConfigInternal.h"

// NOTE: This has to be the last file included
#include "tier0/memdbgon.h"

float CImagePacker::GetEfficiency( void )
{
	return ( float )m_AreaUsed / ( float )( m_MaxLightmapWidth * CeilPow2( m_MinimumHeight ) );
}

bool CImagePacker::Reset( int nSortId, int maxLightmapWidth, int maxLightmapHeight )
{
	int i;
	
	Assert( maxLightmapWidth <= MAX_MAX_LIGHTMAP_WIDTH );
	
	m_MaxLightmapWidth = maxLightmapWidth;
	m_MaxLightmapHeight = maxLightmapHeight;
	
	m_MaxBlockWidth = maxLightmapWidth + 1;
	m_MaxBlockHeight = maxLightmapHeight + 1;

	m_nSortID = nSortId;

	m_AreaUsed = 0;
	m_MinimumHeight = -1;
	for( i = 0; i < m_MaxLightmapWidth; i++ )
    {
		m_pLightmapWavefront[i] = -1;
    }
	return true;
}

inline int CImagePacker::GetMaxYIndex( int firstX, int width )
{
	int maxY = -1;
	int maxYIndex = 0;
	for( int x = firstX; x < firstX + width; ++x )
	{
		// NOTE: Want the equals here since we'll never be able to fit
		// in between the multiple instances of maxY
		if( m_pLightmapWavefront[x] >= maxY )
		{
			maxY = m_pLightmapWavefront[x];
			maxYIndex = x;
		}
	}
	return maxYIndex;
}

//#define ADD_ONE_TEXEL_BORDER

bool CImagePacker::AddBlock( int width, int height, 
						   int *returnX, int *returnY )
{
#ifdef ADD_ONE_TEXEL_BORDER
	width += 2;
	height += 2;
	width = clamp( width, m_MaxLightmapWidth );
	height = clamp( height, m_MaxLightmapHeight );
#endif

	// If we've already determined that a block this big couldn't fit
	// then blow off checking again...
	if ( ( width >= m_MaxBlockWidth ) && ( height >= m_MaxBlockHeight ) )
		return false;

	int bestX = -1;	
	int maxYIdx;
	int outerX = 0;
	int outerMinY = m_MaxLightmapHeight;
	int lastX = m_MaxLightmapWidth - width;
	int lastMaxYVal = -2;
	while (outerX <= lastX)
	{
		// Skip all tiles that have the last Y value, these
		// aren't going to change our min Y value
		if (m_pLightmapWavefront[outerX] == lastMaxYVal)
		{
			++outerX;
			continue;
		}

		maxYIdx = GetMaxYIndex( outerX, width );
		lastMaxYVal = m_pLightmapWavefront[maxYIdx];
		if (outerMinY > lastMaxYVal)
		{
			outerMinY = lastMaxYVal;
			bestX = outerX;

			// Early out for the first row...
//			if (outerMinY == -1)
//				break;
		}
		outerX = maxYIdx + 1;
	}
	
	if( bestX == -1 )
	{
		// If we failed to add it, remember the block size that failed
		// *only if both dimensions are smaller*!!
		// Just because a 1x10 block failed, doesn't mean a 10x1 block will fail
		if ( ( width <= m_MaxBlockWidth ) && ( height <= m_MaxBlockHeight )	)
		{
			m_MaxBlockWidth = width;
			m_MaxBlockHeight = height;
		}

		return false;
	}
	
	// Set the return positions for the block.
	*returnX = bestX;
	*returnY = outerMinY + 1;
	
	// Check if it actually fit height-wise.
	// hack
	//  if( *returnY + height > maxLightmapHeight )
	if( *returnY + height >= m_MaxLightmapHeight - 1 )
	{
		if ( ( width <= m_MaxBlockWidth ) && ( height <= m_MaxBlockHeight )	)
		{
			m_MaxBlockWidth = width;
			m_MaxBlockHeight = height;
		}

		return false;
	}
						   
	// It fit!
	// Keep up with the smallest possible size for the image so far.
	if( *returnY + height > m_MinimumHeight )
		m_MinimumHeight = *returnY + height;
	
	// Update the wavefront info.
	int x;
	for( x = bestX; x < bestX + width; x++ )
    {
		m_pLightmapWavefront[x] = outerMinY + height;
    }
	
	//  AddBlockToLightmapImage( *returnX, *returnY, width, height );
	m_AreaUsed += width * height;
#ifdef ADD_ONE_TEXEL_BORDER
	*returnX++;
	*returnY++;
#endif
	return true;
}

void CImagePacker::GetMinimumDimensions( int *pReturnWidth, int *pReturnHeight )
{
	*pReturnWidth = CeilPow2( m_MaxLightmapWidth );
	*pReturnHeight = CeilPow2( m_MinimumHeight );

	int aspect = *pReturnWidth / *pReturnHeight;
	if (aspect > HardwareConfig()->MaxTextureAspectRatio())
	{
		*pReturnHeight = *pReturnWidth / HardwareConfig()->MaxTextureAspectRatio(); 
	}
}
