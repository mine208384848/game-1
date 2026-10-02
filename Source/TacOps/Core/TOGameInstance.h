// TAC-OPS - game instance: session hand-off between level loads, profile save, settings

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Core/TOTypes.h"
#include "TOGameInstance.generated.h"

class UTOSaveGame;

UCLASS()
class TACOPS_API UTOGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	static UTOGameInstance* Get(const UObject* WorldContext);

	UTOSaveGame* GetSave();
	void WriteSave();

	/** Deploys into a new match (reloads the level with the current session). */
	void StartMatch(ETOMatchMode Mode);
	void ReturnToLobby();
	void ApplyGraphicsQuality(int32 Quality);

	int64 GetCredits();
	bool SpendCredits(int64 Amount);
	void AddCredits(int64 Amount);

	/** Current / next match description. */
	UPROPERTY() FTOSession Session;
	/** Result of the last match (shown in the lobby). */
	UPROPERTY() FTOMatchResult LastResult;

	/** Loadout cost already paid for the running raid. */
	int64 PaidLoadoutCost = 0;

	static const TCHAR* SlotName() { return TEXT("TacOpsProfile"); }

private:
	UPROPERTY() TObjectPtr<UTOSaveGame> Save;
};
