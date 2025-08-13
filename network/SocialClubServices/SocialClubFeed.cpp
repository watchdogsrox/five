#include "SocialClubFeed.h"

#if GEN9_STANDALONE_ENABLED

#include "diag/output.h"
#include "diag/seh.h"
#include "fwnet/netchannel.h"
#include "rline/rldiag.h"
#include "rline/rlsystemui.h"
#include "rline/ros/rlrostitleid.h"
#include "system/guidutil.h"
#include "system/param.h"
#include "system/serviceargs.h"
#include "vector/color32.h" // For crew color support
#include "text/textfile.h"
#include "text/textformat.h"
#include "network/networkinterface.h"
#include "string/stringutil.h"
#include "Peds/Ped.h"
#include "scene/world/GameWorld.h"
#include "core/gamesessionstatemachine.h"
#include "control/ScriptRouterLink.h"

NETWORK_OPTIMISATIONS()

#undef __net_channel
#define __net_channel net_feed
RAGE_DECLARE_SUBCHANNEL(net, feed);

#if TEST_FAKE_FEED
NOSTRIP_XPARAM(scDebugFeed);
NOSTRIP_XPARAM(scFeedShowAllCharacters);
#endif

#define SC_FEED_LOCAL_PLAYER_DISPLAY_NAME RSG_DURANGO

const unsigned ScFeedRequest::INVALID_REQUEST_ID = 0;
unsigned ScFeedRequest::s_NextRequestId = 1;

//#####################################################################################


static const char* ActorTypeStrings[static_cast<unsigned>(ScFeedActorType::Count) + 1] =
{
    "RockstarId",
    "UserId",
    "crew",
    "application",

    "invalid",
};

static const char* ActorTypeFilterStrings[static_cast<unsigned>(ScFeedActorTypeFilter::Count) + 1] =
{
    "All",
    "Friends",
    "Crew",
    "Me",

    "invalid",
};

static const char* PostTypeFilterStrings[static_cast<unsigned>(ScFeedPostTypeFilter::Count) + 1] =
{
    "", // all
    "marketing",
    "landing",
    "priority",
    "heist",
    "series",
    "", // invalid
};

static const char* RegardingTypeStrings[static_cast<unsigned>(ScFeedRegardingType::Count) + 1] =
{
    "crew",
    "ugc",
    "emblem",
    "dlc",
    "news",
    "item",

    "contest",
    "event",
    "accomplishment",
    "presetmessage",
    "milestone",
    "challenge",
    "subject",
    "game",
    "platform",
    "title",
    "oldvalue",
    "newvalue",
    "missiontype",
    "parent", //I think this is no longer used but too scared to remove it right now.
    "ugc_publish",
    "ugc_category",
    "marketing",
    "posse",
    "character",

    "invalid"
};

struct ScFeedTypeNameAndFlags
{
    const char* m_Name;
    unsigned m_ImageFlags;
    ScFeedUI m_UI;
    unsigned m_HeightEst;
    unsigned m_Flags;
};

#define SCH_DNA 100
#define SCH_SMA 165
#define SCH_MED 450
#define SCH_LAR 550
#define SCH_XLR 700

static ScFeedTypeNameAndFlags FeedActivityTypeStrings[static_cast<unsigned>(ScFeedType::Count) + 1] =
{
    { "account_registered",               SC_F_I_NONE,              ScFeedUI::Default,       SCH_SMA,      SCF_NONE },

    { "rockstar_gta5_landing",            SC_F_I_FROM_REGARDING,    ScFeedUI::Marketing,     SCH_DNA,      SCF_BIGFEED },
    { "rockstar_gta5_priority",           SC_F_I_FROM_REGARDING,    ScFeedUI::Marketing,     SCH_DNA,      SCF_BIGFEED },
    { "rockstar_gta5_heist",              SC_F_I_FROM_REGARDING,    ScFeedUI::Marketing,     SCH_DNA,      SCF_BIGFEED },
    { "rockstar_gta5_series",             SC_F_I_FROM_REGARDING,    ScFeedUI::Marketing,     SCH_DNA,      SCF_BIGFEED },

#if TEST_FAKE_FEED
    //This is a fake feed I use to display feeds whose type I don't know. Makes it easier to bug those.
    { "basic_feed_post",                  SC_F_I_NONE,              ScFeedUI::Default,       SCH_DNA,      SCF_NONE },
#endif
    
    { "invalid",                          SC_F_I_NONE,              ScFeedUI::Default,       SCH_DNA,      SCF_NONE }
};


struct ScFeedStampMapping
{
    atHashString m_ServerName;
    atHashString m_Texture;
    atHashString m_Txd;
};

// Localizable stamps are handled differently
#define LOC_STAMP_TXD atHashString()

// The stamps from the base build
#define OLD_STAMP_TXD ATSTRINGHASH("elements_stamps_icons", 0xB66E3D00)

// the title update stamps
#define NEW_STAMP_TXD ATSTRINGHASH("elements_stamps_icons_tu", 0x95106EA3)

// Mapping between server names and game texture and and texture dictionary
static ScFeedStampMapping FeedStamps[] =
{
    { ATSTRINGHASH("featured", 0xC13223A9),     ATSTRINGHASH("STAMP_FEATURED", 0x4DEF17ED),         LOC_STAMP_TXD },
    { ATSTRINGHASH("rewards", 0xA31D3648),      ATSTRINGHASH("STAMP_REWARDS", 0xA83578BB),          LOC_STAMP_TXD },
    { ATSTRINGHASH("new", 0xC7FEA6CD),          ATSTRINGHASH("STAMP_NEW", 0x9C1188BD),              LOC_STAMP_TXD },

    { ATSTRINGHASH("doublexp", 0x2FE3CD78),     ATSTRINGHASH("STAMP_X2_XP", 0x79961D34),            NEW_STAMP_TXD },
    { ATSTRINGHASH("triplexp", 0x5992BCF2),     ATSTRINGHASH("STAMP_X3_XP", 0xAAC918A7),            NEW_STAMP_TXD },
    { ATSTRINGHASH("doublerp", 0x6D2CCA49),     ATSTRINGHASH("STAMP_X2_RP", 0x2325C109),            NEW_STAMP_TXD },
    { ATSTRINGHASH("triplerp", 0x2D08DB6F),     ATSTRINGHASH("STAMP_X3_RP", 0x28D4A8E4),            NEW_STAMP_TXD },
};

const char* ScFeedLoc::LOCAL_IMAGE_PROTOCOL = "tx://";
const char* ScFeedLoc::LOCAL_TEXT_PROTOCOL = "locid://";

const atHashString ScFeedLoc::TITLE_ID = ATSTRINGHASH("title", 0x72AAD81D);
const atHashString ScFeedLoc::BIGTITLE_ID = ATSTRINGHASH("bigtitle", 0xE1A91B7C);
const atHashString ScFeedLoc::BODY_ID = ATSTRINGHASH("body", 0x8C255AA8);
const atHashString ScFeedLoc::STICKERTEXT_ID = ATSTRINGHASH("stickertext", 0xE2663CBD);
const atHashString ScFeedLoc::FOOTER_ID = ATSTRINGHASH("footer", 0x67BD5A47);
const atHashString ScFeedLoc::BUTTON_ID = ATSTRINGHASH("button", 0x134A257D);
const atHashString ScFeedLoc::IMAGE_ID = ATSTRINGHASH("imagepath", 0x53CC2F40);
const atHashString ScFeedLoc::BIGIMAGE_ID = ATSTRINGHASH("bigimagepath", 0xC6276EA7);
const atHashString ScFeedLoc::LOGO_ID = ATSTRINGHASH("logo", 0x276F6D38);
const atHashString ScFeedLoc::URL_ID = ATSTRINGHASH("url", 0xBC113F8C);

const atHashString ScFeedLoc::UNSUPPORTED_CHARS_COMMON_REPLACEMENT = ATSTRINGHASH("UI_SCF_UNSUPPORTED_CHARS_COMMON_REPLACEMENT", 0xF1ED38AD);
const atHashString ScFeedLoc::UNSUPPORTED_CHARS_UGC_TITLE_REPLACEMENT = ATSTRINGHASH("UI_SCF_UNSUPPORTED_CHARS_UGC_TITLE_REPLACEMENT", 0x7E35D774);
const atHashString ScFeedLoc::UNSUPPORTED_CHARS_UGC_DESC_REPLACEMENT = ATSTRINGHASH("UI_SCF_UNSUPPORTED_CHARS_UGC_DESC_REPLACEMENT", 0xF321DC6D);

const atHashString ScFeedParam::LAYOUT_TYPE_ID = ATSTRINGHASH("layout", 0x2F19C989);
const atHashString ScFeedParam::LAYOUT_PUBLISH_ID = ATSTRINGHASH("lid", 0x2454C1F5);
const atHashString ScFeedParam::LAYOUT_PRIORITY_ID = ATSTRINGHASH("lpriority", 0xD3CB6A9B);
const atHashString ScFeedParam::ROW_ID = ATSTRINGHASH("row", 0x2F64EB8B);
const atHashString ScFeedParam::COL_ID = ATSTRINGHASH("col", 0x7C2387EB);
const atHashString ScFeedParam::DISABLED_ID = ATSTRINGHASH("disabled", 0x994EDFE1);
const atHashString ScFeedParam::DEEP_LINK_ID = ATSTRINGHASH("dlink", 0x3155ED6B);
const atHashString ScFeedParam::STICKER_ID = ATSTRINGHASH("sticker", 0x22C10B34);

const atHashString ScFeedParam::PRIORITY_IMPRESSION_COUNT_ID = ATSTRINGHASH("imp", 0xF082BC9);
const atHashString ScFeedParam::PRIORITY_TAB_ID = ATSTRINGHASH("tab", 0xAE3704EB);
const atHashString ScFeedParam::PRIORITY_SKIP_DELAY_ID = ATSTRINGHASH("skipdelay", 0xF8D326AD);
const atHashString ScFeedParam::PRIORITY_PRIORITY_ID = ATSTRINGHASH("prio", 0xB771F34);

const atHashString ScFeedParam::PRIORITY_TAB_SP = ATSTRINGHASH("sp", 0xB886F000);
const atHashString ScFeedParam::PRIORITY_TAB_MP = ATSTRINGHASH("mp", 0xD5A1FA5);

// UI dlink params
const atHashString ScFeedParam::DLINK_MODE_UIAPP = ATSTRINGHASH("ui", 0x1B2D32DB);
const atHashString ScFeedParam::DLINK_UIAPP_ID = ATSTRINGHASH("uiappid", 0x837034B9);
const atHashString ScFeedParam::DLINK_UIAPP_ENTRY_POINT = ATSTRINGHASH("uiappentry", 0xD2AB048D);
const atHashString ScFeedParam::DLINK_MODE_FREEROAM = ATSTRINGHASH("freeroam", 0xC167ECC7);
const atHashString ScFeedParam::DLINK_CLASS_UI = ATSTRINGHASH("uiGoToPageMessage", 0x6A1D7748);
const atHashString ScFeedParam::DLINK_CLASS_MP = ATSTRINGHASH("uiGoToOnlineMode", 0xD688B89D);
const atHashString ScFeedParam::DLINK_UI_TARGET_ID = ATSTRINGHASH("target", 0xA2220B09);
const atHashString ScFeedParam::DLINK_UI_TARGET_NEWS = ATSTRINGHASH("whatsNewPage", 0x328CE97A);

