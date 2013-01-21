/*
 *
 * (C) Copyright IBM Corp. 1998-2010 - All Rights Reserved
 *
 */

#ifndef __LESWAPS_H
#define __LESWAPS_H

#include "LETypes.h"

/**
 * \file 
 * \brief C++ API: Endian independent access to data for LayoutEngine
 */

U_NAMESPACE_BEGIN

/**
 * A convenience macro which invokes the swapWord member function
 * from a concise call.
 *
 * @stable ICU 2.8
 */
#define SWAPW(value) LESwaps::swapWord((le_uint16)(value))

/**
 * A convenience macro which invokes the swapLong member function
 * from a concise call.
 *
 * @stable ICU 2.8
 */
#define SWAPL(value) LESwaps::swapLong((le_uint32)(value))

/**
 * This class is used to access data which stored in big endian order
 * regardless of the conventions of the platform.
 *
 * All methods are static and inline in an attempt to induce the compiler
 * to do most of the calculations at compile time.
 *
 * @stable ICU 2.8
 */
class U_LAYOUT_API LESwaps /* not : public UObject because all methods are static */ {
public:

    /**
     * This method does the byte swap required on little endian platforms
     * to correctly access a (16-bit) word.
     *
     * @param value - the word to be byte swapped
     *
     * @return the byte swapped word
     *
     * @stable ICU 2.8
     */
    static le_uint16 swapWord(le_uint16 value)
    {
        return (le_uint16)((value << 8) | (value >> 8));
    };

    /**
     * This method does the byte swapping required on little endian platforms
     * to correctly access a (32-bit) long.
     *
     * @param value - the long to be byte swapped
     *
     * @return the byte swapped long
     *
     * @stable ICU 2.8
     */
    static le_uint32 swapLong(le_uint32 value)
    {
        return (le_uint32)(
            (value << 24) |
            ((value << 8) & 0xff0000) |
            ((value >> 8) & 0xff00) |
            (value >> 24));
    };

private:
    LESwaps() {} // private - forbid instantiation
};

U_NAMESPACE_END
#endif
