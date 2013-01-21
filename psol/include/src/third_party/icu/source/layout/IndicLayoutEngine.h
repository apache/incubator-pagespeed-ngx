
/*
 *
 * (C) Copyright IBM Corp. 1998-2009 - All Rights Reserved
 *
 */

#ifndef __INDICLAYOUTENGINE_H
#define __INDICLAYOUTENGINE_H

#include "LETypes.h"
#include "LEFontInstance.h"
#include "LEGlyphFilter.h"
#include "LayoutEngine.h"
#include "OpenTypeLayoutEngine.h"

#include "GlyphSubstitutionTables.h"
#include "GlyphDefinitionTables.h"
#include "GlyphPositioningTables.h"

U_NAMESPACE_BEGIN

class MPreFixups;
class LEGlyphStorage;

/**
 * This class implements OpenType layout for Indic OpenType fonts, as
 * specified by Microsoft in "Creating and Supporting OpenType Fonts for
 * Indic Scripts" (http://www.microsoft.com/typography/otspec/indicot/default.htm)
 *
 * This class overrides the characterProcessing method to do Indic character processing
 * and reordering, and the glyphProcessing method to implement post-GSUB processing for
 * left matras. (See the MS spec. for more details)
 *
 * @internal
 */
class IndicOpenTypeLayoutEngine : public OpenTypeLayoutEngine
{
public:
    /**
     * This is the main constructor. It constructs an instance of IndicOpenTypeLayoutEngine for
     * a particular font, script and language. It takes the GSUB table as a parameter since
     * LayoutEngine::layoutEngineFactory has to read the GSUB table to know that it has an
     * Indic OpenType font.
     *
     * @param fontInstance - the font
     * @param scriptCode - the script
     * @param langaugeCode - the language
     * @param gsubTable - the GSUB table
     * @param success - set to an error code if the operation fails
     *
     * @see LayoutEngine::layoutEngineFactory
     * @see OpenTypeLayoutEngine
     * @see ScriptAndLangaugeTags.h for script and language codes
     *
     * @internal
     */
    IndicOpenTypeLayoutEngine(const LEFontInstance *fontInstance, le_int32 scriptCode, le_int32 languageCode,
                            le_int32 typoFlags, le_bool version2, const GlyphSubstitutionTableHeader *gsubTable, LEErrorCode &success);

    /**
     * This constructor is used when the font requires a "canned" GSUB table which can't be known
     * until after this constructor has been invoked.
     *
     * @param fontInstance - the font
     * @param scriptCode - the script
     * @param langaugeCode - the language
     * @param success - set to an error code if the operation fails
     *
     * @see OpenTypeLayoutEngine
     * @see ScriptAndLangaugeTags.h for script and language codes
     *
     * @internal
     */
    IndicOpenTypeLayoutEngine(const LEFontInstance *fontInstance, le_int32 scriptCode, le_int32 languageCode,
			      le_int32 typoFlags, LEErrorCode &success);

    /**
     * The destructor, virtual for correct polymorphic invocation.
     *
     * @internal
     */
   virtual ~IndicOpenTypeLayoutEngine();

    /**
     * ICU "poor man's RTTI", returns a UClassID for the actual class.
     *
     * @stable ICU 2.8
     */
    virtual UClassID getDynamicClassID() const;

    /**
     * ICU "poor man's RTTI", returns a UClassID for this class.
     *
     * @stable ICU 2.8
     */
    static UClassID getStaticClassID();

protected:

    /**
     * This method does Indic OpenType character processing. It assigns the OpenType feature
     * tags to the characters, and may generate output characters which have been reordered. For
     * some Indic scripts, it may also split some vowels, resulting in more output characters
     * than input characters.
     *
     * Input parameters:
     * @param chars - the input character context
     * @param offset - the index of the first character to process
     * @param count - the number of characters to process
     * @param max - the number of characters in the input context
     * @param rightToLeft - <code>TRUE</code> if the characters are in a right to left directional run
     * @param glyphStorage - the glyph storage object. The glyph and character index arrays will be set.
     *                       the auxillary data array will be set to the feature tags.
     *
     * Output parameters:
     * @param success - set to an error code if the operation fails
     *
     * @return the output character count
     *
     * @internal
     */
    virtual le_int32 characterProcessing(const LEUnicode chars[], le_int32 offset, le_int32 count, le_int32 max, le_bool rightToLeft,
            LEUnicode *&outChars, LEGlyphStorage &glyphStorage, LEErrorCode &success);

    /**
     * This method does character to glyph mapping, applies the GSUB table and applies
     * any post GSUB fixups for left matras. It calls OpenTypeLayoutEngine::glyphProcessing
     * to do the character to glyph mapping, and apply the GSUB table.
     *
     * Note that in the case of "canned" GSUB tables, the output glyph indices may be
     * "fake" glyph indices that need to be converted to "real" glyph indices by the
     * glyphPostProcessing method.
     *
     * Input parameters:
     * @param chars - the input character context
     * @param offset - the index of the first character to process
     * @param count - the number of characters to process
     * @param max - the number of characters in the input context
     * @param rightToLeft - <code>TRUE</code> if the characters are in a right to left directional run
     * @param featureTags - the feature tag array
     * @param glyphStorage - the glyph storage object. The glyph and char index arrays will be set.
     *
     * Output parameters:
     * @param success - set to an error code if the operation fails
     *
     * @return the number of glyphs in the output glyph index array
     *
     * Note: if the character index array was already set by the characterProcessing
     * method, this method won't change it.
     *
     * @internal
     */
    virtual le_int32 glyphProcessing(const LEUnicode chars[], le_int32 offset, le_int32 count, le_int32 max, le_bool rightToLeft,
            LEGlyphStorage &glyphStorage, LEErrorCode &success);

    le_bool fVersion2;

private:

    MPreFixups *fMPreFixups;

};

U_NAMESPACE_END
#endif