const atHashString ScFeedParam::DLINK_MODE_GOLDSTORE_METRO = ATSTRINGHASH("goldstoremetro", 0x2E95DA5B);
const atHashString ScFeedParam::DLINK_MODE_GOLDSTORE_REAL = ATSTRINGHASH("goldstore", 0x981211E6);
const atHashString ScFeedParam::DLINK_MODE_GOLDSTORE_SPUPGRADE = ATSTRINGHASH("spupgrade", 0xB319EA37);

const atHashString ScFeedParam::DLINK_MODE_MP_SUBSCRIPTION_UPSELL = ATSTRINGHASH("plus_upsell", 0x884947A0);
const atHashString ScFeedParam::DLINK_SCAUTHLINK = ATSTRINGHASH("scauthlink", 0xE5A36160);
const atHashString ScFeedParam::DLINK_TRACKING_ID = ATSTRINGHASH("sk", 0x91F922BD);

const atHashString ScFeedParam::VISIBILITY_TAG_ID = ATSTRINGHASH("vistag", 0x1DC45C4F);

const atHashString ScFeedParam::VIS_TAG_MP_SUBSCRIPTION_ONLY = ATSTRINGHASH("mp_only", 0x77C379F1);
const atHashString ScFeedParam::VIS_TAG_NO_MP_SUBSCRIPTION_ONLY = ATSTRINGHASH("no_mp_only", 0x5E6D0DE7);
const atHashString ScFeedParam::STANDALONE_ONLY = ATSTRINGHASH("standalone_only", 0xF209CC82);
const atHashString ScFeedParam::NOT_STANDALONE_ONLY = ATSTRINGHASH("not_standalone_only", 0xA74EBF5C);

ORBIS_ONLY(const atHashString ScFeedParam::VIS_TAG_CUSTOM_UPSELL_ONLY = ATSTRINGHASH("custom_upsell_only", 0xB172C1BD));
ORBIS_ONLY(const atHashString ScFeedParam::VIS_TAG_MP_PROMOTION_ONLY = ATSTRINGHASH("mp_promo_only", 0x5CB543B9));

const char* ScFeedParam::DLINK_FLAG_IS_STORE = "is_store";
const char* ScFeedParam::DLINK_FLAG_IS_PERSISTENT_POSSE = "is_p_posse";
const char* ScFeedParam::DLINK_FLAG_IS_STD_FREEROAM = "is_std_freeroam";
const char* ScFeedParam::DLINK_SPUPGRADE_STRING = "mode=spupgrade";

//#####################################################################################

RosFeedGuid::RosFeedGuid()
{
}

RosFeedGuid::RosFeedGuid(const sysGuid& other)
{
    m_data64[0] = other.m_data64[0];
    m_data64[1] = other.m_data64[1];
}

RosFeedGuidFixedString RosFeedGuid::ToFixedString() const
{
    RosFeedGuidFixedString str;
    formatf_sized(str.GetInternalBuffer(), str.GetInternalBufferSize(), "%08x%08x%08x", m_data32[0], m_data32[1], m_data32[2]);
    str.SetLengthUnsafe(static_cast<short>(strlen(str.GetInternalBuffer())));

    return str;
}

unsigned RosFeedGuid::ToHash() const
{
    return m_data32[0] ^ m_data32[1] ^ m_data32[2] ^ m_data32[3];
}

bool RosFeedGuid::FromString(const char* string, RosFeedGuid& result)
{
    result.Clear();

    rtry
    {
        rverify(string, catchall, gnetError("RosFeedGuid - string is null"));

        if (strlen(string) == RosFeedGuid::NUM_CHARS_FOR_GUID_STRING)
        {
            rverify(::sscanf(string, "%08x%08x%08x", &result.m_data32[0], &result.m_data32[1], &result.m_data32[2]) == 3, catchall, gnetError("RosFeedGuid - string %s is no valid feed hex string", string));
        }
        else
        {
            rverify(sysGuidUtil::GuidFromString(result, string) > 0, catchall, gnetError("RosFeedGuid - string %s is no valid guid hex string", string));
        }

    }
    rcatchall
    {
        return false;
    }

    return true;
}

RosFeedGuid RosFeedGuid::FromString(const char* string)
{
	RosFeedGuid result;
	FromString(string, result);
	return result;
}

RosFeedGuid RosFeedGuid::GenerateGuid()
{
	RosFeedGuid guid = sysGuidUtil::GenerateGuid();
	guid.m_data32[3] = 0;

	return guid;
}

//#####################################################################################

ScFeedActor::ScFeedActor()
    : m_Id(0)
    , m_Type(ScFeedActorType::Invalid)
{
    memset(m_Name, 0, sizeof(m_Name));
    memset(m_RockstarName, 0, sizeof(m_RockstarName));
}

const char* ScFeedActor::GetDisplayName() const
{
#if SC_FEED_LOCAL_PLAYER_DISPLAY_NAME
    if (!IsUserActor())
    {
        return GetName();
    }

    const rlGamerInfo* me = NetworkInterface::GetActiveGamerInfo();
    if (me && GetGamerHandle() == NetworkInterface::GetLocalGamerHandle() && me->HasDisplayName())
    {
        return me->GetDisplayName();
    }
#endif //SC_FEED_LOCAL_PLAYER_DISPLAY_NAME

    return GetName();
}

bool ScFeedActor::DeserialiseActor(const RsonReader& rr, const ScFeedType feedType)
{
    bool result = false;

    rtry
    {
        char typeBuffer[64] = { 0 };
        char idBuffer[64] = { 0 };

        // Don't waste memory on the image when the post doesn't need it.
        const bool needsImage = (ScFeedPost::ImageFlagsFromType(feedType) & SC_F_I_FROM_ACTOR) != 0;

        rverify(rr.HasMember("type") && rr.ReadString("type", typeBuffer) && TypeFromString(typeBuffer, m_Type), catchall, gnetError("Could not read 'type' element %s", typeBuffer));
        rverify(!rr.HasMember("id") || rr.ReadString("id", idBuffer), catchall, gnetError("Could not read 'id' element as string"));
        rverify(!rr.HasMember("name") || rr.ReadString("name", m_Name), catchall , gnetError("Could not read 'name' element"));
        rverify(!needsImage || !rr.HasMember("image") || rr.ReadString("image", m_Image), catchall, gnetError("Could not read 'image' element"));

        if (m_Type == ScFeedActorType::RockstarId)
        {
            rverify(!rr.HasMember("id") || rr.ReadInt64("id", m_Id), catchall, gnetError("Could not read 'id' element as int64"));

            // If it's PC or another platform using social club directly this is also the GamerHandle
        #if RL_SC_PLATFORMS
            rverify(m_GamerHandle.FromUserId(idBuffer), catchall, gnetError("Could parse the gamerhandle %s", idBuffer));
        #elif (!ALLOW_FEED_USERS_FROM_OTHER_PLATFORMS)
        #if TEST_FAKE_FEED
            const bool fakeFeedsOn = PARAM_scDebugFeed.Get();
            rverify(m_Type != ScFeedActorType::RockstarId || fakeFeedsOn, catchall, gnetError("On this platform RockstarId as main user id is not allowed"));
        #else
            rverify(m_Type != ScFeedActorType::RockstarId, catchall, gnetError("On this platform RockstarId as main user id is not allowed"));
        #endif //TEST_FAKE_FEED
        #endif //elif (!ALLOW_FEED_USERS_FROM_OTHER_PLATFORMS)

            safecpy(m_RockstarName, m_Name);
        }
        else if (m_Type == ScFeedActorType::UserId)
        {
            //TODO_ANDI: we may need a similar check on the subject or regarding. There's currently no working feed returning users there.
        #if !ALLOW_FEED_USERS_FROM_OTHER_PLATFORMS
            int onlineServiceType = -1;
            rverify(rr.ReadInt("os", onlineServiceType), catchall, gnetError("Could not read 'os' (online service type) from altId"));

            rverify(onlineServiceType == rlGamerHandle::NATIVE_SERVICE, catchall, gnetError("Users with a wrong online service are not allowed user os[%d] game os[%d]",
                onlineServiceType, static_cast<int>(rlGamerHandle::NATIVE_SERVICE)));
        #endif //!ALLOW_FEED_USERS_FROM_OTHER_PLATFORMS

            rverify(m_GamerHandle.FromUserId(idBuffer), catchall, gnetError("Could parse the gamerhandle %s", idBuffer));

            RsonReader altids;
            bool hasRegarding = rr.GetMember("altids", &altids);
            hasRegarding = altids.GetFirstMember(&altids);
            while (hasRegarding)
            {
                ScFeedActorType altType = ScFeedActorType::Invalid;

                rverify(altids.HasMember("type") && altids.ReadString("type", typeBuffer) && TypeFromString(typeBuffer, altType), catchall, gnetError("Could not read 'type' element %s from altId", typeBuffer));

                if (altType == ScFeedActorType::RockstarId)
                {
                    rverify(!altids.HasMember("id") || altids.ReadInt64("id", m_Id), catchall, gnetError("Could not read 'id' element from altId"));
                    rverify(!altids.HasMember("name") || altids.ReadString("name", m_RockstarName), catchall, gnetError("Could not read 'name' element  from altId"));

                    break;
                }
                
                hasRegarding = altids.GetNextSibling(&altids);
            }
        }
        else
        {
            rverify(!rr.HasMember("id") || rr.ReadInt64("id", m_Id), catchall, gnetError("Could not read 'id' element as int64"));
        }

        result = true;
    }
    rcatchall
    {
    }

    return result;
}

void ScFeedActor::SetAsRockstar()
{
    safecpy(m_Name, "Rockstar");
    safecpy(m_RockstarName, "Rockstar");
    m_GamerHandle.Clear();
    m_Id = 0;
    m_Type = ScFeedActorType::Application;
}

bool ScFeedActor::TypeFromString(const char* typeString, ScFeedActorType& resultType)
{
    resultType = ScFeedActorType::Invalid;
    if (typeString == nullptr)
    {
        return false;
    }
    for (unsigned i = 0; i < static_cast<unsigned>(ScFeedActorType::Count); ++i)
    {
        if (stricmp(typeString, ActorTypeStrings[i]) == 0)
        {
            resultType = static_cast<ScFeedActorType>(i);
            return true;
        }
    }
    return false;
}

bool ScFeedActor::Matches(const ScFeedActorType type, const char* id) const
{
    if (m_Type != type)
    {
        return false;
    }

    switch (type)
    {
    case ScFeedActorType::Crew:
    case ScFeedActorType::RockstarId:
    case ScFeedActorType::Application:
        {
            s64 rockstarId = 0;

            int res = sscanf(id, "%" I64FMT "d", &rockstarId);
            (void)res;

            rlAssertf(res == 1, "Failed to decode rockstarId");

            return rockstarId == m_Id;
        } break;
    case ScFeedActorType::UserId:
        {
            rlGamerHandle handle;
            handle.FromUserId(id);

            return m_GamerHandle == handle;
        } break;
    default: break;
    }

    return false;
}

