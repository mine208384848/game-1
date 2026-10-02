// TAC-OPS - game mode
//
// One game mode drives the whole loop (lobby background, Operations raids and Warfare):
//   * builds the procedural world, sky, combat services and the runtime nav mesh bounds
//   * Operations (Hazard extraction): loot, guards per POI, commander boss, rival operator
//     squads that loot & extract, AI squad mates, raid timer, conditional extractions, economy
//   * Warfare (attack & defend): 4 sectors x 2 capture points, tickets, respawning bots
//
// Unity port mapping: GameManager.cs / WaveSpawner.cs / ScoreManager.cs -> ATOGameMode

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/TOTypes.h"
#include "TOGameMode.generated.h"

class ATOCharacter;
class ATOAIController;
class ATOWorldGenerator;
class ATOEnvironment;
class ATOCombatManager;
class ATODoor;
class ATOExtractionZone;
class ATOCapturePoint;
class ACameraActor;

struct FTOKillFeedEntry
{
	FString Killer;
	FString Victim;
	FString Weapon;
	bool bHeadshot = false;
	bool bDowned = false;
	int32 KillerTeam = -1;
	int32 VictimTeam = -1;
	float Time = 0.f;
};

struct FTOMessage
{
	FString Text;
	FLinearColor Color = FLinearColor::White;
	float Time = 0.f;
};

struct FTOSquadInfo
{
	int32 Id = 0;
	int32 Team = 0;
	ETOAIRole Role = ETOAIRole::Guard;
	TArray<TWeakObjectPtr<ATOCharacter>> Members;
	FVector Objective = FVector::ZeroVector;
	bool bHasObjective = false;
	bool bLoot = false;
	int32 Visits = 0;
	bool bExtracting = false;
	FVector ExtractLocation = FVector::ZeroVector;
	int32 Poi = -1;
	TArray<int32> VisitedPois;
};

struct FTOPendingRespawn
{
	int32 Team = 0;
	float Time = 0.f;
	ETOOperator Operator = ETOOperator::Viper;
	int32 SquadId = -1;
};

UENUM()
enum class ETOGMState : uint8
{
	Loading,
	Menu,
	Playing,
	Ended
};

UCLASS()
class TACOPS_API ATOGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ATOGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	static ATOGameMode* Get(const UObject* WorldContext);

	// World access -------------------------------------------------------------------------
	ATOWorldGenerator* GetWorldGen() const { return WorldGen; }
	ATOEnvironment* GetEnvironment() const { return Environment; }
	bool IsNight() const;
	ETOMatchMode GetMatchMode() const { return Mode; }
	ETOGMState GetState() const { return State; }
	bool GetWaterHeight(const FVector& Location, float& OutZ) const;
	const FTOSession& GetSession() const { return Session; }

	// Registries ---------------------------------------------------------------------------
	void RegisterCharacter(ATOCharacter* C);
	void UnregisterCharacter(ATOCharacter* C);
	const TArray<TWeakObjectPtr<ATOCharacter>>& GetCharacters() const { return Characters; }
	const TArray<TWeakObjectPtr<ATODoor>>& GetDoors() const { return Doors; }
	const TArray<TWeakObjectPtr<ATOExtractionZone>>& GetExtractionZones() const { return ExtractionZones; }
	const TArray<TWeakObjectPtr<ATOCapturePoint>>& GetCapturePoints() const { return CapturePoints; }
	ATOCharacter* GetPlayerCharacter() const;

	// Gameplay events ----------------------------------------------------------------------
	void ReportNoise(const FVector& Location, float Radius, ATOCharacter* Source, bool bGunshot);
	void ReportGrenade(const FVector& Location, int32 Team);
	void AlertSquad(int32 SquadId, ATOCharacter* Target, const FVector& LastKnown, ATOCharacter* Reporter);
	void RequestNextObjective(ATOAIController* AI);
	void OnAIExtracted(ATOCharacter* C);
	void OnCharacterKilled(ATOCharacter* Victim, ATOCharacter* Killer, const FTODamageInfo& Info);
	void OnCharacterDowned(ATOCharacter* Victim, const FTODamageInfo& Info);
	void OnCharacterRevived(ATOCharacter* Who, ATOCharacter* By);
	void RevealEnemiesAround(ATOCharacter* Source, float Radius, float Seconds);
	bool TryPayExtraction(ATOCharacter* Payer, int32 Cost);
	void PushMessage(const FString& Text, const FLinearColor& Color = FLinearColor::White);

	// Player flow --------------------------------------------------------------------------
	void PlayerGiveUp();
	void PlayerAbandonMatch();
	void PlayerRequestRespawn(ETOOperator Operator, const FTOWeaponConfig& Weapon);
	bool CanPlayerRespawn() const;
	float GetRespawnWait() const;
	bool IsPlayerAwaitingRespawn() const { return bPlayerAwaitingRespawn; }

	// HUD data -----------------------------------------------------------------------------
	TArray<FTOKillFeedEntry> KillFeed;
	TArray<FTOMessage> Messages;
	float GetRaidTimeLeft() const;
	float GetMatchTime() const;
	ATOExtractionZone* GetPlayerExtractZone() const { return PlayerExtractZone.Get(); }
	float GetExtractProgress() const { return ExtractProgress; }
	float GetExtractDuration() const { return 8.f; }
	FString GetExtractBlockReason() const { return ExtractBlockReason; }
	int64 GetPendingExtractionCost() const { return PendingExtractionCost; }
	FTOMatchResult Result;
	float GetLoadingProgress() const { return LoadingProgress; }

	// Warfare
	int32 GetTickets() const { return Tickets; }
	int32 GetMaxTickets() const { return MaxTickets; }
	int32 GetCurrentSector() const { return CurrentSector; }
	int32 GetNumSectors() const;
	FString GetSectorName() const;
	int32 GetAttackerTeam() const { return AttackerTeam; }
	int32 GetPlayerTeam() const { return PlayerTeam; }
	int32 GetTeamScore(int32 Team) const;

