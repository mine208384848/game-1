// TAC-OPS - soldier AI

#include "AI/TOAIController.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOHealthComponent.h"
#include "Characters/TOInventoryComponent.h"
#include "Weapons/TOWeaponComponent.h"
#include "Core/TOGameMode.h"
#include "Core/TODatabase.h"
#include "World/TODoor.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

ATOAIController::ATOAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bSetControlRotationFromPawnOrientation = true;
}

ATOCharacter* ATOAIController::GetChar() const
{
	return Cast<ATOCharacter>(GetPawn());
}

void ATOAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	HomeLocation = InPawn ? InPawn->GetActorLocation() : FVector::ZeroVector;
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	NextPerception = Now + FMath::FRandRange(0.f, 0.3f);
	NextThink = Now + FMath::FRandRange(0.f, 0.3f);
}

void ATOAIController::Configure(ETOAIRole InRole, int32 InSquad, const FVector& InHome, const TArray<FVector>& InPatrol, float InSkill)
{
	Role = InRole;
	SquadId = InSquad;
	HomeLocation = InHome;
	PatrolPoints = InPatrol;
	Skill = FMath::Clamp(InSkill, 0.f, 1.f);
	ReactionTime = FMath::Lerp(0.95f, 0.3f, Skill);
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	switch (Role)
	{
	case ETOAIRole::Teammate: EnterState(ETOAIState::Follow, Now); break;
	case ETOAIRole::Rival:
	case ETOAIRole::WarfareBot: EnterState(ETOAIState::Objective, Now); break;
	default: EnterState(PatrolPoints.Num() > 1 ? ETOAIState::Patrol : ETOAIState::Idle, Now); break;
	}
}

void ATOAIController::SetObjective(const FVector& Location, float Radius, bool bInLoot)
{
	ObjectiveLocation = Location;
	ObjectiveRadius = Radius;
	bHasObjective = true;
	bObjectiveIsLoot = bInLoot;
	if (State == ETOAIState::Objective || State == ETOAIState::Loot || State == ETOAIState::Idle)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		EnterState(ETOAIState::Objective, Now);
	}
}

void ATOAIController::SetExtractTarget(const FVector& Location)
{
	ExtractLocation = Location;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (State != ETOAIState::Combat)
	{
		EnterState(ETOAIState::Extract, Now);
	}
	else
	{
		bHasObjective = false;
		Role = ETOAIRole::Rival;
	}
}

void ATOAIController::SetFollowLeader(ATOCharacter* Leader, int32 InFormationIndex)
{
	FollowLeader = Leader;
	FormationIndex = InFormationIndex;
}

void ATOAIController::CommandMoveTo(const FVector& Location)
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	InvestigateLocation = Location;
	EnterState(ETOAIState::Investigate, Now);
	MoveToPoint(Location, 120.f, true);
}

bool ATOAIController::IsAtObjective() const
{
	const ATOCharacter* Me = GetChar();
	return Me && bHasObjective && FVector::Dist2D(Me->GetActorLocation(), ObjectiveLocation) < ObjectiveRadius;
}

// ---------------------------------------------------------------------------------------------
//  Movement helpers
// ---------------------------------------------------------------------------------------------

bool ATOAIController::ProjectToNav(const FVector& In, FVector& Out) const
{
	UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Nav)
	{
		return false;
	}
	FNavLocation Loc;
	if (Nav->ProjectPointToNavigation(In, Loc, FVector(300.f, 300.f, 400.f)))
	{
		Out = Loc.Location;
		return true;
	}
	return false;
}

void ATOAIController::MoveToPoint(const FVector& Goal, float Acceptance, bool bSprint)
{
	ATOCharacter* Me = GetChar();
	if (!Me)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	MoveGoal = Goal;
	MoveAcceptance = Acceptance;
	bMoving = true;
	LastProgressPos = Me->GetActorLocation();
	LastProgressTime = Now;
	Me->SetWantsSprint(bSprint);

	const EPathFollowingRequestResult::Type Result = MoveToLocation(Goal, Acceptance, true, true, true, true, nullptr, true);
	if (Result == EPathFollowingRequestResult::Failed)
	{
		bDirectMove = true;
	}
	else if (Result == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		bDirectMove = false;
		bMoving = false;
	}
	else
	{
		bDirectMove = false;
	}
}

void ATOAIController::StopMove()
{
	StopMovement();
	bMoving = false;
	bDirectMove = false;
	if (ATOCharacter* Me = GetChar())
	{
		Me->SetWantsSprint(false);
	}
}

bool ATOAIController::HasReachedGoal() const
{
	const ATOCharacter* Me = GetChar();
	if (!Me)
	{
		return true;
	}
	if (!bMoving)
	{
		return true;
	}
	if (FVector::Dist2D(Me->GetActorLocation(), MoveGoal) < MoveAcceptance + 60.f)
	{
		return true;
	}
	if (!bDirectMove && GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		return true;
	}
	return false;
}