bool ScFeedActor::operator==(const ScFeedActor& other) const
{
    return m_Type == other.m_Type && m_Id == other.m_Id;
}

//#####################################################################################

ScFeedSubject::ScFeedSubject()
{

}

bool ScFeedSubject::DeserialiseSubject(const RsonReader& rr)
{
    bool result = false;

    rtry
    {
        rverify(!rr.HasMember("id") || rr.ReadString("id", m_Id), catchall ,gnetError("Could not read 'id' element"));
        rverify(!rr.HasMember("type") || rr.ReadString("type", m_Type), catchall, gnetError("Could not read 'type' element"));
        rverify(!rr.HasMember("name") || rr.ReadString("name", m_Name), catchall, gnetError("Could not read 'name' element"));
        rverify(!rr.HasMember("image") || rr.ReadString("image", m_Image), catchall, gnetError("Could not read 'image' element"));
        rverify(!rr.HasMember("data") || rr.ReadString("data", m_Data), catchall, gnetError("Could not read 'data' element"));

        result = true;
    }
    rcatchall
    {
    }

    return result;
}

s64 ScFeedSubject::GetIdInt64() const
{
    s64 userId = 0;

    int res = sscanf(m_Id.c_str(), "%" I64FMT "d", &userId);
    (void)res;

    rlAssertf(res == 1, "Failed to decode userId");

    return userId;
}

bool ScFeedSubject::GetIdInt64(s64& value) const
{
    value = 0;
    int res = sscanf(m_Id.c_str(), "%" I64FMT "d", &value);
    return res == 1;
}

//#####################################################################################
ScFeedRegardignExtraData::ScFeedRegardignExtraData()
{
    memset(&m_Crew, 0, sizeof(m_Crew));
}

//#####################################################################################

ScFeedRegarding::ScFeedRegarding()
    : m_RegardingType(ScFeedRegardingType::Invalid)
{
}

bool ScFeedRegarding::DeserialiseRegarding(const RsonReader& rr, ScFeedActor& actor, ScFeedPost& post)
{
    bool result = false;

    rtry
    {
        char buffer[128] = { 0 };

        rverify(rr.HasName() && rr.GetName(buffer) && TypeFromString(buffer, m_RegardingType), catchall, gnetError("Could not read 'regarting type' %s", buffer));
        rverify(m_Subject.DeserialiseSubject(rr), catchall, gnetError("The subject deserialization has failed"));

        if (m_RegardingType == ScFeedRegardingType::Subject || m_RegardingType == ScFeedRegardingType::Crew)
        {
            rverify(actor.DeserialiseActor(rr, post.GetFeedType()), catchall, gnetError("A subject or crew are supposed to contain a valid actor"));
        }

        if (m_RegardingType == ScFeedRegardingType::Ugc)
        {
            // We don't fail the parsing if only played or dislike fails to read
            if (rr.HasMember("played")) rr.ReadUns("played", m_ExtraData.m_Ugc.m_NumPlayed);
            if (rr.HasMember("dislike")) rr.ReadUns("dislike", m_ExtraData.m_Ugc.m_NumDislikes);
        }

        if (m_RegardingType == ScFeedRegardingType::Posse)
        {
            // We don't fail the parsing if only size or reputation fails to read
            // m_Type and m_Data aren't used for the posse so we use those for size and reputation.
            if (rr.HasMember("size")) rr.ReadString("size", m_Subject.m_Type);
            if (rr.HasMember("reputation")) rr.ReadString("reputation", m_Subject.m_Data);

            atString leaderpic;
            RsonReader leader;
            if (rr.GetMember("lead", &leader) && leader.ReadString("pedshot", leaderpic))
            {
                post.AddImage(leaderpic);
            }

            RsonReader members;
            
            bool hasMembers = rr.GetMember("members", &members);
			hasMembers = hasMembers && members.GetFirstMember(&members);

            while (hasMembers)
            {
                atString pic;
                if (members.ReadString("pedshot", pic))
                {
                    post.AddImage(pic);
                }

				hasMembers = members.GetNextSibling(&members);
            }
        }

        if (m_RegardingType == ScFeedRegardingType::Crew)
        {
            m_ExtraData.m_Crew.m_CrewMemberCount = 1; // Just in case the param is missing we assume there's one member

            rverify(!rr.HasMember("tag") || rr.ReadString("tag", m_ExtraData.m_Crew.m_CrewTag), catchall, gnetError("Could not read crew tag"));
            rverify(!rr.HasMember("oc") || rr.ReadBool("oc", m_ExtraData.m_Crew.m_CrewOpen), catchall, gnetError("Could not read crew oc (open crew)"));
            rverify(!rr.HasMember("mc") || rr.ReadUns("mc", m_ExtraData.m_Crew.m_CrewMemberCount), catchall, gnetError("Could not read crew mc (member count)"));

            if (rr.HasMember("color"))
            {
                char col[16] = { 0 };
                int r = 0, g = 0, b = 0;

                rverify(rr.ReadString("color", col), catchall, gnetError("Could not read crew color"));
                if (sscanf(col, "#%2x%2x%2x", &r, &g, &b) == 3)
                {
                    Color32 colorHelper(r, g, b);
                    m_ExtraData.m_Crew.m_CrewColor = colorHelper.GetColor();
                }
            }

        }

        if (m_RegardingType == ScFeedRegardingType::Ugc || m_RegardingType == ScFeedRegardingType::UgcPublish)
        {
            ScFeedLoc::SanitizeText(m_Subject.m_Name, ScFeedLoc::REQUIRED_READABLE_PERCENTAGE, ScFeedLoc::UNSUPPORTED_CHARS_UGC_TITLE_REPLACEMENT);
            ScFeedLoc::SanitizeText(m_Subject.m_Data, ScFeedLoc::REQUIRED_READABLE_PERCENTAGE, ScFeedLoc::UNSUPPORTED_CHARS_UGC_DESC_REPLACEMENT);
        }

#if 0 && !ALLOW_FEED_POSTS_FROM_OTHER_PLATFORMS
        if (m_RegardingType == ScFeedRegardingType::Platform)
        {
            const u32 feedPlatform = static_cast<u32>(m_Subject.GetIdInt64());
            rverify(feedPlatform == rlRosTitleId::GetPlatformId(), catchall, gnetError("The platform of the feed [%u] does not match our platform [%u,%s]",
                feedPlatform, rlRosTitleId::GetPlatformId(), rlRosTitleId::GetPlatformName()));
        }
#endif //!ALLOW_FEED_POSTS_FROM_OTHER_PLATFORMS

        result = true;
    }
    rcatchall
    {
    }

    return result;
}

bool ScFeedRegarding::TypeFromString(const char* typeString, ScFeedRegardingType& resultType)
{
    resultType = ScFeedRegardingType::Invalid;
    if (typeString == nullptr)
    {
        return false;
    }

    // In _agg childs ucg is called ugccontentid but it's the same data as ugc
    if (strcmp(typeString, "ugccontentid") == 0)
    {
        resultType = ScFeedRegardingType::Ugc;
        return true;
    }

    for (unsigned i = 0; i < static_cast<unsigned>(ScFeedRegardingType::Count); ++i)
    {
        if (strcmp(typeString, RegardingTypeStrings[i]) == 0)
        {
            resultType = static_cast<ScFeedRegardingType>(i);
            return true;
        }
    }
    return false;
}

const char* ScFeedRegarding::TypeToString(ScFeedRegardingType feedRegardingType)
{
    if (static_cast<unsigned>(feedRegardingType) >= static_cast<unsigned>(ScFeedRegardingType::Count))
    {
        return  RegardingTypeStrings[static_cast<unsigned>(ScFeedRegardingType::Invalid)];
    }
    return RegardingTypeStrings[static_cast<unsigned>(feedRegardingType)];
}

//#####################################################################################

ScFeedFlags::ScFeedFlags()
    : m_AllowComment(false)
    , m_AllowReaction(false)
    , m_AllowShare(false)
    , m_AllowDelete(false)
    , m_AllowReport(false)
    , m_AllowBlock(false)
    , m_MultipleActors(false)
    , m_Unused(false)
{

}

//#####################################################################################

ScFeedFilterId::ScFeedFilterId()
    : m_OwnerId(0)
    , m_WallType(ScFeedWallType::None)
    , m_Type(ScFeedRequestType::Feeds)
    , m_ActorTypeFilter(ScFeedActorTypeFilter::All)
    , m_PostTypeFilter(ScFeedPostTypeFilter::All)
{
}

ScFeedFilterId::ScFeedFilterId(const ScFeedRequestType requestType, const ScFeedWallType wallType, const RockstarId ownerId,
    const ScFeedActorTypeFilter actorFilter, const ScFeedPostTypeFilter typeFilter)
    : m_OwnerId(ownerId)
    , m_WallType(wallType)
    , m_Type(requestType)
    , m_ActorTypeFilter(actorFilter)
    , m_PostTypeFilter(typeFilter)
{
}

bool ScFeedFilterId::operator==(const ScFeedFilterId& other) const
{
    return m_OwnerId == other.m_OwnerId && m_WallType == other.m_WallType
        && m_Type == other.m_Type && m_ActorTypeFilter == other.m_ActorTypeFilter
        && m_PostTypeFilter == other.m_PostTypeFilter;
}

const char* ScFeedFilterId::ActorTypeFilterToString(const ScFeedActorTypeFilter actorTypeFilter)
{
    if (static_cast<unsigned>(actorTypeFilter) >= static_cast<unsigned>(ScFeedActorTypeFilter::Count))
    {
        return  ActorTypeFilterStrings[static_cast<unsigned>(ScFeedActorTypeFilter::Invalid)];
    }
    return ActorTypeFilterStrings[static_cast<unsigned>(actorTypeFilter)];
}

const char* ScFeedFilterId::PostTypeFilterToString(const ScFeedPostTypeFilter postTypeFilter)
{
    if (static_cast<unsigned>(postTypeFilter) >= static_cast<unsigned>(ScFeedPostTypeFilter::Count))
    {
        return  PostTypeFilterStrings[static_cast<unsigned>(ScFeedPostTypeFilter::Invalid)];
    }
    return PostTypeFilterStrings[static_cast<unsigned>(postTypeFilter)];
}

bool ScFeedFilterId::IsSafeFilterForAlUsers(const ScFeedPostTypeFilter postTypeFilter)
{
    return postTypeFilter >= ScFeedPostTypeFilter::SafeFilterFirst && postTypeFilter <= ScFeedPostTypeFilter::SafeFilterLast;
}

#if TEST_FAKE_FEED
atFixedString<256> ScFeedFilterId::FormatCloudFilePath(const char* suffix) const
{
    char buffer[256];
    formatf(buffer, "../all/non_final/feed_gta5_%s%s.json", m_PostTypeFilter == ScFeedPostTypeFilter::All ? "all" : PostTypeFilterToString(m_PostTypeFilter), suffix);
    return buffer;
}
#endif

//#####################################################################################

bool ScFeedLoc::IsLocalImage(const char* path)
{
    return path && strncmp(path, ScFeedLoc::LOCAL_IMAGE_PROTOCOL, strlen(ScFeedLoc::LOCAL_IMAGE_PROTOCOL)) == 0;
}

