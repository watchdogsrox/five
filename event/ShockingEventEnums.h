#ifndef SHOCKING_EVENT_ENUMS_H
#define SHOCKING_EVENT_ENUMS_H

// TODO: Figure out if this will still be needed when the shocking events are in
// the global queue.

// Keep these in synch with the corresponding enum in event_enums.sch.
enum eSEventShockLevels
{
	EShockLevelNone = -1,
	EInteresting,
	EAffectsOthers,
	EPotentiallyDangerous,
	EDangerous,
	ESeriousDanger,

	ENumShockingLevels
};

#endif //#ifndef SHOCKING_EVENT_ENUMS_H


