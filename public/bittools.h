//===== Copyright © 1996-2009, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//

#ifndef BITTOOLS_H
#define BITTOOLS_H
#ifdef _WIN32
#pragma once
#endif

namespace bittools
{
	template<int N, int C = 0>
	struct RecurseBit
	{
		enum {result = RecurseBit<N/2, C+1>::result};
	};
	
	template<int C>
	struct RecurseBit<0, C>
	{
		enum {result = C};
	};
	
	template<int N, int C = 1>
	struct RecursePow2
	{
		enum {result = RecursePow2<N/2, C*2>::result};
	};

	template<int C>
	struct RecursePow2<0, C>
	{
		enum {result = C};
	};
	
}

#define ROUND_TO_POWER_OF_2( n ) ( bittools::RecursePow2< (n) - 1 >::result )
#define MINIMUM_BITS_NEEDED( n ) ( bittools::RecurseBit< (n) - 1 >::result )

#endif //BITTOOLS_H

