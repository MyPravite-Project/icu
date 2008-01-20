/*
*******************************************************************************
* Copyright (C) 2008, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* File PLURFMT.CPP
*
* Modification History:
*
*   Date        Name        Description
*******************************************************************************
*/


#include "unicode/utypes.h"
#include "unicode/plurfmt.h"
#include "unicode/plurrule.h"
#include "plurrule_impl.h"

#if !UCONFIG_NO_FORMATTING

U_CDECL_BEGIN
static void U_CALLCONV
deleteHashStrings(void *obj) {
    delete (UnicodeString *)obj;
}
U_CDECL_END

U_NAMESPACE_BEGIN
UOBJECT_DEFINE_RTTI_IMPLEMENTATION(PluralFormat)

#define MAX_KEYWORD_SIZE 30

PluralFormat::PluralFormat(UErrorCode& status) {
    init(NULL, Locale::getDefault(), status);
}

PluralFormat::PluralFormat(const Locale& locale, UErrorCode& status) {
    init(NULL, locale, status);
}

PluralFormat::PluralFormat(const PluralRules& rules, UErrorCode& status) {
    init(&rules, Locale::getDefault(), status);
}

PluralFormat::PluralFormat(const Locale& locale, const PluralRules& rules, UErrorCode& status) {
    init(&rules, locale, status);
}

PluralFormat::PluralFormat(const UnicodeString& pattern, UErrorCode& status) {
    init(NULL, Locale::getDefault(), status);
    applyPattern(pattern, status);
}

PluralFormat::PluralFormat(const Locale& locale, const UnicodeString& pattern, UErrorCode& status) {
    init(NULL, locale, status);
    applyPattern(pattern, status);
}

PluralFormat::PluralFormat(const PluralRules& rules, const UnicodeString& pattern, UErrorCode& status) {
    init(&rules, locale, status);
    applyPattern(pattern, status);
}

PluralFormat::PluralFormat(const Locale& locale, const PluralRules& rules, const UnicodeString& pattern, UErrorCode& status) {
    init(&rules, locale, status);
    applyPattern(pattern, status);
}

PluralFormat::PluralFormat(const PluralFormat& other) : Format(other) {
    UErrorCode status = U_ZERO_ERROR;
    locale = other.locale;
    pluralRules = other.pluralRules->clone();
    pattern = other.pattern;
    copyHashtable(other.fParsedValuesHash, status);
    numberFormat=NumberFormat::createInstance(locale, status);
    replacedNumberFormat=other.replacedNumberFormat;
}

PluralFormat::~PluralFormat() {
    delete pluralRules;   
    delete fParsedValuesHash;
    delete numberFormat;
}

void
PluralFormat::init(const PluralRules* rules, const Locale& curLocale, UErrorCode& status) {
    locale = curLocale;
    if ( rules==NULL) {
        pluralRules = PluralRules::forLocale(locale, status);
    }
    else {
        pluralRules = rules->clone();
    }
    fParsedValuesHash=NULL;
    pattern.remove();
    numberFormat= NumberFormat::createInstance(curLocale, status);
    replacedNumberFormat=NULL;
}

