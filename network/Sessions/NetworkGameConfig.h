//
// name:        gameconfig.h
// description: Game configuration.  Used to host games and to search
//				for games that match the parameters in the config.
//				Instances of this are typically converted from attribute
//				arrays passed in from script.

#ifndef GAMECONFIG_H
#define GAMECONFIG_H

#include "rline/rlmatchingattributes.h"
#include "rline/rlsessionfinder.h"
#include "fwnet/nettypes.h"
#include "fwnet/netchannel.h"
#include "network/NetworkTypes.h"

class NetworkGameFilter;

enum // connection
{
	GOOD_CONNECTION = 0,
	BAD_CONNECTION = 1
};

enum // activity filtering
{
	// content host setting
	ACTIVITY_CONTENT_ROCKSTAR_CREATED = 0,
	ACTIVITY_CONTENT_USER_CREATED = 1,

	// content search setting
	ACTIVITY_CONTENT_SEARCH_ROCKSTAR_ONLY = 0,
	ACTIVITY_CONTENT_SEARCH_USER_ONLY = 1,
	ACTIVITY_CONTENT_SEARCH_ANY = 2,
};

enum // activity island
{
	ACTIVITY_ISLAND_GENERAL = 0,
	ACTIVITY_ISLAND_SPECIFIC
};

enum StaticDiscriminatorType
{
	Type_Early,
	Type_InGame,
	Type_Max,
};

//PURPOSE
//  Base class for network game configurations.  Also used as
//  a base for matchmaking filters.
class NetworkBaseConfig
{

public:

	// functions to cook our matchmaking settings
	static unsigned GetStaticDiscriminator(const StaticDiscriminatorType discriminatorType);
	static unsigned GetStaticUser(const StaticDiscriminatorType discriminatorType);

	static unsigned GetMatchmakingRegion();
	static unsigned GetMatchmakingVersion();
	static unsigned GetMatchmakingPort();
	static unsigned GetMatchmakingBuild();
	static unsigned GetMatchmakingUser(const bool capvalue = true);
	static unsigned GetMatchmakingDataHash();
    static s32 GetAimPreference(); 
	static unsigned GetAimBucket(s32 nAimPreference); 
	static unsigned GetMatchmakingAimBucket(s32 nAimPreference = -1); 
    static s32 GetMatchmakingLanguage();
    static eMultiplayerPool GetMatchmakingPool();
	static bool GetMatchmakingExclude();

	static bool SetUsingExclude();
	static bool SetExcludeCheaters();
	static bool SetExcludeJapaneseSKU();

#if RSG_DURANGO || RSG_ORBIS
	static bool SetExcludeFreeAim();
#endif

#if RSG_DURANGO
	static bool SetExcludeBadReputation();
#endif

	// functions to enable / disable aim bucketing
	static void SetAimBucketingEnabled(const bool bEnabled);
	static void SetAimBucketingEnabledForArcadeGameModes(const bool bEnabled);

public:

    //What is a discriminator?
    //It's used in matchmaking to discriminate sessions based on
    //a variety of attributes like network port, game version, and whether
    //the session is visible.  A matchmaking filter will only match sessions
    //in which the discriminator values match the filter's discriminator.
    //It's required to have a matchmaking attribute called MMATTR_DISCRIMINATOR
    //defined in the matchmaking attributes for the game (e.g. in the xlast
    //matchmaking session).  All matchmaking filters must contain a condition
    //that tests equality of MMATTR_DISCRIMINATOR.

    enum DiscriminatorBits
    {
		//  Port discriminator.  The port is unique number known
		//  amongst a set of players that is used to restrict matchmaking to
		//  a known set of sessions.  It is used only in debugging.
        PORT_BITS           = 8,
        PORT_SHIFT          = 0,
        PORT_MASK           = (1<<PORT_BITS)-1,
        PORT_MAX_VALUE      = (1<<PORT_BITS)-1,

		//  Sets/gets the game version.  This ensures players who have different
		//  versions of the game will not match up in matchmaking.
        VERSION_BITS        = 4,
        VERSION_SHIFT       = PORT_BITS,
        VERSION_MASK        = (1<<VERSION_BITS)-1,
        VERSION_MAX_VALUE   = (1<<VERSION_BITS)-1,

