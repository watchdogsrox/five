#ifndef INC_SOCIALCLUBFEED_H_
#define INC_SOCIALCLUBFEED_H_

#include "fwutil/Gen9Settings.h"
#if GEN9_STANDALONE_ENABLED

// Rage
#include "atl/array.h"
#include "atl/atfixedstring.h"
#include "atl/inlist.h"
#include "atl/map.h"
#include "atl/singleton.h"
#include "atl/string.h"
#include "data/rson.h"
#include "rline/clan/rlclancommon.h"
#include "rline/feed/rlfeed.h"
#include "rline/rlgamerhandle.h"
#include "system/guid.h"
#include "system/lambda.h"

// Net
#include "net/status.h"
#include "net/pool.h"

// Game
#include "script/value.h"

// Enabled in bank as for debugging we use some not fully compliant feeds
#define ALLOW_FEED_USERS_FROM_OTHER_PLATFORMS __BANK
#define ALLOW_FEED_POSTS_FROM_OTHER_PLATFORMS __BANK

// The number of extracted wall filters from a feed
#define MAX_WALLS_FILTERS_FROM_FEED 4

class ScFeedRequest;
class ScFeedPost;
typedef int CloudRequestID;

//Please be careful with this enum, there's an array with the corresponding strings in the cpp
enum class ScFeedActorType : unsigned char
{
    RockstarId,
    UserId,
    Crew,
    Application,

    Count,
    Invalid = Count
};

//Please be careful with this enum, there's an array with the corresponding strings in the cpp
enum class ScFeedRegardingType : unsigned char
{
    Crew,
    Ugc,
    Emblem,
    Dlc,
    News,
    Item,

    Contest,
    EventObject,
    Accomplishment,
    PresetMessage,
    Milestone,
    Challenge,
    Subject,
    Game,
    Platform,
    Title,
    OldValue,
    NewValue,
    Missiontype,
    Parent,
    UgcPublish,
    UgcCategory,
    Marketing,
    Posse,
    Character,

    Count,
    Invalid = Count
};

//Please be careful with this enum, there's an array with the corresponding strings in the cpp
enum class ScFeedType
{
    AccountRegistered,

    RockstarFeedFirst,
    RockstarGta5Landing = RockstarFeedFirst,
    RockstarGta5Priority,
    RockstarGta5Heist,
    RockstarGta5Series,
    RockstarFeedLast = RockstarGta5Series,

#if TEST_FAKE_FEED
    UndefinedPost, //This is a fake feed I use to display feeds whose type I don't know. Makes it easier to bug those.
#endif

    Count,
    Invalid = Count
};

enum class ScFeedReaction : unsigned char
{
    Unknown = 0,
    Like = 1,
    Dislike = 2,
    Count,
    Invalid = Count
};


//PURPOSE
// Enum when requesting different feed 'types' like my feeds or a players wall
enum class ScFeedRequestType : uint8_t
{
    Feeds,
    Wall,
    FeedsList
};

// See rage\ROS\rage\src\InboxInterface\InboxKeyType.cs (InboxRecipientKeyType)
enum class ScFeedWallType : uint8_t
{
    None = 0,
    UserId = 5, // Platform specific user identifiers
    RockstarId = 7,
    Crew = 8
};

// This one is used to filter by the actor of the feed
enum class ScFeedActorTypeFilter : uint8_t
{
    All = 0,
    Friends,
    Crew,
    Me,
    Count,
    Invalid = Count
};

// This filters by specific post types
enum class ScFeedPostTypeFilter : uint8_t
{
    All = 0,
    SafeFilterFirst, // Filters between SafeFilterFirst and ..Last can always be used safely regardless of UGC restrictions & co.
    Marketing = SafeFilterFirst,
    Landing,
    Priority,
    Heist,
    Series,
    SafeFilterLast = Series,
    Count,
    Invalid = Count
};

// An enum of the legal pages we can display
enum class ScFeedLegals
{
    Eula,
    TermsAndConditions,
    Privacy
};

//PURPOSE
//  From where to read the image data
enum ScFeedImageFlags
{
    SC_F_I_NONE,
    SC_F_I_AVATAR           = 1 << 0,
    SC_F_I_CURRENT_CREW     = 1 << 1,
    SC_F_I_URL_CREW         = 1 << 2,
    SC_F_I_FROM_REGARDING   = 1 << 3,
    SC_F_I_FROM_GROUP       = 1 << 4,
    SC_F_I_FROM_ACTOR       = 1 << 5,
    SC_F_I_FROM_IMAGES      = 1 << 6
};

//PURPOSE
//  Various feed type specific flags
enum ScFeedTypeFlags
{
    SCF_NONE           = 0,
    SCF_CREW           = 1 << 1,
    SCF_UGC            = 1 << 2,
    SCF_REAL_UGC       = (1 << 3) | SCF_UGC, // Real UGC is UGC such as photos, missions, videos. Common UGC also contains simple text such as wrote_wall_message
    SCF_UGC_PHOTO      = (1 << 4) | SCF_REAL_UGC,
    SCF_UGC_PLAYLIST   = (1 << 5) | SCF_REAL_UGC,
    SCF_UGC_MISSION    = (1 << 6) | SCF_REAL_UGC,
    SCF_UGC_VIDEO      = (1 << 7) | SCF_REAL_UGC,
    SCF_EMBEDDED_UGC   = (1 << 8) | SCF_REAL_UGC,
    SCF_AGG            = 1 << 9,
    SCF_BIGFEED        = 1 << 10, // bigfeed and webview exclude each other. This is about which "detail view" to show
    SCF_WEBVIEW        = 1 << 11,
    SCF_PLAINTEXT_BODY = 1 << 12 // don't apply any formatting over the text
};