void ATOAIController::UpdateDirectMove(float Dt)
{
	ATOCharacter* Me = GetChar();
	if (!Me || !bMoving || !bDirectMove)
	{
		return;
	}
	FVector Dir = (MoveGoal - Me->GetActorLocation()).GetSafeNormal2D();
	if (Dir.IsNearlyZero())
	{
		return;
	}
	// Whisker avoidance
	UWorld* World = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TOAISteer), false, Me);
	const FVector Origin = Me->GetActorLocation() - FVector(0.f, 0.f, 30.f);
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Origin, Origin + Dir * 160.f, ECC_Visibility, Params))
	{
		static const float Angles[] = { 40.f, -40.f, 80.f, -80.f, 120.f, -120.f };
		for (float A : Angles)
		{
			const FVector Alt = Dir.RotateAngleAxis(A, FVector::UpVector);
			if (!World->LineTraceSingleByChannel(Hit, Origin, Origin + Alt * 160.f, ECC_Visibility, Params))
			{
				Dir = Alt;
				break;
			}
		}
	}
	Me->AddMovementInput(Dir, 1.f);
}

void ATOAIController::CheckStuck(float Now)
{
	ATOCharacter* Me = GetChar();
	if (!Me || !bMoving)
	{
		StuckCount = 0;
		return;
	}
	if (Now - LastProgressTime < 1.5f)
	{
		return;
	}
	const float Moved = FVector::Dist2D(Me->GetActorLocation(), LastProgressPos);
	LastProgressPos = Me->GetActorLocation();
	LastProgressTime = Now;
	if (Moved > 60.f)
	{
		StuckCount = 0;
		return;
	}
	++StuckCount;
	if (!bDirectMove && StuckCount >= 1)
	{
		// Nav mesh may not be built here yet: steer directly.
		StopMovement();
		bDirectMove = true;
	}
	else if (StuckCount >= 3)
	{
		// Try a jump / give up on this goal.
		Me->InputJump();
		if (StuckCount >= 5)
		{
			StopMove();
			StuckCount = 0;
		}
	}
}

void ATOAIController::OpenNearbyDoors()
{
	ATOCharacter* Me = GetChar();
	ATOGameMode* GM = ATOGameMode::Get(this);
	if (!Me || !GM)
	{
		return;
	}
	for (const TWeakObjectPtr<ATODoor>& Ptr : GM->GetDoors())
	{
		ATODoor* D = Ptr.Get();
		if (!D || D->IsOpen() || D->IsLocked())
		{
			continue;
		}
		if (FVector::DistSquared(D->GetActorLocation(), Me->GetActorLocation()) < FMath::Square(220.f))
		{
			D->OpenFor(Me);
		}
	}
}

// ---------------------------------------------------------------------------------------------
//  Perception
// ---------------------------------------------------------------------------------------------

bool ATOAIController::HasLineOfSight(const FVector& From, const ATOCharacter* Other, FVector& OutVisiblePoint) const
{
	UWorld* World = GetWorld();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TOAISight), false, GetPawn());
	Params.AddIgnoredActor(Other);
	const FVector Points[] = { Other->GetHeadLocation(), Other->GetChestLocation(), Other->GetActorLocation() - FVector(0.f, 0.f, 50.f) };
	for (const FVector& P : Points)
	{
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, From, P, ECC_Visibility, Params))
		{
			OutVisiblePoint = P;
			return true;
		}
	}
	return false;
}

void ATOAIController::UpdatePerception(float Now)
{
	ATOCharacter* Me = GetChar();
	ATOGameMode* GM = ATOGameMode::Get(this);
	if (!Me || !GM)
	{
		return;
	}
	if (Now < BlindUntil)
	{
		bTargetVisible = false;
		return;
	}

	const bool bNight = GM->IsNight();
	float SightRange = (Role == ETOAIRole::Rival ? 12000.f : (Role == ETOAIRole::WarfareBot ? 11000.f : 9000.f));
	if (bNight)
	{
		SightRange *= 0.6f;
	}
	const FVector Eye = Me->GetEyeLocation();
	const FVector Fwd = Me->GetControlRotation().Vector();
	const bool bAlert = State == ETOAIState::Combat || State == ETOAIState::Search || State == ETOAIState::Investigate;
	const float FovCos = bAlert ? FMath::Cos(FMath::DegreesToRadians(85.f)) : FMath::Cos(FMath::DegreesToRadians(62.f));

	ATOCharacter* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	FVector BestPoint = FVector::ZeroVector;
	float BestDist = 0.f;
	float BestRange = 1.f;

	for (const TWeakObjectPtr<ATOCharacter>& Ptr : GM->GetCharacters())
	{
		ATOCharacter* C = Ptr.Get();
		if (!C || C == Me || C->IsDeadState() || !Me->IsHostileTo(C))
		{
			continue;
		}
		const FVector To = C->GetChestLocation() - Eye;
		const float Dist = To.Size();
		const float Range = SightRange * C->GetVisibilityFactor();
		if (Dist > Range)
		{
			continue;
		}
		if (Dist > 500.f && FVector::DotProduct(Fwd, To / FMath::Max(Dist, 1.f)) < FovCos)
		{
			continue;
		}
		FVector VisPoint;
		if (!HasLineOfSight(Eye, C, VisPoint))
		{
			continue;
		}
		float Score = Dist * (C == Target.Get() ? 0.6f : 1.f) * (C->IsDowned() ? 2.5f : 1.f);
		if (C->IsPlayerCharacter())
		{
			Score *= 0.9f;
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = C;
			BestPoint = VisPoint;
			BestDist = Dist;
			BestRange = Range;
		}
	}

	bTargetVisible = false;
	if (!Best)
	{
		Awareness = FMath::Max(0.f, Awareness - 0.1f);
		return;
	}

	if (Best == Target.Get() && State == ETOAIState::Combat)
	{
		bTargetVisible = true;
		LastKnownPos = Best->GetActorLocation();
		TargetVisiblePoint = BestPoint;
		LastSeenTime = Now;
		return;
	}

	// Build awareness before reacting (stealthy players can slip by distant guards).
	if (AwarenessCandidate.Get() != Best)
	{
		AwarenessCandidate = Best;
		Awareness = 0.f;
	}
	const float Rate = FMath::Max(0.25f, 1.7f - BestDist / FMath::Max(BestRange, 1.f)) * (0.6f + Skill) * (bAlert ? 3.f : 1.f);
	Awareness += 0.25f * Rate;
	if (Awareness >= 1.f || BestDist < 600.f || State == ETOAIState::Combat)
	{
		Awareness = 0.f;
		EnterCombat(Best, Now);
		bTargetVisible = true;
		TargetVisiblePoint = BestPoint;
		LastKnownPos = Best->GetActorLocation();
		LastSeenTime = Now;
		GM->AlertSquad(SquadId, Best, LastKnownPos, Me);
	}
	else if (Awareness > 0.45f && State != ETOAIState::Investigate)
	{
		InvestigateLocation = Best->GetActorLocation();
		LookAtPoint = InvestigateLocation;
		EnterState(ETOAIState::Investigate, Now);
	}
}