bool ScFeedLoc::IsLocalTextId(const char* path)
{
    return path && strncmp(path, ScFeedLoc::LOCAL_TEXT_PROTOCOL, strlen(ScFeedLoc::LOCAL_TEXT_PROTOCOL)) == 0;
}


int Remove(const char* searchStr, char* inoutStr, const int inoutStrLen)
{
    int length = inoutStrLen;

    const int lenSearch = static_cast<int>(strlen(searchStr));
    char *p = (inoutStr != nullptr && searchStr != nullptr && lenSearch > 0) ? strstr(inoutStr, searchStr) : nullptr;

    while (p)
    {
        int moveLen = length - ((int)(p - inoutStr) + lenSearch);
        // Not super efficient to move the whole block each time a hit is found but will do
        memmove(p, p + lenSearch, moveLen);
        length -= lenSearch;

        inoutStr[length] = '\0';

        p = strstr(p, searchStr);
    }

    return length;
}

void ScFeedLoc::SanitizeText(atString& text, const unsigned requiredPercentage, const atHashString replacementTextId)
{
    if (text.length() == 0)
    {
        return;
    }

    // Remove any newline
    const int lenOrig = static_cast<int>(text.length());
    int num = Remove("~n~", &text[0], lenOrig);

    if (num >= 0 && num != lenOrig)
    {
        text.SetLengthUnsafe(static_cast<u16>(num));
    }

    char16 buffer[ScFeedLoc::LONGEST_USER_TEXT_BUFFER];
    buffer[0] = 0;

    const int len = rage::Min(text.GetLength(), static_cast<int>(ScFeedLoc::LONGEST_USER_TEXT_BUFFER - 1));

    // This is slightly confusing. The third parameter is the input string max length in bytes but also the output size.
    Utf8ToWide(buffer, text.c_str(), len);
    int wlen = static_cast<int>(wcslen(buffer));

    bool cropped = false;
    if (wlen >= ScFeedLoc::LONGEST_USER_TEXT_CHARS)
    {
        buffer[ScFeedLoc::LONGEST_USER_TEXT_CHARS - 1] = L'\u2026'; // the ellipsis
        buffer[ScFeedLoc::LONGEST_USER_TEXT_CHARS] = 0;
        wlen = ScFeedLoc::LONGEST_USER_TEXT_CHARS;

        cropped = true;
    }

    if (CanBeDisplayed(buffer, wlen, requiredPercentage))
    {
        if (cropped)
        {
            char convBuffer[ScFeedLoc::LONGEST_USER_TEXT_BUFFER];
            convBuffer[0] = 0;

            int numConverted = 0;

            WideToUtf8(convBuffer, buffer, wlen, static_cast<int>(sizeof(convBuffer)) - 1, &numConverted);

            text = convBuffer;
        }

        return;
    }

    const char* replacement = TheText.Get(replacementTextId, nullptr);
    gnetDebug2("Text [%s] replaced with [%s]", text.c_str(), replacement);

    text = replacement;
}

bool ScFeedLoc::CanBeDisplayed(const u16* text, const unsigned textLength, const unsigned requiredPercentage)
{
    rtry
    {
        rcheck(textLength > 0, catchall,);

        int numSpaces = 0;
        for (int i = 0; i < textLength; ++i)
        {
            const char16 c = text[i];
            if (c == L'\t' || c == L' ') // I know there are other spaces but this should be fine for us
            {
                ++numSpaces;
            }
        }
        rcheck(numSpaces < textLength, catchall, );

        int num = 0;
        for (int i = 0; i < textLength; ++i)
        {
            // This call is a bit slow. doing the loop internally would be faster. If any performance issues show up we can do that.
            num += CTextFormat::DoesCharacterExistInFont(/*FONT_STYLE_SOCIAL_CLUB_REG*/FONT_STYLE_STANDARD, text[i]) ? 0 : 1;
        }

        const int missing = (num * 100) / (textLength - numSpaces);
        if (missing > (100 - requiredPercentage))
        {
            return false;
        }
    }
    rcatchall
    {
    }

    return true;
}

//#####################################################################################

s64 ScFeedParam::ValueAsInt64() const
{
    s64 value = 0;

    ASSERT_ONLY(int res = )sscanf(m_Value.c_str(), "%" I64FMT "d", &value);
    ASSERT_ONLY(rlAssertf(res == 1, "Failed to decode int from[%s] for[%s]", m_Value.c_str(), m_Id.TryGetCStr()));

    return value;
}

bool ScFeedParam::ValueAsInt64(s64& value) const
{
    value = 0;
    int res = sscanf(m_Value.c_str(), "%" I64FMT "d", &value);
    return res == 1;
}

bool ScFeedParam::ValueAsInt64(const char* prefix, s64& value) const
{
	if (m_Value.length() == 0 || prefix == nullptr)
	{
		return false;
	}

	if (!StringStartsWithI(m_Value.c_str(), prefix))
	{
		return false;
	}

	const int len = istrlen(prefix);
	int res = sscanf(m_Value.c_str() + len, "%" I64FMT "d", &value);
	return res == 1;
}

atHashString ScFeedParam::ValueAsHash() const
{
    return atHashString(m_Value.c_str());
}

bool ScFeedParam::ValueAsStampFile(atHashString& texture, atHashString& txd) const
{
    const unsigned num = COUNTOF(FeedStamps);

    const atHashString val = ValueAsHash();

    for (unsigned i = 0; i < num; ++i)
    {
        if (FeedStamps[i].m_ServerName == val)
        {
            texture = FeedStamps[i].m_Texture;
            txd = FeedStamps[i].m_Txd;

            return true;
        }
    }

    return false;
}

bool ScFeedParam::ValueAsBool() const
{
    if (m_Value.length() == 0)
    {
        return false;
    }

    return stricmp(m_Value.c_str(), "1") == 0 || stricmp(m_Value.c_str(), "true") == 0;
}

//#####################################################################################

ScFeedPost::ScFeedPost()
    : m_Type(ScFeedType::Invalid)
    , m_NumGroups(0)
    , m_NumComments(0)
    , m_NumLikes(0)
    , m_CreatePosixTime(0)
    , m_ReadPosixTime(0)
    , m_MyReaction(ScFeedReaction::Unknown)
    , m_HeightEstimate(0)
{

}

ScFeedPost::~ScFeedPost()
{

}

const ScFeedActor* ScFeedPost::GetActor(const char* type, const char* id) const
{
    if (!gnetVerify(type != nullptr && id != nullptr))
    {
        return nullptr;
    }

    ScFeedActorType typeVal = ScFeedActorType::Invalid;
    if (!ScFeedActor::TypeFromString(type, typeVal) || typeVal == ScFeedActorType::Invalid)
    {
        return nullptr;
    }

    if (m_Actor.Matches(typeVal, id))
    {
        return &m_Actor;
    }

    for (int i = 0; i < m_Actors.size(); ++i)
    {
        if (m_Actors[i].Matches(typeVal, id))
        {
            return &m_Actors[i];
        }
    }

    return nullptr;
}

const char* ScFeedPost::GetSubjectDisplayName(const ScFeedSubject& subject) const
{
#if SC_FEED_LOCAL_PLAYER_DISPLAY_NAME
    const ScFeedActor* actor = subject.m_Type.length() == 0 ? nullptr : GetActor(subject.m_Type.c_str(), subject.m_Id.c_str());
    return actor ? actor->GetDisplayName() : subject.m_Name.c_str();
#else
    return subject.m_Name.c_str();
#endif
}

const atString* ScFeedPost::GetLoc(const atHashString id) const
{
    for (int i = 0; i < m_Locs.size(); ++i)
    {
        if (m_Locs[i].m_Id == id)
        {
            return &(m_Locs[i].m_Text);
        }
    }

    return nullptr;
}

const ScFeedParam* ScFeedPost::GetParam(const atHashString id) const
{
    for (int i = 0; i < m_Params.size(); ++i)
    {
        if (m_Params[i].m_Id == id)
        {
            return &(m_Params[i]);
        }
    }

    return nullptr;
}

const char* ScFeedPost::GetField(const char* fieldPath) const
{
    rtry
    {
        rverify(fieldPath != nullptr, catchall, gnetError("fieldPath is null"));

        if (strncmp(fieldPath, "loc.", strlen("loc.")) == 0)
        {
            const atString* loc = GetLoc(fieldPath + strlen("loc."));
            return loc ? loc->c_str() : nullptr;
        }

        if (strncmp(fieldPath, "cdata.", strlen("cdata.")) == 0)
        {
            const ScFeedParam* param = GetParam(fieldPath + strlen("cdata."));
            return param ? param->m_Value.c_str() : nullptr;
        }

        if (strcmp(fieldPath, "actor.name") == 0)
        {
            return m_Actor.GetName();
        }

        if (strncmp(fieldPath, "regarding.", strlen("regarding.")) == 0)
        {
            ScFeedRegardingType regType = ScFeedRegardingType::Invalid;
            const char* regfield = GetRegardingParamFromPath(fieldPath, regType);

            rcheck(regfield != nullptr, catchall, gnetWarning("Invalid fieldPath[%s]", fieldPath));

            const ScFeedRegarding* regarding = GetRegarding(regType);
            rcheck(regarding != nullptr, catchall, gnetWarning("Regarding not found. fieldPath[%s]", fieldPath));

            if (stricmp(regfield, "name") == 0)
            {
                return regarding->m_Subject.m_Name.c_str();
            }
            else if (stricmp(regfield, "data") == 0)
            {
                return regarding->m_Subject.m_Data.c_str();
            }
            else if (stricmp(regfield, "type") == 0)
            {
                return regarding->m_Subject.m_Type.c_str();
            }
            else if (stricmp(regfield, "id") == 0)
            {
                return regarding->m_Subject.m_Id.c_str();
            }

        }
    }
    rcatchall
    {

    }

    gnetWarning("Field not found or supported [%s]", fieldPath);
    return nullptr;
}

bool ScFeedPost::GetFieldAsInt64(const char* fieldPath, s64& result) const
{
    result = 0;

    rtry
    {
        rverify(fieldPath != nullptr, catchall, gnetError("fieldPath is null"));

        if (strncmp(fieldPath, "cdata.", strlen("cdata.")) == 0)
        {
            const ScFeedParam* param = GetParam(fieldPath + strlen("cdata."));
            return param ? param->ValueAsInt64(result) : false;
        }

        if (strncmp(fieldPath, "regarding.", strlen("regarding.")) == 0)
        {
            ScFeedRegardingType regType = ScFeedRegardingType::Invalid;
            const char* regfield = GetRegardingParamFromPath(fieldPath, regType);

            rcheck(regfield != nullptr, catchall, gnetWarning("Invalid fieldPath[%s]", fieldPath));

            const ScFeedRegarding* regarding = GetRegarding(regType);
            rcheck(regarding != nullptr, catchall, gnetWarning("Regarding not found. fieldPath[%s]", fieldPath));

            if (stricmp(regfield, "oc") == 0)
            {
                result = regarding->m_ExtraData.m_Crew.m_CrewOpen ? 1 : 0;
                return true;
            }
            else if (stricmp(regfield, "mc") == 0)
            {
                result = regarding->m_ExtraData.m_Crew.m_CrewMemberCount;
                return true;
            }
            else if (stricmp(regfield, "color") == 0)
            {
                result = regarding->m_ExtraData.m_Crew.m_CrewColor;
                return true;
            }
            else if (stricmp(regfield, "id") == 0)
            {
                return regarding->m_Subject.GetIdInt64(result);
            }
        }
    }
    rcatchall
    {

    }

    gnetDebug1("fieldPath not found or supported [%s]", fieldPath);
    return false;
}