//PURPOSE
//  Mainly UI related but I'm putting it here so it's easier to track what is using what
//  This will map onto one of the ScFeedBindingEntry classes
enum class ScFeedUI
{
    Default,
    CrewSubject,
    CrewMotto,
    WallMessage,
    PhotoItem,
    UGCItem,
    MultiItem,
    Marketing,
    LocalPhoto,
    Posse,
	LocalMission
};

// The place where the priority feed is shown
enum class PriorityFeedArea
{
	Sp = 0,
	Mp,
	Any,
	Count
};

typedef atFixedString<26> RosFeedGuidFixedString;

//PURPOSE
// Wrapper for sysGuid used for the id of a feed.
// Unfortunately feeds are made of 12 bytes worth of characters, not 16.
class RosFeedGuid : public sysGuid
{
public:
	RosFeedGuid();
	RosFeedGuid(const sysGuid& other);
	RosFeedGuid& operator=(const sysGuid& other);

	bool operator==(const RosFeedGuid& other) const;

	//PURPOSE
	// To quickly convert to a string
	RosFeedGuidFixedString ToFixedString() const;

	//PURPOSE
	// Generates a smaller hash out of the guid. Obviously lossy so there can be clashes.
	unsigned ToHash() const;

	//PURPOSE
	// Converts a string to a guid. The string must be in hex and 24 chars long.
	static bool FromString(const char* string, RosFeedGuid& result);

	//PURPOSE
	// Convenience function to Convert a string to a guid. The string must be in hex and 24 chars long.
	// If the string is not valid a null-guid will be returned
	static RosFeedGuid FromString(const char* string);

	//PURPOSE
	// Generates a new unique id. Can be used for debug purpose and such things.
	static RosFeedGuid GenerateGuid();

public:
	//PURPOSE
	// The number of chars of a feed-guid (excluding the 0-terminator)
	static const unsigned NUM_CHARS_FOR_GUID_STRING = 24;
};

inline RosFeedGuid& RosFeedGuid::operator=(const sysGuid& other)
{
	m_data64[0] = other.m_data64[0];
	m_data64[1] = other.m_data64[1];
	return *this;
}

inline bool RosFeedGuid::operator==(const RosFeedGuid& other) const
{
	return *static_cast<const sysGuid*>(this) == static_cast<sysGuid>(other);
}

// PURPOSE: Specialized hash function functor for RosFeedGuid keys.
template <>
struct atMapHashFn<RosFeedGuid>
{
	u64 operator ()(RosFeedGuid key) const { return key.m_data64[0] ^ key.m_data64[1]; }
};

//PURPOSE
// Contains the name of the actor and it's type. Visually this is going to be like the title of the feed with texts like "Rockstar Games" or a gamertag.
class ScFeedActor
{
public:
    ScFeedActor();

    //PURPOSE
    // True if it's a valid actor
    bool IsValid() const { return m_Id != 0; }

    //PURPOSE
    // The RockstarId of the actor. When it's a crew this is the CrewId and when an application it's the application id
    RockstarId GetId() const { return m_Id; }

    //PURPOSE
    // The rockstar nickname of the user
    const char* GetRockstarName() const { return m_RockstarName; }

    //PURPOSE
    // In case it's a user this contains the XUID/PSN id
    const rlGamerHandle& GetGamerHandle() const { return m_GamerHandle; }

    //PURPOSE
    // The name of the actor. This will be the gamertag, psn name for the game or something generic like "Rockstar Games" if it's a global feed
    const char* GetName() const { return m_Name; }

    //PURPOSE
    // Returns the display name or the normal name depending on the actor and actor type
    const char* GetDisplayName() const;

    //PURPOSE
    //  The pedshot/mugshot image. Will be empty in most posts.
    const char* GetImage() const { return m_Image.c_str(); }

    //PURPOSE
    // Says if it's Rockstar or a user or a group
    ScFeedActorType GetActorType() const { return m_Type; }

    //PURPOSE
    // Returns true if the actor is a user/player.
    const bool IsUserActor() const { return m_Type == ScFeedActorType::RockstarId || m_Type == ScFeedActorType::UserId; }

    //PURPOSE
    // Builds the feed actor from a json text. Returns true if succeeded
    bool DeserialiseActor(const RsonReader& rr, const ScFeedType feedType);

    //PURPOSE
    // Sets this actor as Rockstar actor
    void SetAsRockstar();

    //PURPOSE
    // Converts a string to the ScFeedActorType. Returns true if succeeded
    static bool TypeFromString(const char* typeString, ScFeedActorType& resultType);

    bool Matches(const ScFeedActorType type, const char* id) const;

    //PURPOSE
    //  Equality check. If the ScFeedActorType and RockstarId match it's a match for us
    bool operator==(const ScFeedActor& other) const;

public:
    char            m_Name[RL_CLAN_NAME_MAX_CHARS]; // This is the platform specific name (for crew & others this is the one to use)
    rlGamerHandle   m_GamerHandle; // The platform specific id of the user

    char            m_RockstarName[RL_MAX_NAME_BUF_SIZE]; // This is the Rockstar account name

    atString        m_Image;

    RockstarId      m_Id;

    ScFeedActorType m_Type;
};

CompileTimeAssert(RL_CLAN_NAME_MAX_CHARS >= RL_MAX_NAME_BUF_SIZE); // If yaou hit this then ScFeedActor::m_Name needs changing

