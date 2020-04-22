//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: IObjectContainer.h: interface for the ObjectContainer class.
//
// $NoKeywords: $
//=============================================================================//

#ifndef IOBJECTCONTAINER_H
#define IOBJECTCONTAINER_H

#pragma once

class IObjectContainer  
{
public:
	virtual ~IObjectContainer() {};
	
	virtual void		Init() = 0;

	virtual bool		Add(void * newObject) = 0;
	virtual bool		Remove(void * object) = 0;
	virtual void		Clear(bool freeElementsMemory) = 0;
	
	virtual void *		GetFirst() = 0;
	virtual void *		GetNext() = 0;

	virtual int			CountElements() = 0;;
	virtual bool		Contains(void * object) = 0;
	virtual bool		IsEmpty() = 0;
};

#endif // !defined IOBJECTCONTAINER_H