ScFeedReaction ScFeedPost::GetMyReaction() const
{
    return m_MyReaction;
}

ScFeedReaction ScFeedPost::GetMyUgcReaction() const
{
    if (!HasFlags(m_Type, SCF_REAL_UGC))
    {
        return ScFeedReaction::Unknown;
    }

    if (HasFlags(m_Type, SCF_EMBEDDED_UGC))
    {
        return m_RegardingPosts.size() > 0 ? m_RegardingPosts[0].GetMyUgcReaction() : ScFeedReaction::Unknown;
    }

    return m_MyReaction;
}

unsigned ScFeedPost::GetNumLikes() const
{
    if (m_NumLikes >= 1)
    {
        return m_NumLikes;
    }

    // There can be a delay on the like count. If I liked it and it's 0 we set it to 1
    return (GetMyReaction() == ScFeedReaction::Like) ? 1 : 0;
}

unsigned ScFeedPost::GetNumUgcLikes() const
{
    if (!HasFlags(m_Type, SCF_REAL_UGC))
    {
        return 0;
    }

    if (HasFlags(m_Type, SCF_EMBEDDED_UGC))
    {
        return m_RegardingPosts.size() > 0 ? m_RegardingPosts[0].GetNumUgcLikes() : 0;
    }

    return GetNumLikes();
}

bool ScFeedPost::GetUgcStats(unsigned& likes, unsigned& dislikes, unsigned& playcount) const
{
    likes = 0;
    dislikes = 0;
    playcount = 0;

    if (!HasFlags(m_Type, SCF_REAL_UGC))
    {
        return false;
    }

    if (HasFlags(m_Type, SCF_EMBEDDED_UGC))
    {
        return m_RegardingPosts.size() > 0 ? m_RegardingPosts[0].GetUgcStats(likes, dislikes, playcount) : false;
    }

    likes = GetNumUgcLikes();

    const ScFeedRegarding* ugc = GetRegarding(ScFeedRegardingType::Ugc);

    if (ugc)
    {
        dislikes = ugc->m_ExtraData.m_Ugc.m_NumDislikes;
        playcount = ugc->m_ExtraData.m_Ugc.m_NumPlayed;
    }

    return true;
}

const ScFeedRegarding* ScFeedPost::GetRegarding(const ScFeedRegardingType regardingType) const
{
    for (int i = 0; i < m_Regarding.size(); ++i)
    {
        if (m_Regarding[i].GetRegardingType() == regardingType)
        {
            return &m_Regarding[i];
        }
    }
    return nullptr;
}

bool ScFeedPost::TypeFromString(const char* typeString, ScFeedType& resultType)
{
    resultType = ScFeedType::Invalid;
    if (typeString == nullptr)
    {
        return false;
    }
    for (unsigned i = 0; i < static_cast<unsigned>(ScFeedType::Count); ++i)
    {
        if (strcmp(typeString, FeedActivityTypeStrings[i].m_Name) == 0)
        {
            resultType = static_cast<ScFeedType>(i);
            return true;
        }
    }

    return false;
}

const char* ScFeedPost::TypeToString(const ScFeedType feedType)
{
    if (static_cast<unsigned>(feedType) >= static_cast<unsigned>(ScFeedType::Count))
    {
        return  FeedActivityTypeStrings[static_cast<unsigned>(ScFeedType::Invalid)].m_Name;
    }

    return FeedActivityTypeStrings[static_cast<unsigned>(feedType)].m_Name;
}

unsigned ScFeedPost::ImageFlagsFromType(const ScFeedType feedType)
{
    if (static_cast<unsigned>(feedType) >= static_cast<unsigned>(ScFeedType::Count))
    {
        return  FeedActivityTypeStrings[static_cast<unsigned>(ScFeedType::Invalid)].m_ImageFlags;
    }

    return FeedActivityTypeStrings[static_cast<unsigned>(feedType)].m_ImageFlags;
}

unsigned ScFeedPost::FlagsFromType(const ScFeedType feedType)
{
    if (static_cast<unsigned>(feedType) >= static_cast<unsigned>(ScFeedType::Count))
    {
        return  FeedActivityTypeStrings[static_cast<unsigned>(ScFeedType::Invalid)].m_Flags;
    }

    return FeedActivityTypeStrings[static_cast<unsigned>(feedType)].m_Flags;
}

ScFeedUI ScFeedPost::UIFromType(const ScFeedType feedType)
{
    if (static_cast<unsigned>(feedType) >= static_cast<unsigned>(ScFeedType::Count))
    {
        return  FeedActivityTypeStrings[static_cast<unsigned>(ScFeedType::Invalid)].m_UI;
    }

    return FeedActivityTypeStrings[static_cast<unsigned>(feedType)].m_UI;
}

unsigned ScFeedPost::HeightEstimateFromType(const ScFeedType feedType)
{
    if (static_cast<unsigned>(feedType) >= static_cast<unsigned>(ScFeedType::Count))
    {
        return FeedActivityTypeStrings[static_cast<unsigned>(ScFeedType::Invalid)].m_HeightEst;
    }

    return FeedActivityTypeStrings[static_cast<unsigned>(feedType)].m_HeightEst;
}

bool ScFeedPost::HasFlags(const ScFeedType feedType, const unsigned flags)
{
    return (FlagsFromType(feedType) & flags) == flags;
}

bool ScFeedPost::TypeFromJson(const char* jsonDoc, ScFeedType& resultType)
{
    bool result = false;
    rtry
    {
        RsonReader rr;
        rverify(rr.Init(jsonDoc, StringLength(jsonDoc)), catchall, gnetError("Failed to set up reader for the feed"));
        result = TypeFromJson(rr, resultType);
    }
    rcatchall
    {
    }

    return result;
}

bool ScFeedPost::TypeFromJson(const RsonReader& rr, ScFeedType& resultType)
{
    bool result = false;
    rtry
    {
        char typeBuffer[64] = { 0 };

        rverify(rr.HasMember("type") && rr.ReadString("type", typeBuffer), catchall, gnetError("Could not read 'type' element %s", typeBuffer));

        bool tpyeOk = TypeFromString(typeBuffer, resultType);

#if TEST_FAKE_FEED
        if (!tpyeOk)
        {
            gnetWarning("ScFeedPost - Unrecognized feed type: %s", typeBuffer);
            resultType = ScFeedType::UndefinedPost;
        }
#else
        rverify(tpyeOk, catchall, gnetError("ScFeedPost - Unrecognized feed type: %s", typeBuffer));
#endif //TEST_FAKE_FEED

        result = true;
    }
    rcatchall
    {
    }

    return result;
}

bool ScFeedPost::IsRockstarFeedType(const ScFeedType feedType)
{
    return (feedType >= ScFeedType::RockstarFeedFirst && feedType <= ScFeedType::RockstarFeedLast);
}

bool ScFeedPost::IsCrewFeedType(const ScFeedType feedType)
{
    return HasFlags(feedType, SCF_CREW);
}

bool ScFeedPost::IsUgcFeedType(const ScFeedType feedType)
{
    return HasFlags(feedType, SCF_UGC);
}

bool ScFeedPost::IsAggregatedFeedType(const ScFeedType feedType)
{
    return HasFlags(feedType, SCF_AGG);
}

ScFeedType ScFeedPost::GetAggregetedChildType(const ScFeedType feedType)
{
	if (!IsAggregatedFeedType(feedType))
	{
		return feedType;
	}

	return static_cast<ScFeedType>(static_cast<int>(feedType) - 1);
}

bool ScFeedPost::ParseContent(const char* body)
{
    bool result = false;
    rtry
    {
        RsonReader reader;

        ScFeedType feedType = ScFeedType::Invalid;
        rverify(reader.Init(body, static_cast<unsigned>(StringLength(body))), catchall, gnetError("Failed to set up reader for the feed"));

        // Currently it's all a ScFeedPost so this isn't needed but just to be prepared
        rverify(ScFeedPost::TypeFromJson(reader, feedType), catchall, gnetError("Failed to set up reader for the feed"));

        rverify(DeserialiseFeed(reader, ScFeedType::Invalid), catchall, gnetError("Failed to deserialize reader for the feed"));

        // The ID of the aggregated post isn't a real unique ID. It's the ID of first child-feed. To avoid ID clashes we generate a temporary random guid.
        if (IsAggregatedFeedType(m_Type))
        {
            m_Id = RosFeedGuid::GenerateGuid();
        }

        result = true;
    }
    rcatchall
    {
    }

    return result;
}

