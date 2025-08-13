#ifndef INC_AMBIENT_AUDIO_H
#define INC_AMBIENT_AUDIO_H

// Rage headers
#include "ai/aichannel.h"
#include "atl/array.h"
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "audio/speechaudioentity.h"
#include "data/base.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"
#include "string/stringhash.h"
#include "system/bit.h"

// Game headers
#include "Peds/ped.h"
#include "Peds/PedType.h"

// Game forward declarations
class CPed;

////////////////////////////////////////////////////////////////////////////////
// CAmbientAudio
////////////////////////////////////////////////////////////////////////////////

struct CAmbientAudio
{
	enum Flags
	{
		IsInsulting		= BIT0,
		IsHarassing		= BIT1,
		ForcePlay		= BIT2,
		AllowRepeat		= BIT3,
		IsIgnored		= BIT4,
		IsRant			= BIT5,
		IsHostile		= BIT6,
		IsConversation	= BIT7,
	};

	CAmbientAudio()
	: m_Context(0)
	, m_uFinalizedHash(0)
	, m_Flags(0)
	, m_uConversationVariation(0)
	, m_uConversationPart(0)
	{}

	CAmbientAudio(const char* pContext, fwFlags8 uFlags = 0)
	: m_Context(0)
	, m_uFinalizedHash(0)
	, m_Flags(uFlags)
	, m_uConversationVariation(0)
	, m_uConversationPart(0)
	{
		SetContext(pContext);
	}

	void Clear()
	{
		//Clear the context.
		m_Context = 0;

		//Clear the flags.
		m_Flags.ClearAllFlags();

		//Clear the finalized hash.
		m_uFinalizedHash = 0;

		//Clear the conversation information.
		m_uConversationVariation = 0;
		m_uConversationPart = 0;
	}

	bool DoesContextExistForPed(const CPed& rPed) const
	{
		aiAssert(m_Context > 0);
		return (rPed.GetSpeechAudioEntity() && rPed.GetSpeechAudioEntity()->DoesContextExistForThisPed(GetPartialHash()));
	}

	u32 GetContext() const
	{
		return m_Context;
	}

	u8 GetConversationPart() const
	{
		return m_uConversationPart;
	}

	u8 GetConversationVariation() const
	{
		return m_uConversationVariation;
	}

	fwFlags8& GetFlags()
	{
		return m_Flags;
	}

	const fwFlags8 GetFlags() const
	{
		return m_Flags;
	}

	bool IsValid() const
	{
		return (m_Context != 0);
	}

	void OnPostLoad()
	{
		//Set the finalized hash from the context.
		SetFinalizedHashFromContext();
	}

	void SetContext(const char* pContext)
	{
		//Set the context.
		m_Context = atPartialStringHash(pContext);

#if !__FINAL
		//Add the hash to the string lookup table.
		atHashWithStringNotFinal hAddStringToLookupTable;
		hAddStringToLookupTable.SetFromString(pContext);
#endif

		//Set the finalized hash from the context.
		SetFinalizedHashFromContext();
	}

	void SetConversationPart(u8 uConversationPart)
	{
		m_uConversationPart = uConversationPart;
	}

	void SetConversationVariation(u8 uConversationVariation)
	{
		m_uConversationVariation = uConversationVariation;
	}

	void SetFinalizedHash(u32 uFinalizedHash)
	{
		m_Context			= 0;
		m_uFinalizedHash	= uFinalizedHash;
	}

public:

	u32		GetFinalizedHash() const;
	u32		GetPartialHash() const;
	bool	IsFollowUp() const;

private:

	void SetFinalizedHashFromContext()
	{
		//The context is a partial hash -- finalize it.
		m_uFinalizedHash = (m_Context > 0) ? atFinalizeHash(m_Context) : 0;
	}

private:

	u32			m_Context;
	u32			m_uFinalizedHash;
	fwFlags8	m_Flags;
	u8			m_uConversationVariation;
	u8			m_uConversationPart;