// ---------------------------------------------------------------------------------------------
//  Stimuli
// ---------------------------------------------------------------------------------------------

void ATOAIController::EnterCombat(ATOCharacter* NewTarget, float Now)
{
	if (!NewTarget)
	{
		return;
	}
	const bool bNew = Target.Get() != NewTarget || State != ETOAIState::Combat;
	Target = NewTarget;
	LastKnownPos = NewTarget->GetActorLocation();
	if (bNew)
	{
		EngageStart = Now + ReactionTime * FMath::FRandRange(0.8f, 1.3f);
		NextTactic = Now + FMath::FRandRange(0.3f, 1.2f);
		NextBurst = EngageStart;
	}
	if (State != ETOAIState::Combat)
	{
		EnterState(ETOAIState::Combat, Now);
	}
}

void ATOAIController::ShareTarget(ATOCharacter* NewTarget, const FVector& LastKnown)
{
	if (!NewTarget || State == ETOAIState::Combat || State == ETOAIState::Extract)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	Target = NewTarget;
	LastKnownPos = LastKnown;
	LastSeenTime = Now - 2.f;
	EngageStart = Now + ReactionTime;
	EnterState(ETOAIState::Combat, Now);
}

void ATOAIController::OnDamaged(const FTODamageInfo& Info, ATOCharacter* Attacker)
{
	ATOCharacter* Me = GetChar();
	if (!Me || !Attacker || !Me->IsHostileTo(Attacker))
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (State != ETOAIState::Combat || !bTargetVisible)
	{
		EnterCombat(Attacker, Now);
		LastSeenTime = Now - 1.f;
		LookAtPoint = Attacker->GetActorLocation();
	}
	SuppressedUntil = Now + 1.2f;
	NextTactic = FMath::Min(NextTactic, Now + 0.3f);
	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		GM->AlertSquad(SquadId, Attacker, Attacker->GetActorLocation(), Me);
	}
}

void ATOAIController::OnHeardNoise(const FVector& Location, ATOCharacter* Source, bool bGunshot)
{
	ATOCharacter* Me = GetChar();
	if (!Me || !Source || Source == Me)
	{
		return;
	}
	if (State == ETOAIState::Combat && bTargetVisible)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (!Me->IsHostileTo(Source))
	{
		// Friendly gunfire: if they have a target, come help.
		if (bGunshot)
		{
			if (ATOAIController* Other = Cast<ATOAIController>(Source->GetController()))
			{
				if (Other->GetTarget() && Me->IsHostileTo(Other->GetTarget()) && State != ETOAIState::Combat && FVector::Dist(Location, Me->GetActorLocation()) < 6000.f)
				{
					ShareTarget(Other->GetTarget(), Other->GetTarget()->GetActorLocation());
				}
			}
		}
		return;
	}
	if (State == ETOAIState::Combat)
	{
		LastKnownPos = Source->GetActorLocation();
		return;
	}
	if (State == ETOAIState::Extract && !bGunshot)
	{
		return;
	}
	InvestigateLocation = bGunshot ? Source->GetActorLocation() : Location;
	LookAtPoint = InvestigateLocation;
	if (bGunshot && FVector::Dist(Location, Me->GetActorLocation()) < 2500.f)
	{
		// Close gunfire: treat as contact.
		ShareTarget(Source, Source->GetActorLocation());
		return;
	}
	EnterState(ETOAIState::Investigate, Now);
}

