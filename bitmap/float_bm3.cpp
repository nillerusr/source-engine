//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//===========================================================================//

#include <tier0/platform.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "bitmap/float_bm.h"
#include "vstdlib/vstdlib.h"
#include "vstdlib/random.h"
#include "tier1/strtools.h"

void FloatBitMap_t::InitializeWithRandomPixelsFromAnotherFloatBM(FloatBitMap_t const &other)
{
	for(int y=0;y<Height;y++)
		for(int x=0;x<Width;x++)
		{
			float x1=RandomInt(0,other.Width-1);
			float y1=RandomInt(0,other.Height-1);
			for(int c=0;c<4;c++)
			{
				Pixel(x,y,c)=other.Pixel(x1,y1,c);
			}
		}
}


FloatBitMap_t *FloatBitMap_t::QuarterSizeWithGaussian(void) const
{
	// generate a new bitmap half on each axis, using a separable gaussian. 
	static float kernel[]={.05,.25,.4,.25,.05};
	FloatBitMap_t *newbm=new FloatBitMap_t(Width/2,Height/2);
	
	for(int y=0;y<Height/2;y++)
		for(int x=0;x<Width/2;x++)
		{
			for(int c=0;c<4;c++)
			{
				float sum=0;
				float sumweights=0;							// for versatility in handling the
															// offscreen case
				for(int xofs=-2;xofs<=2;xofs++)
				{
					int orig_x=max(0,min(Width-1,x*2+xofs));
					for(int yofs=-2;yofs<=2;yofs++)
					{
						int orig_y=max(0,min(Height-1,y*2+yofs));
						float coeff=kernel[xofs+2]*kernel[yofs+2];
						sum+=Pixel(orig_x,orig_y,c)*coeff;
						sumweights+=coeff;
					}
				}
				newbm->Pixel(x,y,c)=sum/sumweights;
			}
		}
	return newbm;
}

FloatImagePyramid_t::FloatImagePyramid_t(FloatBitMap_t const &src, ImagePyramidMode_t mode)
{
	memset(m_pLevels,0,sizeof(m_pLevels));
	m_nLevels=1;
	m_pLevels[0]=new FloatBitMap_t(&src);
	ReconstructLowerResolutionLevels(0);
}

void FloatImagePyramid_t::ReconstructLowerResolutionLevels(int start_level)
{
	while( (m_pLevels[start_level]->Width>1) && (m_pLevels[start_level]->Height>1) )
	{
		if (m_pLevels[start_level+1])
			delete m_pLevels[start_level+1];
		m_pLevels[start_level+1]=m_pLevels[start_level]->QuarterSizeWithGaussian();
		start_level++;
	}
	m_nLevels=start_level+1;
}

float & FloatImagePyramid_t::Pixel(int x, int y, int component, int level) const
{
	assert(level<m_nLevels);
	x<<=level;
	y<<=level;
	return m_pLevels[level]->Pixel(x,y,component);
}

void FloatImagePyramid_t::WriteTGAs(char const *basename) const
{
	for(int l=0;l<m_nLevels;l++)
	{
		char bname_out[1024];
		Q_snprintf(bname_out,sizeof(bname_out),"%s_%02d.tga",basename,l);
		m_pLevels[l]->WriteTGAFile(bname_out);
	}

}


FloatImagePyramid_t::~FloatImagePyramid_t(void)
{
	for(int l=0;l<m_nLevels;l++)
		if (m_pLevels[l])
			delete m_pLevels[l];
}
