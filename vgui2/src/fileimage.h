//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef __FILEIMAGE_H__
#define __FILEIMAGE_H__

#ifdef _WIN32
#pragma once
#endif

#include <stdio.h>

typedef void *FileHandle_t;

class FileImageStream
{
public:
	virtual void	Read(void *pOut, int len)=0;
	
	// Returns true if there were any Read errors.
	// Clears error status.
	virtual bool	ErrorStatus()=0;
};


// Use to read out of a memory buffer..
class FileImageStream_Memory : public FileImageStream
{
public:
						FileImageStream_Memory(void *pData, int dataLen);

	virtual void		Read(void *pOut, int len);
	virtual bool		ErrorStatus();


private:		
	unsigned char		*m_pData;
	int					m_DataLen;
	int					m_CurPos;
	bool				m_bError;
};



// Generic image representation..
class FileImage
{
public:
					FileImage()
					{
						Clear();
					}

					~FileImage()
					{
						Term();
					}

	void			Term()
	{
		if(m_pData)
			delete [] m_pData;

		Clear();
	}

	// Clear the structure without deallocating.
	void			Clear()
	{
		m_Width = m_Height = 0;
		m_pData = NULL;
	}
	
	int				m_Width, m_Height;
	unsigned char	*m_pData;
};


// Functions to load/save FileImages.
bool Load32BitTGA(
	FileImageStream *fp,
	FileImage *pImage);

void Save32BitTGA(
	FileHandle_t fp,
	FileImage *pImage);


#endif


