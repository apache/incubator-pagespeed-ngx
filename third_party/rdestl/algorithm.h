#ifndef RDESTL_ALGORITHM_H
#define RDESTL_ALGORITHM_H

#include "int_to_type.h"
#include "iterator.h"
#include "type_traits.h"
#include "utility.h"

namespace rde
{

//-----------------------------------------------------------------------------
template<typename T> RDE_FORCEINLINE
void copy_construct(T* mem, const T& orig)
{
	//new (mem) T(orig);
	internal::copy_construct(mem, orig, int_to_type<has_trivial_copy<T>::value>());
}

//-----------------------------------------------------------------------------
template<typename T> RDE_FORCEINLINE
void construct(T* mem)
{
	internal::construct(mem, int_to_type<has_trivial_constructor<T>::value>());
}

//-----------------------------------------------------------------------------
template<typename T> RDE_FORCEINLINE
void destruct(T* mem)
{
	internal::destruct(mem, int_to_type<has_trivial_destructor<T>::value>());
}

//-----------------------------------------------------------------------------
template<typename T>
void copy_n(const T* first, size_t n, T* result)
{
	internal::copy_n(first, n, result, int_to_type<has_trivial_copy<T>::value>());
}

//-----------------------------------------------------------------------------
template<typename T>
void copy(const T* first, const T* last, T* result)
{
	internal::copy(first, last, result, int_to_type<has_trivial_copy<T>::value>());
}

//-----------------------------------------------------------------------------
template<typename T>
void copy_construct_n(T* first, size_t n, T* result)
{
	internal::copy_construct_n(first, n, result, int_to_type<has_trivial_copy<T>::value>());
}
 
//-----------------------------------------------------------------------------
template<typename T>
void move_n(const T* from, size_t n, T* result)
{
	RDE_ASSERT(from != result || n == 0);
	// Overlap? 
	if (result + n >= from && result < from + n)
	{
		internal::move_n(from, n, result, int_to_type<has_trivial_copy<T>::value>());
	}
	else
	{
		internal::copy_n(from, n, result, int_to_type<has_trivial_copy<T>::value>());
	}
}

//-----------------------------------------------------------------------------
template<typename T>
inline void move(const T* first, const T* last, T* result)
{
	RDE_ASSERT(first != result || first == last);
	const size_t n = reinterpret_cast<uintptr_t>(last) - reinterpret_cast<uintptr_t>(first);
	const unsigned char* resultEnd = reinterpret_cast<const unsigned char*>(result) + n;
	if (resultEnd >= reinterpret_cast<const unsigned char*>(first) && result < last)
	{
		internal::move(first, last, result, int_to_type<has_trivial_copy<T>::value>());
	}
	else
	{
		internal::copy(first, last, result, int_to_type<has_trivial_copy<T>::value>());
	}
}

//-----------------------------------------------------------------------------
template<typename T>
void construct_n(T* first, size_t n)
{
	internal::construct_n(first, n, int_to_type<has_trivial_constructor<T>::value>());
}

//-----------------------------------------------------------------------------
template<typename T>
void destruct_n(T* first, size_t n)
{
	internal::destruct_n(first, n, int_to_type<has_trivial_destructor<T>::value>());
}

//-----------------------------------------------------------------------------
template<typename T> RDE_FORCEINLINE
void fill_n(T* first, size_t n, const T& val)
{
	//for (size_t i = 0; i < n; ++i)
	//	first[i] = val;
	// Loop unrolling with Duff's Device.
	T* last = first + n;
	switch (n & 0x7)
	{
	case 0:
		while (first != last)
		{
			*first = val; ++first;
	case 7:	*first = val; ++first;
	case 6:	*first = val; ++first;
	case 5:	*first = val; ++first;
	case 4:	*first = val; ++first;
	case 3:	*first = val; ++first;
	case 2:	*first = val; ++first;
	case 1:	*first = val; ++first;
		}
	}
}

//-----------------------------------------------------------------------------
template<typename TIter, typename TDist> inline
void distance(TIter first, TIter last, TDist& dist)
{
	internal::distance(first, last, dist, typename iterator_traits<TIter>::iterator_category());
}

//-----------------------------------------------------------------------------
template<typename TIter, typename TDist> inline
void advance(TIter& iter, TDist off)
{
	internal::advance(iter, off, typename iterator_traits<TIter>::iterator_category());
}

//-----------------------------------------------------------------------------
template<class TIter, typename T, class TPred> inline
TIter lower_bound(TIter first, TIter last, const T& val, const TPred& pred)
{
	internal::test_ordering(first, last, pred);
	int dist(0);
	distance(first, last, dist);
	while (dist > 0)
	{
		const int halfDist = dist >> 1;
		TIter mid = first;
		advance(mid, halfDist);
		if (internal::debug_pred(pred, *mid, val))
			first = ++mid, dist -= halfDist + 1;
		else
			dist = halfDist;
	}
	return first;
}

//-----------------------------------------------------------------------------
template<class TIter, typename T, class TPred> inline
TIter upper_bound(TIter first, TIter last, const T& val, const TPred& pred)
{
	internal::test_ordering(first, last, pred);
	int dist(0);
	distance(first, last, dist);
	while (dist > 0)
	{
		const int halfDist = dist >> 1;
		TIter mid = first;
		advance(mid, halfDist);
		if (!internal::debug_pred(pred, val, *mid))
			first = ++mid, dist -= halfDist + 1;
		else
			dist = halfDist;
	}
	return first;
}

//-----------------------------------------------------------------------------
template<class TIter, typename T>
TIter find(TIter first, TIter last, const T& val)
{
	while (first != last)
	{
		if ((*first) == val)
			return first;
		++first;
	}
	return last;
}

//-----------------------------------------------------------------------------
template<class TIter, typename T, class TPred>
TIter find_if(TIter first, TIter last, const T& val, const TPred& pred)
{
	while (first != last)
	{
		if (pred(*first, val))
			return first;
		++first;
	}
	return last;
}

//-----------------------------------------------------------------------------
template<class TIter, typename T>
void accumulate(TIter first, TIter last, T& result)
{
	while (first != last)
	{
		result += *first;
		++first;
	}
}

//-----------------------------------------------------------------------------
template<typename T>
T abs(const T& t)
{
	return t >= T(0) ? t : -t;
}
// No branches, Hacker's Delight way.
RDE_FORCEINLINE int abs(int x)
{
	const int y = x >> 31;
	return (x ^ y) - y;
}
RDE_FORCEINLINE short abs(short x)
{
	const short y = x >> 15;
	return (x ^ y) - y;
}

//-----------------------------------------------------------------------------
template<typename T> inline
T max(const T& x, const T& y)
{
    return x > y ? x : y;
}

//-----------------------------------------------------------------------------
template<typename T> inline
T min(const T& x, const T& y)
{
	return x < y ? x : y;
}
// @TODO: determine if it REALLY is quicker than version with branches.
/*RDE_FORCEINLINE float min(float x, float y)
{
	float result;
	__asm
	{
		fld		[x]
		fld		[y]
		fcomi	st(0), st(1)
		fcmovnb	st(0), st(1)
		fstp	[result]
		fcomp
	}
	return result;
}*/

//-----------------------------------------------------------------------------
template<typename TAssignable>
void swap(TAssignable& a, TAssignable& b)
{
	TAssignable tmp(a);
	a = b;
	b = tmp;
}

} // namespace rde

//-----------------------------------------------------------------------------
#endif // #ifndef RDESTL_ALGORITHM_H