void ATOAIController::OnSuppressed(const FVector& From)
{
	const float Now = GetWorld()->GetTimeSeconds();
	SuppressedUntil = Now + 1.5f;
	if (State != ETOAIState::Combat)
	{
		LookAtPoint = From;
		InvestigateLocation = From;
		EnterState(ETOAIState::Investigate, Now);
	}
}

void ATOAIController::OnGrenadeNearby(const FVector& Location)
{
	ATOCharacter* Me = GetChar();
	if (!Me || State == ETOAIState::Evade)
	{
		return;
	}
	if (FVector::Dist(Location, Me->GetActorLocation()) > 900.f)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	EvadeFrom = Location;
	EnterState(ETOAIState::Evade, Now);
	const FVector Away = (Me->GetActorLocation() - Location).GetSafeNormal2D();
	FVector Goal = Me->GetActorLocation() + Away * 900.f;
	ProjectToNav(Goal, Goal);
	MoveToPoint(Goal, 80.f, true);
}

void ATOAIController::Blind(float Seconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	BlindUntil = FMath::Max(BlindUntil, Now + Seconds);
	if (ATOCharacter* Me = GetChar())
	{
		Me->GetWeapons()->SetTrigger(false);
	}
}

// ---------------------------------------------------------------------------------------------
//  State machine
// ---------------------------------------------------------------------------------------------

void ATOAIController::EnterState(ETOAIState NewState, float Now)
{
	State = NewState;
	StateStart = Now;
	WaitUntil = 0.f;
	if (NewState != ETOAIState::Combat)
	{
		bTargetVisible = false;
		if (ATOCharacter* Me = GetChar())
		{
			Me->GetWeapons()->SetTrigger(false);
			Me->GetWeapons()->SetAiming(false);
		}
	}
	switch (NewState)
	{
	case ETOAIState::Investigate:
		MoveToPoint(InvestigateLocation, 150.f, false);
		break;
	case ETOAIState::Search:
		MoveToPoint(LastKnownPos, 150.f, false);
		break;
	case ETOAIState::Objective:
		if (bHasObjective)
		{
			FVector Goal = ObjectiveLocation + FVector(FMath::FRandRange(-0.4f, 0.4f) * ObjectiveRadius, FMath::FRandRange(-0.4f, 0.4f) * ObjectiveRadius, 0.f);
			ProjectToNav(Goal, Goal);
			ATOCharacter* Me = GetChar();
			const bool bFar = Me && FVector::Dist2D(Me->GetActorLocation(), Goal) > 3000.f;
			MoveToPoint(Goal, 150.f, bFar);
		}
		break;
	case ETOAIState::Extract:
		MoveToPoint(ExtractLocation, 250.f, true);
		break;
	default:
		break;
	}
}

void ATOAIController::ReturnToDefault(float Now)
{
	Target = nullptr;
	bTargetVisible = false;
	ATOCharacter* Me = GetChar();
	if (Me && Me->GetStance() != ETOStance::Stand)
	{
		Me->SetStance(ETOStance::Stand);
	}
	switch (Role)
	{
	case ETOAIRole::Teammate:
		EnterState(ETOAIState::Follow, Now);
		break;
	case ETOAIRole::Rival:
		if (!ExtractLocation.IsZero() && !bHasObjective)
		{
			EnterState(ETOAIState::Extract, Now);
		}
		else
		{
			EnterState(ETOAIState::Objective, Now);
		}
		break;
	case ETOAIRole::WarfareBot:
		EnterState(ETOAIState::Objective, Now);
		break;
	default:
		EnterState(PatrolPoints.Num() > 1 ? ETOAIState::Patrol : ETOAIState::Idle, Now);
		if (Me && FVector::Dist(Me->GetActorLocation(), HomeLocation) > 600.f && PatrolPoints.Num() <= 1)
		{
			MoveToPoint(HomeLocation, 100.f, false);
		}
		break;
	}
}

