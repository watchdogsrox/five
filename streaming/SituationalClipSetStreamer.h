#ifndef INC_SITUATIONAL_CLIP_SET_STREAMER
#define INC_SITUATIONAL_CLIP_SET_STREAMER

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"

// Game headers
#include "task/System/TaskHelpers.h"
#include "task/System/Tuning.h"

// Game forward declarations
class CPed;

////////////////////////////////////////////////////////////////////////////////
// CSituationalClipSetStreamer
////////////////////////////////////////////////////////////////////////////////

class CSituationalClipSetStreamer
{

private:

	enum RunningFlags
	{
		RF_HasRequestedParachutePackVariation = BIT0,
	};

public:

	struct Tunables : CTuning
	{
		struct Avoids
		{
			struct Variation
			{
				Variation()
				{}

				atHashWithStringNotFinal	m_ClipSet;
				bool						m_IsCasual;
				float						m_Chances;

				PAR_SIMPLE_PARSABLE;
			};

			Avoids()
			{}

			atHashWithStringNotFinal m_ClipSet;
			atHashWithStringNotFinal m_ClipSetForCasual;

			atArray<Variation> m_Variations;

			PAR_SIMPLE_PARSABLE;
		};

		struct FleeReactions
		{
			FleeReactions()
			{}

			atHashWithStringNotFinal	m_ClipSetForIntro;
			atHashWithStringNotFinal	m_ClipSetForIntroV1;
			atHashWithStringNotFinal	m_ClipSetForIntroV2;
			atHashWithStringNotFinal	m_ClipSetForRuns;
			atHashWithStringNotFinal	m_ClipSetForRunsV1;
			atHashWithStringNotFinal	m_ClipSetForRunsV2;
			float						m_MinTimeInCombatToNotStreamIn;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		Avoids			m_Avoids;
		FleeReactions	m_FleeReactions;

		PAR_PARSABLE;
	};

public:

	static CSituationalClipSetStreamer& GetInstance() { FastAssert(sm_Instance); return *sm_Instance; }

private:

	CSituationalClipSetStreamer();
	~CSituationalClipSetStreamer();

public:

	fwMvClipSetId GetClipSetForAvoids(bool bCasual) const;
	
public:

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);
	static void Update();

private:

	bool HasBeenInCombatForTime(float fTime) const;
	bool IsInCombat() const;
	bool IsPedArmedWithGun(const CPed& rPed) const;
	bool IsPlayerArmedWithGun() const;
	bool IsPlayerOnFoot() const;
	bool ShouldStreamInClipSetForFleeReactions() const;
	bool ShouldStreamInClipSetsForCasualAvoids() const;
	void Update(float fTimeStep);
	void UpdateClipSetForFleeReactions();
	void UpdateClipSets();
	void UpdateClipSetsForAvoids();
	void UpdateParachutePackVariation();
	void UpdatePedVariations();

private:

	atArray<CPrioritizedClipSetRequestHelper>	m_ClipSetsForAvoidVariations;
	CPrioritizedClipSetRequestHelper			m_ClipSetRequestHelperForFleeReactionsIntro;
	CPrioritizedClipSetRequestHelper			m_ClipSetRequestHelperForFleeReactionsIntroV1;
	CPrioritizedClipSetRequestHelper			m_ClipSetRequestHelperForFleeReactionsIntroV2;
	CPrioritizedClipSetRequestHelper			m_ClipSetRequestHelperForFleeReactionsRuns;
	CPrioritizedClipSetRequestHelper			m_ClipSetRequestHelperForFleeReactionsRunsV1;
	CPrioritizedClipSetRequestHelper			m_ClipSetRequestHelperForFleeReactionsRunsV2;
	CPrioritizedClipSetRequestHelper			m_ClipSetRequestHelperForCasualAvoids;
	u32											m_uTimeToReleaseParachutePackVariation;
	fwFlags8									m_uRunningFlags;
	
private:

	static CSituationalClipSetStreamer* sm_Instance;

private:

	static Tunables sm_Tunables;
	
};

#endif // INC_SITUATIONAL_CLIP_SET_STREAMER