//PURPOSE
// Used for the regarding list and the group list containing info about parameters from a text.
class ScFeedSubject
{
public:
    ScFeedSubject();

    //PURPOSE
    // Builds the feed subject from a json text. Returns true if succeeded
    bool DeserialiseSubject(const RsonReader& rr);

    //PURPOSE
    //  Return the id as s64. The id should be a user or crew when used
    s64 GetIdInt64() const;

    //PURPOSE
    //  Read the id as s64. Returns true if converted successfully
    bool GetIdInt64(s64& value) const;

public:
    atString m_Id;

    atString m_Type;
    atString m_Name;

    atString m_Image;
    atString m_Data;
};

// ScFeedRegarding extra data for the crew
struct ScFeedCrewExtraData
{
    u32                 m_CrewMemberCount;
    u32                 m_CrewColor;
    char                m_CrewTag[RL_CLAN_TAG_MAX_CHARS];
    bool                m_CrewOpen;
};

// ScFeedRegarding extra data for UGC
struct ScUgcExtraData
{
    u32 m_NumDislikes;
    u32 m_NumPlayed;
};

union ScFeedRegardignExtraData
{
    ScFeedCrewExtraData m_Crew;
    ScUgcExtraData      m_Ugc;

    ScFeedRegardignExtraData();
};

//PURPOSE
// The "regarding" list contains the parameters. For example a text like ""Created ~crew~" would contain a regarding crew and then ~crew~ would be replaced with 'regarding.crew.name'
class ScFeedRegarding
{
public:
    ScFeedRegarding();

    bool IsValid() const { return m_RegardingType != ScFeedRegardingType::Invalid; }

    //PURPOSE
    // This is an id of the content type. It's also used as string replacement token for the feed text.
    ScFeedRegardingType GetRegardingType() const { return m_RegardingType; }

    //PURPOSE
    // The content of this regarding. Can cointain an image, some text ...
    const ScFeedSubject& GetSubject() const { return m_Subject; }

    //PURPOSE
    // Builds the feed regarding from a json text. Returns true if succeeded
    //PARAMS
    // rr    - The json reader with the data
    // actor - a regarding can contain an actor
    bool DeserialiseRegarding(const RsonReader& rr, ScFeedActor& actor, ScFeedPost& post);

    //PURPOSE
    // Converts a string to the ScFeedRegardingType. Returns true if succeeded
    static bool TypeFromString(const char* typeString, ScFeedRegardingType& resultType);

    //PURPOSE
    // Converts a ScFeedRegardingType to a string
    static const char* TypeToString(ScFeedRegardingType feedType);

public:
    ScFeedSubject               m_Subject;
    ScFeedRegardignExtraData    m_ExtraData;
    ScFeedRegardingType         m_RegardingType;
};

struct ScFeedFlags
{
    ScFeedFlags();

    bool m_AllowComment : 1;
    bool m_AllowReaction : 1;
    bool m_AllowShare : 1;
    bool m_AllowDelete : 1;
    bool m_AllowReport : 1;
    bool m_AllowBlock : 1;
    bool m_MultipleActors : 1;
    bool m_Unused : 1;
};

//PURPOSE
// Helper class to keep all info needed to do a feed http request
class ScFeedFilterId
{
public:
    ScFeedFilterId();

    //PURPOSE
    // Construct a custom request
    //PARAMS
    // requestType - the type
    // ownerId - this is the player from which to get the feed data. In case of the wall this is the wall owner
    ScFeedFilterId(const ScFeedRequestType requestType, const ScFeedWallType wallType, const RockstarId ownerId,
        const ScFeedActorTypeFilter actorFilter, const ScFeedPostTypeFilter typeFilter);

    //PURPOSE
    // The request type
    ScFeedRequestType GetType() const { return m_Type; }

    //PURPOSE
    // In case of a wall request this is the owner of the wall
    RockstarId GetOwnerId() const { return m_OwnerId; }

    //PURPOSE
    // The type of the wall. Currently fixed to 7 which is the RockstarId
    ScFeedWallType GetWallType() const { return m_WallType; }

    //PURPOSE
    // Filter's according to the actor of the feed.
    ScFeedActorTypeFilter GetActorTypeFilter() const { return m_ActorTypeFilter; }

    //PURPOSE
    // Filter about the type of feed, ugc, store ...
    ScFeedPostTypeFilter GetTypeFilter() const { return m_PostTypeFilter; }

    //PURPOSE
    // Change the actor filter
    void SetActorTypeFilter(const ScFeedActorTypeFilter filter) { m_ActorTypeFilter = filter; }

    //PURPOSE
    // Change the type filter
    void SetFeedTypeFiler(const ScFeedPostTypeFilter filter) { m_PostTypeFilter = filter; }

#if TEST_FAKE_FEED
    //PURPOSE
    // The path to use for a debug file on the cloud
    atFixedString<256> FormatCloudFilePath(const char* suffix = "") const;
#endif

    //PURPOSE
    // Converts a ScFeedActorTypeFilter to a string
    static const char* ActorTypeFilterToString(const ScFeedActorTypeFilter actorTypeFilter);

    //PURPOSE
    // Converts a ScFeedTypeFilter to a string
    static const char* PostTypeFilterToString(const ScFeedPostTypeFilter postTypeFilter);

    //PURPOSE
    // Returns true this filter can be used even with UGC restrictions & co.
    static bool IsSafeFilterForAlUsers(const ScFeedPostTypeFilter filter);