		//  Gets the session's build attribute. Each build will have a different 
		//  value so that we can identify the session and give an error debug.
		BUILD_BITS			= 2,
		BUILD_SHIFT			= VERSION_SHIFT + VERSION_BITS,
		BUILD_MASK			= (1<<BUILD_BITS)-1,
		BUILD_MAX_VALUE		= (1<<BUILD_BITS)-1,

		//  Gets the sessions exclude attribute
        EXCLUDE_BITS        = 1,
        EXCLUDE_SHIFT       = BUILD_SHIFT + BUILD_BITS,
        EXCLUDE_MASK        = (1<<EXCLUDE_BITS)-1,
        EXCLUDE_MAX_VALUE   = (1<<EXCLUDE_BITS)-1,

		//  Gets the session's visibility attribute.  Matching filters
		//  always have this value set to true, so only visible sessions will
		//  be matched.
		VISIBLE_BITS        = 1,
		VISIBLE_SHIFT       = EXCLUDE_SHIFT + EXCLUDE_BITS,
		VISIBLE_MASK        = (1<<VISIBLE_BITS)-1,
		VISIBLE_MAX_VALUE   = (1<<VISIBLE_BITS)-1,

		//  Gets the session's in progress attribute. This allows us to block
		//  out in progress sessions from our matchmaking queries
		IN_PROGRESS_BITS		= 1,
		IN_PROGRESS_SHIFT       = VISIBLE_SHIFT + VISIBLE_BITS,
		IN_PROGRESS_MASK        = (1<<IN_PROGRESS_BITS)-1,
		IN_PROGRESS_MAX_VALUE   = (1<<IN_PROGRESS_BITS)-1,

        //  Gets the session's aim attribute. 
        AIM_BITS            = 1,
        AIM_SHIFT           = IN_PROGRESS_SHIFT + IN_PROGRESS_BITS,
        AIM_MASK            = (1<<AIM_BITS)-1,
        AIM_MAX_VALUE       = (1<<AIM_BITS)-1,

		//  User is packed into remaining space
		//  Sets/gets application-defined attributes in the discriminator.
        USER_BITS           = 32 - (AIM_SHIFT + AIM_BITS),
        USER_SHIFT          = AIM_SHIFT + AIM_BITS,
        USER_MASK           = (1<<USER_BITS)-1,
        USER_MAX_VALUE      = (1<<USER_BITS)-1,
    };

    enum
    {
        SESSION_GAME_MODE_MAX_BITS = 16,
        SESSION_GAME_MODE_MASK = (1 << SESSION_GAME_MODE_MAX_BITS) - 1,
    };

    NetworkBaseConfig();
    virtual ~NetworkBaseConfig();

	// sets all values to their initial / default values.
	virtual void Clear();

    template<typename T>
	void Reset() 
	{ 
		this->Clear();

		// required fields
		m_DiscriminatorFieldIndex = T::GetFieldIndexFromFieldId(T::FIELD_ID_MMATTR_DISCRIMINATOR);
		m_RegionFieldIndex = T::GetFieldIndexFromFieldId(T::FIELD_ID_MMATTR_REGION);

        // aim-type moved to discriminator, use for language (pending XLAST change)
		m_LanguageFieldIndex = T::GetFieldIndexFromFieldId(T::FIELD_ID_MMATTR_AIM_TYPE);

		// optional fields
		m_GroupFieldIndex[0] = T::GetFieldIndexFromFieldId(T::FIELD_ID_MMATTR_MM_GROUP_1);
		m_GroupFieldIndex[1] = T::GetFieldIndexFromFieldId(T::FIELD_ID_MMATTR_MM_GROUP_2);
		
		// activity fields
		m_ActivityTypeFieldIndex = T::GetFieldIndexFromFieldId(T::FIELD_ID_MMATTR_ACTIVITY_TYPE);
		m_ActivityIDFieldIndex = T::GetFieldIndexFromFieldId(T::FIELD_ID_MMATTR_ACTIVITY_ID);
        m_ActivityPlayersFieldIndex = T::GetFieldIndexFromFieldId(T::FIELD_ID_MMATTR_ACTIVITY_PLAYERS);

		Init(); 
	}

	// sets up discriminator
	void Init();

    // returns the full 32-bit discriminator, which is a combination of the discriminator attributes.
    unsigned GetDiscriminator() const;