void ATOAIController::Think(float Now)
{
	ATOCharacter* Me = GetChar();
	ATOGameMode* GM = ATOGameMode::Get(this);
	if (!Me || !GM)
	{
		return;
	}

	CheckStuck(Now);
	if (Now >= NextDoorCheck)
	{
		NextDoorCheck = Now + 0.5f;
		OpenNearbyDoors();
	}

	// Self care: bandage / heal out of combat or in cover
	UTOHealthComponent* H = Me->GetHealth();
	if (Now > NextHeal && !Me->IsUsingItem() && (H->IsBleeding() || H->GetHealth01() < 0.45f) && (!bTargetVisible || bInCover))
	{
		NextHeal = Now + 6.f;
		Me->InputQuickHeal();
	}

	// Squad mates revive downed teammates (including the player).
	if ((Role == ETOAIRole::Teammate || Role == ETOAIRole::WarfareBot) && State != ETOAIState::Revive && !bTargetVisible)
	{
		for (const TWeakObjectPtr<ATOCharacter>& Ptr : GM->GetCharacters())
		{
			ATOCharacter* C = Ptr.Get();
			if (C && C != Me && C->IsDowned() && !Me->IsHostileTo(C) && FVector::Dist(C->GetActorLocation(), Me->GetActorLocation()) < 3500.f)
			{
				ReviveTarget = C;
				EnterState(ETOAIState::Revive, Now);
				MoveToPoint(C->GetActorLocation(), 100.f, true);
				break;
			}
		}
	}

	switch (State)
	{
	case ETOAIState::Idle:
	{
		if (Now > NextLookChange)
		{
			NextLookChange = Now + FMath::FRandRange(2.f, 5.f);
			LookAtPoint = Me->GetActorLocation() + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() * 1000.f;
		}
		if (bMoving && HasReachedGoal())
		{
			StopMove();
		}
		break;
	}
	case ETOAIState::Patrol:
	{
		if (PatrolPoints.Num() == 0)
		{
			EnterState(ETOAIState::Idle, Now);
			break;
		}
		if (!bMoving || HasReachedGoal())
		{
			if (WaitUntil <= 0.f)
			{
				StopMove();
				WaitUntil = Now + FMath::FRandRange(3.f, 9.f);
				LookAtPoint = Me->GetActorLocation() + Me->GetActorForwardVector().RotateAngleAxis(FMath::FRandRange(-120.f, 120.f), FVector::UpVector) * 1000.f;
			}
			else if (Now >= WaitUntil)
			{
				WaitUntil = 0.f;
				PatrolIndex = (PatrolIndex + 1) % PatrolPoints.Num();
				MoveToPoint(PatrolPoints[PatrolIndex], 100.f, false);
			}
		}
		break;
	}
	case ETOAIState::Follow:
	{
		ATOCharacter* Leader = FollowLeader.Get();
		if (!Leader || Leader->IsDeadState())
		{
			// Leader is gone: hold position and fight.
			if (bMoving && HasReachedGoal()) StopMove();
			break;
		}
		const FRotator LeaderYaw(0.f, Leader->GetActorRotation().Yaw, 0.f);
		const FVector Offset = FormationIndex == 0 ? FVector(-350.f, -320.f, 0.f) : FVector(-420.f, 300.f, 0.f);
		FVector Desired = Leader->GetActorLocation() + LeaderYaw.RotateVector(Offset);
		const float Dist = FVector::Dist2D(Me->GetActorLocation(), Desired);
		if (Dist > 9000.f)
		{
			// Catch up if hopelessly separated.
			ProjectToNav(Desired, Desired);
			Me->SetActorLocation(Desired + FVector(0.f, 0.f, 90.f), false, nullptr, ETeleportType::TeleportPhysics);
			StopMove();
		}
		else if (Dist > 450.f)
		{
			if (!bMoving || FVector::Dist2D(MoveGoal, Desired) > 300.f)
			{
				MoveToPoint(Desired, 120.f, Dist > 1400.f || Leader->IsSprinting());
			}
		}
		else if (bMoving && HasReachedGoal())
		{
			StopMove();
		}
		// Mirror the leader's stance.
		if (Me->GetStance() != Leader->GetStance() && !Leader->IsDowned())
		{
			Me->SetStance(Leader->GetStance() == ETOStance::Prone ? ETOStance::Crouch : Leader->GetStance());
		}
		LookAtPoint = Leader->GetActorLocation() + Leader->GetActorForwardVector() * 2000.f + LeaderYaw.RotateVector(Offset) * 3.f;
		break;
	}
	case ETOAIState::Objective:
	{
		if (!bHasObjective)
		{
			GM->RequestNextObjective(this);
			break;
		}
		if (!bMoving || HasReachedGoal())
		{
			if (IsAtObjective())
			{
				if (bObjectiveIsLoot)
				{
					EnterState(ETOAIState::Loot, Now);
					StopMove();
					Me->SetStance(ETOStance::Crouch);
					break;
				}
				// Hold the objective: reposition inside it from time to time.
				if (WaitUntil <= 0.f)
				{
					WaitUntil = Now + FMath::FRandRange(5.f, 12.f);
					StopMove();
					if (FMath::FRand() < 0.4f) Me->SetStance(ETOStance::Crouch);
					LookAtPoint = Me->GetActorLocation() + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() * 1500.f;
				}
				else if (Now > WaitUntil)
				{
					WaitUntil = 0.f;
					Me->SetStance(ETOStance::Stand);
					FVector Goal = ObjectiveLocation + FVector(FMath::FRandRange(-0.7f, 0.7f) * ObjectiveRadius, FMath::FRandRange(-0.7f, 0.7f) * ObjectiveRadius, 0.f);
					ProjectToNav(Goal, Goal);
					MoveToPoint(Goal, 120.f, false);
				}
			}
			else
			{
				EnterState(ETOAIState::Objective, Now);
			}
		}
		break;
	}
	case ETOAIState::Loot:
	{
		if (Now - StateStart > 7.f)
		{
			bHasObjective = false;
			Me->SetStance(ETOStance::Stand);
			GM->RequestNextObjective(this);
			if (State == ETOAIState::Loot)
			{
				EnterState(ETOAIState::Objective, Now);
			}
		}
		break;
	}
	case ETOAIState::Investigate:
	{
		if (HasReachedGoal())
		{
			if (WaitUntil <= 0.f)
			{
				StopMove();
				WaitUntil = Now + FMath::FRandRange(4.f, 7.f);
			}
			if (Now > NextLookChange)
			{
				NextLookChange = Now + 1.5f;
				LookAtPoint = Me->GetActorLocation() + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() * 1500.f;
			}
			if (Now > WaitUntil)
			{
				ReturnToDefault(Now);
			}
		}
		else if (Now - StateStart > 30.f)
		{
			ReturnToDefault(Now);
		}
		break;
	}
	case ETOAIState::Search:
	{
		if (HasReachedGoal() || Now - StateStart > 25.f)
		{
			if (WaitUntil <= 0.f)
			{
				StopMove();
				WaitUntil = Now + FMath::FRandRange(5.f, 9.f);
			}
			if (Now > NextLookChange)
			{
				NextLookChange = Now + 1.4f;
				LookAtPoint = Me->GetActorLocation() + FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), 0.f).GetSafeNormal() * 1500.f;
			}
			if (Now > WaitUntil)
			{
				ReturnToDefault(Now);
			}
		}
		break;
	}
	case ETOAIState::Revive:
	{
		ATOCharacter* T = ReviveTarget.Get();
		if (!T || !T->IsDowned())
		{
			ReviveTarget = nullptr;
			ReturnToDefault(Now);
			break;
		}
		if (FVector::Dist2D(T->GetActorLocation(), Me->GetActorLocation()) < 200.f)
		{
			if (bMoving)
			{
				StopMove();
				ReviveStart = Now;
				Me->SetStance(ETOStance::Crouch);
			}
			LookAtPoint = T->GetActorLocation();
			if (Now - ReviveStart > 4.5f)
			{
				T->Revive(Me);
				ReviveTarget = nullptr;
				ReturnToDefault(Now);
			}
		}
		else if (!bMoving || HasReachedGoal())
		{
			MoveToPoint(T->GetActorLocation(), 100.f, true);
			ReviveStart = Now;
		}
		break;
	}
	case ETOAIState::Evade:
	{
		if (HasReachedGoal() || Now - StateStart > 3.5f)
		{
			if (Target.IsValid())
			{
				EnterState(ETOAIState::Combat, Now);
			}
			else
			{
				ReturnToDefault(Now);
			}
		}
		break;
	}
	case ETOAIState::Extract:
	{
		if (FVector::Dist2D(Me->GetActorLocation(), ExtractLocation) < 600.f)
		{
			GM->OnAIExtracted(Me);
			return;
		}
		if (!bMoving || HasReachedGoal())
		{
			MoveToPoint(ExtractLocation, 250.f, true);
		}
		break;
	}
	case ETOAIState::Combat:
	{
		ATOCharacter* T = Target.Get();
		if (!T || T->IsDeadState())
		{
			Target = nullptr;
			Me->GetWeapons()->SetTrigger(false);
			EnterState(ETOAIState::Search, Now);
			LastKnownPos = Me->GetActorLocation();
			WaitUntil = Now + 2.f;
			break;
		}
		if (!bTargetVisible && Now - LastSeenTime > 5.f)
		{
			// Lost contact: maybe flush them out with a grenade, then search.
			const float D = FVector::Dist(LastKnownPos, Me->GetActorLocation());
			if (Now > NextGrenade && D > 900.f && D < 3000.f && Me->GetInventory()->CountGrenades(ETOGrenadeType::Frag) > 0 && FMath::FRand() < 0.35f)
			{
				NextGrenade = Now + 20.f;
				Me->ThrowGrenadeAt(ETOGrenadeType::Frag, LastKnownPos);
			}
			// Guards don't chase forever.
			if ((Role == ETOAIRole::Guard || Role == ETOAIRole::Boss) && FVector::Dist(LastKnownPos, HomeLocation) > 7000.f)
			{
				ReturnToDefault(Now);
				MoveToPoint(HomeLocation, 150.f, false);
				break;
			}
			EnterState(ETOAIState::Search, Now);
			break;
		}

		// Tactical movement
		if (Now >= NextTactic)
		{
			NextTactic = Now + FMath::FRandRange(1.8f, 3.6f);
			const float Dist = FVector::Dist(T->GetActorLocation(), Me->GetActorLocation());
			const FTOWeaponStats& WS = Me->GetWeapons()->GetStats();
			const float Engage = FMath::Max(2500.f, (WS.Def ? WS.Range : 50.f) * 100.f * 1.4f);
			const bool bHurt = Me->GetHealth()->GetHealth01() < 0.4f;
			const bool bSuppressed = Now < SuppressedUntil;

			if (Role == ETOAIRole::Teammate && FollowLeader.IsValid() && FVector::Dist(FollowLeader->GetActorLocation(), Me->GetActorLocation()) > 2500.f)
			{
				MoveToPoint(FollowLeader->GetActorLocation(), 300.f, true);
			}
			else if ((bHurt || bSuppressed || FMath::FRand() < 0.35f) && !bInCover)
			{
				FVector Cover;
				if (FindCover(T, Cover))
				{
					MoveToPoint(Cover, 60.f, bHurt);
					bInCover = true;
				}
				else
				{
					Me->SetStance(ETOStance::Crouch);
				}
			}
			else if (!bTargetVisible || Dist > Engage)
			{
				// Push towards the last known position.
				FVector Goal = Me->GetActorLocation() + (LastKnownPos - Me->GetActorLocation()) * 0.6f;
				ProjectToNav(Goal, Goal);
				Me->SetStance(ETOStance::Stand);
				MoveToPoint(Goal, 120.f, Dist > Engage * 1.5f);
				bInCover = false;
			}
			else
			{
				// Strafe / peek
				bInCover = false;
				const FVector ToT = (T->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D();
				const FVector Side = FVector::CrossProduct(ToT, FVector::UpVector) * (FMath::RandBool() ? 1.f : -1.f);
				FVector Goal = Me->GetActorLocation() + Side * FMath::FRandRange(250.f, 600.f);
				if (Dist < 700.f)
				{
					Goal -= ToT * 300.f;
				}
				ProjectToNav(Goal, Goal);
				if (Dist > 2500.f && FMath::FRand() < 0.5f)
				{
					StopMove();
					Me->SetStance(Me->GetWeapons()->GetStats().Def && Me->GetWeapons()->GetStats().Def->Class == ETOWeaponClass::Sniper ? ETOStance::Prone : ETOStance::Crouch);
				}
				else
				{
					Me->SetStance(ETOStance::Stand);
					MoveToPoint(Goal, 60.f, false);
				}
			}

			// Operator abilities for bots
			if (Now > NextAbility && Me->GetAbilityCooldownRemaining() <= 0.f && (Role == ETOAIRole::Teammate || Role == ETOAIRole::WarfareBot || Role == ETOAIRole::Rival))
			{
				NextAbility = Now + FMath::FRandRange(15.f, 35.f);
				Me->InputAbility();
			}
		}
		break;
	}
	default:
		break;
	}
}