    bool operator==(const ScFeedFilterId& other) const;

protected:
    RockstarId m_OwnerId;
    ScFeedWallType m_WallType;
    ScFeedRequestType m_Type;
    ScFeedActorTypeFilter m_ActorTypeFilter;
    ScFeedPostTypeFilter m_PostTypeFilter;
};

//PURPOSE
// Helper class that contains the actor and filter (like a pair)
class ScFeedFilterAndActor
{
public:
    ScFeedFilterAndActor() { m_CrewMotto[0] = 0; m_CrewTag[0] = 0; }

    ScFeedFilterId m_Filter;
    ScFeedActor m_Actor;
    char m_CrewMotto[RL_CLAN_MOTTO_MAX_CHARS];
    char m_CrewTag[RL_CLAN_TAG_MAX_CHARS];
};

typedef atFixedArray<ScFeedFilterAndActor, MAX_WALLS_FILTERS_FROM_FEED> ScFeedWallFilters;

//PURPOSE
//  Little helper class to keep the localization for marketing feeds. I keep it as array of ScFeedLoc as that's smaller.
class ScFeedLoc 
{
public:
    static const unsigned LONGEST_ROCKSTAR_TEXT_CHARS = 710;
    static const unsigned LONGEST_USER_TEXT_CHARS = 160; // It's actually 140 but we leave a bit of margin.
    static const unsigned LONGEST_USER_TEXT_BUFFER = (LONGEST_USER_TEXT_CHARS * 4) + 2;
    static const unsigned REQUIRED_READABLE_PERCENTAGE = 80;
    static const unsigned MAX_LAYOUT_ITEMS = 3;

    static const char* LOCAL_IMAGE_PROTOCOL; // The prefix used to identify a local image
    static const char* LOCAL_TEXT_PROTOCOL; // The prefix used to identify a local text ID

    static const atHashString TITLE_ID;
    static const atHashString BIGTITLE_ID;
    static const atHashString BODY_ID;
    static const atHashString STICKERTEXT_ID;
    static const atHashString FOOTER_ID;
    static const atHashString BUTTON_ID;
    static const atHashString IMAGE_ID;
    static const atHashString BIGIMAGE_ID;
    static const atHashString LOGO_ID;
    static const atHashString URL_ID;

    static const atHashString UNSUPPORTED_CHARS_COMMON_REPLACEMENT;
    static const atHashString UNSUPPORTED_CHARS_UGC_TITLE_REPLACEMENT;
    static const atHashString UNSUPPORTED_CHARS_UGC_DESC_REPLACEMENT;

    //PURPOSE
    //  True if this is not an image to download abut an image from a local texture dictionary
    static bool IsLocalImage(const char* path);

    //PURPOSE
    //  True if this is not a raw text but a text ID from TheText
    static bool IsLocalTextId(const char* path);

    //PURPOSE
    //  Checks the texts and if the non-displayable characters are < requiredPercentage it replaces it with the loc from replacementTextId
    static void SanitizeText(atString& text, const unsigned requiredPercentage, const atHashString replacementTextId);

    //PURPOSE
    //  Checks the text and returns true if the displayable characters are >= requiredPercentage
    static bool CanBeDisplayed(const u16* text, const unsigned textLength, const unsigned requiredPercentage);

    atHashString m_Id;
    atString m_Text;
};

//PURPOSE
//  Little helper class to keep additional parameters for marketing feeds.
class ScFeedParam
{
public:
    static const atHashString LAYOUT_TYPE_ID; // landing page layout type
    static const atHashString LAYOUT_PUBLISH_ID; // groups posts from the same published layout
    static const atHashString LAYOUT_PRIORITY_ID; // the lower the number the higher the priority
    static const atHashString ROW_ID; // landing page row
    static const atHashString COL_ID; // landing page column
    static const atHashString DISABLED_ID; // if set the tile is visible but not usable
    static const atHashString DEEP_LINK_ID;

    static const atHashString STICKER_ID; // Sale, Live, Coming Soon ...

    static const atHashString PRIORITY_IMPRESSION_COUNT_ID;
    static const atHashString PRIORITY_TAB_ID; // sp, mp or both
    static const atHashString PRIORITY_SKIP_DELAY_ID; // time before the skip button shows up
    static const atHashString PRIORITY_PRIORITY_ID; // priority posts can have a display priority

    static const atHashString PRIORITY_TAB_SP;
    static const atHashString PRIORITY_TAB_MP;

    // UI specific dlink params. There are different ways to open an app.
    // Can do with some cleaning up but we have to keep them all working for now.
    // Examples:
    // mode=uiapp uiappid=hub     ... directly opens the app. Can only be used for apps which can be launched in single player, multiplayer and on the landing page
    // mode=freeroam uiappid=hub  ... boots into freeroam and opens the app.
    // mode=hub
    // mode=uiapp uiappid=hub
    static const atHashString DLINK_MODE_UIAPP; // Open a ui app specified with DLINK_UIAPP_ID
    static const atHashString DLINK_UIAPP_ID; // The appid to open
    static const atHashString DLINK_UIAPP_ENTRY_POINT; // The entry point of the app. Can be omitted
    // Used for the 'old' ways to open UI pages. mode=goldstoremetro, mode=freeroam arg_0=hub ...
    static const atHashString DLINK_CLASS_UI;
    static const atHashString DLINK_CLASS_MP;
    static const atHashString DLINK_UI_TARGET_ID;
    static const atHashString DLINK_UI_TARGET_NEWS;
    static const atHashString DLINK_MODE_GOLDSTORE_METRO;
    static const atHashString DLINK_MODE_GOLDSTORE_REAL;
    static const atHashString DLINK_MODE_GOLDSTORE_SPUPGRADE;