	// access to discriminator values
    unsigned GetPort() const		{ return m_Port; }
    unsigned GetVersion() const		{ return m_VersionBits; }
	unsigned GetBuildType() const	{ return DEFAULT_BUILD_TYPE_VALUE; }
    unsigned GetExclude() const	    { return m_ExcludeBits; }
    bool IsVisible() const			{ return m_VisibleBits == 1 ? true : false; }
	bool IsInProgress() const		{ return m_InProgressBits == 1 ? true : false; }
	unsigned GetAimBucket() const   { return m_AimBucket; }
    unsigned GetUser() const		{ return m_UserBits; }

	// returns field id for attributes
	int GetDiscriminatorFieldIndex() const				{ return m_DiscriminatorFieldIndex; }
	int GetRegionFieldIndex() const						{ return m_RegionFieldIndex; }
	int GetLanguageFieldIndex() const					{ return m_LanguageFieldIndex; }
	int GetGroupFieldIndex(const u32 nGroup) const		{ return m_GroupFieldIndex[nGroup]; }
	int GetActivityTypeFieldIndex() const				{ return m_ActivityTypeFieldIndex; }
	int GetActivityIDFieldIndex() const					{ return m_ActivityIDFieldIndex; }
    int GetActivityPlayersFieldIndex() const            { return m_ActivityPlayersFieldIndex; }

	// re-use group fields for activity MM 
	int GetContentCreatorFieldIndex() const				{ return m_GroupFieldIndex[0]; }
	int GetActivityIslandFieldIndex() const				{ return m_GroupFieldIndex[1]; }

    void SetExclude(const unsigned nExclude);

protected:

	// sets the full discriminator
	void SetDiscriminator(const unsigned discriminator);

	// protected - allow the config or filter to define what is exposed
	void SetPort(const unsigned port);
	void SetVersion(const unsigned version);
	void SetBuildType(const unsigned nBuildType);
	void SetVisible(const bool bVisible);
	void SetInProgress(const bool bInProgress);
	void SetAimBucket(const unsigned nAimBucket);
    void SetUser(const unsigned user);

private:

    unsigned m_Port;
    unsigned m_PortBits			: PORT_BITS;
    unsigned m_VersionBits		: VERSION_BITS;
	unsigned m_BuildBits		: BUILD_BITS;
	unsigned m_ExcludeBits		: EXCLUDE_BITS;
	unsigned m_VisibleBits		: VISIBLE_BITS;
	unsigned m_InProgressBits	: IN_PROGRESS_BITS;
    unsigned m_AimBucket        : AIM_BITS;
    unsigned m_UserBits			: USER_BITS;
    
	// field ids
	int m_DiscriminatorFieldIndex;
	int m_RegionFieldIndex;
	int m_LanguageFieldIndex;
	int m_GroupFieldIndex[MAX_ACTIVE_MM_GROUPS];
	int m_ActivityTypeFieldIndex;
	int m_ActivityIDFieldIndex;
    int m_ActivityPlayersFieldIndex;

	static bool s_bUseExclude;
	static bool s_bExcludeCheaters;

#if RSG_DURANGO || RSG_ORBIS
	static bool s_bExcludeFreeAim;
#endif

#if RSG_DURANGO
	static bool s_bExcludeBadReputation;
#endif

	static bool sm_bAimBucketingEnabled;
	static bool sm_bAimBucketingEnabledForArcadeGameModes;
};

//PURPOSE
//  Used to configure games for hosting.
class NetworkGameConfig : public NetworkBaseConfig
{
public:

    NetworkGameConfig();