bool ScFeedPost::DeserialiseFeed(const RsonReader& rr, const ScFeedType feedType)
{
    bool result = false;

    rtry
    {
        RsonReader actorRR;
        char stringBuffer[64] = { 0 };

        m_Type = feedType;

        rverify(!rr.HasMember("id") || rr.ReadString("id", stringBuffer), catchall, gnetError("Could not read 'id' element"));
        rverify(RosFeedGuid::FromString(stringBuffer, m_Id), catchall, gnetError("Could not convert 'id' element"));
        rverify(feedType != ScFeedType::Invalid || TypeFromJson(rr, m_Type), catchall, gnetError("Could not read 'type'"));
        
        rverify(!rr.HasMember("actor") || rr.GetMember("actor", &actorRR), catchall, gnetError("Retrieving actor has failed"));

        // Rockstar feeds don't contain an actor
        if (ScFeedPost::IsRockstarFeedType(m_Type)
#if TEST_FAKE_FEED
            || m_Type == ScFeedType::UndefinedPost
#endif
            )
        {
            m_Actor.SetAsRockstar();
        }
        else
        {
            rverify(m_Actor.DeserialiseActor(actorRR, m_Type), catchall, gnetError("Deserializing actor has failed"));
        }

        rverify(!rr.HasMember("content") || rr.ReadString("content", m_Content), catchall, gnetError("Could not read 'content' element"));

        if (IsUgcFeedType(m_Type))
        {
            ScFeedLoc::SanitizeText(m_Content, ScFeedLoc::REQUIRED_READABLE_PERCENTAGE, ScFeedLoc::UNSUPPORTED_CHARS_COMMON_REPLACEMENT);
        }

        rverify(!rr.HasMember("numGroup") || rr.ReadUns("numGroup", m_NumGroups), catchall, gnetError("Could not read 'numGroup' element"));
        rverify(!rr.HasMember("numCommnets") || rr.ReadUns("numCommnets", m_NumComments), catchall, gnetError("Could not read 'numCommnets' element"));
        rverify(!rr.HasMember("time") || rr.ReadUns64("time", m_CreatePosixTime), catchall, gnetError("Could not read 'time' element"));
        rverify(!rr.HasMember("rt") || rr.ReadUns64("rt", m_ReadPosixTime), catchall, gnetError("Could not read 'rt' element"));
        rverify(!rr.HasMember("numlikes") || rr.ReadUns("numlikes", m_NumLikes), catchall, gnetError("Could not read 'numlikes' element"));

        int mr = 0;
        rverify(!rr.HasMember("mr") || rr.ReadInt("mr", mr), catchall, gnetError("Could not read 'mr' element"));
        m_MyReaction = (mr == 1) ? ScFeedReaction::Like : ScFeedReaction::Unknown;

        if (rr.HasMember("allowedactions"))
        {
            RsonReader sm;
            rr.GetMember("allowedactions", &sm);

            // The flags are 'bittified' so for safety I first read them into a normal bool
            int val = 0;
            rverify(!sm.HasMember("allowreaction") || sm.ReadInt("allowreaction", val), catchall, gnetError("Could not read 'allowreaction' element"));
            m_Flags.m_AllowReaction = val == 1;

            val = 0;
            rverify(!sm.HasMember("allowcomment") || sm.ReadInt("allowcomment", val), catchall, gnetError("Could not read 'allowcomment' element"));
            m_Flags.m_AllowComment = val == 1;

            val = 0;
            rverify(!sm.HasMember("allowshare") || sm.ReadInt("allowshare", val), catchall, gnetError("Could not read 'allowshare' element"));
            m_Flags.m_AllowShare = val == 1;

            val = 0;
            rverify(!sm.HasMember("allowdelete") || sm.ReadInt("allowdelete", val), catchall, gnetError("Could not read 'allowdelete' element"));
            m_Flags.m_AllowDelete = val == 1;

            val = 0;
            rverify(!sm.HasMember("allowreport") || sm.ReadInt("allowreport", val), catchall, gnetError("Could not read 'allowreport' element"));
            m_Flags.m_AllowReport = val == 1;

            val = 0;
            rverify(!sm.HasMember("allowblock") || sm.ReadInt("allowblock", val), catchall, gnetError("Could not read 'allowblock' element"));
            m_Flags.m_AllowBlock = val == 1;

            val = 0;
            rverify(!sm.HasMember("multipleactors") || sm.ReadInt("multipleactors", val), catchall, gnetError("Could not read 'multipleactors' element"));
            m_Flags.m_MultipleActors = val == 1;
        }


        RsonReader regard;
        bool hasRegarding = rr.GetMember("regarding", &regard);
        hasRegarding = regard.GetFirstMember(&regard);
        while (hasRegarding)
        {
            ScFeedRegarding reg;
            ScFeedActor actor;

            if (reg.DeserialiseRegarding(regard, actor, *this))
            {
                if (reg.GetRegardingType() == ScFeedRegardingType::Parent || reg.GetRegardingType() == ScFeedRegardingType::UgcPublish)
                {
                    ScFeedPost groupFeed;
                    if (gnetVerify(groupFeed.DeserialiseFeed(regard, ScFeedType::Invalid)))
                    {
                        m_RegardingPosts.PushAndGrow(groupFeed);
                    }
                }
                else
                {
                    m_Regarding.PushAndGrow(reg);

                    if (actor.IsValid())
                    {
                        AddActor(actor);
                    }
                }
            }

            hasRegarding = regard.GetNextSibling(&regard);
        }

        unsigned groupCount = 0;
        RsonReader group;
        bool hasGroup = rr.GetMember("group", &group);
        hasGroup = group.GetFirstMember(&group);
        while (hasGroup)
        {
            ScFeedPost groupFeed;

            const ScFeedType childType = GetAggregetedChildType(m_Type);

            // For now I don't fail on a single item here as this is still changing
            // This is super not optimal as this copies a lot of atVectors and atStrings around.
            if (groupCount < ScFeedPost::MAX_GROUP_ENTRIES && gnetVerify(groupFeed.DeserialiseFeed(group, childType)))
            {
                m_Group.PushAndGrow(groupFeed);
            }
            else if (m_AdditionalGroupGuids.size() < ScFeedPost::MAX_ADDITIONAL_GROUP_GUIDS)
            {
                RosFeedGuid aggItemId;
                if (group.HasMember("id") && group.ReadString("id", stringBuffer) && RosFeedGuid::FromString(stringBuffer, aggItemId))
                {
                    m_AdditionalGroupGuids.PushAndGrow(aggItemId);
                }
            }

            hasGroup = group.GetNextSibling(&group);

            ++groupCount;
        }

        RsonReader locsList;
        RsonReader locs;
        bool hasLoc = rr.GetMember("loc", &locsList) && locsList.GetFirstMember(&locs);
        while (hasLoc)
        {
            ScFeedLoc loc;

            char nameId[64];
            locs.GetName(nameId);
            loc.m_Id = atHashString(nameId);

            if (loc.m_Id.IsNotNull() && locs.AsAtString(loc.m_Text))
            {
                m_Locs.PushAndGrow(loc);
            }

            hasLoc = locs.GetNextSibling(&locs);
        }

        RsonReader paramsList;
        RsonReader params;
        bool hasParam = rr.GetMember("cdata", &paramsList) && paramsList.GetFirstMember(&params);
        while (hasParam)
        {
            ScFeedParam param;

            char nameId[64];

            params.GetName(nameId);
            param.m_Id = atHashString(nameId);

            if (param.m_Id.IsNotNull() && params.AsAtString(param.m_Value))
            {
                m_Params.PushAndGrow(param);
            }

            hasParam = params.GetNextSibling(&params);
        }

        // m_NumGroups doesn't seem to be returned right now so this will do the trick to get the number of entries even if deserializing them fails.
        m_NumGroups = rage::Max(m_NumGroups, groupCount);

        result = true;
    }
    rcatchall
    {
    }

    return result;
}

ScFeedPost* ScFeedPost::Clone() const
{
    ScFeedPost* theOther = rage_new ScFeedPost();
    *theOther = *this;
    return theOther;
}

void ScFeedPost::FillTextureUrls(atArray<atString>& urls) const
{
    for (int i = 0; i < m_Group.size(); ++i)
    {
        m_Group[i].FillTextureUrls(urls);
    }

    for (int i = 0; i < m_Regarding.size(); ++i)
    {
        const atString& img = m_Regarding[i].GetSubject().m_Image;
        if (img.length() > 0)
        {
            urls.PushAndGrow(img);
        }
    }
}

unsigned ScFeedPost::GetHeightEstimate() const
{
    if (m_HeightEstimate)
    {
        return m_HeightEstimate;
    }

    return GetRawHeightEstimate();
}

unsigned ScFeedPost::GetRawHeightEstimate() const
{
	unsigned estimate = ScFeedPost::HeightEstimateFromType(m_Type);

	//if (m_Type == ScFeedType::UGCPhotoPublishedAgg && m_Group.size() == 2)
	//{
	//	return 300; // the agg with 2 is smaller
	//}

	if (IsUgcFeedType(m_Type))
	{
		// Super approximative since it doesn't consider char lengths and utf8 multibyte chars.
		const unsigned rows = (m_Content.length() / 50);
		estimate += rows * 20;
	}

	return estimate;
}

void ScFeedPost::SetHeightEstimate(const u16 height)
{
#if __BANK
    if (m_HeightEstimate != height)
    {
        gnetDebug3("SetHeightEstimate new[%u] old[%u] estimate[%u] type[%s] [%s]", 
            height, m_HeightEstimate, GetRawHeightEstimate(), ScFeedPost::TypeToString(m_Type), m_Id.ToFixedString().c_str());
    }
#endif

    m_HeightEstimate = height;
}

const char* ScFeedPost::GetRegardingParamFromPath(const char* path, ScFeedRegardingType& regardingType)
{
    rtry
    {
        regardingType = ScFeedRegardingType::Invalid;

        rverify(strncmp(path, "regarding.", strlen("regarding.")) == 0, catchall, gnetError("this is regarding path [%s]", path));

        const char* next = path + strlen("regarding.");
        static const size_t MAX_REG_BUF = 64;
        char subst[MAX_REG_BUF] = { 0 };
        safecpy(subst, next);

        size_t idx = 0;
        while (subst[idx] != 0 && idx < MAX_REG_BUF && subst[idx] != '.')
        {
            ++idx;
        }

        rverify(idx < MAX_REG_BUF && subst[idx] == '.', catchall, gnetError("Invalid path [%s]", path));
        subst[idx] = 0;

        rverify(ScFeedRegarding::TypeFromString(subst, regardingType), catchall, gnetError("Invalid regarding type. path[%s]", path));

        const char* regfield = next + strlen(subst) + 1;
        return regfield;

    }
    rcatchall
    {

    }

    return nullptr;
}

const ScFeedParam* ScFeedPost::GetGameActionDlink(const ScFeedPost& post)
{
    rtry
    {
        const ScFeedParam* dlink = post.GetParam(ScFeedParam::DEEP_LINK_ID);
        rcheck(dlink, catchall, );

        rcheck(sysServiceArgs::HasParam(dlink->m_Value.c_str(), sysServiceArgs::MODE_PARAM_NAME), catchall, );

        return dlink;
    }
    rcatchall
    {

    }

    return nullptr;
}

atHashString ScFeedPost::GetDlinkModeHash(const ScFeedPost& post)
{
    return GetDlinkParamHash(post, sysServiceArgs::MODE_PARAM_NAME);
}

atHashString ScFeedPost::GetDlinkClassHash(const ScFeedPost& post)
{
    return GetDlinkParamHash(post, sysServiceArgs::CLASS_PARAM_NAME);
}

atHashString ScFeedPost::GetDlinkParamHash(const ScFeedPost& post, const atHashString paramName)
{
    rtry
    {
        rverify(paramName, catchall, );

        const ScFeedParam* dlink = post.GetParam(ScFeedParam::DEEP_LINK_ID);
        rcheck(dlink, catchall, );

        char uribuffer[sysServiceArgs::MAX_ARG_LENGTH];
        bool hasArg = sysServiceArgs::GetParamValue(dlink->m_Value.c_str(), dlink->m_Value.length(), paramName, uribuffer);

        rcheck(hasArg, catchall, );

        return atHashString(uribuffer);
    }
    rcatchall
    {

    }

    return atHashString();
}

u32 ScFeedPost::GetDlinkTrackingId(const ScFeedPost& post)
{
    u32 val = GetDlinkParamHash(post, ScFeedParam::DLINK_TRACKING_ID).GetHash();
    return val != 0 ? val : ATSTRINGHASH("idmissing", 0x30E36989);
}

bool ScFeedPost::HasDlinkParam(const ScFeedPost& post, const atHashString paramName)
{
    rtry
    {
        rverify(paramName, catchall,);

        const ScFeedParam* dlink = post.GetParam(ScFeedParam::DEEP_LINK_ID);
        rcheck(dlink, catchall,);

        return sysServiceArgs::HasParam(dlink->m_Value.c_str(), paramName);
    }
    rcatchall
    {

    }

    return false;
}

bool ScFeedPost::IsAnyGoldStoreDlink(const ScFeedPost& post)
{
    const atHashString modeHash = GetDlinkModeHash(post);

    return modeHash == ScFeedParam::DLINK_MODE_GOLDSTORE_METRO || modeHash == ScFeedParam::DLINK_MODE_GOLDSTORE_REAL
        || modeHash == ScFeedParam::DLINK_MODE_GOLDSTORE_SPUPGRADE || ScFeedPost::HasDlinkParam(post, ScFeedParam::DLINK_FLAG_IS_STORE);
}