void
PluralFormat::applyPattern(const UnicodeString& newPattern, UErrorCode& status) {
    this->pattern = newPattern;
    UnicodeString token;
    int32_t braceCount=0;
    fmtToken type;
    UBool spaceIncluded=FALSE;
    
    if (fParsedValuesHash==NULL) {
        fParsedValuesHash = new Hashtable(TRUE, status);
        if (U_FAILURE(status)) {
            return;
        }
        fParsedValuesHash->setValueDeleter(deleteHashStrings);
    }
    
    UBool getKeyword=TRUE;
    UnicodeString hashKeyword;
    UnicodeString *hashPattern;
    
    for (int32_t i=0; i<pattern.length(); ++i) {
        UChar ch=pattern.charAt(i);

        if ( !inRange(ch, type) ) {
            if (getKeyword) {
                status = U_ILLEGAL_CHARACTER;
                return;
            }
            else {
                token += ch;
                continue;
            }
        }
        switch (type) {
            case tSpace:
                if (token.length()==0) {
                    continue;
                }
                if (getKeyword) {
                    // space after keyword
                    spaceIncluded = TRUE;
                }
                else {
                    token += ch;
                }
                break;
            case tLeftBrace:
                if ( getKeyword ) {
                    if (fParsedValuesHash->get(token)!= NULL) {
                        status = U_DUPLICATE_KEYWORD;
                        return; 
                    }
                    if (token.length()==0) {
                        status = U_PATTERN_SYNTAX_ERROR;
                        return;
                    }
                    if (!pluralRules->isKeyword(token) && 
                        pluralRules->getKeywordOther()!=token) {
                        status = U_UNDEFINED_KEYWORD;
                        return;
                    }
                    hashKeyword = token;
                    getKeyword = FALSE;
                    token.remove();
                }
                else  {
                    if (braceCount==0) {
                        status = U_UNEXPECTED_TOKEN;
                        return;
                    }
                    else {
                        token += ch;
                    }
                }
                braceCount++;
                spaceIncluded = FALSE;
                break;
            case tRightBrace:
                if ( getKeyword ) {
                    status = U_UNEXPECTED_TOKEN;
                    return;
                }
                else  {
                    hashPattern = new UnicodeString(token);
                    fParsedValuesHash->put(hashKeyword, hashPattern, status);
                    braceCount--;
                    if ( braceCount==0 ) {
                        getKeyword=TRUE;
                        hashKeyword.remove();
                        hashPattern=NULL;
                        token.remove();
                    }
                    else {
                        token += ch;
                    }
                }
                spaceIncluded = FALSE;
                break;
            case tLetter:
            case tNumberSign:
                if (spaceIncluded) {
                    status = U_PATTERN_SYNTAX_ERROR;
                    return;
                }
            default:
                token+=ch;
                break;
        }
    }
    if ( checkSufficientDefinition() ) {
        return;
    }
    else {
        status = U_DEFAULT_KEYWORD_MISSING;
        return;
    }
}

UnicodeString&
PluralFormat::format(const Formattable& obj,
                   UnicodeString& appendTo,
                   FieldPosition& pos,
                   UErrorCode& status) const
{
    if (U_FAILURE(status)) return appendTo;
    int32_t number;
    
    switch (obj.getType())
    {
    case Formattable::kDouble:
        return format((int32_t)obj.getDouble(), appendTo, pos, status);
        break;
    case Formattable::kLong:
        number = (int32_t)obj.getLong();
        return format(number, appendTo, pos, status);
        break;
    case Formattable::kInt64:
        return format((int32_t)obj.getInt64(), appendTo, pos, status);
    default:
        status = U_ILLEGAL_ARGUMENT_ERROR;
        return appendTo;
    }
}

UnicodeString
PluralFormat::format(int32_t number, UErrorCode& status) const {
    FieldPosition fpos(0);
    UnicodeString result;
    
    return format(number, result, fpos, status);
}

UnicodeString&
PluralFormat::format(int32_t number,
                     UnicodeString& appendTo, 
                     FieldPosition& pos,
                     UErrorCode& /*status*/) const {

    if (fParsedValuesHash==NULL) {
        if ( replacedNumberFormat== NULL ) {
            return numberFormat->format(number, appendTo, pos);
        }
        else {
            replacedNumberFormat->format(number, appendTo, pos);
        }
    }
    UnicodeString selectedRule = pluralRules->select(number);
    UnicodeString *selectedPattern = (UnicodeString *)fParsedValuesHash->get(selectedRule);
    if (selectedPattern==NULL) {
        selectedPattern = (UnicodeString *)fParsedValuesHash->get(pluralRules->getKeywordOther());
    }
    appendTo = insertFormattedNumber(number, *selectedPattern, appendTo, pos);
    
    return appendTo;
}

UnicodeString&
PluralFormat::toPattern(UnicodeString& appendTo) {
    appendTo+= pattern;
    return appendTo;
}

UBool
PluralFormat::inRange(UChar ch, fmtToken& type) {
    if ((ch>=CAP_A) && (ch<=CAP_Z)) {
        // we assume all characters are in lower case already.
        return FALSE;
    }
    if ((ch>=LOW_A) && (ch<=LOW_Z)) {
        type = tLetter;
        return TRUE;
    }
    switch (ch) {
        case LEFTBRACE: 
            type = tLeftBrace;
            return TRUE;
        case SPACE:
            type = tSpace;
            return TRUE;
        case RIGHTBRACE:
            type = tRightBrace;
            return TRUE;
        case NUMBER_SIGN:
            type = tNumberSign;
            return TRUE;
        default :
            type = none;
            return FALSE;
    }
}