    static const atHashString DLINK_MODE_FREEROAM;
    static const atHashString DLINK_MODE_KNOWNPOSSE;
    static const atHashString DLINK_MODE_MP_SUBSCRIPTION_UPSELL; // Open the upsell screen
    static const atHashString DLINK_SCAUTHLINK;
    static const atHashString DLINK_TRACKING_ID;

    // Tags used to hide posts based on player states
    static const atHashString VISIBILITY_TAG_ID;

    // Only when the player has PS+/Gold
    static const atHashString VIS_TAG_MP_SUBSCRIPTION_ONLY;
    
    // Only when the player has no PS+/Gold
    static const atHashString VIS_TAG_NO_MP_SUBSCRIPTION_ONLY;
	
	// Only if the player is on the stand alone version
	static const atHashString STANDALONE_ONLY;
	
	// Only if the player is not on the stand alone version. So legacy or full
	static const atHashString NOT_STANDALONE_ONLY;
    
    // Only when the player has the upsell promotion
    ORBIS_ONLY(static const atHashString VIS_TAG_CUSTOM_UPSELL_ONLY);
    
    // Only when the player has the play multiplayer without PS+ promotion
    ORBIS_ONLY(static const atHashString VIS_TAG_MP_PROMOTION_ONLY);

    static const char* DLINK_FLAG_IS_STORE;
    static const char* DLINK_FLAG_IS_PERSISTENT_POSSE;
    static const char* DLINK_FLAG_IS_STD_FREEROAM;
    static const char* DLINK_SPUPGRADE_STRING;

    s64 ValueAsInt64() const;

    bool ValueAsInt64(s64& value) const;

	//PURPOSE
	//  Returns the number of a string with prefix such as mbit_2355.
	bool ValueAsInt64(const char* prefix, s64& value) const;

    atHashString ValueAsHash() const;

    bool ValueAsBool() const;

    //PURPOSE
    //  Reads the value as stamp and returns the texture and txd name
    //  Returns false if the value isn't a stamp
    bool ValueAsStampFile(atHashString& texture, atHashString& txd) const;

public:
    atHashString m_Id;
    atString m_Value;
};

//PURPOSE
//  Identifiers for various groups of feeds. Roughly identifies where they show up on the UI.
namespace ScFeedUIArare
{
    static const atHashString LANDING_FEED          = ATSTRINGHASH("landing", 0xFA9B6C5);
    static const atHashString PRIORITY_FEED         = ATSTRINGHASH("priority_feed", 0x321FA363);
    static const atHashString SERIES_FEED           = ATSTRINGHASH("series", 0xB0B2C4A7);
    static const atHashString HEIST_FEED            = ATSTRINGHASH("heist", 0x7AF639CC);
    static const atHashString HUB                   = ATSTRINGHASH("hub", 0x70ABE292);
}

//PURPOSE
// The base class containing all the data of a feed
class ScFeedPost
{
public:
    // The maximum number of group entries we actually parse and keep in memory.
    static const unsigned MAX_GROUP_ENTRIES = 4;
    static const unsigned MAX_ADDITIONAL_GROUP_GUIDS = 12; // If the server returns 100 of them we don't store them all

    ScFeedPost();
    virtual ~ScFeedPost();

    ScFeedType GetFeedType() const { return m_Type; }

    //PURPOSE
    // Returns the unique id of the feed
    RosFeedGuid GetId() const { return m_Id; }

    //PURPOSE
    // Returns the actor (person, group application) of the feed
    const ScFeedActor& GetActor() const { return m_Actor; }

    //PURPOSE
    // Certain feeds contain multiple actors. This can be used to get the actor from a regarding or subject field
    //PARAMS
    // type   - The string representation of the actor type
    // id     - The string representational of the actor id. This can be a UserId, RockstarId, crew ...
    const ScFeedActor* GetActor(const char* type, const char* id) const;

    //PURPOSE
    // Returns the display name or the normal name depending on the actor and actor type
    const char* GetSubjectDisplayName(const ScFeedSubject& subject) const;

    //PURPOSE
    // Returns the localization text by id. This can also contain a url to a localized image.
    const atString* GetLoc(const atHashString id) const;

    //PURPOSE
    // Returns a param value by id. If the param isn't set it will return null.
    const ScFeedParam* GetParam(const atHashString id) const;

    //PURPOSE
    //  Pseudo json-path like field lookup.
    //  Returns the string if found or nullptr if not. Only usable on fields stored as string.
    //PARAMS
    //  fieldPath    - The path to look up. Examples: "regarding.crew.name", "actor.name"
    const char* GetField(const char* fieldPath) const;

    //PURPOSE
    //  Pseudo json-path like field lookup. Returns true if found and it's a valid int.
    //PARAMS
    //  fieldPath    - The path to look up. Examples: "regarding.presetmessage.id", "regarding.crew.id", "cdata.row"
    //  result       - The read integer
    bool GetFieldAsInt64(const char* fieldPath, s64& result) const;

    //PURPOSE
    // Returns the unformatted feed text
    const atString& GetRawText() const { return m_Content; }

    //PURPOSE
    // The time when the feed was created
    u64 GetCreatePosixTime() const { return m_CreatePosixTime; }

    //PURPOSE
    // The time when the feed flagged as read by the user
    u64 GetReadPosixTime() const { return m_ReadPosixTime; }

    //PURPOSE
    // Says if I liked or disliked the feed. Currently only likes are allowed so the dislike should be treated as none
    ScFeedReaction GetMyReaction() const;