bool ScFeedPost::IsAnyUiDlink(const ScFeedPost& post)
{
    const atHashString classHash = GetDlinkClassHash(post);
    return classHash == ScFeedParam::DLINK_CLASS_UI;
}

bool ScFeedPost::IsAboutGambling(const ScFeedPost& post)
{
    rtry
    {
        const atHashString classHash = GetDlinkClassHash(post);
        rcheckall(classHash == ScFeedParam::DLINK_CLASS_MP);

        sysServiceArgs arg;
        char buffer[128] = { 0 };

        const ScFeedParam* dlink = post.GetParam(ScFeedParam::DEEP_LINK_ID);
        rcheck(dlink, catchall, );
        arg.Set(dlink->m_Value.c_str());

        rcheck(arg.GetParamValue(ScriptRouterLink::SRL_ARGTYPE, buffer), catchall, gnetDebug1("IsAboutGambling 'argtype' not found"));
        eScriptRouterContextArgument argtype = ScriptRouterContext::GetSRCArgFromString(atString(buffer));
        return argtype == eScriptRouterContextArgument::SRCA_GAMBLING;
    }
    rcatchall
    {
    }

    return false;
}

bool ScFeedPost::IsAboutNews(const ScFeedPost& post)
{
    return IsAnyUiDlink(post) && (GetDlinkParamHash(post, ScFeedParam::DLINK_UI_TARGET_ID) == ScFeedParam::DLINK_UI_TARGET_NEWS);
}

bool ScFeedPost::IsStandardFreeroam(const ScFeedPost& post)
{
    rtry
    {
        const atHashString classHash = GetDlinkClassHash(post);
        rcheckall(classHash == ScFeedParam::DLINK_CLASS_MP);

        sysServiceArgs arg;
        char buffer[128] = { 0 };

        const ScFeedParam* dlink = post.GetParam(ScFeedParam::DEEP_LINK_ID);
        rcheck(dlink, catchall, );
        arg.Set(dlink->m_Value.c_str());

        rcheck(arg.GetParamValue(ScriptRouterLink::SRL_MODE, buffer), catchall, gnetDebug1("IsStandardFreeroam 'mode' not found"));
        eScriptRouterContextMode mode = ScriptRouterContext::GetSRCModeFromString(atString(buffer));
        rcheck(mode == eScriptRouterContextMode::SRCM_FREE, catchall, );

        rcheck(arg.GetParamValue(ScriptRouterLink::SRL_ARGTYPE, buffer), catchall, gnetDebug1("IsAboutGambling 'argtype' not found"));
        eScriptRouterContextArgument argtype = ScriptRouterContext::GetSRCArgFromString(atString(buffer));
        return argtype == eScriptRouterContextArgument::SRCA_NONE || argtype == eScriptRouterContextArgument::SRCA_LOCATION;
    }
    rcatchall
    {
    }

    return false;
}

bool ScFeedPost::HasWebUrl(const ScFeedPost& post)
{
    //TODO_ANDI: Keeping the CanShowBrowser() checks around as reminder.
    // Remove it someday post launch if this is all fine.
    /*if (!g_SystemUi.CanShowBrowser())
    {
        // In theory we might need the URL even if the browser isn't supported but
        // right now that's never the case so we can keep this centralized here.
        return false;
    }*/

    const atString* url = post.GetLoc(ScFeedLoc::URL_ID);

    if (url != nullptr && url->length() > 0)
    {
        return true;
    }

    const ScFeedParam* dlink = post.GetParam(ScFeedParam::DEEP_LINK_ID);
    if (dlink && sysServiceArgs::HasParam(dlink->m_Value.c_str(), ScFeedParam::DLINK_SCAUTHLINK))
    {
        return true;
    }

    return false;
}

bool ScFeedPost::HasWebViewContent(const ScFeedPost& post)
{
    /*if (!g_SystemUi.CanShowBrowser())
    {
        // In theory we might need the content even if the browser isn't supported but
        // right now that's never the case so we can keep this centralized here.
        return false;
    }*/

    const bool hasWebView = ScFeedPost::HasFlags(post.GetFeedType(), SCF_WEBVIEW);
    const bool hasUGC = ScFeedPost::HasFlags(post.GetFeedType(), SCF_REAL_UGC);

    return hasWebView && (hasUGC || ScFeedPost::HasWebUrl(post));
}

bool ScFeedPost::IsHiddenByDlinkFlag(const ScFeedPost& post)
{
    #define MISSION_FLAG_PREFIX "mflag_"
    #define NOT_MISSION_FLAG_PREFIX "notmflag_"

    bool canShow = true;

    const ScFeedParam* param = post.GetParam(ScFeedParam::VISIBILITY_TAG_ID);

    if (param == nullptr)
    {
        return false;
    }

    const atHashString value = param->ValueAsHash();
    s64 bitIndex = 0;

    // If the user managed to download a feed post he's connected so we probably know 
    // if he has PLUS or not although it's not guaranteed.

    if (value == ScFeedParam::VIS_TAG_MP_SUBSCRIPTION_ONLY)
    {
        canShow = !CLiveManager::IsPlatformSubscriptionCheckPending() && CLiveManager::HasPlatformSubscription();
    }
    else if (value == ScFeedParam::VIS_TAG_NO_MP_SUBSCRIPTION_ONLY)
    {
        canShow = !CLiveManager::IsPlatformSubscriptionCheckPending() && !CLiveManager::HasPlatformSubscription();
    }
    else if (value == ScFeedParam::STANDALONE_ONLY)
    {
        //Note: Checking for eGameVersion::GV_STANDALONE woudl be better but there are cases on Xbox where that fails
        canShow = true;//CGameSessionStateMachine::IsAllowedInSP() == false; //TODO_ANDI
    }
    else if (value == ScFeedParam::NOT_STANDALONE_ONLY)
    {
        canShow = false;//CGameSessionStateMachine::IsAllowedInSP();
    }
#if RSG_ORBIS
    // We may not know this information yet but at worst we don't show a post.
    else if (value == ScFeedParam::VIS_TAG_CUSTOM_UPSELL_ONLY)
    {
        canShow = !CLiveManager::IsPlatformSubscriptionCheckPending() &&
            CLiveManager::IsPlayStationPlusCustomUpsellEnabled() &&
            !CLiveManager::HasPlatformSubscription();
    }
    else if (value == ScFeedParam::VIS_TAG_MP_PROMOTION_ONLY)
    {
        canShow = !CLiveManager::IsPlatformSubscriptionCheckPending() &&
            CLiveManager::IsPlayStationPlusPromotionEnabled() &&
            !CLiveManager::HasPlatformSubscription();
    }
#endif //!RSG_ORBIS
    else if (param->ValueAsInt64(MISSION_FLAG_PREFIX, bitIndex))
    {
        canShow = false;//(NetworkInterface::HasMpMissionFlagOnCurrentSlot(static_cast<int>(bitIndex)) == IntroSettingResult::Result_Complete); //TODO_ANDI
    }
    // This one shouldn't be needed but I add it anyway for safety
    else if (param->ValueAsInt64(NOT_MISSION_FLAG_PREFIX, bitIndex))
    {
        canShow = true;//(NetworkInterface::HasMpMissionFlagOnCurrentSlot(static_cast<int>(bitIndex)) != IntroSettingResult::Result_Complete);
    }

    return !canShow;
}

bool ScFeedPost::IsDLinkDisabledByGame(const ScFeedPost& UNUSED_PARAM(post))
{
    return false;
}

void ScFeedPost::GetDlinkMetricData(const ScFeedPost& post, u32& type, u32& tile, u32& area)
{
    // Not used for anything other than the metric so keeping this here should be fine
    //static const unsigned TILE_AREA_LANDING_BOOT = 0;
    static const unsigned TILE_AREA_SP = 1;
    static const unsigned TILE_AREA_MP = 2;

    type = 0;
    tile = 0;
    area = 0;

    const ScFeedParam* dlink = GetGameActionDlink(post);
    const bool hasWebUrl = HasWebUrl(post);

    // Retrieve the type value. If it's a game dlink we use mode=<abc> as type.
    if (dlink)
    {
        type = GetDlinkModeHash(post).GetHash();
    }
    else if (hasWebUrl) // If it's a url we simply set a generic weburl as type.
    {
        type = ATSTRINGHASH("weburl", 0x66D27240);
    }

    // Retrieve the tile value
    {
        // If there's a manually set param use it
        if (dlink) // If it's a game link get the second parameter
        {
            //TODO_ANDI: needs an update for GTA. Not used right now
            if (tile == 0)
            {
                tile = GetDlinkParamHash(post, ATSTRINGHASH("state_id", 0xD277F4C3));
            }

            if (tile == 0)
            {
                tile = GetDlinkParamHash(post, ATSTRINGHASH("series_id", 0x3DC9BB6));
            }

            if (tile == 0)
            {
                tile = GetDlinkParamHash(post, ATSTRINGHASH("mission_id", 0x2BDCAD24));
            }

            if (tile == 0)
            {
                tile = GetDlinkParamHash(post, ATSTRINGHASH("minigame_id", 0xCBCEBC94));
            }

            if (tile == 0)
            {
                tile = GetDlinkParamHash(post, ATSTRINGHASH("legendary", 0xEC6F9A7F));
            }

            // Request from QA. If tile is 0 set it to the type value.
            // For now only done on UI so we don't miss new types.
            if (tile == 0 && IsAnyUiDlink(post))
            {
                tile = type;
            }

            if (tile == 0)
            {
                gnetDebug1("No tile type found for dlink [%s]", dlink->m_Value.c_str());
            }

            // We don't have a getparam2 or something like that because the order doesn't matter
            // so for now this hard-coded list has to do. If new ones are added we have to remember
            // to add them here. Also not every dlink has a param os 0 is a valid value.
        }
        else if (hasWebUrl) // It's a link to either a webpage or a social club page
        {
            // Nothing to set here yet. This isn't a particularly common case anyway.
        }
    }

    if (NetworkInterface::IsNetworkOpen())
    {
        area = TILE_AREA_MP;
    }
    else //if (uiThreadHelper::IsGameUpdateRunning()) // Ugly but this is the safest check
    {
        area = TILE_AREA_SP;
    }
    /*else
    {
        area = TILE_AREA_LANDING_BOOT; //TODO_ANDI: fix if needed
    }*/
}

void ScFeedPost::AddActor(const ScFeedActor& actor)
{
    if (actor == m_Actor)
    {
        // If it's the main actor we overwrite the image with the new one.
        if (!actor.m_Image.empty())
        {
            m_Actor.m_Image = actor.m_Image;
        }

        return;
    }

    for (int i=0; i < m_Actors.size(); ++i)
    {
        if (m_Actors[i] == actor)
        {
            return;
        }
    }

    m_Actors.PushAndGrow(actor);
}

void ScFeedPost::AddImage(const atString& img)
{
    for (const atString& m : m_Images)
    {
        if (m == img)
        {
            return;
        }
    }

    m_Images.PushAndGrow(img);
}

void ScFeedPost::SetupLocalFeed(const ScFeedType type, RosFeedGuid id)
{
    m_Type = type;
    m_Id = id;
}