	PAR_SIMPLE_PARSABLE;
};


////////////////////////////////////////////////////////////////////////////////
// CAmbientAudioExchange
////////////////////////////////////////////////////////////////////////////////

struct CAmbientAudioExchange
{
	CAmbientAudioExchange()
	{}

	CAmbientAudio	m_Initial;
	CAmbientAudio	m_Response;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientAudioConditions
////////////////////////////////////////////////////////////////////////////////

struct CAmbientAudioConditions
{
	enum Flags
	{
		IsExercising					= BIT0,
		WillJeerAtHotPeds				= BIT1,
		IsFriendlyWithTarget			= BIT2,
		IsNotFriendlyWithTarget			= BIT3,
		IsThreatenedByTarget			= BIT4,
		IsNotThreatenedByTarget			= BIT5,
		IsTargetHot						= BIT6,
		IsTargetNotHot					= BIT7,
		IsTargetPlayer					= BIT8,
		IsTargetNotPlayer				= BIT9,
		TargetMustHaveResponse			= BIT10,
		IsTargetOnFoot					= BIT11,
		IsTargetMale					= BIT12,
		IsTargetNotMale					= BIT13,
		TargetMustBeKnockedDown			= BIT14,
		IsTargetCop						= BIT15,
		IsTargetGang					= BIT16,
		IsTargetFat						= BIT17,
		DidNotTalkToTargetLast			= BIT18,
		IsTargetFriendlyWithUs			= BIT19,
		IsTargetNotFriendlyWithUs		= BIT20,
		MustHaveContext					= BIT21,
		IsUsingScenario					= BIT22,
		TargetCanBeFleeing				= BIT23,
		TargetMustBeFleeing				= BIT24,
		TargetCanBeInCombat				= BIT25,
		TargetMustBeInCombat			= BIT26,
		TargetCanBeDead					= BIT27,
		TargetMustBeDead				= BIT28,
		IsFriendFollowedByPlayer		= BIT29,
		IsInGangTerritory				= BIT30,
		HasTargetAchievedCombatVictory	= BIT31,
	};

	struct FollowUp
	{
		FollowUp()
		{}

		bool IsValid() const
		{
			return (m_Context.IsNotNull());
		}

		atHashWithStringNotFinal	m_Context;
		float						m_MaxTime;

		PAR_SIMPLE_PARSABLE;
	};

	struct Friends
	{
		Friends()
		{}

		bool IsValid() const
		{
			return (m_MaxDistance >= 0.0f);
		}

		u8		m_Min;
		u8		m_Max;
		float	m_MaxDistance;

		PAR_SIMPLE_PARSABLE;
	};

	CAmbientAudioConditions()
	{}

	bool HasModelName(atHashWithStringNotFinal hName) const
	{
		for(int i = 0; i < m_ModelNames.GetCount(); ++i)
		{
			if(hName == m_ModelNames[i])
			{
				return true;
			}
		}

		return false;
	}

	bool HasPedPersonality(atHashWithStringNotFinal hName) const
	{
		for(int i = 0; i < m_PedPersonalities.GetCount(); ++i)
		{
			if(hName == m_PedPersonalities[i])
			{
				return true;
			}
		}

		return false;
	}

	bool HasPedType(ePedType nPedType) const
	{
		for(int i = 0; i < m_aPedTypes.GetCount(); ++i)
		{
			if(nPedType == m_aPedTypes[i])
			{
				return true;
			}
		}

		return false;
	}

	bool HasModelNames() const
	{
		return (m_ModelNames.GetCount() > 0);
	}

	bool HasPedPersonalities() const
	{
		return (m_PedPersonalities.GetCount() > 0);
	}

	bool HasPedTypes() const
	{
		return (m_aPedTypes.GetCount() > 0);
	}

	bool IsFollowUp() const
	{
		return (m_WeSaid.IsValid() || m_TheySaid.IsValid());
	}