    //PURPOSE
    // Posts containing UGC can have distinct reactions on the post and the UGC
    // In some cases this can be mapped one to one onto GetMyReaction(), in other cases it will differ
    ScFeedReaction GetMyUgcReaction() const;

    //PURPOSE
    // We read the groups as json elements but there may be a lot in which case only a few are returned.
    // Therefore we also get the total number in case it has to be displayed in the text of the feed.
    unsigned GetNumGroups() const { return m_NumGroups; }

    //PURPOSE
    // How often the feed has been commented. We don't get all comments so this can be useful
    unsigned GetNumComments() const { return m_NumComments; }

    //PURPOSE
    // How often the feed has been liked
    unsigned GetNumLikes() const;

    //PURPOSE
    // How often the contained UGC has been liked. Will return 0 on post types which don't contain UGC.
    unsigned GetNumUgcLikes() const;

    //PURPOSE
    // Some ugc has extra data
    bool GetUgcStats(unsigned& likes, unsigned& dislikes, unsigned& playcount) const;

    //PURPOSE
    // True if you can comment this feed
    bool GetAllowComment() const { return m_Flags.m_AllowComment; }

    //PURPOSE
    // True if you can react to the feed. Right now react == like
    bool GetAllowReaction() const { return m_Flags.m_AllowReaction; }

    //PURPOSE
    // True if you can share the feed
    bool GetAllowShare() const { return m_Flags.m_AllowShare; }

    //PURPOSE
    // True if the user can delete/hide the post
    bool GetAllowDelete() const { return m_Flags.m_AllowDelete; }

    //PURPOSE
    // True if the user can report the post
    bool GetAllowReport() const { return m_Flags.m_AllowReport; }

    //PURPOSE
    // True if the user block all feeds of the user that created this feed post
    bool GetAllowBlock() const { return m_Flags.m_AllowBlock; }

    //PURPOSE
    // When true the group/subject contains a list of actors/people
    bool GetMultipleActors() const { return m_Flags.m_MultipleActors; }

    //PURPOSE
    // Returns the regarding data of a feed with a specific type. If it's not found a nullptr will be returned
    // Each feed can only have one entry for each type
    //PARAMS
    // regardingType ... the type of regarding to return.
    const ScFeedRegarding* GetRegarding(const ScFeedRegardingType regardingType) const;

    //PURPOSE
    // Returns the regarding data array
    const atArray<ScFeedRegarding>& GetRegardings() const { return m_Regarding; }

    //PURPOSE
    // Some feeds contain a sub feed post
    const atArray<ScFeedPost>& GetRegardingPosts() const { return m_RegardingPosts; }

    //PURPOSE
    // Returns the regarding data array
    const atArray<ScFeedPost>& GetGroup() const { return m_Group; }

    //PURPOSE
    // The group can be large. We don't store all data but we store some of the IDs
    const atArray<RosFeedGuid>& GetAdditionalGroupGuids() const { return m_AdditionalGroupGuids; }

    //PURPOSE
    // Returns the localizations (mainly used by marketing feeds).
    const atArray<ScFeedLoc>& GetLocs() const { return m_Locs; }

    //PURPOSE
    // Returns additional feed parameters (mainly used by marketing feeds or big feeds).
    const atArray<ScFeedParam>& GetParams() const { return m_Params; }

    //PURPOSE
    // Returns the post from GetGroup or GetRegardingPosts with the specified id. null if not found.
    const ScFeedPost* GetChildPost(const RosFeedGuid feedId) const;

    //PURPOSE
    // Returns the post from GetGroup or GetRegardingPosts with the specified id. null if not found.
    ScFeedPost* GetChildPost(const RosFeedGuid feedId);

    //PURPOSE
    // Returns if there is a post in Group, RegardingPosts or in AdditionalGroupGuids with the specified id
    bool HasChildPost( const RosFeedGuid feedId ) const;

    // PURPOSE
    // Returns the number of Child Posts this feed post has from Group, RegardingPosts and AdditionalGroupGuids 
    int GetChildPostCount() const;

    //PURPOSE
    // Deletes a child post from it's group by it's Id
    // Returns if successfully deleted
    bool DeleteChildPostById( const RosFeedGuid feedId );

    //PURPOSE
    //  It provides the walls which can be opened for that feed. For example a crew feed may have a wall for the user whoi created the feed and a wall for the crew itself.
    //  There are some feeds which refer to multiple users or to a user and a wall.
    //  There are some feeds which don't have any wall, for example marketing feeds.
    //  The 'most important' wall from an UI point of view is in front.
    void GetWallFilterFromFeed(ScFeedWallFilters& filters) const;

    //PURPOSE
    // Create a feed locally with data specified instead of parsing a json file.
    void SetupLocalFeed(const ScFeedType type, RosFeedGuid id);

    //PURPOSE
    // Updates the user reaction on this feed
    void SetMyReaction(const ScFeedReaction reaction);

    //PURPOSE
    // Updates the user reaction on this feeds UGC
    void SetMyUgcReaction(const ScFeedReaction reaction);

    //PURPOSE
    // Can be used to set the id for when the post is manually generated
    void SetId(const RosFeedGuid guid) { m_Id = guid; }

    //PURPOSE
    // Parses a feed from a Json text
    bool ParseContent(const char* body);

    //PURPOSE
    // Builds the feed content from a RsonReader. Returns true if succeeded
    //PARAMS
    // rr       - The Json data
    // feedType - When this is not ScFeedType::Invalid the feed is forced to the specified type
    bool DeserialiseFeed(const RsonReader& rr, const ScFeedType feedType);

