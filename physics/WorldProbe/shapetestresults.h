#ifndef SHAPETEST_RESULTS_H
#define SHAPETEST_RESULTS_H

// RAGE headers:

// Game headers:
#include "physics/physics.h"
#include "physics/WorldProbe/shapetestmgr.h"

// Turn this on to not allocate intersections until a shapetest is created with the result structure
#define DEFER_RESULT_ALLOCATION 1

namespace WorldProbe
{

	////////////////////////////////////////////////////
	// Class to wrap an individual intersection result.
	////////////////////////////////////////////////////
	class CShapeTestHitPoint : public phIntersection
	{
	public:
		bool GetHitDetected() const
		{
			return IsAHit();
		}
		const Vector3& GetHitPosition() const
		{
			return RCC_VECTOR3(GetPosition());
		}
		const Vec3V_Out GetHitPositionV() const
		{
			return GetPosition();
		}
		const Vector3& GetHitNormal() const
		{
			return RCC_VECTOR3(GetNormal());
		}
		const Vec3V_Out GetHitNormalV() const
		{
			return GetNormal();
		}
		const Vector3& GetHitPolyNormal() const
		{
			return RCC_VECTOR3(GetIntersectedPolyNormal());
		}
		const Vec3V_Out GetHitPolyNormalV() const
		{
			return GetIntersectedPolyNormal();
		}
		// This function checks that the phInst pointer is still valid. If not, it returns NULL. To just get
		// access to the raw pointer, regardless of safety, use GetHitInstUnchecked().
		phInst* GetHitInst()
		{
			phInst* pInst = NULL;
			// Check that the object we hit is still in the physics level. Otherwise, just return a NULL ptr.
			const u16 nLevelIndex = GetLevelIndex();
			if(nLevelIndex != phInst::INVALID_INDEX)
			{
				pInst = GetInstance();
				physicsAssert(!pInst || nLevelIndex == pInst->GetLevelIndex());
				// If we have a stored physics inst ptr but it is out of date (e.g. async result) we return NULL from this function.
				if(pInst && CPhysics::GetLevel()->GetGenerationID(pInst->GetLevelIndex())!=GetGenerationID())
				{
					return NULL;
				}
			}
			return pInst;
		}
		// This function checks that the phInst pointer is still valid. If not, it returns NULL. To just get
		// access to the raw pointer, regardless of safety, use GetHitInstUnchecked().
		const phInst* GetHitInst() const
		{
			const phInst* pInst = NULL;
			// Check that the object we hit is still in the physics level. Otherwise, just return a NULL ptr.
			const u16 nLevelIndex = GetLevelIndex();
			if(nLevelIndex != phInst::INVALID_INDEX)
			{
				pInst = GetInstance();
				physicsAssert(!pInst || nLevelIndex == pInst->GetLevelIndex());
				// If we have a stored physics inst ptr but it is out of date (e.g. async result) we return NULL from this function.
				if(pInst && CPhysics::GetLevel()->GetGenerationID(pInst->GetLevelIndex())!=GetGenerationID())
				{
					return NULL;
				}
			}
			return pInst;
		}
		const phInst* GetHitInstUnchecked() const
		{
			// This is really only useful when the phInst pointer on the intersection has
			// been manually set to some magic value as in the virtual road tests.
			return GetInstance();
		}
		// Return the CEntity which corresponds to the hit phInst. This function performs checks to make sure that
		// the physics instance is valid and will return a NULL entity ptr if it isn't. Useful when dealing
		// with asynchronous tests where the instance could have been deleted since we stored the level index.
		CEntity* GetHitEntity()
		{
			CEntity* pEntity = NULL;
			if(CPhysics::GetLevel()->LegitLevelIndex(m_LevelIndex) && !CPhysics::GetLevel()->IsNonexistent(m_LevelIndex) && CPhysics::GetLevel()->GetInstance(m_LevelIndex))
			{
				// We must have a valid physics instance if we are here. Now we need to check that it's the one we want.
				phInst* pInst = CPhysics::GetLevel()->GetInstance(m_LevelIndex);
				if(pInst && pInst->GetLevelIndex() == m_LevelIndex && CPhysics::GetLevel()->GetGenerationID(pInst->GetLevelIndex()) == m_GenerationID)
				{
					pEntity = CPhysics::GetEntityFromInst(pInst);
				}
			}

			return pEntity;
		}
		// Return the CEntity which corresponds to the hit phInst. This function performs checks to make sure that
		// the physics instance is valid and will return a NULL entity ptr if it isn't. Useful when dealing
		// with asynchronous tests where the instance could have been deleted since we stored the level index.
		const CEntity* GetHitEntity() const
		{
			const CEntity* pEntity = NULL;
			if(CPhysics::GetLevel()->LegitLevelIndex(m_LevelIndex) && !CPhysics::GetLevel()->IsNonexistent(m_LevelIndex) && CPhysics::GetLevel()->GetInstance(m_LevelIndex))
			{
				// We must have a valid physics instance if we are here. Now we need to check that it's the one we want.
				phInst* pInst = CPhysics::GetLevel()->GetInstance(m_LevelIndex);
				physicsAssert(pInst);
				if(pInst && pInst->GetLevelIndex() == m_LevelIndex && CPhysics::GetLevel()->GetGenerationID(pInst->GetLevelIndex()) == m_GenerationID)
				{
					pEntity = CPhysics::GetEntityFromInst(pInst);
				}
			}

			return pEntity;
		}
		phMaterialMgr::Id GetHitMaterialId() const
		{
			return GetMaterialId();
		}
		u16 GetHitComponent() const
		{
			return GetComponent();
		}
		float GetHitTValue() const
		{
			return GetT();
		}
		float GetHitDepth() const
		{
			return GetDepth();
		}
		ScalarV_Out GetHitDepthV() const
		{
			return GetDepthV();
		}
		u16 GetHitPartIndex() const
		{
			return GetPartIndex();
		}
		// Allow modification of result:
		void SetHitPosition(const Vector3& vHitPos)
		{
			SetPosition(RCC_VEC3V(vHitPos));
		}
		void SetHitPositionV(Vec3V_In vHitPos)
		{
			SetPosition(vHitPos);
		}
		void SetHitNormal(const Vector3& vHitNormal)
		{
			SetNormal(RCC_VEC3V(vHitNormal));
		}
		void SetHitNormalV(Vec3V_In vHitNormal)
		{
			SetNormal(vHitNormal);
		}
		void SetHitInst(u16 nLevelIndex, u16 nGenerationID)
		{
			SetInstance(nLevelIndex, nGenerationID);
		}
		void SetHitMaterialId(phMaterialMgr::Id materialId)
		{
			SetMaterialId(materialId);
		}
		void SetHitComponent(u16 nComponent)
		{
			SetComponent(nComponent);
		}
		void SetHitTValue(float fTValue)
		{
			SetT(fTValue);
		}
		void SetHitDepth(float fDepth)
		{
			SetDepth(fDepth);
		}
		void SetHitPartIndex(u16 nPartIndex)
		{
			SetPartIndex(nPartIndex);
		}
		void SetHitLevelIndex(u16 nLevelIndex)
		{
			m_LevelIndex = nLevelIndex;
		}
		void SetHitGenerationId(u16 nGenerationId)
		{
			m_GenerationID = nGenerationId;
		}
	};

