// TAC-OPS - soldier AI
//
// A compact state machine (no behavior tree assets needed) driving guards, the boss and his
// escort, rival operator squads (PvP-like threat that loots and extracts), the player's AI
// squad mates and Warfare bots.  Perception uses sight (FOV + line of sight + target
// visibility factor + reaction time) and hearing (gunshots, footsteps, reloads).
//
// Unity port mapping: EnemyAI.cs (NavMeshAgent + raycast shooting) -> ATOAIController + UTOWeaponComponent

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Core/TOTypes.h"
#include "TOAIController.generated.h"

class ATOCharacter;

UENUM()
enum class ETOAIState : uint8
{
	Idle,
	Patrol,
	Follow,
	Objective,
	Investigate,
	Combat,
	Search,
	Revive,
	Evade,
	Extract,
	Loot
};

UCLASS()
class TACOPS_API ATOAIController : public AAIController
{
	GENERATED_BODY()

public:
	ATOAIController();

	virtual void Tick(float DeltaTime) override;
	virtual void OnPossess(APawn* InPawn) override;

	void Configure(ETOAIRole InRole, int32 InSquad, const FVector& InHome, const TArray<FVector>& InPatrol, float InSkill);
	void SetObjective(const FVector& Location, float Radius, bool bInLoot = false);
	void SetExtractTarget(const FVector& Location);
	void SetFollowLeader(ATOCharacter* Leader, int32 InFormationIndex);
	void CommandMoveTo(const FVector& Location);

	// Stimuli
	void OnDamaged(const FTODamageInfo& Info, ATOCharacter* Attacker);
	void OnHeardNoise(const FVector& Location, ATOCharacter* Source, bool bGunshot);
	void OnSuppressed(const FVector& From);
	void OnGrenadeNearby(const FVector& Location);
	void Blind(float Seconds);
	void ShareTarget(ATOCharacter* NewTarget, const FVector& LastKnown);

	ETOAIState GetState() const { return State; }
	ETOAIRole GetRole() const { return Role; }
	int32 GetSquadId() const { return SquadId; }
	ATOCharacter* GetTarget() const { return Target.Get(); }
	bool HasObjective() const { return bHasObjective; }
	bool IsAtObjective() const;
	float GetSkill() const { return Skill; }
	bool IsExtracting() const { return State == ETOAIState::Extract; }

private:
	ATOCharacter* GetChar() const;
	void UpdatePerception(float Now);
	void Think(float Now);
	void UpdateCombat(float Dt, float Now);
	void UpdateFocus(float Now);
	void UpdateDirectMove(float Dt);
	void CheckStuck(float Now);
	void EnterState(ETOAIState NewState, float Now);
	void EnterCombat(ATOCharacter* NewTarget, float Now);
	void ReturnToDefault(float Now);

	void MoveToPoint(const FVector& Goal, float Acceptance = 90.f, bool bSprint = false);
	void StopMove();
	bool HasReachedGoal() const;
	bool FindCover(const ATOCharacter* Threat, FVector& OutCover) const;
	bool ProjectToNav(const FVector& In, FVector& Out) const;
	bool HasLineOfSight(const FVector& From, const ATOCharacter* Other, FVector& OutVisiblePoint) const;
	void OpenNearbyDoors();

	ETOAIRole Role = ETOAIRole::Guard;
	ETOAIState State = ETOAIState::Idle;
	int32 SquadId = -1;
	float Skill = 0.5f;

	TWeakObjectPtr<ATOCharacter> Target;
	bool bTargetVisible = false;
	FVector LastKnownPos = FVector::ZeroVector;
	FVector TargetVisiblePoint = FVector::ZeroVector;
	float LastSeenTime = -100.f;
	float Awareness = 0.f;
	TWeakObjectPtr<ATOCharacter> AwarenessCandidate;
	float EngageStart = 0.f;
	float ReactionTime = 0.6f;

	FVector HomeLocation = FVector::ZeroVector;
	TArray<FVector> PatrolPoints;
	int32 PatrolIndex = 0;
	float WaitUntil = 0.f;

	FVector ObjectiveLocation = FVector::ZeroVector;
	float ObjectiveRadius = 800.f;
	bool bHasObjective = false;
	bool bObjectiveIsLoot = false;
	FVector ExtractLocation = FVector::ZeroVector;

	TWeakObjectPtr<ATOCharacter> FollowLeader;
	int32 FormationIndex = 0;
	TWeakObjectPtr<ATOCharacter> ReviveTarget;
	float ReviveStart = 0.f;

	FVector InvestigateLocation = FVector::ZeroVector;
	FVector LookAtPoint = FVector::ZeroVector;
	float NextLookChange = 0.f;
	float StateStart = 0.f;
	float NextThink = 0.f;
	float NextPerception = 0.f;
	float BlindUntil = -1.f;
	float SuppressedUntil = -1.f;
	FVector EvadeFrom = FVector::ZeroVector;

	// Movement
	FVector MoveGoal = FVector::ZeroVector;
	float MoveAcceptance = 90.f;
	bool bMoving = false;
	bool bDirectMove = false;
	FVector LastProgressPos = FVector::ZeroVector;
	float LastProgressTime = 0.f;
	int32 StuckCount = 0;
	float NextDoorCheck = 0.f;

	// Combat
	float NextTactic = 0.f;
	float BurstEnd = 0.f;
	float NextBurst = 0.f;
	FVector AimOffset = FVector::ZeroVector;
	bool bAimHead = false;
	float NextGrenade = 0.f;
	float NextHeal = 0.f;
	float NextAbility = 0.f;
	bool bInCover = false;
};