bool ATOAIController::FindCover(const ATOCharacter* Threat, FVector& OutCover) const
{
	const ATOCharacter* Me = GetChar();
	UWorld* World = GetWorld();
	if (!Me || !Threat || !World)
	{
		return false;
	}
	const FVector MyLoc = Me->GetActorLocation();
	const FVector ThreatEye = Threat->GetEyeLocation();
	const float ThreatDist = FVector::Dist(ThreatEye, MyLoc);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TOCover), false, Me);
	Params.AddIgnoredActor(Threat);

	float BestScore = TNumericLimits<float>::Max();
	bool bFound = false;
	for (int32 i = 0; i < 14; ++i)
	{
		const float Angle = (360.f / 14.f) * i + FMath::FRandRange(-10.f, 10.f);
		const float R = FMath::FRandRange(500.f, 1500.f);
		FVector P = MyLoc + FVector(1.f, 0.f, 0.f).RotateAngleAxis(Angle, FVector::UpVector) * R;
		if (!ProjectToNav(P, P))
		{
			// No nav data: ground trace instead.
			FHitResult Ground;
			if (!World->LineTraceSingleByChannel(Ground, P + FVector(0.f, 0.f, 500.f), P - FVector(0.f, 0.f, 1500.f), ECC_Visibility, Params))
			{
				continue;
			}
			P = Ground.ImpactPoint + FVector(0.f, 0.f, 90.f);
		}
		const float DistToThreat = FVector::Dist(P, ThreatEye);
		if (DistToThreat < ThreatDist * 0.6f || DistToThreat < 500.f)
		{
			continue;
		}
		// Crouched chest height must be hidden from the threat.
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, ThreatEye, P + FVector(0.f, 0.f, 10.f), ECC_Visibility, Params))
		{
			continue;
		}
		if (Hit.Distance > DistToThreat - 60.f)
		{
			continue;
		}
		const float Score = FVector::Dist(P, MyLoc) + FMath::Max(0.f, ThreatDist - DistToThreat) * 2.f;
		if (Score < BestScore)
		{
			BestScore = Score;
			OutCover = P;
			bFound = true;
		}
	}
	return bFound;
}