	///////////////////////////////////////////////////////////////////////
	// Class to encapsulate the intersection results from the shape tests.
	///////////////////////////////////////////////////////////////////////
	class CShapeTestResults
	{
	public:

		CShapeTestResults(u8 nNumResults=MAX_SHAPE_TEST_INTERSECTIONS)
			: m_nNumResults(nNumResults),
			m_nNumHits(0),
			m_ResultsStatus(TEST_NOT_SUBMITTED),
			m_pResults(NULL),
			m_ownResults(false)
		{
			physicsAssertf(m_nNumResults > 0, "Trying to create a shape test result structure with zero size.");
#if !DEFER_RESULT_ALLOCATION
			AllocateIntersections();
#endif // !DEFER_RESULT_ALLOCATION
		}

		CShapeTestResults(CShapeTestHitPoint& result)
			: m_nNumResults(1),
			m_nNumHits(0),
			m_ResultsStatus(TEST_NOT_SUBMITTED),
			m_pResults(&result),
			m_ownResults(false)
		{}

		CShapeTestResults(CShapeTestHitPoint* pResults, u8 size)
			: m_nNumResults(size),
			m_nNumHits(0),
			m_ResultsStatus(TEST_NOT_SUBMITTED),
			m_pResults(pResults),
			m_ownResults(false)
		{}