UBool
PluralFormat::checkSufficientDefinition() {
    // Check that at least the default rule is defined.
    if (fParsedValuesHash==NULL)  return FALSE;
    if (fParsedValuesHash->get(pluralRules->getKeywordOther()) == NULL) {
        return FALSE;
    }
    else {
        return TRUE;
    }
}

void
PluralFormat::setLocale(const Locale& locale, UErrorCode& status) {
    if (pluralRules!=NULL) {
        delete pluralRules;
        pluralRules=NULL;
    }
    if (fParsedValuesHash!= NULL) {
        delete fParsedValuesHash;
        fParsedValuesHash = NULL;
    }
    if (numberFormat!=NULL) {
        delete numberFormat;
        numberFormat = NULL;
        replacedNumberFormat=NULL;
    }
    init(NULL, locale, status);
}

void
PluralFormat::setNumberFormat(const NumberFormat* format, UErrorCode& /*status*/) {
    // TODO: The copy constructor and assignment op of NumberFormat class are protected.
    // create a pointer as the workaround.
    replacedNumberFormat = (NumberFormat *)format;
}

Format*
PluralFormat::clone() const
{
    return new PluralFormat(*this);
}

/*
Format*
PluralFormat::clone() const {
}
*/

UBool
PluralFormat::operator==(const Format& other) const {
    // This protected comparison operator should only be called by subclasses
    // which have confirmed that the other object being compared against is
    // an instance of a sublcass of PluralFormat.  THIS IS IMPORTANT.
    // Format::operator== guarantees that this cast is safe
    PluralFormat* fmt = (PluralFormat*)&other;
    return ((*pluralRules == *(fmt->pluralRules)) && 
            (*numberFormat == *(fmt->numberFormat)));
}

UBool
PluralFormat::operator!=(const Format& other) const {
    return  !operator==(other);
}

void
PluralFormat::parseObject(const UnicodeString& /*source*/,
                        Formattable& /*result*/,
                        ParsePosition& /*pos*/) const
{
    // TODO: not yet supported in icu4j and icu4c
}

UnicodeString
PluralFormat::insertFormattedNumber(int32_t number, 
                                    UnicodeString& message,
                                    UnicodeString& appendTo,
                                    FieldPosition& pos) const {
    UnicodeString result;
    int32_t braceStack=0;
    int32_t startIndex=0;
    
    if (message.length()==0) {
        return result;
    }
    appendTo = numberFormat->format(number, appendTo, pos);
    for(int32_t i=0; i<message.length(); ++i) {
        switch(message.charAt(i)) {
        case LEFTBRACE:
            ++braceStack;
            break;
        case RIGHTBRACE:
            --braceStack;
            break;
        case NUMBER_SIGN:
            if (braceStack==0) {
                result += UnicodeString(message, startIndex, i);
                result += appendTo;
                startIndex = i + 1;
            }
            break;
        }
    }
    if ( startIndex < message.length() ) {
        result += UnicodeString(message, startIndex, message.length()-startIndex);
    }
    appendTo = result;
    return result;
}

void
PluralFormat::copyHashtable(Hashtable *other, UErrorCode& status) {
    if (other == NULL) {
        fParsedValuesHash = NULL;
        return;
    }
    fParsedValuesHash = new Hashtable(TRUE, status);
    if(U_FAILURE(status)){
        return;
    }
    fParsedValuesHash->setValueDeleter(deleteHashStrings);
    int32_t pos = -1;
    const UHashElement* elem = NULL;
    // walk through the hash table and create a deep clone
    while((elem = other->nextElement(pos))!= NULL){
        const UHashTok otherKeyTok = elem->key;
        UnicodeString* otherKey = (UnicodeString*)otherKeyTok.pointer;
        const UHashTok otherKeyToVal = elem->value;
        UnicodeString* otherValue = (UnicodeString*)otherKeyToVal.pointer;
        fParsedValuesHash->put(*otherKey, new UnicodeString(*otherValue), status);
        if(U_FAILURE(status)){
            return;
        }
    }
}


U_NAMESPACE_END


#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
