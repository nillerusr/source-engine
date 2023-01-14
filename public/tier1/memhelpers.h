// ======= Copyright nillerusr, 2022 =======

// Helper аunctions for setting/сopying memory ( specially for non-POD types )
// FUCK STL

#ifndef MEMHELPERS_H
#define MEMHELPERS_H

namespace memutils
{
	template<typename T>
	inline void copy( T *dest, const T *src, size_t n )
	{
		for(; n; n--)
			*(dest++) = *(src++);
	}

	template<typename T>
	inline void set( T *dest, const T& value, size_t n )
	{
		for(; n; n--)
			*(dest++) = value;
	}
}

#endif //MEMHELPERS_H