		~CShapeTestResults()
		{
			//The CShapeTestManager currently owns the results, so don't delete them
			if( GetWaitingOnResults() )
			{
				WorldProbe::GetShapeTestManager()->AbortTest(this);
			}

			//The CShapeTestManager may have taken full ownership of the results when doing an AbortTest and set our m_pIsects to NULL
			if(m_ownResults)
			{
				FreeIntersections();
			}
		}

		// Clear any previous intersection data.
		void Reset()
		{
			//The CShapeTestManager currently owns the results, so don't delete them
			if( GetWaitingOnResults() )
			{
				WorldProbe::GetShapeTestManager()->AbortTest(this);
			}

			m_nNumHits = 0;
			m_ResultsStatus = TEST_NOT_SUBMITTED;

			// Do a fast clear of the data (this stops the cache from loading before we write)
			if(m_pResults)
			{
				sysMemSet(m_pResults, 0, sizeof(CShapeTestHitPoint) * m_nNumResults);
				for(u32 i = 0; i < m_nNumResults; ++i)
				{
					m_pResults[i].Reset();
				}
			}
		}

		eResultsState GetResultsStatus() const
		{ 
			return m_ResultsStatus;
		}

		bool GetResultsReady() const
		{ 
			return GetResultsStatus() == RESULTS_READY;
		}

		bool GetWaitingOnResults() const
		{ 
			return GetResultsStatus() == WAITING_ON_RESULTS;
		}

		//Conceptually a non-NULL handle
		bool GetResultsWaitingOrReady() const
		{
			return m_ResultsStatus == WAITING_ON_RESULTS || m_ResultsStatus == RESULTS_READY; //Order of comparisons is important
		}

		// Return the number of CShapeTestHitPoint objects accessed through this object.
		u32 GetSize() const { return m_nNumResults; }

		u32 GetNumHits() const
		{
			physicsAssertf(GetResultsReady(), "Make sure results are ready using CShapeTestResults::GetResultsReady().");
			return m_nNumHits;
		}		

		bool OwnsResults() { return m_ownResults; }
		bool HasResultsAllocated() const { return m_pResults != NULL; }

		// If any individual result/intersection was directly set, allow updating the container's hit count, etc.
		// NB - This may not be necessary once the whole code base has been converted to the new API.
		void Update()
		{
			// I don't know why would you would ever want to use this function while the test is in progress, but if you do,
			//   changing m_eResultsStatus without aborting the test will potentially crash the game. 
			TrapNE(GetWaitingOnResults(),false);
			physicsAssertf(HasResultsAllocated(),"Trying to update a shapetest results structure without performing a shapetest.");
			m_nNumHits = 0;
			for(u32 i = 0; i < m_nNumResults; ++i)
			{
				if(m_pResults[i].GetHitDetected())
				{
					++m_nNumHits;
					m_ResultsStatus = RESULTS_READY;
				}
			}
		}

		// Callback for sorting, passed to rage::qsort().
		typedef s32 (*SortFunctionCB)(const WorldProbe::CShapeTestHitPoint* a, const WorldProbe::CShapeTestHitPoint* b);

		// Sort the results and intersections according to the user's criterion - defined by sortFn().
		void SortHits(SortFunctionCB sortFn)
		{
			physicsAssertf(HasResultsAllocated(),"Trying to sort a shapetest results structure without performing a shapetest.");
			if(m_nNumHits > 0)
			{
				qsort(m_pResults, m_nNumHits, sizeof(WorldProbe::CShapeTestHitPoint), (int (/*__cdecl*/ *)(const void*, const void*))sortFn);
			}
		}

