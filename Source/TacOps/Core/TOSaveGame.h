// TAC-OPS - persistent profile (credits, secure container, stats, settings, last loadout)

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Core/TOTypes.h"
#include "TOSaveGame.generated.h"

UCLASS()
class TACOPS_API UTOSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int32 Version = 1;
	UPROPERTY() int64 Credits = 400000;

	UPROPERTY() int32 Raids = 0;
	UPROPERTY() int32 Extractions = 0;
	UPROPERTY() int32 Kills = 0;
	UPROPERTY() int32 Deaths = 0;
	UPROPERTY() int64 TotalExtracted = 0;
	UPROPERTY() int64 BestRaid = 0;
	UPROPERTY() int32 WarfareMatches = 0;
	UPROPERTY() int32 WarfareWins = 0;

	/** Contents of the secure container survive death. */
	UPROPERTY() TArray<FTOItemInstance> SafeBoxItems;

	UPROPERTY() FTOLoadout Loadout;
	UPROPERTY() TArray<FTOWeaponConfig> Presets;
	UPROPERTY() ETOTimeOfDay LastTime = ETOTimeOfDay::Day;
	UPROPERTY() int32 LastDifficulty = 0;
	UPROPERTY() bool bLastSquad = true;
	UPROPERTY() int32 LastWarfareSide = 0;

	// Settings
	UPROPERTY() float MouseSensitivity = 1.f;
	UPROPERTY() float ADSSensitivity = 0.8f;
	UPROPERTY() float FieldOfView = 90.f;
	UPROPERTY() float MasterVolume = 0.8f;
	UPROPERTY() int32 GraphicsQuality = 3;
	UPROPERTY() bool bInvertY = false;
	UPROPERTY() bool bShowFPS = false;
	UPROPERTY() bool bToggleADS = false;
};