	void OnPostLoad()
	{
		m_aPedTypes.Reserve(m_PedTypes.GetCount());
		for(int i = 0; i < m_PedTypes.GetCount(); ++i)
		{
			m_aPedTypes.Append() = CPedType::FindPedType(m_PedTypes[i].c_str());
		}
	}

	atArray<ConstString>				m_PedTypes;
	atArray<atHashWithStringNotFinal>	m_ModelNames;
	atArray<atHashWithStringNotFinal>	m_PedPersonalities;
	float								m_Chances;
	float								m_MinDistance;
	float								m_MaxDistance;
	float								m_MinDistanceFromPlayer;
	float								m_MaxDistanceFromPlayer;
	fwFlags32							m_Flags;
	FollowUp							m_WeSaid;
	FollowUp							m_TheySaid;
	Friends								m_Friends;
	float								m_NoRepeat;

	atArray<ePedType> m_aPedTypes;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientAudioTrigger
////////////////////////////////////////////////////////////////////////////////

struct CAmbientAudioTrigger
{
	struct ConditionalExchange
	{
		ConditionalExchange()
		{}

		CAmbientAudioConditions				m_Conditions;
		atHashWithStringNotFinal			m_Exchange;
		atArray<atHashWithStringNotFinal>	m_Exchanges;

		PAR_SIMPLE_PARSABLE;
	};

	struct WeightedExchange
	{
		WeightedExchange()
		{}

		float						m_Weight;
		atHashWithStringNotFinal	m_Exchange;

		PAR_SIMPLE_PARSABLE;
	};

	CAmbientAudioTrigger()
	{}

	CAmbientAudioConditions				m_Conditions;
	atHashWithStringNotFinal			m_Exchange;
	atArray<atHashWithStringNotFinal>	m_Exchanges;
	atArray<WeightedExchange>			m_WeightedExchanges;
	atArray<ConditionalExchange>		m_ConditionalExchanges;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientAudioSet
////////////////////////////////////////////////////////////////////////////////

struct CAmbientAudioSet
{
	CAmbientAudioSet()
	{}

	atHashWithStringNotFinal		m_Parent;
	atArray<CAmbientAudioTrigger>	m_Triggers;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientAudios
////////////////////////////////////////////////////////////////////////////////

struct CAmbientAudios
{
	CAmbientAudios()
	{}

	atBinaryMap<CAmbientAudioExchange, atHashWithStringNotFinal>	m_Exchanges;
	atBinaryMap<CAmbientAudioSet, atHashWithStringNotFinal>			m_Sets;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAmbientAudioManager
////////////////////////////////////////////////////////////////////////////////

class CAmbientAudioManager : public datBase
{

public:

	static CAmbientAudioManager& GetInstance()
	{	FastAssert(sm_Instance); return *sm_Instance;	}

public:

	struct Options
	{
		enum Flags
		{
			OnlyFollowUps		= BIT0,
			MustBeFriendly		= BIT1,
			MustBeUnfriendly	= BIT2,
		};

		Options()
		: m_uFlags(0)
		{}

		fwFlags8 m_uFlags;
	};

private:

	explicit CAmbientAudioManager();
	~CAmbientAudioManager();

public:

	const CAmbientAudios& GetAmbientAudios() const { return m_AmbientAudios; }

public:

	const CAmbientAudioExchange* GetExchange(atHashWithStringNotFinal hName) const
	{
		return m_AmbientAudios.m_Exchanges.SafeGet(hName);
	}

	const CAmbientAudioSet* GetSet(atHashWithStringNotFinal hName) const
	{
		return m_AmbientAudios.m_Sets.SafeGet(hName);
	}

public:

	const CAmbientAudioExchange* GetExchange(const CPed& rPed, const CPed& rTarget) const;
	const CAmbientAudioExchange* GetExchange(const CPed& rPed, const CPed& rTarget, const Options& rOptions) const;

public:

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

private:

