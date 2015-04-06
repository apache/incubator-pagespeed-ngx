#ifndef RDESTL_HASH_H
#define RDESTL_HASH_H

namespace rde
{

typedef unsigned long	hash_value_t;
    
// Default implementations, just casts to hash_value.
template<typename T>
hash_value_t extract_int_key_value(const T& t)
{
	return (hash_value_t)t;
}
 
// Default implementation of hasher.
// Works for keys that can be converted to 32-bit integer
// with extract_int_key_value.
// Algorithm by Robert Jenkins.
// (see http://www.cris.com/~Ttwang/tech/inthash.htm for example).
template<typename T>
struct hash
{
	hash_value_t operator()(const T& t) const
	{
		hash_value_t a = extract_int_key_value(t);
        a = (a+0x7ed55d16) + (a<<12);
        a = (a^0xc761c23c) ^ (a>>19);
        a = (a+0x165667b1) + (a<<5);
        a = (a+0xd3a2646c) ^ (a<<9);
        a = (a+0xfd7046c5) + (a<<3);
        a = (a^0xb55a4f09) ^ (a>>16);
        return a;
	}
};

}

#endif
