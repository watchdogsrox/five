//
// name:		DebugScene
// description:	Debug functions for the scene
#ifndef INC_DEBUG_SCENE_H_
#define INC_DEBUG_SCENE_H_

// Rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "input/mouse.h"
#include "scene/regdreftypes.h"
#include "string/stringhash.h"
#include "vector/vector3.h"
#include "physics/inst.h"
#include "phbound/bound.h"
#include "debug/UiGadget/UiGadgetList.h"

class CEntity;
class CUiGadgetInspector;
class CAuthoringHelper; 

namespace rage
{
	class fwEntity;
	class fwRect;
	class phIntersection;
	class Color32;
	class spdAABB;
	class spdPlane; 
	
};

/////////////////////////////////////////////////////////////////////////////////
// Helper function for the TUNE_GROUP_ macros below...
/////////////////////////////////////////////////////////////////////////////////
bkGroup* GetOrCreateGroup(bkBank* pBankToSearchOrAddTo, const char* groupName);

enum
{
	TUNE_TYPE_INVALID = -1,
	TUNE_TYPE_BOOL,
	TUNE_TYPE_INT,
	TUNE_TYPE_FLOAT,
	TUNE_TYPE_COLOR,
};


/////////////////////////////////////////////////////////////////////////////////
// Debug break macro.
// Used to make it easier to create switched breaks on the focus entity under
// various conditions.
// It is used like:
//		void MyClass::ProcessControl()
//		{
//			DEV_BREAK_IF_FOCUS( MyClass::ms_bBreakOnProcessControlOfFocusEntity, this );
//			...
//		}
/////////////////////////////////////////////////////////////////////////////////
#if __DEV
	#define DEV_BREAK_IF_FOCUS( controlParam, entityToTestIfIsFocus )											\
		Assertf(!(controlParam) || CDebugScene::FocusEntities_Get(0) != entityToTestIfIsFocus, "Breaking on " #controlParam " for focus entity " #entityToTestIfIsFocus );	
#else // __DEV
	#define DEV_BREAK_IF_FOCUS( x, y )
#endif // __DEV



/////////////////////////////////////////////////////////////////////////////////
// Debug proximity break macro.
// Used to make it easier to create switched breaks on something happening close
// to a position.
// It is used like:
//		void MyClass::ProcessControl()
//		{
//			DEV_BREAK_ON_PROXIMITY( MyClass::ms_bBreakOnProximityOfThingToTestPoint, thing->GetPos() );
//			...
//		}
/////////////////////////////////////////////////////////////////////////////////
#if __DEV 
	#define DEV_BREAK_ON_PROXIMITY( controlParam, positionToTest )															\
		if( controlParam )																									\
		{																													\
			const Vector3 diffFromProximityTestPoint = CDebugScene::GetProximityTestPoint() - positionToTest;				\
			const float maxVerticalDiff = CDebugScene::GetProxmityTestAllowableVerticalDiff();								\
			if(Abs(diffFromProximityTestPoint.z) < maxVerticalDiff)															\
			{																												\
				Vector3 diffFromProximityTestPointFlat(diffFromProximityTestPoint.x, diffFromProximityTestPoint.y, 0.0f);	\
				const float radiusAway2 = diffFromProximityTestPointFlat.Mag2();											\
				const float maxRadialDiff = CDebugScene::GetProxmityTestAllowableRadialDiff();								\
				Assertf(radiusAway2 < (maxRadialDiff*maxRadialDiff), "Breaking on " #controlParam " for position " #positionToTest);	\
			}																												\
		}
#else // __DEV
#define DEV_BREAK_ON_PROXIMITY( x, y )
#endif // __DEV


/////////////////////////////////////////////////////////////////////////////////
// Tune bool macro
// Used to make it easier to create bank toggles while debugging.  These can
// just be placed right in the code there they are needed and they will
// auto-magically appear in the "_TUNE_" debug bank.
// It is used like:
//		void MyClass::ProcessControl()
//		{
//			TUNE_BOOL( foobar1, false );
//			if(foobar1)
//			{
//				...
//			}
//		}
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
#define REGISTER_TUNE_BOOL( toggleName, defaultBoolVal )						\
	static bool toggleName = defaultBoolVal;									\
	static u32 hasAddedBankItem_##toggleName = 0;

#define INSTANTIATE_TUNE_BOOL( toggleName )										\
	if(!hasAddedBankItem_##toggleName)											\
	{																			\
		bkBank* pBank = BANKMGR.FindBank("_TUNE_");								\
		if(pBank && sysInterlockedIncrement(&hasAddedBankItem_##toggleName) == 1)	\
		{																		\
			if(bkManager::IsBankThread())										\
			{																	\
				pBank->AddToggle(#toggleName, &toggleName);						\
			}																	\
			else																\
			{																	\
				CDebugScene::TuneWidgetData data(TUNE_TYPE_BOOL, NULL, #toggleName, &toggleName);	\
				if(!CDebugScene::QueueTuneWidget(data))							\
				{																\
					hasAddedBankItem_##toggleName = 0;							\
				}																\
			}																	\
		}																		\
	}
#else // __BANK
// If not a bank build make sure the var still exists, just for convenience.	
#define REGISTER_TUNE_BOOL( toggleName, defaultBoolVal )						\
	static const bool toggleName = defaultBoolVal;

#define INSTANTIATE_TUNE_BOOL( toggleName )										\
	(void)sizeof(toggleName);// Placate the compiler in case the var isn't used.
#endif // __BANK

#define TUNE_BOOL( toggleName, defaultBoolVal )									\
	REGISTER_TUNE_BOOL( toggleName, defaultBoolVal )							\
	INSTANTIATE_TUNE_BOOL( toggleName )

/////////////////////////////////////////////////////////////////////////////////
// Tune group bool macro
// Used to make it easier to create bank toggles while debugging.  These can
// just be placed right in the code there they are needed and they will
// auto-magically appear in the "_TUNE_" debug bank.
// It is used like:
//		void MyClass::ProcessControl()
//		{
//			TUNE_GROUP_BOOL( groupName, foobar1, false );
//			if(foobar1)
//			{
//				...
//			}
//		}
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
#define REGISTER_TUNE_GROUP_BOOL( toggleName, defaultBoolVal )					\
	static bool toggleName = defaultBoolVal;									\
	static u32 hasAddedBankItem_##toggleName = 0;

#define INSTANTIATE_TUNE_GROUP_BOOL( groupName, toggleName )					\
	if(!hasAddedBankItem_##toggleName)											\
	{																			\
		bkBank* pBank = BANKMGR.FindBank("_TUNE_");								\
		if(pBank && sysInterlockedIncrement(&hasAddedBankItem_##toggleName) == 1)	\
		{																		\
			if(bkManager::IsBankThread())										\
			{																	\
				bkGroup* pGroup = GetOrCreateGroup(pBank, #groupName);			\
				pBank->SetCurrentGroup(*pGroup);								\
				pBank->AddToggle(#toggleName, &toggleName);						\
				pBank->UnSetCurrentGroup(*pGroup);								\
			}																	\
			else																\
			{																	\
				CDebugScene::TuneWidgetData data(TUNE_TYPE_BOOL, #groupName, #toggleName, &toggleName);	\
				if(!CDebugScene::QueueTuneWidget(data))							\
				{																\
					hasAddedBankItem_##toggleName = 0;							\
				}																\
			}																	\
		}																		\
	}
#else // __BANK
// If not a bank build make sure the var still exists, just for convenience.	
#define REGISTER_TUNE_GROUP_BOOL( toggleName, defaultBoolVal )					\
	static const bool toggleName = defaultBoolVal;

#define INSTANTIATE_TUNE_GROUP_BOOL( groupName, toggleName )					\
	(void)sizeof(toggleName);// Placate the compiler in case the var isn't used.
#endif // __BANK

#define TUNE_GROUP_BOOL( groupName, toggleName, defaultBoolVal )				\
	REGISTER_TUNE_GROUP_BOOL( toggleName, defaultBoolVal )						\
	INSTANTIATE_TUNE_GROUP_BOOL( groupName, toggleName )

/////////////////////////////////////////////////////////////////////////////////
// Tune int macro
// Same as above but for int sliders instead of bool toggles.
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
#define REGISTER_TUNE_INT( sliderName, defaultIntVal )							\
	static int sliderName = defaultIntVal;										\
	static u32 hasAddedBankItem_##sliderName = 0;

#define INSTANTIATE_TUNE_INT( sliderName, min, max, delta )						\
	if(!hasAddedBankItem_##sliderName)											\
	{																			\
		bkBank* pBank = BANKMGR.FindBank("_TUNE_");								\
		if(pBank && sysInterlockedIncrement(&hasAddedBankItem_##sliderName) == 1)	\
		{																		\
			if(bkManager::IsBankThread())										\
			{																	\
				pBank->AddSlider(#sliderName, &sliderName, min, max, delta);	\
			}																	\
			else																\
			{																	\
				const s32 _min = min;											\
				const s32 _max = max;											\
				const float _delta = delta;										\
				CDebugScene::TuneWidgetData data(TUNE_TYPE_INT, NULL, #sliderName, &sliderName, &_min, &_max, &_delta);	\
				if(!CDebugScene::QueueTuneWidget(data))							\
				{																\
					hasAddedBankItem_##sliderName = 0;							\
				}																\
			}																	\
		}																		\
	}
#else // __BANK
// If not a bank build make sure the var still exists, just for convenience.
#define REGISTER_TUNE_INT( sliderName, defaultIntVal )							\
	static const int sliderName = defaultIntVal;

#define INSTANTIATE_TUNE_INT( sliderName, min, max, delta )						\
	(void)sizeof(sliderName);// Placate the compiler in case the var isn't used.
#endif // __BANK

#define TUNE_INT( sliderName, defaultIntVal, min, max, delta )					\
	REGISTER_TUNE_INT( sliderName, defaultIntVal )								\
	INSTANTIATE_TUNE_INT( sliderName, min, max, delta )

/////////////////////////////////////////////////////////////////////////////////
// Tune group int macro
// Same as above but for int sliders instead of bool toggles.
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
#define REGISTER_TUNE_GROUP_INT( sliderName, defaultIntVal )					\
	static int sliderName = defaultIntVal;										\
	static u32 hasAddedBankItem_##sliderName = 0;

#define INSTANTIATE_TUNE_GROUP_INT( groupName, sliderName, min, max, delta)		\
	if(!hasAddedBankItem_##sliderName)											\
	{																			\
		bkBank* pBank = BANKMGR.FindBank("_TUNE_");								\
		if(pBank && sysInterlockedIncrement(&hasAddedBankItem_##sliderName) == 1)	\
		{																		\
			if(bkManager::IsBankThread())										\
			{																	\
				bkGroup* pGroup = GetOrCreateGroup(pBank, #groupName);			\
				pBank->SetCurrentGroup(*pGroup);								\
				pBank->AddSlider(#sliderName, &sliderName, min, max, delta);	\
				pBank->UnSetCurrentGroup(*pGroup);								\
			}																	\
			else																\
			{																	\
				const s32 _min = min;											\
				const s32 _max = max;											\
				const float _delta = delta;										\
				CDebugScene::TuneWidgetData data(TUNE_TYPE_INT, #groupName, #sliderName, &sliderName, &_min, &_max, &_delta);	\
				if(!CDebugScene::QueueTuneWidget(data))							\
				{																\
					hasAddedBankItem_##sliderName = 0;							\
				}																\
			}																	\
		}																		\
	}
#else // __BANK
// If not a bank build make sure the var still exists, just for convenience.	
#define REGISTER_TUNE_GROUP_INT( sliderName, defaultIntVal )					\
	static const int sliderName = defaultIntVal;

#define INSTANTIATE_TUNE_GROUP_INT( groupName, sliderName, min, max, delta)		\
	(void)sizeof(sliderName);// Placate the compiler in case the var isn't used.
#endif // __BANK

#define TUNE_GROUP_INT( groupName, sliderName, defaultIntVal, min, max, delta ) \
	REGISTER_TUNE_GROUP_INT( sliderName, defaultIntVal )						\
	INSTANTIATE_TUNE_GROUP_INT( groupName, sliderName, min, max, delta)

/////////////////////////////////////////////////////////////////////////////////
// Tune float macro
// Same as above but for float sliders instead of bool toggles.
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
#define REGISTER_TUNE_FLOAT( sliderName, defaultFloatVal )						\
	static float sliderName = defaultFloatVal;									\
	static u32 hasAddedBankItem_##sliderName = 0;

#define INSTANTIATE_TUNE_FLOAT( sliderName, min, max, delta )					\
	if(!hasAddedBankItem_##sliderName)											\
	{																			\
		bkBank* pBank = BANKMGR.FindBank("_TUNE_");								\
		if(pBank && sysInterlockedIncrement(&hasAddedBankItem_##sliderName) == 1)	\
		{																		\
			if(bkManager::IsBankThread())										\
			{																	\
				pBank->AddSlider(#sliderName, &sliderName, min, max, delta);	\
			}																	\
			else																\
			{																	\
				const float _min = min;											\
				const float _max = max;											\
				const float _delta = delta;										\
				CDebugScene::TuneWidgetData data(TUNE_TYPE_FLOAT, NULL, #sliderName, &sliderName, &_min, &_max, &_delta);	\
				if(!CDebugScene::QueueTuneWidget(data))							\
				{																\
					hasAddedBankItem_##sliderName = 0;							\
				}																\
			}																	\
		}																		\
	}
#else // __BANK
// If not a bank build make sure the var still exists, just for convenience.	
#define REGISTER_TUNE_FLOAT( sliderName, defaultFloatVal )						\
	static const float sliderName = defaultFloatVal;

#define INSTANTIATE_TUNE_FLOAT( sliderName, min, max, delta )					\
	(void)sizeof(sliderName);// Placate the compiler in case the var isn't used.
#endif // __BANK

#define TUNE_FLOAT( sliderName, defaultFloatVal, min, max, delta )				\
	REGISTER_TUNE_FLOAT( sliderName, defaultFloatVal )							\
	INSTANTIATE_TUNE_FLOAT( sliderName, min, max, delta )

/////////////////////////////////////////////////////////////////////////////////
// Tune group float macro
// Same as above but for float sliders instead of bool toggles.
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
#define REGISTER_TUNE_GROUP_FLOAT( sliderName, defaultFloatVal )				\
	static float sliderName = defaultFloatVal;									\
	static u32 hasAddedBankItem_##sliderName = 0;

#define INSTANTIATE_TUNE_GROUP_FLOAT( groupName, sliderName, min, max, delta )	\
	if(!hasAddedBankItem_##sliderName)											\
	{																			\
		bkBank* pBank = BANKMGR.FindBank("_TUNE_");								\
		if(pBank && sysInterlockedIncrement(&hasAddedBankItem_##sliderName) == 1)	\
		{																		\
			if(bkManager::IsBankThread())										\
			{																	\
				bkGroup* pGroup = GetOrCreateGroup(pBank, #groupName);			\
				pBank->SetCurrentGroup(*pGroup);								\
				pBank->AddSlider(#sliderName, &sliderName, min, max, delta);	\
				pBank->UnSetCurrentGroup(*pGroup);								\
			}																	\
			else																\
			{																	\
				const float _min = min;											\
				const float _max = max;											\
				const float _delta = delta;										\
				CDebugScene::TuneWidgetData data(TUNE_TYPE_FLOAT, #groupName, #sliderName, &sliderName, &_min, &_max, &_delta);	\
				if(!CDebugScene::QueueTuneWidget(data))							\
				{																\
					hasAddedBankItem_##sliderName = 0;							\
				}																\
			}																	\
		}																		\
	}
#else // __BANK
// If not a bank build make sure the var still exists, just for convenience.	
#define REGISTER_TUNE_GROUP_FLOAT( sliderName, defaultFloatVal )				\
	static const float sliderName = defaultFloatVal;

#define INSTANTIATE_TUNE_GROUP_FLOAT( groupName, sliderName, min, max, delta )	\
	(void)sizeof(sliderName);// Placate the compiler in case the var isn't used.
#endif // __BANK

#define TUNE_GROUP_FLOAT( groupName, sliderName, defaultFloatVal, min, max, delta )	\
	REGISTER_TUNE_GROUP_FLOAT( sliderName, defaultFloatVal )					\
	INSTANTIATE_TUNE_GROUP_FLOAT( groupName, sliderName, min, max, delta )


/////////////////////////////////////////////////////////////////////////////////
// Tune color macro
// Same as above but for colors instead of bool toggles.
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
#define REGISTER_TUNE_COLOR( colorName, defaultColorVal )						\
	static Color32 colorName = defaultColorVal;									\
	static u32 hasAddedBankItem_##colorName = 0;

#define INSTANTIATE_TUNE_COLOR( colorName )										\
	if(!hasAddedBankItem_##colorName)											\
	{																			\
		bkBank* pBank = BANKMGR.FindBank("_TUNE_");								\
		if(pBank && sysInterlockedIncrement(&hasAddedBankItem_##colorName) == 1)	\
		{																		\
			if(bkManager::IsBankThread())										\
			{																	\
				pBank->AddColor(#colorName, &colorName);						\
			}																	\
			else																\
			{																	\
				CDebugScene::TuneWidgetData data(TUNE_TYPE_COLOR, NULL, #colorName, &colorName);	\
				if(!CDebugScene::QueueTuneWidget(data))							\
				{																\
					hasAddedBankItem_##colorName = 0;							\
				}																\
			}																	\
		}																		\
	}
#else // __BANK
// If not a bank build make sure the var still exists, just for convenience.	
#define REGISTER_TUNE_COLOR( colorName, defaultColorVal )						\
	static const Color32 colorName = defaultColorVal;

#define INSTANTIATE_TUNE_COLOR( colorName )										\
	(void)sizeof(colorName);// Placate the compiler in case the var isn't used.
#endif // __BANK

#define TUNE_COLOR( colorName, defaultColorVal )								\
	REGISTER_TUNE_COLOR( colorName, defaultColorVal )							\
	INSTANTIATE_TUNE_COLOR( colorName )

/////////////////////////////////////////////////////////////////////////////////
// Tune group color macro
// Same as above but for colors instead of bool colors.
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
#define REGISTER_TUNE_GROUP_COLOR( colorName, defaultColorVal )					\
	static Color32 colorName = defaultColorVal;									\
	static u32 hasAddedBankItem_##colorName = 0;

#define INSTANTIATE_TUNE_GROUP_COLOR( groupName, colorName )					\
	if(!hasAddedBankItem_##colorName)											\
	{																			\
		bkBank* pBank = BANKMGR.FindBank("_TUNE_");								\
		if(pBank && sysInterlockedIncrement(&hasAddedBankItem_##colorName) == 1)	\
		{																		\
			if(bkManager::IsBankThread())										\
			{																	\
				bkGroup* pGroup = GetOrCreateGroup(pBank, #groupName);			\
				pBank->SetCurrentGroup(*pGroup);								\
				pBank->AddColor(#colorName, &colorName);						\
				pBank->UnSetCurrentGroup(*pGroup);								\
			}																	\
			else																\
			{																	\
				CDebugScene::TuneWidgetData data(TUNE_TYPE_COLOR, #groupName, #colorName, &colorName);	\
				if(!CDebugScene::QueueTuneWidget(data))							\
				{																\
					hasAddedBankItem_##colorName = 0;							\
				}																\
			}																	\
		}																		\
	}
#else // __BANK
// If not a bank build make sure the var still exists, just for convenience.	
#define REGISTER_TUNE_GROUP_COLOR( colorName, defaultColorVal )					\
	static const Color32 colorName = defaultColorVal;

#define INSTANTIATE_TUNE_GROUP_COLOR( groupName, colorName )					\
	(void)sizeof(colorName);// Placate the compiler in case the var isn't used.
#endif // __BANK

#define TUNE_GROUP_COLOR( groupName, colorName, defaultColorVal )				\
	REGISTER_TUNE_GROUP_COLOR( colorName, defaultColorVal )						\
	INSTANTIATE_TUNE_GROUP_COLOR( groupName, colorName )

#if !__FINAL
/////////////////////////////////////////////////////////////////////////////////
// CDebugScene
//
// Just a centralized store of mainly static function to help with game debugging
/////////////////////////////////////////////////////////////////////////////////
class CDebugScene
{
friend class BudgetDisplay;
public:
#if __BANK
	class TuneWidgetData
	{
	public:
		TuneWidgetData();
		TuneWidgetData(const s32 type, const char* groupName, const char* title, void* data, const void* min = NULL, const void* max = NULL, const void* delta = NULL);
		~TuneWidgetData() {}

		s32 m_Type;
		const char *m_GroupName;
		const char *m_Title;

		union
		{
			struct
			{
				bool* m_Data;
				u32 m_Unused1;
				u32 m_Unused2;
				u32 m_Unused3;
			} m_AsBool;

			struct
			{
				s32* m_Data;
				s32 m_Min;
				s32 m_Max;
				float m_Delta;
			} m_AsInt;

			struct
			{
				float* m_Data;
				float m_Min;
				float m_Max;
				float m_Delta;
			} m_AsFloat;

			struct
			{
				Color32* m_Data;
				u32 m_Unused1;
				u32 m_Unused2;
				u32 m_Unused3;
			} m_AsColor;
		} m_Params;
	};

	static void		AddWidgets(bkBank &bank);
	static bool		QueueTuneWidget(const TuneWidgetData& data);
	static void		FlushTuneWidgetQueue();

	static void		SetClickForObjects(bool bSetClickedObejcts) { 
		ms_clickTestForObjects = bSetClickedObejcts; 
		ms_bEnableFocusEntityDragging = bSetClickedObejcts; 
	}
#endif // __BANK

	struct FocusFilter
	{
		FocusFilter()
			: camPos(V_ZERO)
			, camFwd(V_Y_AXIS_WZERO)
			, minDot(0.86f)
			, upCloseDist(2.f)
			, bAnimal(false)
			, bBird(false)
			, bHuman(false)
			, bPlayer(false)
			, bVehicle(false)
			, bDead(false)
			, bRider(false)
		{}

		float CalculateScore(CEntity* pEntity) const;

		Vec3V camPos;
		Vec3V camFwd;
		float minDot;
		float upCloseDist;

		bool bAnimal : 1;
		bool bBird : 1;
		bool bHuman : 1;
		bool bPlayer : 1;
		bool bVehicle : 1;
		bool bDead : 1;
		bool bRider : 1;
	};

#if USE_PROFILER_BASIC
	static void			AttachProfilingVariables();
#endif

	static void			Update();
	static void			PreUpdate();
	static void			PreRender();
	static void			Render_MainThread();
	static void			Render_RenderThread();

	static bool			GetClosestEntityCB(CEntity* pEntity, void* pData);
	static void			ChangeProximityTestPoint();
	static const char*	GetEntityDescription(const CEntity * pEntity);

	static CEntity*		GetBestFilteredEntity(const FocusFilter& filter);
	static void			FocusAddClosestAIPed(FocusFilter& filter);

	enum {FOCUS_ENTITIES_MAX = 8};
	static void			FocusEntities_Clear();
	static bool			FocusEntities_IsEmpty();
	static void			FocusEntities_Set(CEntity* pEntity, int index);
	static bool			FocusEntities_Add(CEntity* pEntity, int& out_index);
	static bool			FocusEntities_IsInGroup(const CEntity* const pEntity, int& out_index);
	static bool			FocusEntities_IsInGroup(const CEntity* const pEntity);
	static bool			FocusEntities_IsNearGroup(Vec3V_In vPos, float fThreshold=1.0f);
	static CEntity*		FocusEntities_Get(int index);
	

#if !__FINAL
	static const Vector3&	GetProximityTestPoint(){return ms_proximityTestPoint;}
	static void				SetProximityTestPoint(const Vector3& proximityTestPoint){ms_proximityTestPoint = proximityTestPoint;}
	static float			GetProxmityTestAllowableVerticalDiff(){return ms_proximityMaxAllowableVerticalDiff;}
	static void				SetProxmityTestAllowableVerticalDiff(float maxAllowableVerticalDiff){ms_proximityMaxAllowableVerticalDiff = maxAllowableVerticalDiff;}
	static float			GetProxmityTestAllowableRadialDiff(){return ms_proximityMaxAllowableRadialDiff;}
	static void				SetProxmityTestAllowableRadialDiff(float maxAllowableRadialDiff){ms_proximityMaxAllowableRadialDiff = maxAllowableRadialDiff;}

	static bool				GetDisplayDebugSummary(){return ms_bDisplayDebugSummary;}
	static bool				GetDisplayLODCounts() { return ms_bDisplayLODCounts;}
	static void				SetDisplayDebugSummary(bool bDisplay){ms_bDisplayDebugSummary=bDisplay;}
#endif


#if (__BANK) || (__DEV)
	static void		GetDebugSummary(char* summary, bool addLodCount);

	static bool		ShouldStopProcessCtrlAllEntities()										{ return ms_bStopProcessCtrlAllEntities; }
	static bool		ShouldStopProcessCtrlAllExceptFocusEntity0()							{ return ms_bStopProcessCtrlAllExceptFocus0Entity; }
	static bool		ShouldStopProcessCtrlAllEntitiesOfFocus0Type()							{ return ms_bStopProcessCtrlAllEntitiesOfFocus0Type; }
	static bool		ShouldStopProcessCtrlAllEntitiesOfFocus0TypeExceptFocus0()				{ return ms_bStopProcessCtrlAllOfFocus0TypeExceptFocus0; }
#endif	// BANK


#if __DEV
	static bool		ShouldDebugBreakOnWorldAddOfFocusEntity()								{ return ms_bBreakOnWorldAddOfFocusEntity; }
	static bool		ShouldDebugBreakOnWorldRemoveOfFocusEntity()							{ return ms_bBreakOnWorldRemoveOfFocusEntity; }
	static bool		ShouldDebugBreakOnProcessControlOfFocusEntity()							{ return ms_bBreakOnProcessControlOfFocusEntity; }
	static bool		ShouldDebugBreakOnProcessIntelligenceOfFocusEntity()					{ return ms_bBreakOnProcessIntelligenceOfFocusEntity; }
	static bool		ShouldDebugBreakOnProcessPhysicsOfFocusEntity()							{ return ms_bBreakOnProcessPhysicsOfFocusEntity; }
	static bool		ShouldDebugBreakOnPreRenderOfFocusEntity()								{ return ms_bBreakOnPreRenderOfFocusEntity; }
	static bool		ShouldDebugBreakOnUpdateAnimOfFocusEntity()								{ return ms_bBreakOnUpdateAnimOfFocusEntity; }
	static bool		ShouldDebugBreakOnUpdateAnimAfterCameraUpdateOfFocusEntity()			{ return ms_bBreakOnUpdateAnimAfterCameraUpdateOfFocusEntity; }
	static bool		ShouldDebugBreakOnRenderOfFocusEntity()									{ return ms_bBreakOnRenderOfFocusEntity; }
	static bool		ShouldDebugBreakOnAddToDrawListOfFocusEntity()							{ return ms_bBreakOnAddToDrawListOfFocusEntity; }
	static bool		ShouldDebugBreakOnDestroyOfFocusEntity()								{ return ms_bBreakOnDestroyOfFocusEntity; }
	static bool		ShouldDebugBreakOnCalcDesiredVelocityOfFocusEntity()					{ return ms_bBreakOnCalcDesiredVelocityOfFocusEntity; }

	static bool		ShouldDebugBreakOnProximityOfDestroyCallingEntity()						{ return ms_bBreakOnProximityOfDestroyCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfAddCallingEntity()							{ return ms_bBreakOnProximityOfAddCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfRemoveCallingEntity()						{ return ms_bBreakOnProximityOfRemoveCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfRemoveAndAddCallingEntity()				{ return ms_bBreakOnProximityOfAddAndRemoveCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfAddToInteriorCallingEntity()				{ return ms_bBreakOnProximityOfAddToInteriorCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfRemoveFromInteriorCallingEntity()			{ return ms_bBreakOnProximityOfRemoveFromInteriorCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfWorldAddCallingEntity()					{ return ms_bBreakOnProximityOfWorldAddCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfWorldAddCallingPed()						{ return ms_bBreakOnProximityOfWorldAddCallingPed; }
	static bool		ShouldDebugBreakOnProximityOfTeleportCallingObject()					{ return ms_bBreakOnProximityOfTeleportCallingObject; }
	static bool		ShouldDebugBreakOnProximityOfTeleportCallingPed()						{ return ms_bBreakOnProximityOfTeleportCallingPed; }
	static bool		ShouldDebugBreakOnProximityOfTeleportCallingVehicle()					{ return ms_bBreakOnProximityOfTeleportCallingVehicle; }
	static bool		ShouldDebugBreakOnProximityOfWorldRemoveCallingEntity()					{ return ms_bBreakOnProximityOfWorldRemoveCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfProcessControlCallingEntity()				{ return ms_bBreakOnProximityOfProcessControlCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfProcessControlCallingPed()					{ return ms_bBreakOnProximityOfProcessControlCallingPed; }
	static bool		ShouldDebugBreakOnProximityOfProcessPhysicsCallingEntity()				{ return ms_bBreakOnProximityOfProcessPhysicsCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfPreRenderCallingEntity()					{ return ms_bBreakOnProximityOfPreRenderCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfUpdateAnimCallingEntity()					{ return ms_bBreakOnProximityOfUpdateAnimCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfUpdateAnimAfterCameraUpdateCallingEntity()	{ return ms_bBreakOnProximityOfUpdateAnimAfterCameraUpdateCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfRenderCallingEntity()						{ return ms_bBreakOnProximityOfRenderCallingEntity; }
	static bool		ShouldDebugBreakOnProximityOfAddToDrawListCallingEntity()				{ return ms_bBreakOnProximityOfAddToDrawListCallingEntity; }

	static void		UpdateAttachTest();
	static void		AttachFocusEntitiesTogether();
	static void		AttachFocusEntitiesTogetherPhysically();
	static void		AttachFocusEntitiesToWorld();
	static void		DetachFocusEntity();
	static void		DeleteFocusEntities();
#endif // __DEV

	// this displays the scene's static collision geometry (octree triangles)
	static void RenderCollisionGeometry(void);
	static void DrawBoundCollisionGeometry(const Vector3 & vOrigin, const class phBound * pBound);
	static void DrawEntityBoundingBox(CEntity* pEntity, Color32 colour, bool bDrawWorldAABB = false, bool bDrawGeometryBoxes = false);
	static void Draw3DBoundingBox(const Vector3 & vMin, const Vector3 & vMax, const Matrix34 & mat, Color32 color);
	static void DrawEntitySubBounds(CEntity * pEntity,Color32 color);
	static void DrawSubBounds(phInst *pInst, phBound* pBound, Matrix34* pCurrMat, Color32 color);
	static void PrintInfoAboutEntity(CEntity* pEntity);

	static void DrawEntityBoundingBoxOnVMap(CEntity* pEntity, Color32 colour);

	//any debug only functions that use the mouse, ie teleport to mouse, select ai entity etc
	static void CheckMouse();
	static CEntity* GetEntityUnderMouse(Vector3* pvOptionalIntersectionPosition = NULL, s32* piOptionalComponent = NULL, s32 iOptionalFlags = 0);
	static void GetScreenPosFromWorldPoint(const Vector3& vWorldPos, float& fWindowX, float& fWindowY);
	static bool GetWorldPositionFromScreenPos(float &fScreenX, float &fScreenY, Vector3& posn, s32 iOptionalFlags = 0, Vector3* pvNormal = NULL, void **entity = NULL);
	static bool GetWorldPositionUnderMouse(Vector3& posn, s32 iOptionalFlags = 0, Vector3* pvNormal = NULL, void **entity = NULL, bool onVectorMap = false);
	static Vector2 GetMouse0to1(void);
	static bool GetMouseLeftPressed(void);
	static bool GetMouseLeftReleased(void);
	static bool GetMouseRightPressed(void);
	static bool GetMouseRightReleased(void);
	static bool GetMouseMiddlePressed(void);
	static bool GetMouseMiddleReleased(void);
	static void GetMousePointing( Vector3& vMouseNear, Vector3& vMouseFar, bool onVectorMap = false );

#if __DEV
	// model viewer
	static void AddModelViewerBankWidgets(bkBank& bank);
	static void InitModelViewer();
	static void ShutdownModelViewer(unsigned shutdownMode);
	static void UpdateModelViewer();
	static void RenderModelViewer();
#endif // __DEV

#if __BANK
	static void DisableInputsForTouchDebug();

	static void RegisterEntityChangeCallback(datCallback callback);
	static void UpdateSceneUpdateCostStep();
	static void PrintCoverTuningForEntity(CEntity* pEntity);

	inline static int GetMouseDX() { return ioMouse::GetX() - ms_lastFrameMouseX; }
	inline static int GetMouseDY() { return ioMouse::GetY() - ms_lastFrameMouseY; }
	inline static bool MouseMovedThisFrame() { return (GetMouseDX()!=0 || GetMouseDY()!=0);}
#endif //__BANK

#if __DEV
	static int iDisplaySceneUpdateCostStep;
	static char DisplaySceneUpdateCostStepName[16];
	static bool bDisplaySceneUpdateCostOnVMap;
	static bool bDisplaySceneUpdateCostSelectedOnly;
#endif // __DEV

#if __BANK
	static const s32 TUNE_WIDGET_QUEUE_SIZE = 1024;
	static sysMessageQueue<TuneWidgetData, TUNE_WIDGET_QUEUE_SIZE, false> sm_TuneWidgetQueue;

	static void DisplayEntitiesOnVectorMap();
	static bool bDisplayBuildingsOnVMap;
	static bool bDisplaySceneScoredEntities;

	static bool bDisplayVehiclesOnVMap;
	static bool bDisplayVehiclesOnVMapAsActiveInactive;
	static bool bDisplayVehiclesOnVMapTimesliceUpdates;
	static bool bDisplayVehiclesOnVMapBasedOnOcclusion;
	static bool bDisplayLinesToLocalDrivingCars;
	static bool bDisplayVehiclesUsesFadeOnVMap;
	static bool bDisplayVehPopFailedCreateEventsOnVMap;
	static bool bDisplayVehPopCreateEventsOnVMap;
	static bool bDisplayVehPopDestroyEventsOnVMap;
	static bool bDisplayVehPopConversionEventsOnVMap;
	static bool bDisplayVehGenLinksOnVM;
	static bool bDisplayVehicleCreationPathsOnVMap;
	static bool bDisplayVehicleCreationPathsInWorld;
	static bool bDisplayVehicleCreationPathsCurrDensityOnVMap;
	static bool bDisplayVehiclesToBeStreamedOutOnVMap;
	static bool bDisplayVehicleCollisionsOnVMap;
	
	static bool bDisplayPedsOnVMap;
	static bool bDisplayPedsOnVMapAsActiveInactive;
	static bool bDisplayPedsOnVMapTimesliceUpdates;
	static bool bDisplaySpawnPointsRawDensityOnVMap;
	static bool	bDisplayPedPopulationEventsOnVMap;
	static bool bDisplayPedsToBeStreamedOutOnVMap;
	static bool bDisplayCandidateScenarioPointsOnVMap;

	static bool bDisplayNetworkGameOnVMap;
	static bool bDisplayPortalInstancesOnVMap;
    static bool bDisplayRemotePlayerCameras;
    static bool bDisplayCarCreation;
	static bool bDisplayWaterOnVMap;
	static bool bDisplayCalmingWaterOnVMap;
	static bool bDisplayShoreLinesOnVMap;
	static bool bDisplayDuplicateObjectsBB;
	static bool bDisplayDuplicateObjectsOnVMap;

	static bool bDisplayTargetingRanges;
	static bool bDisplayTargetingCones;
	static bool bDisplayTargetingEntities;

	//	static bool bDisplayPedModelNames;
#endif // __BANK

	// Bank only
#if __BANK
	static bool bDisplayLadderDebug;
	static bool bDisplayObjectsOnVMap;
	static bool bDisplayPickupsOnVMap;
	static bool bDisplayLineAboveObjects;
	static bool bDisplayLineAboveAllEntities;
	static float fEntityDebugLineLength;
	static bool bDisplayDoorInfo;
	static bool bDisplayDoorPersistentInfo;
	static bool sm_VisualizeAutoOpenBounds;
#endif // __BANK
	// Dev or bank
#if (__DEV) || (__BANK)
	static bool ms_bStopProcessCtrlAllEntities;
	static bool ms_bStopProcessCtrlAllExceptFocus0Entity;
	static bool ms_bStopProcessCtrlAllEntitiesOfFocus0Type;
	static bool ms_bStopProcessCtrlAllOfFocus0TypeExceptFocus0;
#endif
	// Dev only
#if __DEV
	static bool ms_bBreakOnWorldAddOfFocusEntity;
	static bool ms_bBreakOnWorldRemoveOfFocusEntity;
	static bool ms_bBreakOnProcessControlOfFocusEntity;
	static bool ms_bBreakOnProcessIntelligenceOfFocusEntity;
	static bool ms_bBreakOnProcessPhysicsOfFocusEntity;
	static bool ms_bBreakOnPreRenderOfFocusEntity;
	static bool ms_bBreakOnUpdateAnimOfFocusEntity;
	static bool ms_bBreakOnUpdateAnimAfterCameraUpdateOfFocusEntity;
	static bool ms_bBreakOnRenderOfFocusEntity;
	static bool ms_bBreakOnAddToDrawListOfFocusEntity;
	static bool ms_bBreakOnDestroyOfFocusEntity;
	static bool ms_bBreakOnCalcDesiredVelocityOfFocusEntity;

	static bool ms_bBreakOnProximityOfDestroyCallingEntity;
	static bool ms_bBreakOnProximityOfAddCallingEntity;
	static bool ms_bBreakOnProximityOfRemoveCallingEntity;
	static bool ms_bBreakOnProximityOfAddAndRemoveCallingEntity;
	static bool ms_bBreakOnProximityOfAddToInteriorCallingEntity;
	static bool ms_bBreakOnProximityOfRemoveFromInteriorCallingEntity;
	static bool ms_bBreakOnProximityOfWorldAddCallingEntity;
	static bool ms_bBreakOnProximityOfWorldAddCallingPed;
	static bool ms_bBreakOnProximityOfTeleportCallingObject;
	static bool ms_bBreakOnProximityOfTeleportCallingPed;
	static bool ms_bBreakOnProximityOfTeleportCallingVehicle;
	static bool ms_bBreakOnProximityOfWorldRemoveCallingEntity;
	static bool ms_bBreakOnProximityOfProcessControlCallingEntity;
	static bool ms_bBreakOnProximityOfProcessControlCallingPed;
	static bool ms_bBreakOnProximityOfProcessPhysicsCallingEntity;
	static bool ms_bBreakOnProximityOfPreRenderCallingEntity;
	static bool ms_bBreakOnProximityOfUpdateAnimCallingEntity;
	static bool ms_bBreakOnProximityOfUpdateAnimAfterCameraUpdateCallingEntity;
	static bool ms_bBreakOnProximityOfRenderCallingEntity;
	static bool ms_bBreakOnProximityOfAddToDrawListCallingEntity;

	static bool ms_bEraserEnabled;

	/// Physical attach system test vars ///
	//  Debug drawing
	static bool ms_bDrawAttachmentExtensions;
	static bool ms_bDrawAttachmentEdges;
	static bool ms_bDisplayAttachmentEdgeType;
	static bool	ms_bDisplayAttachmentInvokingFunction;
	static bool	ms_bDisplayAttachmentInvokingFile;
	static bool ms_bDisplayAttachmentPhysicsInfo;
	static bool	ms_bDisplayAttachmentEdgeData;
	static bool ms_bDisplayUpdateRecords;
	static bool	ms_bRenderListOfAttachmentExtensions;
	static bool ms_bDebugRenderAttachmentOfFocusEntitiesOnly;
	static bool ms_fAttachmentNodeRelPosWorldUp;
	static float ms_fAttachmentNodeEntityDist;
	static float ms_fAttachmentNodeRadius;

	//  Attach flags
	static bool ms_bAttachFlagAutoDetachOnRagdoll;
	static bool ms_bAttachFlagCollisionOn;
	static bool ms_bAttachFlagDeleteWithParent;
	static bool ms_bAttachFlagRotConstraint;

	//  Detach flags
	static bool ms_bDetachFlagActivatePhysics;
	static bool ms_bDetachFlagApplyVelocity;
	static bool ms_bDetachFlagNoCollisionUntilClear;
	static bool ms_bDetachFlagIgnoreSafePositionCheck;
#endif // __DEV

#if __BANK
	static bool ms_bEnableFocusEntityDragging;

	enum GADGETENUM
	{
		GADGET_MAT_XAXIS = 1,
		GADGET_MAT_YAXIS,
		GADGET_MAT_ZAXIS,
		GADGET_MAT_TRANS,
		GADGET_SCALE,
		GADGET_ENTITY_HEADING,
		GADGET_ENTITY_TYPE,
		GADGET_PATHSERVER_ID,
		GADGET_MASS,
		GADGET_PED_HEALTH,
		GADGET_COLLISION_MODEL_TYPE,
		GADGET_NUM_2D_FX,
		GADGET_NUM_2D_FX_LIGHTS,
		GADGET_VISIBLE,
		GADGET_FIXED,
		GADGET_FIXED_FOR_NAVIGATION,
		GADGET_NOT_AVOIDED,
		GADGET_DOES_NOT_PROVIDE_AI_COVER,
		GADGET_DOES_NOT_PROVIDE_PLAYER_COVER,
		GADGET_LADDER,
		GADGET_DOOR_PHYSICS,
		GADGET_HAS_ANIM,
		GADGET_ANIM_AUTO_START,
		GADGET_HAS_UV_ANIM,
		GADGET_HAS_SPAWN_POINTS,
		GADGET_GENERATES_WIND_AUDIO,
		GADGET_AMBIENT_SCALE,
		GADGET_IS_PROP,
		GADGET_IS_TRAFFIC_LIGHT,
		GADGET_TINT_PALETTE_INDEX,
		GADGET_CLIMBABLE_BY_AI,
		GADGET_HAS_ADDED_LIGHTS,
		GADGET_IS_TARGETTABLE_WITH_NO_LOS,
		GADGET_IS_A_TARGET_PRIORITY,
		GADGET_CAN_BE_TARGETTED_BY_PLAYER,

		GADGET_IS_CULL_SMALL_SHADOWS,
		GADGET_IS_DONT_CAST_SHADOWS,
		GADGET_IS_RENDER_ONLY_IN_REFLECTIONS,
		GADGET_IS_DONT_RENDER_IN_REFLECTIONS,
		GADGET_IS_RENDER_ONLY_IN_SHADOWS,
		GADGET_IS_DONT_RENDER_IN_SHADOWS,
	
		GADGET_LODGROUP_FLAGS,

		GADGET_LODMASK_FLAGS,
		
		GADGET_IS_USED_IN_MP,

		GADGET_IS_POPTYPE,

		GADGET_OWNED_BY,

		GADGET_REPLAY_ID,
		GADGET_REPLAY_MAPHASH,
		GADGET_REPLAY_HIDECOUNT,
		
		GADGET_NUM
	};
#endif //__BANK

private:
	static bool GetIntersectionUnderMouse(phIntersection& intersection);
	static void OnChangeFocusEntity(int index);
	static void HandleFocusEntityTextBoxChange(void);

	static void UpdateFocusEntityDragging();

#if __DEV
	static void DisplaySceneUpdateCost(fwEntity &entity, void *userData);
#endif // __DEV

	// PURPOSE: Return the relevant archetype flags for mouse dragging given the entity provided
	static int	GetArchetypeFlags(CEntity* pEntity);

#if __DEV
	static CAuthoringHelper* m_AuthorHelper; 
#endif // _

#if !__FINAL
	static RegdEnt	ms_focusEntities[FOCUS_ENTITIES_MAX];
	static char		ms_focusEntity0AddressString[64];
#if __DEV
	static char		ms_focusEntity0ModelInfoString[64];
#endif // __DEV
	static bool		ms_changeSelectedEntityOnHover;
	static bool		ms_doClickTestsViaVectorMapInsteadViewport;
	static bool		ms_drawFocusEntitiesBoundBox;
	static bool		ms_drawFocusEntitiesBoundBoxOnVMap;
	static bool		ms_drawFocusEntitiesInfo;
	static bool		ms_drawFocusEntitiesCoverTuning;
	static bool		ms_drawFocusEntitiesSkeleton;
	static bool		ms_drawFocusEntitiesSkeletonNonOrthonormalities;
	static bool		ms_drawFocusEntitiesSkeletonNonOrthoDataOnly;
	static bool		ms_drawFocusEntitiesSkeletonBoneNamesAndIds;
	static bool		ms_drawFocusEntitiesSkeletonChannels;
	static bool		ms_drawFocusEntitiesSkeletonNonOrthonoMats;
	static bool		ms_makeFocusEntitiesFlash;
	static bool		ms_lockFocusEntities;
	static bool		ms_logFocusEntitiesPosition;
	static bool		ms_clickTestForPeds;
	static bool		ms_clickTestForRagdolls;
	static bool		ms_clickTestForVehicles;
	static bool		ms_clickTestForObjects;
	static bool		ms_clickTestForWeapons;
	static bool		ms_clickTestForBuildingsAndMap;	

	static Vector3	ms_proximityTestPoint;
	static float	ms_proximityMaxAllowableVerticalDiff;
	static float	ms_proximityMaxAllowableRadialDiff;
	static bool		ms_drawProximityArea;
	static bool		ms_drawProximityAreaFlashes;
	static bool		ms_drawProximityAreaOnVM;
	static bool		ms_drawProximityAreaOnVMFlashes;
	static bool		ms_bDisplayDebugSummary;
	static bool		ms_bDisplayLODCounts;
#endif // !__FINAL

#if __BANK
	enum DebugTimerMode
	{
		DTM_Off = 0,
		DTM_FrameCount,
		DTM_TimerStart,
		DTM_NUM_MODES
	};

	// a list of callbacks to call when the selected entity is changed
	static atArray<datCallback> ms_callbacks;
	static Vector3 ms_lastFrameWorldPosition;
	static int ms_lastFrameMouseX;
	static int ms_lastFrameMouseY;
	static int ms_DebugTimerMode;
	static float ms_DebugTimer;

	// Inspector gadget :)
	static CUiGadgetInspector * m_pInspectorWindow;
	static bool m_bIsInspectorAttached;
#endif //__BANK
};

extern CEntity * g_pFocusEntity;

struct BoxDimensions
{
	float RelativePositionOnAxis; 
	float ScaledBoxDimensions;
	float NonScaledBoxDimenstions; 

	BoxDimensions()
	{
		RelativePositionOnAxis = 0.0f;
		ScaledBoxDimensions = 0.0f; 
		NonScaledBoxDimenstions = 0.0f; 
	}

};

#if __BANK
class CAuthoringHelper
{
public:

	// If you change this, don't forget to change the one in grcDebugDraw
	enum GizmoInputType
	{
		NO_INPUT = 0,
		X_AXIS,
		Y_AXIS,
		Z_AXIS,
		XY_PLANE,
		XZ_PLANE,
		ZY_PLANE,
		ROT_X_PLANE, 
		ROT_Y_PLANE,
		ROT_Z_PLANE,
		FREE_AXIS, 
	};

	enum AUTHORMODE
	{
		NO_AUTHOR_MODE = -1, 
		TRANS_GBL, 
		TRANS_LCL, 
		ROT_GBL, 
		ROT_LCL, 
		MAX_AUTHOR_MODES
	};

	enum InputMode
	{
		NO_INPUT_MODE = -1, 
		INPUT_X = MAX_AUTHOR_MODES, 
		INPUT_Y, 
		INPUT_Z
	};

	CAuthoringHelper(const char* Label = NULL, bool useHelpersMenu = true); 
	~CAuthoringHelper(); 
	//Update
	public:

	bool Update(Matrix34& Mat); 

	bool Update(const Matrix34& Mat, float Scale, const Vector3& Offset, Matrix34& UpdatedMat);

	bool Update(const Matrix34& Mat, float Scale, const Vector3& Offset, Matrix34& UpdatedMat, const Matrix34& ParentMat); 

	u32 GetCurrentInput() const { return m_CurrentInput; }

	bool IsMakingAuthorMenuSelection(); 

	void SetLabel(const char* label) {m_Label.Clear(); m_Label = label; }

	static s32 GetCurrentAuthoringMode () { return ms_AuthorMode; }
	static void SetAuthoringMode(s32 authormode) { ms_AuthorMode = authormode; }
private:

	void Init(); 

	//Translation
	Vector3 UpdateTranslationAxis(const Matrix34& Mat, const Vector3& Axis, const spdPlane& Plane); 

	void CreateTranslationCollisionBox(const Matrix34& Mat, const Vector3& Axis, float Scale, const Vector3& Offset, const BoxDimensions& Dimensions, spdOrientedBB & Bound); 

	void CreateBoxMatMinAndMaxVectors(const Matrix34& Mat, const Vector3& Axis, float Scale, const Vector3& Offset, const BoxDimensions& Dimensions, Matrix34& BoxMat, Vector3& VecMin, Vector3& VecMax);

	void RenderTranslationCollisionBox(const Matrix34& Mat, const Vector3& Axis, float Scale, const Vector3& Offset, const Color32 color, const BoxDimensions& Dimensions); 

	void RenderTranslationHelper(const Matrix34& AuthorignMat, float Scale, const Vector3& Offset); 

	void UpdateMouseTranslationCollisionBox(spdOrientedBB& MouseProbe); 

	bool UpdateTranslationHelper(const Matrix34& Mat, float Scale, const Vector3& Offset, Matrix34& UpdatedMat); 

	void UpdateTranslationCollisionBoxes(const Matrix34& AuthoringMat, float Scale, const Vector3& Offset ); 

	void UpdateTranslationPosition(const Matrix34& AuthoringMat, Matrix34& UpdatedMat); 

	//Rotation
	bool UpdateRotationHelper(const Matrix34& Mat, float Scale, const Vector3& Offset, Matrix34& UpdatedMat); 

	void UpdateAxisRotation(const Matrix34& AuthoringMat, Matrix34& UpdatedMat, const Vector3& SelectionVec, const Vector3& SelectionWorld);

	void RenderRotationHelper(const Matrix34& AuthoringMat, const Matrix34& updatedMat, float Scale); 

	void UpdateRotationAxisSelection(const Matrix34& AuthoringMat, float Scale, Vector3& SelectionVec, Vector3&SelectionWorld);

	//Update Selection
	bool ShouldUpdateSelectedHelper(const Vector3& MatrixPosition); 

	//Ui
	void CreateAuthorUi(); 

	void UpdateAuthorUi(const Matrix34& UpdatedMat); 

	//bool UpdateKeyboardInput(float &newValue); 

	//void UpdateHelperFromInput(Matrix34& UpdatedMat); 

	
private:

	BoxDimensions m_AxisBox; 
	BoxDimensions m_PlaneBox;

	Vector2 m_ScreenSpaceTangentVector;

	atString m_Label; 
	static atString m_Keyboardbuffer; 

	static float ms_DistanceToCamera; 

	u32 m_CurrentInput; 
	static s32 ms_AuthorMode; 

	static CUiGadgetSimpleListAndWindow* ms_pAuthorToolBar;
	static CAuthoringHelper* ms_SelectedAuthorHelper; 
	//static CAuthoringHelper* ms_LastSelectedAuthorHelper; 

	bool m_IsAxisSelected; 
	bool m_IsFirstUpdate; 
	bool m_StartedTextInput; 
	bool m_useHelpersMenu; 

	int m_LastFrameMouseX;
	int m_LastFrameMouseY; 

};
#endif // __DEV

#endif // !__FINAL

#endif // INC_DEBUG_SCENE_H_
