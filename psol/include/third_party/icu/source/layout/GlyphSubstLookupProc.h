/*
 * (C) Copyright IBM Corp. 1998-2010 - All Rights Reserved
 *
 */

#ifndef __GLYPHSUBSTITUTIONLOOKUPPROCESSOR_H
#define __GLYPHSUBSTITUTIONLOOKUPPROCESSOR_H

/**
 * \file
 * \internal
 */

#include "LETypes.h"
#include "LEGlyphFilter.h"
#include "LEFontInstance.h"
#include "OpenTypeTables.h"
#include "Lookups.h"
#include "ICUFeatures.h"
#include "GlyphDefinitionTables.h"
#include "GlyphSubstitutionTables.h"
#include "GlyphIterator.h"
#include "LookupProcessor.h"

U_NAMESPACE_BEGIN

class GlyphSubstitutionLookupProcessor : public LookupProcessor
{
public:
    GlyphSubstitutionLookupProcessor(const GlyphSubstitutionTableHeader *glyphSubstitutionTableHeader,
        LETag scriptTag, 
        LETag languageTag, 
        const LEGlyphFilter *filter, 
        const FeatureMap *featureMap, 
        le_int32 featureMapCount, 
        le_bool featureOrder,
        LEErrorCode& success);

    virtual ~GlyphSubstitutionLookupProcessor();

    virtual le_uint32 applySubtable(const LookupSubtable *lookupSubtable, le_uint16 lookupType, GlyphIterator *glyphIterator,
        const LEFontInstance *fontInstance, LEErrorCode& success) const;

protected:
    GlyphSubstitutionLookupProcessor();

private:
    const LEGlyphFilter *fFilter;

    GlyphSubstitutionLookupProcessor(const GlyphSubstitutionLookupProcessor &other); // forbid copying of this class
    GlyphSubstitutionLookupProcessor &operator=(const GlyphSubstitutionLookupProcessor &other); // forbid copying of this class
};

U_NAMESPACE_END
#endif
