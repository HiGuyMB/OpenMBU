#include "platform/platform.h"
#include "console/console.h"
#include "console/consoleInternal.h"

ConsoleFunction(loadZip, void, 2, 2, "(zipName)")
{
    argc;

    const char* zipName = argv[2];
    Con::executef(3, "onZipLoaded", zipName, Con::getIntArg(0));
}

ConsoleFunction(isResourceBGLoaded, bool, 2, 2, "(file)")
{
    argc;
    return true;
}

ConsoleFunction(isDemoLaunch, bool, 1, 1, "()")
{
    argc;
    return false;
}

const char* getSystemLanguage_forConsole()
{
    char* ret = Con::getReturnBuffer(1024);

    LangType lang = Platform::getSystemLanguage();

    const char* language;

    switch (lang)
    {
        case LANGTYPE_CHINESE:
            language = "chinese";
            break;
        case LANGTYPE_GERMAN:
            language = "german";
            break;
        case LANGTYPE_ENGLISH:
            language = "english";
            break;
        case LANGTYPE_SPANISH:
            language = "spanish";
            break;
        case LANGTYPE_FRENCH:
            language = "french";
            break;
        case LANGTYPE_ITALIAN:
            language = "italian";
            break;
        case LANGTYPE_JAPANESE:
            language = "japanese";
            break;
        case LANGTYPE_KOREAN:
            language = "korean";
            break;
        case LANGTYPE_PORTUGUESE:
            language = "portuguese";
            break;
        default:
            language = "english";
            break;
    }

    dSprintf(ret, 1024, "%s", language);
    return ret;
}

ConsoleFunction(getSystemLanguage, const char*, 1, 1, "()")
{
    argc;
    return getSystemLanguage_forConsole();
}

ConsoleFunction(getLanguage, const char*, 1, 1, "()")
{
    argc;
    const char* lang = Con::getVariable("pref::Language");

    if (*lang)
        return lang;

    Con::warnf("getLanguage: $pref::Language is not set, using system language");
    return getSystemLanguage_forConsole();
}

ConsoleFunction(XBLiveIsSignedIn, bool, 1, 2, "([port])")
{
    argc;
    return true;
}

ConsoleFunction(XBLiveIsSignedInSilver, bool, 1, 1, "()")
{
    argc;
    return true;
}

ConsoleFunction(XBLiveIsSignedInGold, bool, 1, 1, "()")
{
    argc;
    return true;
}

ConsoleFunction(XBLiveGetUserName, const char*, 1, 1, "()")
{
    argc;

    // Default to the name Alex, as that is the default username in MBO
    //char* ret = Con::getReturnBuffer(1024);
    //dSprintf(ret, 1024, "%s", "Alex");
    //return ret;

    // Use platform username until we set up a login system.
    return Platform::getUserName(15); // X360 only supported at max 15 characters.
}

ConsoleFunction(XBLiveGetUserId, S32, 1, 1, "()")
{
    argc;

    return 1;
}

ConsoleFunction(XBLiveGetSignInPort, S32, 1, 1, "()")
{
    argc;

    return 0;
}

bool xbliveSessionActive = false;

ConsoleFunction(XBLiveIsStatsSessionActive, bool, 1, 1, "()")
{
    argc;

    return xbliveSessionActive;
}

ConsoleFunction(XBLiveStartStatsSession, void, 1, 1, "()")
{
    argc;

    xbliveSessionActive = true;
}

ConsoleFunction(XBLiveEndStatsSession, void, 1, 1, "()")
{
    argc;

    xbliveSessionActive = false;
}

ConsoleFunction(PDLCAllowMission, bool, 2, 2, "(levelId)")
{
    argc;

    S32 levelId = dAtoi(argv[1]);
    if (levelId < 80)
        return true;
    
    // original x360 code
    // return ((1 << ((levelId - 80) & 7)) & *((U8*)dlcmissionoffset + ((levelId - 80) >> 3))) != 0;

    // Just allow all maps in our version
    return true;
}

ConsoleFunction(ContentQuery, const char*, 1, 2, "([contentCategory])")
{
    S32 category = -1;
    if (argc > 1)
        category = dAtoi(argv[1]);

    Con::printf(" >> Checking for content from categories: %d", category);

    S32 newContentCount = 0;
    S32 totalContentCount = 3;

    char* result = Con::getReturnBuffer(64);

    dSprintf(result, 64, "%d %d", newContentCount, totalContentCount);

    return result;
}

// TODO: This should probably be moved to a better place
ConsoleFunction(getCPPVersion, const char*, 1, 1, "()")
{
    argc;

#ifdef _MSVC_LANG
    U32 version = _MSVC_LANG;
#else
    U32 version = __cplusplus;
#endif

    const char* versionString;
    switch (version)
    {
        case 1L:
            versionString = "pre-C++98";
            break;
        case 199711L:
            versionString = "C++98";
            break;
        case 201103L:
            versionString = "C++11";
            break;
        case 201402L:
            versionString = "C++14";
            break;
        case 201703L:
            versionString = "C++17";
            break;
        case 202002L:
            versionString = "C++20";
            break;
        default:
            versionString = "Unknown";
            break;
    }

    char *ret = Con::getReturnBuffer(1024);
    dSprintf(ret, 1024, "%s (%dL)", versionString, version);

    return ret;
}
