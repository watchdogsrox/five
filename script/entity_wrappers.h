//
// script/entity_wrappers.h : Wrappers to allow us to use entity pointers as script command params
//
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//
#ifndef INC_ENTITY_WRAPPERS_H_
#define INC_ENTITY_WRAPPERS_H_

// rage headers
#include "script/wrapper.h"
// framework header
#include "fwscript/scriptguid.h"
//game headers
#include "peds/ped.h"
#include "objects/object.h"
#include "vehicles/vehicle.h"

namespace rage {

	namespace scrWrapper{

		template<>
		struct Arg<fwEntity*> {
			inline static fwEntity* Value(scrThread::Info& info, int& N) {return fwScriptGuid::FromGuid<fwEntity>(info.Params[N++].Int);}
		};

		template<>
		struct Arg<CEntity*> {
			inline static CEntity* Value(scrThread::Info& info, int& N) {return fwScriptGuid::FromGuid<CEntity>(info.Params[N++].Int);}
		};

		template<>
		struct Arg<CPhysical*> {
			inline static CPhysical* Value(scrThread::Info& info, int& N) {return fwScriptGuid::FromGuid<CPhysical>(info.Params[N++].Int);}
		};

		template<>
		struct Arg<CPed*> {
			inline static CPed* Value(scrThread::Info& info, int& N) {return fwScriptGuid::FromGuid<CPed>(info.Params[N++].Int);}
		};

		template<>
		struct Arg<CVehicle*> {
			inline static CVehicle* Value(scrThread::Info& info, int& N) {return fwScriptGuid::FromGuid<CVehicle>(info.Params[N++].Int);}
		};

		template<>
		struct Arg<CObject*> {
			inline static CObject* Value(scrThread::Info& info, int& N) {return fwScriptGuid::FromGuid<CObject>(info.Params[N++].Int);}
		};

	} //scrWrapper

} //rage



#endif // !INC_ENTITY_WRAPPERS_H_