#ifndef SHAPETEST_DESC_H
#define SHAPETEST_DESC_H

// RAGE headers:

// Game headers:
#include "physics/WorldProbe/shapetestresults.h"

#define EXCLUDE_ENTITY_OPTIONS_NONE 0
#define INCLUDE_ENTITY_OPTIONS_NONE 0

namespace WorldProbe
{

	///////////////////////////////////////////////////////////////
	// Base descriptor class for the various types of shape tests.
	///////////////////////////////////////////////////////////////
	class CShapeTestDesc
	{
	protected:

		CShapeTestDesc()
			: 	m_eShapeTestType(INVALID_TEST_TYPE),
			m_eShapeTestDomain(TEST_IN_LEVEL),
			m_pResults(NULL),
			m_pIsects(NULL),
			m_nOptions(0),
			m_nIncludeFlags(INCLUDE_FLAGS_ALL),
			m_nTypeFlags(ArchetypeFlags::GTA_ALL_SHAPETEST_TYPES),
			m_nStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL),
			m_nNumIntersections(0),
			m_nFirstFreeIntersectionIndex(0),
			m_nExcludeTypeFlags(TYPE_FLAGS_NONE),
			m_eContext(LOS_Unspecified),
			m_bTreatPolyhedralBoundsAsPrimitives(false)
#if WORLD_PROBE_DEBUG
			,
			m_sOriginatingFile("unknown"),
			m_nOriginatingLine(0)
#endif // WORLD_PROBE_DEBUG
		{
		}
		virtual ~CShapeTestDesc()
		{
		}

	public:

		void Reset()
		{
			m_aExcludeInstList.Reset();
		}

		// Enum used to identify what type of shape test this is.
		eShapeTestType GetTestType() const { return m_eShapeTestType; }

		void SetResultsStructure(CShapeTestResults* pResults)
		{
			m_pResults = pResults;
			if(m_pResults)
			{
				if(!Verifyf(pResults->GetResultsStatus() != WAITING_ON_RESULTS,"Attempting to re-use results already in use by an async shapetest"))
				{
					pResults->Reset();
				}
				m_nNumIntersections = pResults->m_nNumResults;
				if(!pResults->HasResultsAllocated())
				{
					// Now that we actually need the intersections, allocate them if they don't already exist. 
					pResults->AllocateIntersections();
				}
				m_pIsects = m_pResults->m_pResults;
			}
			else
			{
				m_nNumIntersections = 0;
				m_pIsects = NULL;
			}
		}
		void SetFirstFreeResultOffset(u32 nFirstFreeIndex)
		{
			if(physicsVerifyf(m_pResults, "Don't call SetFirstFreeResultOffset() when there is no valid result structure set."))
			{
				if(physicsVerifyf(nFirstFreeIndex < m_pResults->m_nNumResults,
					"Trying to set first free result index to %u when size of result array is only %u.", nFirstFreeIndex, m_pResults->m_nNumResults))
				{
					m_nFirstFreeIntersectionIndex = nFirstFreeIndex;
				}
			}
		}
		void SetMaxNumResultsToUse(u32 nMaxNumResults)
		{
			if(physicsVerifyf(m_pResults, "Don't call SetMaxNumResultsToUse() when there is no valid result structure set.") && 
				physicsVerifyf(nMaxNumResults <= m_pResults->GetSize(), "Trying to use more results (%i) than the result structure owns (%i).",nMaxNumResults,m_pResults->GetSize()))
			{
				m_nNumIntersections = nMaxNumResults;
			}
		}
		// Allow using the same results structure in subsequent shape tests.
		CShapeTestResults* GetResultsStructure() const
		{
			return m_pResults;
		}

		// This is a bitfield taken from a combination of the eLineOfSightOptions enumeration, which
		// defines special properties of this shape test (e.g. whether it passes through transparent materials).
		void SetOptions(const int nOptions)
		{
			m_nOptions=nOptions;
		}
		int GetOptions() const
		{
			return m_nOptions;
		}
		void SetDomainForTest(const eTestDomain nTestDomain)
		{
			m_eShapeTestDomain=nTestDomain;
		}

		// If this shape test is against individual objects (set with the "domain for test" variable above), then
		// the objects can be set using the following functions:
		void SetIncludeInstance(const phInst* const pIncludeInstance);
		void SetIncludeInstances(const phInst* const *ppIncludeInsts, const int nNumIncludeInsts);
		void SetIncludeEntity(const CEntity* const pIncludeEnt, const u32 nOptions=INCLUDE_ENTITY_OPTIONS_NONE);
		void SetIncludeEntities(const CEntity** const ppIncludeEnts, const int nNumIncludeEnts, const u32 nOptions=INCLUDE_ENTITY_OPTIONS_NONE);

