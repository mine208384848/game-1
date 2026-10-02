// TAC-OPS - gameplay interfaces

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Core/TOTypes.h"
#include "TOInterfaces.generated.h"

class ATOCharacter;

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UTODamageable : public UInterface
{
	GENERATED_BODY()
};

/** Anything that can be hit by bullets / explosions. */
class TACOPS_API ITODamageable
{
	GENERATED_BODY()

public:
	virtual void ReceiveTODamage(const FTODamageInfo& Info) = 0;
};

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UTOInteractable : public UInterface
{
	GENERATED_BODY()
};

/** Anything the player can use with [F]: containers, doors, switches, downed allies. */
class TACOPS_API ITOInteractable
{
	GENERATED_BODY()

public:
	virtual FString GetInteractLabel(const ATOCharacter* User) const = 0;
	virtual bool CanInteract(const ATOCharacter* User) const { return true; }
	/** > 0 means the key must be held this long. */
	virtual float GetInteractDuration(const ATOCharacter* User) const { return 0.f; }
	virtual void Interact(ATOCharacter* User) = 0;
	/** Shown in yellow under the label (e.g. "Requires Admin Keycard"). */
	virtual FString GetInteractHint(const ATOCharacter* User) const { return FString(); }
};