    //PURPOSE
    // Allocates a new ScFeedPost or derived class and fills it with the same data
    virtual ScFeedPost* Clone() const;

    //PURPOSE
    // Returns a list of URLs/addresses from which to get the needed images
    //PARAMS
    // urls ... the returned list of URLs
    virtual void FillTextureUrls(atArray<atString>& urls) const;

    //PURPOSE
    //  Returns an estimated height of the post
    unsigned GetHeightEstimate() const;
    unsigned GetRawHeightEstimate() const;

    //PURPOSE
    // Adds an actor to m_Actors if it's not in the list and not the same as m_Actor
    void AddActor(const ScFeedActor& actor);

    //PURPOSE
    // Adds an image to m_images if it's not in the list
    void AddImage(const atString& img);

    //PURPOSE
    //  Overwrites the default height estimate
    void SetHeightEstimate(const u16 height);

    //PURPOSE
    // Converts a string to a ScFeedType. Returns true if succeeded
    static bool TypeFromString(const char* typeString, ScFeedType& resultType);

    //PURPOSE
    // Converts a ScFeedType to a string
    static const char* TypeToString(const ScFeedType feedType);

    //PURPOSE
    // Returns the image flags of the type (this is a bitfield of ScFeedImageFlags)
    // This is more a hint to keep a updated list with the wiki https://beta.sc.rockstargames.com/styleguide/ui-components/feed-posts
    // The game-ui code can still do custom things if needed
    static unsigned ImageFlagsFromType(const ScFeedType feedType);

    //PURPOSE
    // Returns the flags of the post type (this is a bitfield of ScFeedTypeFlags)
    static unsigned FlagsFromType(const ScFeedType feedType);

    //PURPOSE
    // Returns the UI-class we want to use on social club feed page
    static ScFeedUI UIFromType(const ScFeedType feedType);

    //PURPOSE
    // Returns a size estimate of the post. This is not a size in pixel or anything like that
    // It's only valid relative to other sizes
    static unsigned HeightEstimateFromType(const ScFeedType feedType);

    //PURPOSE
    // Returns true if the feed type contains the specified flags
    static bool HasFlags(const ScFeedType feedType, const unsigned flags);

    //PURPOSE
    // Extracts the type from a json document
    static bool TypeFromJson(const char* jsonDoc, ScFeedType& resultType);

    //PURPOSE
    // Extracts the type from a json document
    static bool TypeFromJson(const RsonReader& rr, ScFeedType& resultType);

    //PURPOSE
    // Returns true if this is a feed type created by Rockstar
    static bool IsRockstarFeedType(const ScFeedType feedType);

    //PURPOSE
    // Returns true if this is a crew-related feed type
    static bool IsCrewFeedType(const ScFeedType feedType);

	//PURPOSE
	// Return true if this is an SP-related feed type
	static bool IsUgcFeedType(const ScFeedType feedType);

	//PURPOSE
	// Returns true if this is an aggregated feed
	static bool IsAggregatedFeedType(const ScFeedType feedType);

	//PURPOSE
	// Returns the non-aggregated type of an aggregated feed
	static ScFeedType GetAggregetedChildType(const ScFeedType feedType);

    //PURPOSE
    // Given a string like "regarding.<reg>.<param>" it returns the regarding type and the param
    // return nullptr if the string is not valid
    static const char* GetRegardingParamFromPath(const char* path, ScFeedRegardingType& regardingType);

    //PURPOSE
    // This is static because it's a bit dodgy logic. I don't want to dupe it around the code but don't want it to be overused
    // returns nullptr if not found
    static const ScFeedParam* GetGameActionDlink(const ScFeedPost& post);

    //PURPOSE
    // Extracts the mode value from the dlink
    static atHashString GetDlinkModeHash(const ScFeedPost& post);

    //PURPOSE
    // Extracts the arg_0 value from the dlink
    static atHashString GetDlinkClassHash(const ScFeedPost& post);

    //PURPOSE
    // Extracts a dlink param value
    static atHashString GetDlinkParamHash(const ScFeedPost& post, const atHashString paramName);

    //PURPOSE
    // Extracts the telemetry tracking id of the post
    static u32 GetDlinkTrackingId(const ScFeedPost& post);

    //PURPOSE
    // True if the dlink contains the param
    static bool HasDlinkParam(const ScFeedPost& post, const atHashString paramName);

    //PURPOSE
    // True when it links to any page related to the gold store or the sp upgrade store
    // It's called gold store to distinguish it from stores such as butcher & co.
    static bool IsAnyGoldStoreDlink(const ScFeedPost& post);

    //PURPOSE
    // True when this links to any UI page
    static bool IsAnyUiDlink(const ScFeedPost& post);

    //PURPOSE
    // True for any gambling related tile
    static bool IsAboutGambling(const ScFeedPost& post);

    //PURPOSE
    // True for the news til
    static bool IsAboutNews(const ScFeedPost& post);

    //PURPOSE
    // Any freeroam option which throws you randomly into freeroam or back where you were last time
    static bool IsStandardFreeroam(const ScFeedPost& post);

    //PURPOSE
    // True when either a url or a dlink with scauthlink is present
    static bool HasWebUrl(const ScFeedPost& post);

    //PURPOSE
    // True when the post has either a link to open or a UGC id that can be opened
    static bool HasWebViewContent(const ScFeedPost& post);

    //PURPOSE
    // Returns true if the post has any flag which hides it for the local user
    static bool IsHiddenByDlinkFlag(const ScFeedPost& post);

	//PURPOSE
	// Is dlink disabled currently by the game 
	static bool IsDLinkDisabledByGame(const ScFeedPost& post);