			bool						EvaluateConditions(const CAmbientAudioConditions& rConditions, const CAmbientAudioExchange& rExchange, const CPed& rPed, const CPed& rTarget, const Options& rOptions) const;
			bool						EvaluateFlags(fwFlags32 uFlags, const CAmbientAudioExchange& rExchange, const CPed& rPed, const CPed& rTarget, const Options& rOptions) const;
			bool						EvaluateFollowUp(const CAmbientAudioConditions::FollowUp& rFollowUp, const CPed& rPed, const CPed& rTarget) const;
			bool						EvaluateFriends(const CAmbientAudioConditions::Friends& rFriends, const CPed& rPed) const;
			bool						EvaluateModelName(const CAmbientAudioConditions& rConditions, const CPed& rPed) const;
			bool						EvaluateNoRepeat(float fMaxTime, const CAmbientAudioExchange& rExchange, const CPed& rPed, const CPed& rTarget) const;
			bool						EvaluatePedPersonality(const CAmbientAudioConditions& rConditions, const CPed& rPed) const;
			bool						EvaluatePedType(const CAmbientAudioConditions& rConditions, const CPed& rPed) const;
			bool						EvaluateTrigger(const CAmbientAudioTrigger& rTrigger, const CPed& rPed, const CPed& rTarget, const Options& rOptions) const;
	const	CAmbientAudioExchange*		GetExchangeFromTrigger(const CAmbientAudioTrigger& rTrigger, const CPed& rPed, const CPed& rTarget, const Options& rOptions) const;
	const	CAmbientAudioExchange*		GetExchangeFromTriggers(const CPed& rPed, const CPed& rTarget, const Options& rOptions) const;
	const	CAmbientAudioExchange*		GetExchangeFromTriggersForSet(atHashWithStringNotFinal hSet, const CPed& rPed, const CPed& rTarget, const Options& rOptions) const;
	const	CAmbientAudioExchange*		GetProcessedExchange(atHashWithStringNotFinal hExchange, const CPed& rPed, const CPed& rTarget) const;
	const	CAmbientAudioExchange*		GetProcessedExchangeFromTrigger(const CAmbientAudioTrigger& rTrigger, const CPed& rPed, const CPed& rTarget, const Options& rOptions) const;
			atHashWithStringNotFinal	GetSet(const CPed& rPed) const;
			bool						HasPedAchievedCombatVictory(const CPed& rPed, const CPed& rTarget) const;
			bool						IsAudioFriendly(const CAmbientAudio& rAudio) const;
			bool						IsExchangeFriendly(const CAmbientAudioExchange& rExchange) const;
			bool						IsPedDead(const CPed& rPed) const;
			bool						IsPedExercising(const CPed& rPed) const;
			bool						IsPedFat(const CPed& rPed) const;
			bool						IsPedFleeing(const CPed& rPed) const;
			bool						IsPedHot(const CPed& rPed) const;
			bool						IsPedInCombat(const CPed& rPed) const;
			bool						IsPedInGangTerritory(const CPed& rPed) const;
			bool						IsPedKnockedDown(const CPed& rPed) const;
			bool						IsPedMale(const CPed& rPed) const;
			bool						IsPedOnFoot(const CPed& rPed) const;
			bool						IsPedUsingScenario(const CPed& rPed) const;
			void						ProcessConversation(CAmbientAudio& rAudio, const CPed& rPed, const CPed& rTarget) const;
	const	CAmbientAudioExchange*		ProcessExchange(const CAmbientAudioExchange& rExchange, const CPed& rPed, const CPed& rTarget) const;
			bool						TalkedToPedLast(const CPed& rPed, const CPed& rTarget) const;
			bool						WillPedJeerAtHotPeds(const CPed& rPed) const;
			bool						IsFriendFollowedByPlayer(const CPed& rPed) const;
private:

	CAmbientAudios m_AmbientAudios;

private:

	static CAmbientAudioManager* sm_Instance;

};

#endif // INC_AMBIENT_AUDIO_H
