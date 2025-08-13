/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    localisation.h
// PURPOSE : header file for localisation.cpp
// AUTHOR :  Derek Payne
// STARTED : 02/06/2003
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _LOCALISATION_H_
#define _LOCALISATION_H_

class CLocalisation
{
	public:

#if __BANK
	static	bool m_bForceGermanGame;
	static	bool m_bForceAussieGame;
	static int m_eForceLanguage;
	static void InitWidgets();
#endif

	static inline bool GermanGame()
	{	// PURPOSE  : Returns whether this is a german game or not
#if __BANK
		if (m_bForceGermanGame)
		{
			return true;
		}
#endif
#if defined(__GERMAN_BUILD) && __GERMAN_BUILD
		return true;
#else
		return false;
#endif
	}

	static inline bool AussieGame()
	{	// PURPOSE  : Returns whether this is a aussie game or not
#if __BANK
		if (m_bForceAussieGame)
		{
			return true;
		}
#endif
#if defined(__AUSSIE_BUILD) && __AUSSIE_BUILD
		return true;
#else
		return false;
#endif
	}

	static inline bool FrenchGame()
	{	// PURPOSE  : Returns whether this is a french game or not
		return false;
	}

	static inline bool Metric()
	{	// PURPOSE  : Returns whether the game is using metric measurements or imperial
		return true;  // yes its metric
	}

	static inline bool Blood()
	{	// PURPOSE  : Returns whether there is any blood in this game
		if (GermanGame())
		{
			return false;
		}
		
		if (AussieGame())
		{
			return false;
		}
		
		return true;
	}

	static inline bool Porn()
	{	// PURPOSE  : Returns whether there is any porn in this game
		// no porn in aussie version:
		if (AussieGame())
			return false;
		else
			// all other versions get porn:
			return true;
	}

	static inline bool ScreamsFromKills()
	{	// PURPOSE  : Returns whether there is any screams in this game
		if(GermanGame() || AussieGame())
			return false;
		else
			return true;
	}

	static inline bool Prostitutes()
	{	// PURPOSE  : Returns whether hookers can get in your car or not
		if (AussieGame())
			return false;  // no hookers down under
		else
			return true;  // prostitutes for the rest of us
	}

	static inline bool KickingWhenDown()
	{	// PURPOSE  : Returns whether player can kick peds when they are down
		if (GermanGame())
		{
			return false;
		}
		return true;
	}

	static inline bool ShootLimbs()
	{	// PURPOSE  : Returns whether player can shoot limbs & heads off
		if (GermanGame())
		{
			return false;
		}
		return true;
	}

	static inline bool KnockDownPeds()
	{	// PURPOSE  : Returns whether player can knock down peds in cars
		if (GermanGame())
		{
			return false;
		}
		return true;
	}

	static inline bool KillFrenzy()
	{	// PURPOSE  : Returns whether kill frenzy's should be in this game
		if (GermanGame())
		{
			return false;
		}
		return true;
	}

	static inline bool StealFromDeadPed()
	{	// PURPOSE  : Returns whether player can steal from dead peds or not
		if (GermanGame())
		{
			return false;
		}
		return true;
	}

	static inline bool PedsOnFire()
	{	// PURPOSE  : Returns whether peds can catch fire or not
		if (GermanGame())
		{
			return false;
		}
		return true;
	}

	static inline bool KillPeds()
	{	// PURPOSE  : can we actually make a ped "die" in this game
		if (GermanGame())
		{
			return false;
		}
		return true;
	}


	static inline bool PunishingWantedSystem()
	{	// PURPOSE  : can we actually make a ped "die" in this game
		return GermanGame();
	}
};

#endif  // _LOCALISATION_H_

