//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef FACTORYOVERLOADS_H
#define FACTORYOVERLOADS_H
#ifdef _WIN32
#pragma once
#endif

#include "utldict.h"

class IAttributeWidgetFactory;
class IAttributeElementChoiceList;

class CFactoryOverloads : public IFactoryOverloads
{
public:
	virtual void				AddOverload
								( 
									char const *attributeName, 
									IAttributeWidgetFactory *newFactory,
									IAttributeElementChoiceList *newChoiceList 
								);
	int							Count();
	char const					*Name( int index );
	IAttributeWidgetFactory		*Factory( int index );
	IAttributeElementChoiceList *ChoiceList( int index );

private:
	struct Entry_t
	{
		Entry_t() : 
			factory( 0 ),
			choices( 0 )
		{
		}

		IAttributeWidgetFactory			*factory;
		IAttributeElementChoiceList		*choices;
	};

	CUtlDict< Entry_t, int >	m_Overloads;
};

#endif // FACTORYOVERLOADS_H