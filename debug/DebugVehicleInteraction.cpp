#include "DebugVehicleInteraction.h"
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
		class CPedVehicleInteractionDebugEvent : public debugPlayback::PhysicsEvent
		{
			atHashString m_LabelString;
		public:
			CPedVehicleInteractionDebugEvent(){}
			virtual ~CPedVehicleInteractionDebugEvent(){}
			CPedVehicleInteractionDebugEvent(const CallstackHelper rCallstack, const fwEntity *pEntity, const char *pString)
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
			DR_EVENT_DECL(CPedVehicleInteractionDebugEvent);
		};

		DR_EVENT_IMP(CPedVehicleInteractionDebugEvent);

		void CPedVehicleInteractionDebugEvent::DebugDraw3d(eDrawFlags drawFlags) const
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

		void RecordPedSetIntoVehicle(const CPed& rPed, const CVehicle& rVeh, s32 iFlags)
		{
			debugPlayback::FilterToEvent<CPedVehicleInteractionDebugEvent> filter;
			if (filter.IsSet())
			{
				debugPlayback::SetTaggedDataCollectionEvent *pCollection 
					= debugPlayback::RecordDataCollection( rPed.GetCurrentPhysicsInst(), "SetPedIntoVehicle");
				if (pCollection)
				{
					DR_MEMORY_HEAP();

					char debugbuffer[64];
					formatf(debugbuffer, "%s(%p)", rPed.GetModelName(), &rPed);
					pCollection->AddString("Ped", debugbuffer);
					formatf(debugbuffer, "%s(%p)", rVeh.GetModelName(), &rVeh);
					pCollection->AddString("Vehicle", debugbuffer);
					formatf(debugbuffer, "%i", iFlags);
					pCollection->AddString("Flags", debugbuffer);
				}
			}
		}

		void RecordPedSetOutOfVehicle(const CPed& rPed, const CVehicle* pVeh, s32 iFlags)
		{
			debugPlayback::FilterToEvent<CPedVehicleInteractionDebugEvent> filter;
			if (filter.IsSet())
			{
				debugPlayback::SetTaggedDataCollectionEvent *pCollection 
					= debugPlayback::RecordDataCollection( rPed.GetCurrentPhysicsInst(), "SetPedOutOfVehicle");
				if (pCollection)
				{
					DR_MEMORY_HEAP();

					char debugbuffer[64];
					formatf(debugbuffer, "%s(%p)", rPed.GetModelName(), &rPed);
					pCollection->AddString("Ped", debugbuffer);
					formatf(debugbuffer, "%s(%p)", pVeh ? pVeh->GetModelName() : "NULL", pVeh);
					pCollection->AddString("Vehicle", debugbuffer);
					formatf(debugbuffer, "%i", iFlags);
					pCollection->AddString("Flags", debugbuffer);
				}
			}
		}

		void RecordPedDetachFromVehicle(const CPed& rPed, const CVehicle* pVeh, s32 iFlags)
		{
			debugPlayback::FilterToEvent<CPedVehicleInteractionDebugEvent> filter;
			if (filter.IsSet())
			{
				debugPlayback::SetTaggedDataCollectionEvent *pCollection 
					= debugPlayback::RecordDataCollection( rPed.GetCurrentPhysicsInst(), "DetachFromVehicle");
				if (pCollection)
				{
					DR_MEMORY_HEAP();

					char debugbuffer[64];
					formatf(debugbuffer, "%s(%p)", rPed.GetModelName(), &rPed);
					pCollection->AddString("Ped", debugbuffer);
					formatf(debugbuffer, "%s(%p)", pVeh ? pVeh->GetModelName() : "NULL", pVeh);
					pCollection->AddString("Vehicle", debugbuffer);
					formatf(debugbuffer, "%i", iFlags);
					pCollection->AddString("Detach Flags", debugbuffer);
				}
			}
		}

		void RecordComponentReservation(const CComponentReservation* pComponentReservation, bool bPreUpdate)
		{
			if (pComponentReservation && pComponentReservation->GetOwningEntity())
			{
				debugPlayback::FilterToEvent<CPedVehicleInteractionDebugEvent> filter;
				if (filter.IsSet())
				{
					const CDynamicEntity& rEnt = *pComponentReservation->GetOwningEntity();
					debugPlayback::SetTaggedDataCollectionEvent *pCollection 
						= debugPlayback::RecordDataCollection( rEnt.GetCurrentPhysicsInst(), bPreUpdate ? "RecordComponentReservationPreUpdate" : "RecordComponentReservationPostUpdate");
					if (pCollection)
					{
						DR_MEMORY_HEAP();

						char debugbuffer[64];
						formatf(debugbuffer, "%s %s(%p)", rEnt.IsNetworkClone() ? "CLONE" : "LOCAL", rEnt.GetModelName(), &rEnt);
						pCollection->AddString("Vehicle", debugbuffer);
						formatf(debugbuffer, "%s PED %s(%p)", pComponentReservation->GetPedUsingComponent() ? (pComponentReservation->GetPedUsingComponent()->IsNetworkClone() ? "CLONE" : "LOCAL") : "NULL", pComponentReservation->GetPedUsingComponent() ? pComponentReservation->GetPedUsingComponent()->GetModelName() : "NULL", pComponentReservation->GetPedUsingComponent());
						pCollection->AddString("Ped Using Component", debugbuffer);
						formatf(debugbuffer, "s PED %s(%p)", pComponentReservation->GetLocalPedRequestingUseOfComponent() ? (pComponentReservation->GetLocalPedRequestingUseOfComponent()->IsNetworkClone() ? "CLONE" : "LOCAL") : "NULL", pComponentReservation->GetLocalPedRequestingUseOfComponent() ? pComponentReservation->GetLocalPedRequestingUseOfComponent()->GetModelName() : "NULL", pComponentReservation->GetLocalPedRequestingUseOfComponent());
						pCollection->AddString("Local Ped Requesting Use Of Component", debugbuffer);
						if (rEnt.GetDrawable() && rEnt.GetDrawable()->GetSkeletonData() && pComponentReservation->GetBoneIndex() > 0 && pComponentReservation->GetBoneIndex() < rEnt.GetDrawable()->GetSkeletonData()->GetNumBones())
						{
							const char* boneName = rEnt.GetDrawable()->GetSkeletonData()->GetBoneData(pComponentReservation->GetBoneIndex())->GetName();
							formatf(debugbuffer, "%s(%i)", boneName, pComponentReservation->GetBoneIndex());
							pCollection->AddString("Component Bone Name", debugbuffer);
						}
					}
				}
			}
		}

		void RecordReserveUseOfComponent(const CComponentReservation* pComponentReservation, const CPed& rPed)
		{
			if (pComponentReservation && pComponentReservation->GetOwningEntity())
			{
				debugPlayback::FilterToEvent<CPedVehicleInteractionDebugEvent> filter;
				if (filter.IsSet())
				{
					const CDynamicEntity& rEnt = *pComponentReservation->GetOwningEntity();
					debugPlayback::SetTaggedDataCollectionEvent *pCollection 
						= debugPlayback::RecordDataCollection( rEnt.GetCurrentPhysicsInst(), "RecordReserveUseOfComponent");
					if (pCollection)
					{
						DR_MEMORY_HEAP();

						char debugbuffer[64];
						formatf(debugbuffer, "%s %s(%p)", rEnt.IsNetworkClone() ? "CLONE" : "LOCAL",rEnt.GetModelName(), &rEnt);
						pCollection->AddString("Vehicle", debugbuffer);
						formatf(debugbuffer, "%s PED %s(%p)", rPed.IsNetworkClone() ? "CLONE" : "LOCAL", rPed.GetModelName(), &rPed);
						pCollection->AddString("Ped Requesting Use Of Component", debugbuffer);
						if (rEnt.GetDrawable() && rEnt.GetDrawable()->GetSkeletonData() && pComponentReservation->GetBoneIndex() > 0 && pComponentReservation->GetBoneIndex() < rEnt.GetDrawable()->GetSkeletonData()->GetNumBones())
						{
							const char* boneName = rEnt.GetDrawable()->GetSkeletonData()->GetBoneData(pComponentReservation->GetBoneIndex())->GetName();
							formatf(debugbuffer, "%s(%i)", boneName, pComponentReservation->GetBoneIndex());
							pCollection->AddString("Component Bone Name", debugbuffer);
						}
					}
				}
			}
		}

		void RecordKeepUseOfComponent(const CComponentReservation* pComponentReservation, const CPed& rPed)
		{
			if (pComponentReservation && pComponentReservation->GetOwningEntity())
			{
				debugPlayback::FilterToEvent<CPedVehicleInteractionDebugEvent> filter;
				if (filter.IsSet())
				{
					const CDynamicEntity& rEnt = *pComponentReservation->GetOwningEntity();
					debugPlayback::SetTaggedDataCollectionEvent *pCollection 
						= debugPlayback::RecordDataCollection( rEnt.GetCurrentPhysicsInst(), "RecordKeepUseOfComponent");
					if (pCollection)
					{
						DR_MEMORY_HEAP();

						char debugbuffer[64];
						formatf(debugbuffer, "%s %s(%p)", rEnt.IsNetworkClone() ? "CLONE" : "LOCAL",rEnt.GetModelName(), &rEnt);
						pCollection->AddString("Vehicle", debugbuffer);
						formatf(debugbuffer, "%s PED %s(%p)", rPed.IsNetworkClone() ? "CLONE" : "LOCAL", rPed.GetModelName(), &rPed);
						pCollection->AddString("Ped Keeping Use Of Component", debugbuffer);
						if (rEnt.GetDrawable() && rEnt.GetDrawable()->GetSkeletonData() && pComponentReservation->GetBoneIndex() > 0 && pComponentReservation->GetBoneIndex() < rEnt.GetDrawable()->GetSkeletonData()->GetNumBones())
						{
							const char* boneName = rEnt.GetDrawable()->GetSkeletonData()->GetBoneData(pComponentReservation->GetBoneIndex())->GetName();
							formatf(debugbuffer, "%s(%i)", boneName, pComponentReservation->GetBoneIndex());
							pCollection->AddString("Component Bone Name", debugbuffer);
						}
					}
				}
			}
		}

		void RecordUnReserveUseOfComponent(const CComponentReservation* pComponentReservation, const CPed& rPed)
		{
			if (pComponentReservation && pComponentReservation->GetOwningEntity())
			{
				debugPlayback::FilterToEvent<CPedVehicleInteractionDebugEvent> filter;
				if (filter.IsSet())
				{
					const CDynamicEntity& rEnt = *pComponentReservation->GetOwningEntity();
					debugPlayback::SetTaggedDataCollectionEvent *pCollection 
						= debugPlayback::RecordDataCollection( rEnt.GetCurrentPhysicsInst(), "RecordUnReserveUseOfComponent");
					if (pCollection)
					{
						DR_MEMORY_HEAP();

						char debugbuffer[64];
						formatf(debugbuffer, "%s %s(%p)", rEnt.IsNetworkClone() ? "CLONE" : "LOCAL",rEnt.GetModelName(), &rEnt);
						pCollection->AddString("Vehicle", debugbuffer);
						formatf(debugbuffer, "%s PED %s(%p)", rPed.IsNetworkClone() ? "CLONE" : "LOCAL", rPed.GetModelName(), &rPed);
						pCollection->AddString("Ped Unreserving Use Of Component", debugbuffer);
						if (rEnt.GetDrawable() && rEnt.GetDrawable()->GetSkeletonData() && pComponentReservation->GetBoneIndex() > 0 && pComponentReservation->GetBoneIndex() < rEnt.GetDrawable()->GetSkeletonData()->GetNumBones())
						{
							const char* boneName = rEnt.GetDrawable()->GetSkeletonData()->GetBoneData(pComponentReservation->GetBoneIndex())->GetName();
							formatf(debugbuffer, "%s(%i)", boneName, pComponentReservation->GetBoneIndex());
							pCollection->AddString("Component Bone Name", debugbuffer);
						}
					}
				}
			}
		}

		void RecordUpdatePedUsingComponentFromNetwork(const CComponentReservation* pComponentReservation, const CPed* pPed)
		{
			if (pComponentReservation && pComponentReservation->GetOwningEntity())
			{
				debugPlayback::FilterToEvent<CPedVehicleInteractionDebugEvent> filter;
				if (filter.IsSet())
				{
					const CDynamicEntity& rEnt = *pComponentReservation->GetOwningEntity();
					debugPlayback::SetTaggedDataCollectionEvent *pCollection 
						= debugPlayback::RecordDataCollection( rEnt.GetCurrentPhysicsInst(), "RecordUnReserveUseOfComponent");
					if (pCollection)
					{
						DR_MEMORY_HEAP();

						char debugbuffer[64];
						formatf(debugbuffer, "%s %s(%p)", rEnt.IsNetworkClone() ? "CLONE" : "LOCAL",rEnt.GetModelName(), &rEnt);
						pCollection->AddString("Vehicle", debugbuffer);
						formatf(debugbuffer, "%s PED %s(%p)", pPed ? (pPed->IsNetworkClone() ? "CLONE" : "LOCAL") : "NULL", pPed ? pPed->GetModelName() : "NULL", pPed);
						pCollection->AddString("Updating Ped Using Component From Network", debugbuffer);
						if (rEnt.GetDrawable() && rEnt.GetDrawable()->GetSkeletonData() && pComponentReservation->GetBoneIndex() > 0 && pComponentReservation->GetBoneIndex() < rEnt.GetDrawable()->GetSkeletonData()->GetNumBones())
						{
							const char* boneName = rEnt.GetDrawable()->GetSkeletonData()->GetBoneData(pComponentReservation->GetBoneIndex())->GetName();
							formatf(debugbuffer, "%s(%i)", boneName, pComponentReservation->GetBoneIndex());
							pCollection->AddString("Component Bone Name", debugbuffer);
						}
					}
				}
			}
		}
	}
}

//Included last to get all the declarations within the CPP file
#include "DebugVehicleInteraction_parser.h"

#endif //DR_ENABLED


