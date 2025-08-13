#ifndef NA_AUDIODEFINES_H
#define NA_AUDIODEFINES_H

#define NA_RADIO_ENABLED			1
#define NA_POLICESCANNER_ENABLED	1

#if NA_RADIO_ENABLED
#define NA_RADIO_ENABLED_ONLY(x) x
#define NA_RADIO_ENABLED_SWITCH(ifTrue,ifFalse) ifTrue
#else
#define NA_RADIO_ENABLED_ONLY(x)
#define NA_RADIO_ENABLED_SWITCH(ifTrue,ifFalse) ifFalse
#endif


#if NA_POLICESCANNER_ENABLED
#define NA_POLICESCANNER_ENABLED_ONLY(x) x
#else
#define NA_POLICESCANNER_ENABLED_ONLY(x)
#endif

#define ENTITY_SEED_PROB(r, x) (r <= int(RAND_MAX_16 * x))
#define VerifyExists(x, msg) if(!x) { naWarningf("%s", msg); }

class naWaveLoadPriority
{
public:
	enum Priority
	{
		General = 0,
		Speech,
		PlayerInteractive,
	};
};

#endif // NA_AUDIODEFINES_H