void ScFeedPost::SetMyReaction(const ScFeedReaction reaction)
{
    if (reaction != m_MyReaction)
    {
        if (reaction == ScFeedReaction::Like)
        {
            ++m_NumLikes;
        }
        else if (reaction == ScFeedReaction::Unknown && m_MyReaction == ScFeedReaction::Like && m_NumLikes > 0)
        {
            --m_NumLikes;
        }
    }

    m_MyReaction = reaction;
}

void ScFeedPost::SetMyUgcReaction(const ScFeedReaction reaction)
{
    if (!HasFlags(m_Type, SCF_REAL_UGC))
    {
        gnetWarning("Trying to set a UGC reaction on non-ugc type. type[%s] [%s]", ScFeedPost::TypeToString(m_Type), m_Id.ToFixedString().c_str());
        return;
    }

    if (HasFlags(m_Type, SCF_EMBEDDED_UGC))
    {
        if (m_RegardingPosts.size() > 0)
        {
            m_RegardingPosts[0].SetMyUgcReaction(reaction);
        }

        return;
    }

    gnetAssertf(reaction != ScFeedReaction::Dislike, "This isn't fully correct when Disliking but we don't allow that form the feed. We only allow like and clear.");
    SetMyReaction(reaction);
}

void AddIfNew(ScFeedWallFilters& filters, const ScFeedFilterAndActor& val)
{
    if (filters.GetCount() >= filters.GetMaxCount())
    {
        return;
    }

    for (int i = 0; i < filters.GetCount(); ++i)
    {
        if (filters[i].m_Filter == val.m_Filter)
        {
            return;
        }
    }

    filters.Append() = val;
}

const ScFeedPost* ScFeedPost::GetChildPost(const RosFeedGuid feedId) const
{
    ScFeedPost* feed = (const_cast<ScFeedPost*>(this))->GetChildPost(feedId);
    return feed;
}

ScFeedPost* ScFeedPost::GetChildPost(const RosFeedGuid feedId)
{
    for (ScFeedPost& post : m_RegardingPosts)
    {
        if (post.GetId() == feedId)
        {
            return &post;
        }
    }

    for (ScFeedPost& post : m_Group)
    {
        if (post.GetId() == feedId)
        {
            return &post;
        }
    }

    return nullptr;
}

bool ScFeedPost::HasChildPost( const RosFeedGuid feedId ) const
{
    for ( const ScFeedPost& post : m_RegardingPosts )
    {
        if ( post.GetId() == feedId )
        {
            return true;
        }
    }

    for ( const ScFeedPost& post : m_Group )
    {
        if ( post.GetId() == feedId )
        {
            return true;
        }
    }

    for ( const RosFeedGuid& guid : m_AdditionalGroupGuids )
    {
        if ( guid == feedId )
        {
            return true;
        }
    }

    return false;
}

int ScFeedPost::GetChildPostCount() const
{
    int const c_regardingPostsCount = m_RegardingPosts.GetCount();
    int const c_groupCount = m_Group.GetCount();
    int const c_additionalGroupGuidsCount = m_AdditionalGroupGuids.GetCount();
    return c_regardingPostsCount + c_groupCount + c_additionalGroupGuidsCount;
}

bool ScFeedPost::DeleteChildPostById( const RosFeedGuid feedId )
{
    // shouldn't delete any child posts from m_RegardingPosts

    int const c_groupSize = m_Group.GetCount();
    for ( int index = 0; index < c_groupSize; ++index )
    {
        if ( m_Group[index].GetId() == feedId )
        {
            m_Group.Delete( index );
            return true;
        }
    }

    int const c_additionalGroupGuidsCount = m_AdditionalGroupGuids.GetCount();
    for ( int index = 0; index < c_additionalGroupGuidsCount; ++index )
    {
        if ( m_AdditionalGroupGuids[index] == feedId )
        {
            m_AdditionalGroupGuids.Delete( index );
            return true;
        }
    }

    return false;
}

void ScFeedPost::GetWallFilterFromFeed(ScFeedWallFilters& filters) const
{
    filters.clear();

    const ScFeedActor& actor = GetActor();

    const ScFeedType type = GetFeedType();

    const ScFeedRegarding* regarding = nullptr;
    const ScFeedActor* otherActor = nullptr;

    const bool isCrewFeed = ScFeedPost::IsCrewFeedType(type);
    // If it's a crew feed right now it wins over the actor
    if (isCrewFeed)
    {
        regarding = GetRegarding(ScFeedRegardingType::Crew);
        otherActor = regarding ? GetActor(regarding->GetSubject().m_Type.c_str(), regarding->GetSubject().m_Id.c_str()) : nullptr;

        if (regarding && otherActor)
        {
            ScFeedFilterAndActor val;
            val.m_Filter = ScFeedFilterId(ScFeedRequestType::Wall, ScFeedWallType::Crew, otherActor->GetId(), ScFeedActorTypeFilter::All, ScFeedPostTypeFilter::All);
            val.m_Actor = *otherActor;
            safecpy(val.m_CrewMotto, regarding->GetSubject().m_Data.c_str());

            AddIfNew(filters, val);
        }
    }

    if (actor.GetActorType() == ScFeedActorType::Crew || actor.IsUserActor())
    {
        ScFeedFilterAndActor val;
        val.m_Filter = ScFeedFilterId(ScFeedRequestType::Wall, actor.GetActorType() == ScFeedActorType::Crew ? ScFeedWallType::Crew : ScFeedWallType::RockstarId,
            actor.GetId(), ScFeedActorTypeFilter::All, ScFeedPostTypeFilter::All);
        val.m_Actor = actor;
        //The crew motto would be needed here but currently we haven't any case where the actor is a crew and not the same as regarding.crew

        AddIfNew(filters, val);
    }

    regarding = GetRegarding(ScFeedRegardingType::Subject);
    otherActor = regarding ? GetActor(regarding->GetSubject().m_Type.c_str(), regarding->GetSubject().m_Id.c_str()) : nullptr;

    if (regarding && otherActor)
    {
        ScFeedFilterAndActor val;
        val.m_Filter = ScFeedFilterId(ScFeedRequestType::Wall, ScFeedWallType::RockstarId, otherActor->GetId(), ScFeedActorTypeFilter::All, ScFeedPostTypeFilter::All);
        val.m_Actor = *otherActor;

        AddIfNew(filters, val);
    }

    // Some rockstar posts reference UGC, so there we want to get the Author of the UGC.
    if (filters.IsEmpty() && ScFeedPost::IsRockstarFeedType(m_Type) && ScFeedPost::IsUgcFeedType(m_Type) && m_RegardingPosts.size() > 0)
    {
        m_RegardingPosts[0].GetWallFilterFromFeed(filters);
    }
}

//#####################################################################################

ScFeedRequest::ScFeedRequest()
    : m_Start(0)
    , m_Amount(0)
    , m_RequestId(ScFeedRequest::INVALID_REQUEST_ID)
{

}

ScFeedRequest::ScFeedRequest(const RosFeedGuid* feedIds, const unsigned numIds, const ScFeedRequestCallback& requestCb, const ScFeedPostCallback& postCb)
    : m_Amount(0)
    , m_RequestCallback(requestCb)
    , m_PostCallback(postCb)
{
    if (gnetVerify(feedIds != nullptr && numIds > 0))
    {
        gnetAssertf(numIds < MAX_INDIVIDUAL_IDS, "Too many ids requested [%u]. The request will be cut off", numIds);

        m_Amount = rage::Min(numIds, MAX_INDIVIDUAL_IDS);
        m_RequestId = s_NextRequestId++;
        m_RequestedPosts.CopyFrom(feedIds, static_cast<u16>(m_Amount));
    }
}

ScFeedRequest::ScFeedRequest(const ScFeedFilterId& filter, unsigned start, unsigned amount)
    : m_Filter(filter)
    , m_Start(start)
    , m_Amount(amount)
{
    m_RequestId = s_NextRequestId++;
}

ScFeedRequest::ScFeedRequest(const ScFeedFilterId& filter, unsigned start, unsigned amount, const ScFeedRequestCallback& requestCb, const ScFeedPostCallback& postCb)
    : m_Filter(filter)
    , m_Start(start)
    , m_Amount(amount)
    , m_RequestCallback(requestCb)
    , m_PostCallback(postCb)
{
    m_RequestId = s_NextRequestId++;
}

bool ScFeedRequest::ContainsRange(const unsigned start, const unsigned amount) const
{
    return start >= m_Start && (m_Start + m_Amount >= start + amount);
}

bool ScFeedRequest::Contains(const ScFeedRequest& request) const
{
    // If individual posts have been requested we treat them as distinct requests
    if (m_RequestedPosts.size() > 0 || request.m_RequestedPosts.size() > 0)
    {
        return false;
    }

    return m_Filter == request.m_Filter && ContainsRange(request.m_Start, request.m_Amount)
        && m_RequestCallback == m_RequestCallback && m_PostCallback == m_PostCallback;
}

const char* ScFeedRequest::GetErrorString(const int feedDownloadError)
{
    if (feedDownloadError == static_cast<int>(rlFeedError::NOT_ALLOWED_PRIVILEGE.GetHash()))
    {
        return "SC_FEED_ERROR_NOT_ALLOWED_PRIVILEGE";
    }

    // For now we use a generic error
    return "FEED_ERROR_UNAVAILABLE";
}

//#####################################################################################

ScFeedActiveRequest::ScFeedActiveRequest()
    : m_CloudRequestId(INVALID_CLOUD_REQUEST_ID)
{

}

ScFeedActiveRequest::~ScFeedActiveRequest()
{
    Clear();
}

void ScFeedActiveRequest::Clear()
{
    m_Request = ScFeedRequest();

    if (m_RefreshFeedsStatus.Pending())
    {
        if (m_CloudRequestId != INVALID_CLOUD_REQUEST_ID)
        {
            m_RefreshFeedsStatus.ForceFailed();
            m_RefreshFeedsStatus.Reset();
        }
        else
        {
        rlFeed::Cancel(&m_RefreshFeedsStatus);
    }
    }

    m_CloudRequestId = INVALID_CLOUD_REQUEST_ID;
    m_RefreshFeedsStatus.Reset();
    m_FeedIter.ReleaseResources();
}

//#####################################################################################

RosFeedGuid scrScFeedGuid::FeedGuidFromScript(const scrScFeedGuid& scrFeedId)
{
    RosFeedGuid guid;

    guid.m_data32[0] = static_cast<unsigned>(scrFeedId.m_Data0.Int);
    guid.m_data32[1] = static_cast<unsigned>(scrFeedId.m_Data1.Int);
    guid.m_data32[2] = static_cast<unsigned>(scrFeedId.m_Data2.Int);
    guid.m_data32[3] = static_cast<unsigned>(scrFeedId.m_Data3.Int);

    return guid;
}

scrScFeedGuid scrScFeedGuid::FeedGuidToScript(const RosFeedGuid& feedId)
{
    scrScFeedGuid guid;

    guid.m_Data0.Int = static_cast<int>(feedId.m_data32[0]);
    guid.m_Data1.Int = static_cast<int>(feedId.m_data32[1]);
    guid.m_Data2.Int = static_cast<int>(feedId.m_data32[2]);
    guid.m_Data3.Int = static_cast<int>(feedId.m_data32[3]);

    return guid;
}

#endif //GEN9_STANDALONE_ENABLED
