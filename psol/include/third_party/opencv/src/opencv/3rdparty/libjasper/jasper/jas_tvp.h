/*
 * Copyright (c) 2001-2002 Michael David Adams.
 * All rights reserved.
 */

/* __START_OF_JASPER_LICENSE__
 * 
 * JasPer License Version 2.0
 * 
 * Copyright (c) 2001-2006 Michael David Adams
 * Copyright (c) 1999-2000 Image Power, Inc.
 * Copyright (c) 1999-2000 The University of British Columbia
 * 
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person (the
 * "User") obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 * 
 * 1.  The above copyright notices and this permission notice (which
 * includes the disclaimer below) shall be included in all copies or
 * substantial portions of the Software.
 * 
 * 2.  The name of a copyright holder shall not be used to endorse or
 * promote products derived from the Software without specific prior
 * written permission.
 * 
 * THIS DISCLAIMER OF WARRANTY CONSTITUTES AN ESSENTIAL PART OF THIS
 * LICENSE.  NO USE OF THE SOFTWARE IS AUTHORIZED HEREUNDER EXCEPT UNDER
 * THIS DISCLAIMER.  THE SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS
 * "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  NO ASSURANCES ARE
 * PROVIDED BY THE COPYRIGHT HOLDERS THAT THE SOFTWARE DOES NOT INFRINGE
 * THE PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF ANY OTHER ENTITY.
 * EACH COPYRIGHT HOLDER DISCLAIMS ANY LIABILITY TO THE USER FOR CLAIMS
 * BROUGHT BY ANY OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL
 * PROPERTY RIGHTS OR OTHERWISE.  AS A CONDITION TO EXERCISING THE RIGHTS
 * GRANTED HEREUNDER, EACH USER HEREBY ASSUMES SOLE RESPONSIBILITY TO SECURE
 * ANY OTHER INTELLECTUAL PROPERTY RIGHTS NEEDED, IF ANY.  THE SOFTWARE
 * IS NOT FAULT-TOLERANT AND IS NOT INTENDED FOR USE IN MISSION-CRITICAL
 * SYSTEMS, SUCH AS THOSE USED IN THE OPERATION OF NUCLEAR FACILITIES,
 * AIRCRAFT NAVIGATION OR COMMUNICATION SYSTEMS, AIR TRAFFIC CONTROL
 * SYSTEMS, DIRECT LIFE SUPPORT MACHINES, OR WEAPONS SYSTEMS, IN WHICH
 * THE FAILURE OF THE SOFTWARE OR SYSTEM COULD LEAD DIRECTLY TO DEATH,
 * PERSONAL INJURY, OR SEVERE PHYSICAL OR ENVIRONMENTAL DAMAGE ("HIGH
 * RISK ACTIVITIES").  THE COPYRIGHT HOLDERS SPECIFICALLY DISCLAIM ANY
 * EXPRESS OR IMPLIED WARRANTY OF FITNESS FOR HIGH RISK ACTIVITIES.
 * 
 * __END_OF_JASPER_LICENSE__
 */

/*
 * Tag/Value Parser
 *
 * $Id: jas_tvp.h,v 1.2 2008-05-26 09:41:51 vp153 Exp $
 */

#ifndef JAS_TVP_H
#define JAS_TVP_H

/******************************************************************************\
* Includes.
\******************************************************************************/

#include <jasper/jas_config.h>

#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************\
* Types.
\******************************************************************************/

/* Tag information type. */

typedef struct {

	int id;
	/* The ID for the tag. */

	char *name;
	/* The name of the tag. */

} jas_taginfo_t;

/* Tag-value parser type. */

typedef struct {

	char *buf;
	/* The parsing buffer. */

	char *tag;
	/* The current tag name. */

	char *val;
	/* The current value. */

	char *pos;
	/* The current position in the parsing buffer. */

} jas_tvparser_t;

/******************************************************************************\
* Tag information functions.
\******************************************************************************/

/* Lookup a tag by name. */
jas_taginfo_t *jas_taginfos_lookup(jas_taginfo_t *taginfos, const char *name);

/* This function returns a pointer to the specified taginfo object if it
  exists (i.e., the pointer is nonnull); otherwise, a pointer to a dummy
  object is returned.  This is useful in some situations to avoid checking
  for a null pointer. */
jas_taginfo_t *jas_taginfo_nonull(jas_taginfo_t *taginfo);

/******************************************************************************\
* Tag-value parser functions.
\******************************************************************************/

/* Create a tag-value parser for the specified string. */
jas_tvparser_t *jas_tvparser_create(const char *s);

/* Destroy a tag-value parser. */
void jas_tvparser_destroy(jas_tvparser_t *tvparser);

/* Get the next tag-value pair. */
int jas_tvparser_next(jas_tvparser_t *tvparser);

/* Get the tag name for the current tag-value pair. */
char *jas_tvparser_gettag(jas_tvparser_t *tvparser);

/* Get the value for the current tag-value pair. */
char *jas_tvparser_getval(jas_tvparser_t *tvparser);

#ifdef __cplusplus
}
#endif

#endif