    template<typename MatchmakingSchema>
    void Reset(const MatchmakingSchema& mmSchema,
               const u16 gameMode,
               const u16 sessionPurpose,
               const unsigned maxSlots,
               const unsigned maxPrivSlots)
    {
        gnetAssert(maxSlots > 0);
        gnetAssert(maxSlots >= maxPrivSlots);
		gnetAssert(maxSlots <= RL_MAX_GAMERS_PER_SESSION);
		gnetAssert(maxPrivSlots <= RL_MAX_GAMERS_PER_SESSION);

        this->NetworkBaseConfig::Reset<MatchmakingSchema>();
        m_MatchingAttrs.Reset(mmSchema);
        m_MaxSlots = maxSlots;
        m_MaxPrivSlots = maxPrivSlots;

		// cap
		if(m_MaxSlots > RL_MAX_GAMERS_PER_SESSION)
		{
			m_MaxSlots = RL_MAX_GAMERS_PER_SESSION;
		}

		// cap
		if(m_MaxPrivSlots > RL_MAX_GAMERS_PER_SESSION)
		{
			m_MaxPrivSlots = RL_MAX_GAMERS_PER_SESSION;
		}

		// set default values
        SetGameMode(gameMode);
        SetSessionPurpose(sessionPurpose);
		SetVisible(true);
		SetInProgress(false);
		SetExclude(GetMatchmakingExclude());
        SetAimBucket(GetMatchmakingAimBucket());

		if(GetRegionFieldIndex() >= 0)
			m_MatchingAttrs.SetValueByIndex(GetRegionFieldIndex(), GetMatchmakingRegion());

		if(this->GetSessionPurpose() == SessionPurpose::SP_Freeroam)
		{
			for(int i = 0; i < MAX_ACTIVE_MM_GROUPS; i++)
			{
				if(GetGroupFieldIndex(i) >= 0)
					SetNumberInGroup(i, 0);
			}
		}
		else if(this->GetSessionPurpose() == SessionPurpose::SP_Activity)
		{
			if(GetContentCreatorFieldIndex() >= 0)
				SetContentCreator(ACTIVITY_CONTENT_ROCKSTAR_CREATED);
			if(GetActivityIslandFieldIndex() >= 0)
				SetActivityIsland(ACTIVITY_ISLAND_GENERAL);
		}
		else // if these are defined, they need to have a value in them, just use 0 via the group setting
		{
			for(int i = 0; i < MAX_ACTIVE_MM_GROUPS; i++)
			{
				if(GetGroupFieldIndex(i) >= 0)
					SetNumberInGroup(i, 0);
			}
		}

		if(GetLanguageFieldIndex() >= 0)
			SetLanguage(GetMatchmakingLanguage());
		if(GetActivityTypeFieldIndex() >= 0)
			SetActivityType(0);
		if(GetActivityIDFieldIndex() >= 0)
			SetActivityID(0);
        if(GetActivityPlayersFieldIndex() >= 0)
            SetActivityPlayersSlotsAvailable(0);
    }

#if !__NO_OUTPUT
	//PURPOSE
	//  Log differences between this and config
	void LogDifferences(const rlSessionConfig& config, const char* szTag);
#endif

    //PURPOSE
    //  Reset the matchmaking attributes.
    //NOTES
    //  The attribute id/value pairs in attrs must exactly match
    //  those we already have.
    void ResetAttrs(const rlMatchingAttributes& attrs);

	//PURPOSE
	//  Apply from config params
	void Apply(const rlSessionConfig& config);

	//PURPOSE
	//  Apply from filter
	void Apply(const NetworkGameFilter* pFilter);

    //PURPOSE
    //  Resets the object to its initial state.
    void Clear();

    //PURPOSE
    //  Returns true if the current configuration is valid.
    bool IsValid() const;

    //PURPOSE
    //  Returns the attributes that are used for matchmaking.
    const rlMatchingAttributes& GetMatchingAttributes() const;

    //PURPOSE
    //  Sets/gets the game mode.
    void SetGameMode(const u16 gameMode);
    u16 GetGameMode() const;

    //PURPOSE
    //  Sets/gets the session type.
    void SetSessionPurpose(const u16 sessionPurpose);
    u16 GetSessionPurpose() const;

    //PURPOSE
    //  Returns the maximum number of slots.
    unsigned GetMaxSlots() const;

    //PURPOSE
    //  Returns the maximum number of public slots.
    unsigned GetMaxPublicSlots() const;

    //PURPOSE
    //  Returns the maximum number of private slots.
    unsigned GetMaxPrivateSlots() const;

	//PURPOSE
	//  Returns whether we have private slots.
	bool HasPrivateSlots() const;

	//PURPOSE
	//  Sets the visible flag.  Visible sessions are advertised on
	//  the matchmaking service.
	void SetVisible(const bool bVisible);

	//PURPOSE
	//  Sets the in progress flag. 
	void SetInProgress(const bool bInProgress);

	//PURPOSE
	//  Gets/sets the number of group members in a matchmaking group
	void SetNumberInGroup(const unsigned nGroup, u32 nMembers);
	u32 GetNumberInGroup(const unsigned nGroup) const;

    //PURPOSE
    //  Gets/sets the number of players in an activity
    //  Does not include spectators
    void SetActivityPlayersSlotsAvailable(u32 nPlayers);
    u32 GetActivityPlayersSlotsAvailable() const;

	//PURPOSE
	//  Sets/gets what aim type the player prefers
	void SetLanguage(const s32 nLanguage);
	s32 GetLanguage() const;

