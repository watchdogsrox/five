
#ifndef _COMMANDS_FIRE_H_
#define _COMMANDS_FIRE_H_

namespace fire_commands
{
	void SetupScriptCommands();
	void CommandAddOwnedExplosion(int explosionOwner, const scrVector &scrVecCoors, int ExplosionTag, float sizeScale, bool makeSound, bool noFx, float camShakeMult);
}

#endif