		bool IsFull() const	{ return m_nNumHits == m_nNumResults; }
		bool IsEmpty() const { return m_nNumHits == 0; }

		void Push(const WorldProbe::CShapeTestHitPoint& hitPoint)
		{
			FastAssert(!IsFull());
			FastAssert(m_pResults);
			m_pResults[m_nNumHits++] = hitPoint;
		}

		// Split this results structure up into two separate structures
		// If the functor returns true, then we add to a, if it returns false, we add to b
		template <typename SplitFunctor>
		void SplitHits(const SplitFunctor& splitFunctor, CShapeTestResults& a, CShapeTestResults& b) const
		{
			physicsAssert(m_pResults);
			physicsAssert(a.m_pResults);
			physicsAssert(b.m_pResults);
			a.Reset();
			b.Reset();
			for(int intersectionIndex = 0; intersectionIndex < m_nNumHits; ++intersectionIndex)
			{
				if(splitFunctor(m_pResults[intersectionIndex], intersectionIndex))
				{
					if(!a.IsFull())
					{
						a.Push(m_pResults[intersectionIndex]);
					}
				}
				else
				{
					if(!b.IsFull())
					{
						b.Push(m_pResults[intersectionIndex]);
					}
				}
			}
			a.m_ResultsStatus = RESULTS_READY;
			b.m_ResultsStatus = RESULTS_READY;
		}

#if 0 // __BANK
		// TODO: Add toggle to choose between using colour to debug asynchrnous test status and telling synchronous/asynchronous tests apart.
		static rage::Color32 GetDebugDrawColour(eResultsState resultsStatus)
		{
			switch (resultsStatus)
			{
			case TEST_NOT_SUBMITTED:
				return Color_black;
			case TEST_ABORTED:
			case SUBMISSION_FAILED:
			default:
				return Color_red;
			case WAITING_ON_RESULTS:
				return Color_grey;
			case RESULTS_READY:
				return Color_yellow;
			}
		}
#endif

		// Define and implement an iterator for the results.
		class Iterator
		{
		public:
			Iterator() : m_pResultsContainer(NULL), m_nPos(0) {}
			Iterator(CShapeTestResults* pResults, size_t startPos)
				: m_pResultsContainer(pResults),
				m_nPos(startPos)
			{
				physicsAssertf(pResults->HasResultsAllocated(),"Trying to iterate over a shapetest results structure without performing a shapetest.");
			}

			// Overload the operators which actually make this behave as a smart pointer/iterator.
			Iterator& operator=(const Iterator& otherIterator)
			{
				// Avoid self-assignment.
				if(this != &otherIterator)
				{
					m_pResultsContainer = otherIterator.m_pResultsContainer;
					m_nPos = otherIterator.m_nPos;
				}
				return *this;
			}
			Iterator& operator+=(const u32 nIncrement)
			{
				m_nPos += (size_t)nIncrement;
				return *this;
			}
			Iterator operator+(const u32 nIncrement)
			{
				return Iterator(*this) += nIncrement;
			}
			CShapeTestHitPoint& operator*() const
			{
				return m_pResultsContainer->m_pResults[m_nPos];
			}
			CShapeTestHitPoint* operator->() const { return &(operator*()); }
			CShapeTestHitPoint& operator[](u32 nPos) const
			{
				physicsAssert(nPos < m_pResultsContainer->m_nNumResults);
				return m_pResultsContainer->m_pResults[nPos];
			}
			bool operator<(const Iterator& otherIterator) const { return (m_nPos < otherIterator.m_nPos); }
			bool operator<=(const Iterator& otherIterator) const { return (m_nPos <= otherIterator.m_nPos); }
			bool operator==(const Iterator& otherIterator) const { return (m_nPos == otherIterator.m_nPos); }
			bool operator!=(const Iterator& otherIterator) const { return (m_nPos != otherIterator.m_nPos); }
			bool operator>=(const Iterator& otherIterator) const { return (m_nPos >= otherIterator.m_nPos); }
			bool operator>(const Iterator& otherIterator) const { return (m_nPos > otherIterator.m_nPos); }
			void operator++() { ++m_nPos; } // Prefix version.
			void operator++(int) { operator++(); } // Postfix version.