	//PURPOSE
	//  Access to activity setup
	void SetActivityType(const int nActivityType);
	void SetActivityID(const unsigned nActivityID);
	u32 GetActivityType() const;
	u32 GetActivityID() const;

	//PURPOSE
	//  Access activity custom filters
	void SetContentCreator(const unsigned nContentCreator);
	unsigned GetContentCreator() const;
	void SetActivityIsland(const unsigned nActivityIsland);
	unsigned GetActivityIsland() const;

	//PURPOSE
	//  Prints the current configuration to debug output.
	void DebugPrint(bool bDetail) const;

protected:

	unsigned m_MaxSlots;
    unsigned m_MaxPrivSlots;
    mutable rlMatchingAttributes m_MatchingAttrs;
};

//PURPOSE
//  Used to configure matchmaking queries.
class NetworkGameFilter : public NetworkBaseConfig
{
public:

    NetworkGameFilter();

    template<typename FilterSchema>
    void Reset(const FilterSchema& filterSchema)
	{
		this->NetworkBaseConfig::Reset<FilterSchema>();
		m_Filter.Reset(filterSchema);

        SetGameMode(0);
        SetSessionPurpose(static_cast<u16>(SessionPurpose::SP_None));
        ApplyRegion(false);
        ApplyLanguage(false);
        SetExclude(NetworkBaseConfig::GetMatchmakingExclude());
        SetAimBucket(GetMatchmakingAimBucket());
	}

    //PURPOSE
    //  Resets the object to its initial state.
    void Clear();

	//PURPOSE
	//  Returns true if the current filter is valid.
	bool IsValid() const;

    //PURPOSE
    //  Returns the low level filter passed to the rline matchmaking API.
    unsigned GetMatchingFilterID() const;
    const rlMatchingFilter& GetMatchingFilter() const;

    //PURPOSE
    //  Sets/gets the game mode.
    void SetGameMode(const u16 gameMode);
    u16 GetGameMode() const;

    //PURPOSE
    //  Sets/gets the session type.
    void SetSessionPurpose(const u16 sessionPurpose);
    u16 GetSessionPurpose() const;

	//PURPOSE
	//  Region matchmaking access
	#if (RSG_CPU_X86 || RSG_CPU_X64) && RSG_PC
	__declspec(noinline) void ApplyRegion(bool bApplyRegion);
	#else
	void ApplyRegion(bool bApplyRegion);
	#endif
	
	bool IsRegionApplied() const { return m_bIsRegionApplied; }

	//PURPOSE
	//  Language matchmaking access
	#if (RSG_CPU_X86 || RSG_CPU_X64) && RSG_PC
	__declspec(noinline) void ApplyLanguage(bool bApplyLanguage);
	#else
	void ApplyLanguage(bool bApplyLanguage);
	#endif
    bool IsLanguageApplied() const { return m_bIsLanguageApplied; }

	//PURPOSE
	//  Sets/gets the limit of players in a particular group
	//  Note difference with GameConfig, which is number of 
	//  members in a particular group
	void SetGroupLimit(const unsigned nGroup, unsigned nLimit);

    //PURPOSE
	//  Access to activity setup
	void SetActivityType(const int nActivityType);
	void SetActivityID(const unsigned nActivityID);
	bool GetActivityType(unsigned& nActivityType);
	bool GetActivityID(unsigned& nActivityID);

    //PURPOSE
    //  Sets the number of slots required
    //  Note difference with GameConfig, which is number of 
    //  slots available
    void SetActivitySlotsRequired(const unsigned nSlotsRequired);

	//PURPOSE
	//  Sets the in progress flag. 
	void SetInProgress(const bool bInProgress);

	//PURPOSE
	//  For activity MM only - Requests only user created activities
	void SetContentFilter(const unsigned nContentFilter);
	bool GetContentFilter(unsigned& nContentFilter);

	//PURPOSE
	//  For activity MM only - Allows activity sessions to be sectioned off
	//  An example would be when we host 
	void SetActivityIsland(const unsigned nActivityIsland);
	bool GetActivityIsland(unsigned& nActivityID);

    //PURPOSE
    //  Prints the current configuration to debug output.
    void DebugPrint() const;

protected:

    mutable rlMatchingFilter m_Filter;
	bool m_bIsRegionApplied; 
    bool m_bIsLanguageApplied;
};

#endif  //GAMECONFIG_H
