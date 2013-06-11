#ifndef RDESTL_INT_TO_TYPE_H
#define RDESTL_INT_TO_TYPE_H

namespace rde
{

/**
 * Sample usage:
 *	void fun(int_to_type<true>)  { ... }
 *  void fun(int_to_type<false>) { ... }
 *  template<typename T> void bar() 
 *  { 
 *		fun(int_to_type<std::numeric_limits<T>::is_exact>())
 *  }
 */
template<int TVal>
struct int_to_type
{
    enum 
    {
        value = TVal
    };
};

} // namespaces

#endif // #ifndef RDESTL_INT_TO_TYPE_H