protected:
	// Setup
	void BuildWorld();
	void SpawnWorldActors();
	void SpawnNavBounds();
	void SetupMenu();
	void SetupOperations();
	void SetupWarfare();
	void FinishLoading();

	// Spawning
	ATOCharacter* SpawnSoldier(const FVector& Location, float Yaw, int32 Team, bool bPlayer);
	ATOCharacter* SpawnAI(const FVector& Location, float Yaw, int32 Team, ETOAIRole Role, int32 Tier, int32 SquadId, const TArray<FVector>& Patrol, const FString& Name);
	ATOCharacter* SpawnPlayer(const FVector& Location, float Yaw);
	FVector FindSpawnPoint(const FVector& Around, float Radius) const;
	int32 NewSquad(int32 Team, ETOAIRole Role, int32 Poi);
	FTOSquadInfo* FindSquad(int32 Id);

	// Operations
	void TickOperations(float Dt, float Now);
	void SpawnRivalSquad(int32 Index);
	void AssignRivalObjective(FTOSquadInfo& Squad);
	void FinishRaid(bool bExtracted, const FString& Title);

	// Warfare
	void TickWarfare(float Dt, float Now);
	void ActivateSector(int32 Index);
	void AssignWarfareObjective(ATOAIController* AI);
	void SpawnWarfareBot(int32 Team, ETOOperator Op, int32 SquadId);
	FVector GetTeamSpawn(int32 Team) const;
	void FinishWarfare(bool bAttackersWin);

	void TickMenuCamera(float Dt);

	UPROPERTY() TObjectPtr<ATOWorldGenerator> WorldGen;
	UPROPERTY() TObjectPtr<ATOEnvironment> Environment;
	UPROPERTY() TObjectPtr<ATOCombatManager> Combat;
	UPROPERTY() TObjectPtr<ACameraActor> MenuCamera;
	UPROPERTY() TObjectPtr<APlayerController> LocalPC;

	TArray<TWeakObjectPtr<ATOCharacter>> Characters;
	TArray<TWeakObjectPtr<ATODoor>> Doors;
	TArray<TWeakObjectPtr<ATOExtractionZone>> ExtractionZones;
	TArray<TWeakObjectPtr<ATOCapturePoint>> CapturePoints;
	TArray<FTOSquadInfo> Squads;
	TWeakObjectPtr<ATOCharacter> PlayerChar;

	FTOSession Session;
	ETOMatchMode Mode = ETOMatchMode::Menu;
	ETOGMState State = ETOGMState::Loading;
	float LoadingStart = 0.f;
	float LoadingProgress = 0.f;
	float MatchStart = 0.f;
	float RaidDuration = 1500.f;
	float EndTime = -1.f;
	float MenuOrbit = 0.f;

	// Operations
	TArray<float> RivalSpawnTimes;
	int32 RivalsSpawned = 0;
	float NextRivalCheck = 0.f;
	TWeakObjectPtr<ATOExtractionZone> PlayerExtractZone;
	float ExtractProgress = 0.f;
	FString ExtractBlockReason;
	int64 PendingExtractionCost = 0;
	float NextNoiseCleanup = 0.f;
	int32 PlayerSquadId = -1;

	// Warfare
	int32 AttackerTeam = 0;
	int32 PlayerTeam = 0;
	int32 Tickets = 250;
	int32 MaxTickets = 250;
	int32 CurrentSector = 0;
	float NextCaptureTick = 0.f;
	TArray<FTOPendingRespawn> PendingRespawns;
	bool bPlayerAwaitingRespawn = false;
	float PlayerDeathTime = -100.f;
	TArray<int32> TeamKills;
	int32 BotCounter = 0;
	TArray<int32> WarfareSquads;
};
