#ifndef RDESTL_FUNCTIONAL_H
#define RDESTL_FUNCTIONAL_H

namespace rde
{
//=============================================================================
template<typename T>
struct less
{
	bool operator()(const T& lhs, const T& rhs) const
	{
		return lhs < rhs;
	}
};

//=============================================================================
template<typename T>
struct greater
{
	bool operator()(const T& lhs, const T& rhs) const
	{
		return lhs > rhs;
	}
};

//=============================================================================
template<typename T>
struct equal_to
{
	bool operator()(const T& lhs, const T& rhs) const
	{
		return lhs == rhs;
	}
};

}

//-----------------------------------------------------------------------------
#endif // #ifndef RDESTL_FUNCTIONAL_H