    //PURPOSE
    // Send a telemetry metric about the clicked feed post
    //PARAMS
    // post - The feed post
    // type - The extracted action type. Example: freeroam, goldstore, series, weburl ...
    // tile - The extracted tile type. Example: last_region, last_location .... Can be 0.
    // area - The area where the tile was clicked. Can be 0 ... boot, 1 ... single player, 2 ... multi player
    static void GetDlinkMetricData(const ScFeedPost& post, u32& type, u32& tile, u32& area);

public:
    RosFeedGuid m_Id;
    ScFeedType m_Type;

    ScFeedActor m_Actor;
    atString m_Content; // This is usually the feed text

    u64 m_CreatePosixTime;
    u64 m_ReadPosixTime;

    ScFeedReaction m_MyReaction;

    atArray<ScFeedRegarding> m_Regarding;
    atArray<ScFeedPost> m_Group;
    atArray<RosFeedGuid> m_AdditionalGroupGuids; // These are the entries where the server only returns the guid but not the full post.
    atArray<ScFeedPost> m_RegardingPosts;
    atArray<ScFeedActor> m_Actors; // Certain fields like regarding.subject can contain actor data. The actor data is big so we keep a separate list.
    atArray<ScFeedLoc> m_Locs;
    atArray<ScFeedParam> m_Params;
    atArray<atString> m_Images; // Additional images which don't fit in any other category.

    unsigned m_NumGroups; // This can be greater than the list of groups and in many cases we don't display the groups so just storing the number is simpler.
    unsigned m_NumComments; // This can be greater than the list of comments and in many cases we don't display the comments so just storing the number is simpler.
    unsigned m_NumLikes;

    ScFeedFlags m_Flags;
    u16 m_HeightEstimate;
};

typedef LambdaCopy<void(const netStatus& result, const ScFeedRequest& request)> ScFeedRequestCallback;
typedef LambdaCopy<void(const char* feedPost, const ScFeedRequest& request)> ScFeedPostCallback;

//PURPOSE
// A read request with filter and range
class ScFeedRequest
{
public:
    static const unsigned INVALID_REQUEST_ID;
    static const unsigned MAX_INDIVIDUAL_IDS = 16;
    static unsigned s_NextRequestId;

    ScFeedRequest();

    //PURPOSE
    //  A request with a list of feed post Ids
    ScFeedRequest(const RosFeedGuid* feedIds, const unsigned numIds, const ScFeedRequestCallback& requestCb, const ScFeedPostCallback& postCb);

    //PURPOSE
    // Construct the request with the filter and search range
    ScFeedRequest(const ScFeedFilterId& filter, unsigned start, unsigned amount);

    //PURPOSE
    // Construct the request with the filter and search range
    ScFeedRequest(const ScFeedFilterId& filter, unsigned start, unsigned amount, const ScFeedRequestCallback& requestCb, const ScFeedPostCallback& postCb);

    //PURPOSE
    // The search filters which also are used as the id of the wall
    const ScFeedFilterId& GetFilter() const { return m_Filter; }

    //PURPOSE
    // Can be used to keep track of a specific request
    unsigned GetRequestId() const { return m_RequestId; }

    //PURPOSE
    // True if it's a valid search request
    bool IsValid() const { return m_Amount > 0; }

    //PURPOSE
    // Returns true if start,amount are fully enclosed by this request
    bool ContainsRange(const unsigned start, const unsigned amount) const;

    //PURPOSE
    // Returns true if other is contained in the range and filter of this request
    bool Contains(const ScFeedRequest& other) const;

    //PURPOSE
    // Retuens the corresponding error string for a download error
    static const char* GetErrorString(const int feedDownloadError);

public:
    ScFeedFilterId m_Filter;
    ScFeedRequestCallback m_RequestCallback;
    ScFeedPostCallback m_PostCallback;
    atArray<RosFeedGuid> m_RequestedPosts;

    unsigned m_Start;
    unsigned m_Amount;
    unsigned m_RequestId;
};

//PURPOSE
//  Keep track of an in-progress feed download.
//  Contains the netStatus and the iterator which keeps the downloaded xml in memory
class ScFeedActiveRequest
{
public:
    ScFeedActiveRequest();
    ~ScFeedActiveRequest();

    //PURPOSE
    //  Clears/stops the download and frees the resource
    void Clear();

    //PURPOSE
    //  When true this is a valid request which is being processed/downloaded
    bool IsValid() const { return m_Request.IsValid(); }

    const ScFeedRequest& GetRequest() const { return m_Request; }

    unsigned GetRequestId() const { return m_Request.GetRequestId(); }

public:
    ScFeedRequest m_Request;

    rlFeedMessageIterator m_FeedIter;
    netStatus m_RefreshFeedsStatus;
    CloudRequestID m_CloudRequestId;
};

//PURPOSE
//  Script version of th feed guid
class scrScFeedGuid
{
public:
    static RosFeedGuid FeedGuidFromScript(const scrScFeedGuid& scrFeedId);
    static scrScFeedGuid FeedGuidToScript(const RosFeedGuid& feedId);

public:
    rage::scrValue m_Data0;
    rage::scrValue m_Data1;
    rage::scrValue m_Data2;
    rage::scrValue m_Data3;
};

//PURPOSE
//  Script version of a minimalistic feed identifier
class scFeedHeaderBase
{
public:
    scrScFeedGuid   m_FeedId;
    scrValue        m_FeedTypeHash;
};

#endif //GEN9_STANDALONE_ENABLED

#endif //INC_SOCIALCLUBFEED_H_