		void AddExludeInstance(const phInst* const pExcludeInstance);
		void SetExcludeInstance(const phInst* const pExcludeInstance);
		void SetExcludeInstances(const phInst* const *ppExcludeInsts, const int nNumExcludeInsts);
		// Helper function to make it more convenient for game side shape tests to specify
		// physical objects to exclude from the test.
		void SetExcludeEntity(const CEntity* const pExcludeEnt, const u32 nOptions=EXCLUDE_ENTITY_OPTIONS_NONE);
		void AddExcludeEntity(const CEntity* const pExcludeEnt, const u32 nOptions=EXCLUDE_ENTITY_OPTIONS_NONE);
		// Like the above function but allow multiple entities to be specified.
		void SetExcludeEntities(const CEntity** const ppExcludeEnts, const int nNumExcludeEnts, const u32 nOptions=EXCLUDE_ENTITY_OPTIONS_NONE);

		// These flags are to be found in gtaArchetype.h (e.g. ArchetypeFlags::GTA_ALL_MAP_TYPES).
		// They define which entity types this probe will collide with.
		void SetIncludeFlags(const u32 nIncludeFlags)
		{
			m_nIncludeFlags=nIncludeFlags;
		}
		u32 GetIncludeFlags()  const
		{
			return m_nIncludeFlags;
		}
		// Define the type of test we are doing using flags like ArchetypeFlags::GTA_AI_TEST in gtaArchetype.h. Similar
		// to "context" but this actually gets passed on to the RAGE shape tests.
		void SetTypeFlags(const u32 nTypeFlags)
		{
			m_nTypeFlags=nTypeFlags;
		}
		u32 GetTypeFlags() const
		{
			return m_nTypeFlags;
		}

		// Choose whether to test against objects which are active, inactive and fixed in the physics simulator.
		void SetStateIncludeFlags(const u8 nStateIncludeFlags)
		{
			m_nStateIncludeFlags=nStateIncludeFlags;
		}
		void SetExcludeTypeFlags(const u32 nExcludeTypeFlags)
		{
			m_nExcludeTypeFlags=nExcludeTypeFlags;
		}

		// Context is an enumerated type defining which game system issued this probe, and is
		// primarily to aid in debugging / performance analysis.
		void SetContext(const ELosContext eContext)
		{
			m_eContext=eContext;
		}
		ELosContext GetContext() const
		{
			return m_eContext;
		}

		void SetTreatPolyhedralBoundsAsPrimitives(bool treatPolyhedralBoundsAsPrimitives) { m_bTreatPolyhedralBoundsAsPrimitives = treatPolyhedralBoundsAsPrimitives; }
		bool GetTreatPolyhedralBoundsAsPrimitives() const { return m_bTreatPolyhedralBoundsAsPrimitives; }

	protected:
		// Helper functions
		static void AddInstanceToList(InExInstanceArray& instances, const phInst* pInstance);
		static void AddWeaponAndComponents(InExInstanceArray& instances, const CPed* pPed);
		static void AddAttachedEntities(InExInstanceArray& instances, const CPed* pPed);
		static void AddEntityToInstanceList(InExInstanceArray& instances, const CEntity* pEntity, u32 options);
		static void SetInstanceListFromEntities(InExInstanceArray& instances, const CEntity* const * ppEntities, int numEntities, u32 options);
		static void SetInstanceList(InExInstanceArray& instances, const phInst* const * ppInstances, int numInstances);

		eShapeTestType m_eShapeTestType;
		eTestDomain m_eShapeTestDomain;
		CShapeTestResults* m_pResults;
		CShapeTestHitPoint* m_pIsects;
		InExInstanceArray m_aExcludeInstList;
		InExInstanceArray m_aIncludeInstList;
		int m_nOptions;
		u32 m_nIncludeFlags;
		u32 m_nTypeFlags;
		u8 m_nStateIncludeFlags;
		u32 m_nNumIntersections;
		u32 m_nFirstFreeIntersectionIndex;
		u32 m_nExcludeTypeFlags;
		ELosContext m_eContext;
		bool m_bTreatPolyhedralBoundsAsPrimitives;
#if WORLD_PROBE_DEBUG
		// For debugging, store the file and line number of the originating request.
		const char *m_sOriginatingFile;
		u32 m_nOriginatingLine;
#endif // WORLD_PROBE_DEBUG

		friend class CShapeTestManager;
		friend class CShapeTestTaskData;
		friend class CShapeTest;
	};

} // namespace WorldProbe

#endif // SHAPETEST_DESC_H