// ---------------------------------------------------------------------------------------------
//  Per-frame aiming & firing
// ---------------------------------------------------------------------------------------------

void ATOAIController::UpdateCombat(float Dt, float Now)
{
	ATOCharacter* Me = GetChar();
	if (!Me)
	{
		return;
	}
	UTOWeaponComponent* W = Me->GetWeapons();
	ATOCharacter* T = Target.Get();
	const bool bCanShoot = State == ETOAIState::Combat && T && bTargetVisible && Now >= BlindUntil && !Me->IsUsingItem() && T->IsAlive();
	if (!bCanShoot)
	{
		W->SetTrigger(false);
		W->SetAiming(false);
		if (State == ETOAIState::Combat && W->GetMagAmmo() < W->GetMagSize() * 0.5f && !W->IsReloading())
		{
			W->Reload();
		}
		return;
	}

	const FVector Eye = Me->GetEyeLocation();
	const float Dist = FVector::Dist(Eye, T->GetChestLocation());
	const FTOWeaponStats& WS = W->GetStats();
	if (W->GetMagAmmo() <= 0)
	{
		W->SetTrigger(false);
		W->Reload();
		return;
	}

	// Facing check
	const FVector ToT = (T->GetActorLocation() - Me->GetActorLocation()).GetSafeNormal2D();
	const bool bFacing = FVector::DotProduct(Me->GetActorForwardVector().GetSafeNormal2D(), ToT) > 0.9f;
	const float MaxRange = WS.Def ? FMath::Max(3000.f, WS.Range * 100.f * (WS.Def->Class == ETOWeaponClass::Sniper || WS.Def->Class == ETOWeaponClass::DMR ? 4.f : 2.6f)) : 4000.f;

	W->SetAiming(Dist > 1500.f);

	if (Now < EngageStart || !bFacing || Dist > MaxRange)
	{
		W->SetTrigger(false);
		return;
	}

	// Burst control
	if (Now >= NextBurst)
	{
		const bool bAuto = W->GetFireMode() == ETOFireMode::Auto;
		const float BurstLen = bAuto ? FMath::FRandRange(0.18f, 0.5f) * FMath::Lerp(1.2f, 0.7f, FMath::Clamp(Dist / 4000.f, 0.f, 1.f)) : 0.05f;
		BurstEnd = Now + BurstLen;
		const float Pause = bAuto ? FMath::FRandRange(0.35f, 0.9f) : FMath::FRandRange(0.35f, 0.75f) * (WS.Def && WS.Def->Class == ETOWeaponClass::Sniper ? 3.f : 1.f);
		NextBurst = BurstEnd + Pause * FMath::Lerp(1.3f, 0.8f, Skill);

		// New aim error each burst; accuracy improves the longer the target stays visible.
		const float EngageTime = Now - EngageStart;
		const float SpinUp = FMath::Lerp(2.2f, 1.f, FMath::Clamp(EngageTime / 3.f, 0.f, 1.f));
		const float TargetSpeed = T->GetVelocity().Size() / 600.f;
		const bool bSup = Now < SuppressedUntil;
		const float Err = FMath::Max(12.f, Dist * FMath::Lerp(0.045f, 0.016f, Skill) * SpinUp * (1.f + TargetSpeed) * (bSup ? 1.6f : 1.f) * (Me->GetVelocity().Size() > 100.f ? 1.3f : 1.f));
		AimOffset = FMath::VRand() * FMath::FRandRange(0.f, Err);
		bAimHead = FMath::FRand() < FMath::Lerp(0.05f, 0.25f, Skill);
	}

	const FVector AimBase = bAimHead ? T->GetHeadLocation() : T->GetChestLocation();
	W->SetAIAimPoint(AimBase + AimOffset);
	W->SetTrigger(Now < BurstEnd);
}

