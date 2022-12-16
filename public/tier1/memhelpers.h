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
		do
		{
			--n;
			*(dest+n) = *(src+n);
	        } while( n );
	}

	template<typename T>
	inline void set( T *dest, const T &value, size_t n )
	{
		do
		{
			--n;
			*(dest+n) = value;
		} while( n );
	}
}

#endif //MEMHELPERS_H
