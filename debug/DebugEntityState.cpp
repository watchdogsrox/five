#include "DebugEntityState.h"
#include "physics/debugEvents.h"
#include "peds/ped.h"
#include "Peds/PedIntelligence.h"
#include "system/timemgr.h"

#if DR_ENABLED
using namespace rage;
using namespace debugPlayback;

//TODO: PARSER
namespace rage
{
	namespace debugPlayback
	{
		class CEntityFixedFlagDebugEvent : public debugPlayback::PhysicsEvent
		{
			atHashString m_LabelString;
		public:
			CEntityFixedFlagDebugEvent(){}
			virtual ~CEntityFixedFlagDebugEvent(){}
			CEntityFixedFlagDebugEvent(const CallstackHelper rCallstack, const fwEntity *pEntity, const char *pString)
				:debugPlayback::PhysicsEvent(rCallstack, pEntity->GetCurrentPhysicsInst())
				,m_LabelString(pString)
			{
			}
			virtual const char* GetEventSubType() const
			{
				return m_LabelString.GetCStr();
			}
			virtual void DebugDraw3d(eDrawFlags drawFlags) const;
			PAR_PARSABLE;
			DR_EVENT_DECL(CEntityFixedFlagDebugEvent);
		};

		DR_EVENT_IMP(CEntityFixedFlagDebugEvent);

		void CEntityFixedFlagDebugEvent::DebugDraw3d(eDrawFlags drawFlags) const
		{
			Color32 col(200,0,200,255);
			if (drawFlags & eDrawSelected)
			{
				col.SetRed(0);
			}
			if (drawFlags & eDrawHovered)
			{
				col.SetGreen(255);
			}

			grcColor(col);
			grcDrawSphere(0.1f,m_Pos,4,true,false);
			grcWorldIdentity();

			//Draw text for the tasks
			if (drawFlags & (eDrawSelected))
			{
				SetTaggedFloatEvent::LineUpdate();
				grcColor(Color32(0,0,0,255));
				grcDrawLabelf(RCC_VECTOR3(m_Pos), 1, SetTaggedFloatEvent::Get3dLineOffset()+1, "%d - %s", m_iEventIndex, GetEventSubType());
				grcColor(col);
				grcDrawLabelf(RCC_VECTOR3(m_Pos), 0, SetTaggedFloatEvent::Get3dLineOffset(), "%d - %s", m_iEventIndex, GetEventSubType());
			}
		}

		void RecordFixedStateChange(const fwEntity& rEntity, const bool bFixed, const bool bNetwork)
		{
			debugPlayback::FilterToEvent<CEntityFixedFlagDebugEvent> filter;
			if (filter.IsSet())
			{
				debugPlayback::SetTaggedDataCollectionEvent *pCollection 
					= debugPlayback::RecordDataCollection( rEntity.GetCurrentPhysicsInst(), "EntitySetTo%s", bFixed ? "Fixed" : "NotFixed" );
				if (pCollection)
				{
					DR_MEMORY_HEAP();
					char debugbuffer[64];
					formatf(debugbuffer, "%i", fwTimer::GetFrameCount());
					pCollection->AddString("Frame", debugbuffer);
					formatf(debugbuffer, "%s(%p)", rEntity.GetModelName(), &rEntity);
					pCollection->AddString("Ped", debugbuffer);
					pCollection->AddString("bNetwork", bNetwork ? "TRUE" : "FALSE");
				}
			}
		}
	}
}

//Included last to get all the declarations within the CPP file
#include "DebugEntityState_parser.h"

#endif //DR_ENABLED