void ATOAIController::UpdateFocus(float Now)
{
	ATOCharacter* Me = GetChar();
	if (!Me)
	{
		return;
	}
	ATOCharacter* T = Target.Get();
	if (State == ETOAIState::Combat && T && (bTargetVisible || Now - LastSeenTime < 2.f))
	{
		SetFocus(T);
		return;
	}
	ClearFocus(EAIFocusPriority::Gameplay);
	FVector Look;
	if (State == ETOAIState::Combat)
	{
		Look = LastKnownPos;
	}
	else if (bMoving && Me->GetVelocity().Size2D() > 60.f && State != ETOAIState::Follow)
	{
		Look = Me->GetActorLocation() + Me->GetVelocity().GetSafeNormal2D() * 1000.f;
	}
	else if (!LookAtPoint.IsZero())
	{
		Look = LookAtPoint;
	}
	else
	{
		Look = Me->GetActorLocation() + Me->GetActorForwardVector() * 1000.f;
	}
	Look.Z = Me->GetEyeLocation().Z;
	SetFocalPoint(Look, EAIFocusPriority::Gameplay);
}

void ATOAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ATOCharacter* Me = GetChar();
	UWorld* World = GetWorld();
	if (!Me || !World)
	{
		return;
	}
	if (!Me->IsAlive() || Me->IsDowned())
	{
		if (bMoving)
		{
			StopMove();
		}
		Me->GetWeapons()->SetTrigger(false);
		return;
	}
	const float Now = World->GetTimeSeconds();
	const float Dt = FMath::Min(DeltaTime, 0.1f);

	if (Now >= NextPerception)
	{
		NextPerception = Now + 0.22f + FMath::FRandRange(0.f, 0.08f);
		UpdatePerception(Now);
	}
	if (Now >= NextThink)
	{
		NextThink = Now + 0.25f;
		Think(Now);
		Me = GetChar();
		if (!IsValid(Me) || !Me->IsAlive())
		{
			return;
		}
	}
	UpdateCombat(Dt, Now);
	UpdateFocus(Now);
	UpdateDirectMove(Dt);
	if (bMoving && HasReachedGoal() && State != ETOAIState::Patrol && State != ETOAIState::Objective && State != ETOAIState::Investigate && State != ETOAIState::Search)
	{
		bMoving = false;
		Me->SetWantsSprint(false);
	}
	if (!bMoving)
	{
		Me->SetWantsSprint(false);
	}
}