		private:
			CShapeTestResults* m_pResultsContainer;
			size_t m_nPos;
		};
		Iterator begin() { return Iterator(this, 0); }
		Iterator last_result() { return Iterator(this, m_nNumHits); }
		Iterator end() { return Iterator(this, m_nNumResults); }
		const CShapeTestHitPoint& operator[](u32 nPos) const
		{
			physicsAssertf(HasResultsAllocated(),"Trying to access a shapetest results structure without performing a shapetest.");
			physicsAssert(nPos < m_nNumResults);
			return m_pResults[nPos];
		}
		CShapeTestHitPoint& operator[](u32 nPos)
		{
			physicsAssertf(HasResultsAllocated(),"Trying to access a shapetest results structure without performing a shapetest.");
			physicsAssert(nPos < m_nNumResults);
			return m_pResults[nPos];
		}

		void FreeIntersections()
		{
			// Ensure that any async test using these results gets aborted safely and that we no longer claim to have ready results
			Reset();

			if(m_pResults)
			{
				physicsAssertf(m_ownResults,"CShapeTestResults cannot free allocations that it doesn't own.");
				sysMemStartTemp();		
				{
					USE_MEMBUCKET(MEMBUCKET_PHYSICS);
					delete [] m_pResults;
					m_pResults = NULL;
				}
				sysMemEndTemp();
				m_ownResults = false;
			}
		}

	private:
		// We need to protect m_ResultsStatus from changing during async shapetests
		CShapeTestResults& operator=(const CShapeTestResults& rhs);

		//Called by AbortTest if it takes ownership of the phIntersections
		void AllocateIntersections()
		{
			physicsAssertf(m_pResults == NULL, "Trying to allocate intersections when some already exist.");
			physicsAssertf(m_nNumResults > 0,"Trying to allocate an invalid number of intersections.");
			sysMemStartTemp();
			{
				USE_MEMBUCKET(MEMBUCKET_PHYSICS);

#if RSG_XENON || RSG_PS3 // align to cache line
				m_pResults = rage_aligned_new(128) CShapeTestHitPoint[m_nNumResults];
#else
				m_pResults = rage_new CShapeTestHitPoint[m_nNumResults];
#endif
			}
			sysMemEndTemp();
			m_ownResults = true;
		}

		// Number of CShapeTestHitPoint objects accessed through this object.
		const u8 m_nNumResults;
		u8 m_nNumHits;
		bool m_ownResults;
		eResultsState m_ResultsStatus;

		CShapeTestHitPoint* m_pResults;

		friend class CShapeTestManager;
		friend class CShapeTestTaskResultsDescriptor;
		friend class CShapeTestDesc;
		friend class CShapeTestTaskData;
		friend class CShapeTest;
	};

	// Convenience wrapper:
	typedef CShapeTestResults::Iterator ResultIterator;


	class CShapeTestSingleResult : public CShapeTestResults
	{
	public:
		CShapeTestSingleResult() : CShapeTestResults(1) {}
	};

	template <u8 _MaxNumIntersections = MAX_SHAPE_TEST_INTERSECTIONS>
	class CShapeTestFixedResults : public CShapeTestResults
	{
	public:
		CShapeTestFixedResults() : CShapeTestResults(m_Intersections,_MaxNumIntersections)
		{}
	private:
		CShapeTestHitPoint m_Intersections[_MaxNumIntersections];
	};

} // namespace WorldProbe

// Ensure CShapeTestHitPoint is the same size as phIntersection
CompileTimeAssert(sizeof(WorldProbe::CShapeTestHitPoint) == sizeof(phIntersection));

#endif // SHAPETEST_RESULTS_H
