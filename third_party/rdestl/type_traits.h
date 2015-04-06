#ifndef RDESTL_TYPETRAITS_H
#define RDESTL_TYPETRAITS_H

namespace rde
{

template<typename T> struct is_integral 
{
	enum { value = false };
};

template<typename T> struct is_floating_point
{
	enum { value = false };
};

#define RDE_INTEGRAL(TYPE)	template<> struct is_integral<TYPE> { enum { value = true }; }

RDE_INTEGRAL(char);
RDE_INTEGRAL(unsigned char);
RDE_INTEGRAL(bool);
RDE_INTEGRAL(short);
RDE_INTEGRAL(unsigned short);
RDE_INTEGRAL(int);
RDE_INTEGRAL(unsigned int);
RDE_INTEGRAL(long);
RDE_INTEGRAL(unsigned long);
RDE_INTEGRAL(wchar_t);

template<> struct is_floating_point<float> { enum { value = true }; };
template<> struct is_floating_point<double> { enum { value = true }; };

template<typename T> struct is_pointer
{
	enum { value = false };
};
template<typename T> struct is_pointer<T*>
{
	enum { value = true };
};

template<typename T> struct is_pod
{
	enum { value = false };
};

template<typename T> struct is_fundamental
{
	enum 
	{
		value = is_integral<T>::value || is_floating_point<T>::value
	};
};

template<typename T> struct has_trivial_constructor
{
	enum
	{
		value = is_fundamental<T>::value || is_pointer<T>::value || is_pod<T>::value
	};
};

template<typename T> struct has_trivial_copy
{
	enum
	{
		value = is_fundamental<T>::value || is_pointer<T>::value || is_pod<T>::value
	};
};

template<typename T> struct has_trivial_assign
{
	enum
	{
		value = is_fundamental<T>::value || is_pointer<T>::value || is_pod<T>::value
	};
};

template<typename T> struct has_trivial_destructor
{
	enum
	{
		value = is_fundamental<T>::value || is_pointer<T>::value || is_pod<T>::value
	};
};

template<typename T> struct has_cheap_compare
{
	enum
	{
		value = has_trivial_copy<T>::value && sizeof(T) <= 4
	};
};

} // namespace rde

//-----------------------------------------------------------------------------
#endif // #ifndef RDESTL_TYPETRAITS_H

