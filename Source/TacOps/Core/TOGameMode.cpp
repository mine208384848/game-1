// TAC-OPS - game mode (common: world creation, registry, events, spawning)

#include "Core/TOGameMode.h"
#include "Core/TOGameInstance.h"
#include "Core/TOSaveGame.h"
#include "Core/TOPlayerController.h"
#include "Core/TODatabase.h"
#include "UI/TOHUD.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOHealthComponent.h"
#include "Characters/TOInventoryComponent.h"
#include "AI/TOAIController.h"
#include "Weapons/TOCombatManager.h"
#include "World/TOWorldGenerator.h"
#include "World/TOEnvironment.h"
#include "World/TODoor.h"
#include "World/TOExtractionZone.h"
#include "Audio/TOAudio.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "TacOps.h"

ATOGameMode::ATOGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = nullptr;
	PlayerControllerClass = ATOPlayerController::StaticClass();
	HUDClass = ATOHUD::StaticClass();
	TeamKills.Init(0, 2);
	WarfareSquads.Init(-1, 2);
}

ATOGameMode* ATOGameMode::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? World->GetAuthGameMode<ATOGameMode>() : nullptr;
}

void ATOGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Pawns are spawned by the game mode once the procedural world is ready.
	LocalPC = NewPlayer;
}

bool ATOGameMode::IsNight() const
{
	return Environment && Environment->IsNight();
}

bool ATOGameMode::GetWaterHeight(const FVector& Location, float& OutZ) const
{
	return WorldGen && WorldGen->GetWaterHeight(Location, OutZ);
}

void ATOGameMode::StartPlay()
{
	if (UTOGameInstance* GI = UTOGameInstance::Get(this))
	{
		Session = GI->Session;
		if (UTOSaveGame* Save = GI->GetSave())
		{
			GI->ApplyGraphicsQuality(Save->GraphicsQuality);
		}
	}
	Mode = Session.bDeploy ? Session.Mode : ETOMatchMode::Menu;
	if (Mode == ETOMatchMode::Menu)
	{
		Session.Time = ETOTimeOfDay::Sunset;
	}

	BuildWorld();
	SpawnWorldActors();
	SpawnNavBounds();

	Super::StartPlay();

	State = ETOGMState::Loading;
	LoadingStart = GetWorld()->GetTimeSeconds();
}

void ATOGameMode::BuildWorld()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	Environment = World->SpawnActor<ATOEnvironment>(ATOEnvironment::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (Environment)
	{
		Environment->ApplyTimeOfDay(Session.Time);
	}
	Combat = World->SpawnActor<ATOCombatManager>(ATOCombatManager::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
	WorldGen = World->SpawnActor<ATOWorldGenerator>(ATOWorldGenerator::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (WorldGen)
	{
		// The map layout & vegetation are fixed (like a real map); loot and AI vary per raid.
		WorldGen->Generate(20240, Session.Time);
	}
}

void ATOGameMode::SpawnWorldActors()
{
	UWorld* World = GetWorld();
	if (!WorldGen)
	{
		return;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (const FTODoorDef& D : WorldGen->Doors)
	{
		if (ATODoor* Door = World->SpawnActor<ATODoor>(ATODoor::StaticClass(), D.Location, FRotator(0.f, D.Yaw, 0.f), P))
		{
			Door->Setup(D.Width, D.Height, D.Mat, D.KeyId);
			Doors.Add(Door);
		}
	}
	for (const FVector& B : WorldGen->BarrelSpots)
	{
		const FVector G = WorldGen->GroundPoint(B.X, B.Y, B.Z + 500.f);
		World->SpawnActor<ATOExplosiveBarrel>(ATOExplosiveBarrel::StaticClass(), G, FRotator::ZeroRotator, P);
	}
}

void ATOGameMode::SpawnNavBounds()
{
	UWorld* World = GetWorld();
	if (!WorldGen)
	{
		return;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ANavMeshBoundsVolume* Vol = World->SpawnActor<ANavMeshBoundsVolume>(ANavMeshBoundsVolume::StaticClass(), FVector(0.f, 0.f, 6000.f), FRotator::ZeroRotator, P);
	if (!Vol)
	{
		return;
	}
	// The volume has no brush at runtime: give it bounds through a box component.
	UBoxComponent* Box = NewObject<UBoxComponent>(Vol);
	Box->SetupAttachment(Vol->GetRootComponent());
	Box->SetBoxExtent(FVector(WorldGen->GetHalfSize(), WorldGen->GetHalfSize(), 25000.f));
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCanEverAffectNavigation(false);
	Box->RegisterComponent();
	if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		Nav->OnNavigationBoundsUpdated(Vol);
	}
}

void ATOGameMode::FinishLoading()
{
	LoadingProgress = 1.f;
	MatchStart = GetWorld()->GetTimeSeconds();
	switch (Mode)
	{
	case ETOMatchMode::Operations:
		SetupOperations();
		State = ETOGMState::Playing;
		break;
	case ETOMatchMode::Warfare:
		SetupWarfare();
		State = ETOGMState::Playing;
		break;
	default:
		SetupMenu();
		State = ETOGMState::Menu;
		break;
	}
	if (ATOPlayerController* PC = Cast<ATOPlayerController>(LocalPC))
	{
		PC->OnMatchReady();
	}
}

void ATOGameMode::SetupMenu()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	MenuCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FVector(-5000.f, 25000.f, 9000.f), FRotator::ZeroRotator, P);
	if (MenuCamera && MenuCamera->GetCameraComponent())
	{
		MenuCamera->GetCameraComponent()->SetFieldOfView(55.f);
		MenuCamera->GetCameraComponent()->bConstrainAspectRatio = false;
	}
	if (LocalPC && MenuCamera)
	{
		LocalPC->SetViewTarget(MenuCamera);
	}
	TickMenuCamera(0.f);

	// A few patrols in the background make the lobby feel alive.
	if (WorldGen)
	{
		for (int32 i = 0; i < FMath::Min(6, WorldGen->GuardPosts.Num()); ++i)
		{
			const FTOGuardPost& G = WorldGen->GuardPosts[i * 7 % WorldGen->GuardPosts.Num()];
			SpawnAI(G.Location, G.Yaw, 1, ETOAIRole::Guard, G.Tier, -1, TArray<FVector>(), TEXT("Guard"));
		}
	}
}

void ATOGameMode::TickMenuCamera(float Dt)
{
	if (!MenuCamera)
	{
		return;
	}
	MenuOrbit += Dt * 0.015f;
	const FVector Center(34000.f, -2000.f, 3500.f);
	const FVector Loc = Center + FVector(FMath::Cos(MenuOrbit + 3.4f) * 30000.f, FMath::Sin(MenuOrbit + 3.4f) * 30000.f, 7000.f + FMath::Sin(MenuOrbit * 2.f) * 1500.f);
	MenuCamera->SetActorLocationAndRotation(Loc, (Center - Loc).Rotation());
}

// ---------------------------------------------------------------------------------------------
//  Registry & queries
// ---------------------------------------------------------------------------------------------

void ATOGameMode::RegisterCharacter(ATOCharacter* C)
{
	if (C)
	{
		Characters.AddUnique(C);
	}
}

void ATOGameMode::UnregisterCharacter(ATOCharacter* C)
{
	Characters.RemoveAll([C](const TWeakObjectPtr<ATOCharacter>& P) { return !P.IsValid() || P.Get() == C; });
}

ATOCharacter* ATOGameMode::GetPlayerCharacter() const
{
	return PlayerChar.Get();
}

float ATOGameMode::GetMatchTime() const
{
	const UWorld* World = GetWorld();
	return (World && State != ETOGMState::Loading) ? World->GetTimeSeconds() - MatchStart : 0.f;
}

float ATOGameMode::GetRaidTimeLeft() const
{
	if (Mode != ETOMatchMode::Operations || State == ETOGMState::Loading)
	{
		return RaidDuration;
	}
	const float End = (State == ETOGMState::Ended && EndTime > 0.f) ? EndTime : GetWorld()->GetTimeSeconds();
	return FMath::Max(0.f, RaidDuration - (End - MatchStart));
}

int32 ATOGameMode::GetNumSectors() const
{
	return WorldGen ? WorldGen->Sectors.Num() : 0;
}

FString ATOGameMode::GetSectorName() const
{
	return (WorldGen && WorldGen->Sectors.IsValidIndex(CurrentSector)) ? WorldGen->Sectors[CurrentSector].Name : FString();
}

int32 ATOGameMode::GetTeamScore(int32 Team) const
{
	return TeamKills.IsValidIndex(Team) ? TeamKills[Team] : 0;
}

// ---------------------------------------------------------------------------------------------
//  Events
// ---------------------------------------------------------------------------------------------

void ATOGameMode::ReportNoise(const FVector& Location, float Radius, ATOCharacter* Source, bool bGunshot)
{
	if (State != ETOGMState::Playing && State != ETOGMState::Menu)
	{
		return;
	}
	const float R2 = Radius * Radius;
	for (const TWeakObjectPtr<ATOCharacter>& Ptr : Characters)
	{
		ATOCharacter* C = Ptr.Get();
		if (!C || C == Source || !C->IsAlive())
		{
			continue;
		}
		if (FVector::DistSquared(C->GetActorLocation(), Location) > R2)
		{
			continue;
		}
		if (ATOAIController* AI = Cast<ATOAIController>(C->GetController()))
		{
			AI->OnHeardNoise(Location, Source, bGunshot);
		}
	}
}

void ATOGameMode::ReportGrenade(const FVector& Location, int32 Team)
{
	for (const TWeakObjectPtr<ATOCharacter>& Ptr : Characters)
	{
		ATOCharacter* C = Ptr.Get();
		if (!C || !C->IsAlive() || FVector::DistSquared(C->GetActorLocation(), Location) > FMath::Square(900.f))
		{
			continue;
		}
		if (ATOAIController* AI = Cast<ATOAIController>(C->GetController()))
		{
			AI->OnGrenadeNearby(Location);
		}
	}
}

void ATOGameMode::AlertSquad(int32 SquadId, ATOCharacter* Target, const FVector& LastKnown, ATOCharacter* Reporter)
{
	if (SquadId < 0 || !Target)
	{
		return;
	}
	FTOSquadInfo* Squad = FindSquad(SquadId);
	if (!Squad)
	{
		return;
	}
	for (const TWeakObjectPtr<ATOCharacter>& Ptr : Squad->Members)
	{
		ATOCharacter* C = Ptr.Get();
		if (!C || C == Reporter || !C->IsAlive())
		{
			continue;
		}
		if (Reporter && FVector::Dist(C->GetActorLocation(), Reporter->GetActorLocation()) > 9000.f)
		{
			continue;
		}
		if (ATOAIController* AI = Cast<ATOAIController>(C->GetController()))
		{
			AI->ShareTarget(Target, LastKnown);
		}
	}
}

void ATOGameMode::RevealEnemiesAround(ATOCharacter* Source, float Radius, float Seconds)
{
	if (!Source)
	{
		return;
	}
	int32 Count = 0;
	for (const TWeakObjectPtr<ATOCharacter>& Ptr : Characters)
	{
		ATOCharacter* C = Ptr.Get();
		if (C && C->IsAlive() && Source->IsHostileTo(C) && FVector::Dist(C->GetActorLocation(), Source->GetActorLocation()) < Radius)
		{
			C->Reveal(Seconds);
			++Count;
		}
	}
	if (Source->IsPlayerCharacter())
	{
		PushMessage(FString::Printf(TEXT("Recon pulse: %d hostiles detected"), Count), FLinearColor(0.75f, 0.45f, 1.f));
	}
}

bool ATOGameMode::TryPayExtraction(ATOCharacter* Payer, int32 Cost)
{
	UTOGameInstance* GI = UTOGameInstance::Get(this);
	if (!GI || !Payer || !Payer->IsPlayerCharacter())
	{
		return false;
	}
	if (GI->GetCredits() < PendingExtractionCost + Cost)
	{
		return false;
	}
	PendingExtractionCost += Cost;
	return true;
}

void ATOGameMode::PushMessage(const FString& Text, const FLinearColor& Color)
{
	FTOMessage M;
	M.Text = Text;
	M.Color = Color;
	M.Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Messages.Add(M);
	if (Messages.Num() > 6)
	{
		Messages.RemoveAt(0);
	}
}

void ATOGameMode::OnCharacterKilled(ATOCharacter* Victim, ATOCharacter* Killer, const FTODamageInfo& Info)
{
	if (!Victim)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	FTOKillFeedEntry E;
	E.Killer = Killer ? Killer->DisplayName : FString();
	E.Victim = Victim->DisplayName;
	const FTOWeaponDef* W = TODB::FindWeapon(Info.WeaponId);
	E.Weapon = W ? W->Name : (Info.bExplosive ? Info.WeaponId.ToString() : (Info.bBleed ? TEXT("Bleeding") : TEXT("")));
	E.bHeadshot = Info.Part == ETOBodyPart::Head && !Info.bExplosive;
	E.KillerTeam = Killer ? Killer->TeamId : -1;
	E.VictimTeam = Victim->TeamId;
	E.Time = Now;
	KillFeed.Add(E);
	if (KillFeed.Num() > 6)
	{
		KillFeed.RemoveAt(0);
	}

	if (Victim->bBoss)
	{
		PushMessage(TEXT("The Commander is down - his keycard is on the body"), FLinearColor(1.f, 0.75f, 0.2f));
	}
	if (Killer && Killer != Victim && Killer->IsPlayerCharacter() && State == ETOGMState::Playing)
	{
		Result.Kills += 1;
		Result.Headshots += E.bHeadshot ? 1 : 0;
	}

	if (Mode == ETOMatchMode::Warfare)
	{
		if (Killer && TeamKills.IsValidIndex(Killer->TeamId))
		{
			++TeamKills[Killer->TeamId];
		}
		if (Victim->TeamId == AttackerTeam)
		{
			Tickets = FMath::Max(0, Tickets - 1);
		}
		if (Victim == PlayerChar.Get())
		{
			bPlayerAwaitingRespawn = true;
			PlayerDeathTime = Now;
		}
		else if (State == ETOGMState::Playing)
		{
			FTOPendingRespawn R;
			R.Team = Victim->TeamId;
			R.Time = Now + 9.f;
			R.Operator = (ETOOperator)FMath::RandRange(0, TODB::NumOperators() - 1);
			R.SquadId = Victim->SquadId;
			PendingRespawns.Add(R);
		}
		return;
	}

	if (Mode == ETOMatchMode::Operations && Victim == PlayerChar.Get())
	{
		FinishRaid(false, TEXT("KILLED IN ACTION"));
	}
}

void ATOGameMode::OnCharacterDowned(ATOCharacter* Victim, const FTODamageInfo& Info)
{
	if (!Victim)
	{
		return;
	}
	ATOCharacter* Killer = Cast<ATOCharacter>(Info.Instigator.Get());
	FTOKillFeedEntry E;
	E.Killer = Killer ? Killer->DisplayName : FString();
	E.Victim = Victim->DisplayName;
	const FTOWeaponDef* W = TODB::FindWeapon(Info.WeaponId);
	E.Weapon = W ? W->Name : FString();
	E.bDowned = true;
	E.KillerTeam = Killer ? Killer->TeamId : -1;
	E.VictimTeam = Victim->TeamId;
	E.Time = GetWorld()->GetTimeSeconds();
	KillFeed.Add(E);
	if (KillFeed.Num() > 6)
	{
		KillFeed.RemoveAt(0);
	}
	if (Victim == PlayerChar.Get())
	{
		PushMessage(TEXT("You are down - wait for a squad mate or press [G] to give up"), FLinearColor(1.f, 0.4f, 0.3f));
	}
}

void ATOGameMode::OnCharacterRevived(ATOCharacter* Who, ATOCharacter* By)
{
	if (Who && Who == PlayerChar.Get())
	{
		PushMessage(FString::Printf(TEXT("Revived by %s"), By ? *By->DisplayName : TEXT("a squad mate")), FLinearColor(0.4f, 1.f, 0.5f));
	}
}

void ATOGameMode::OnAIExtracted(ATOCharacter* C)
{
	if (!C)
	{
		return;
	}
	AController* Ctrl = C->GetController();
	C->Destroy();
	if (Ctrl)
	{
		Ctrl->Destroy();
	}
}

void ATOGameMode::PlayerGiveUp()
{
	ATOCharacter* P = PlayerChar.Get();
	if (P && P->IsDowned())
	{
		FTODamageInfo Info;
		Info.Instigator = P->LastAttacker.Get();
		Info.bBleed = true;
		P->Kill(Info);
	}
}

void ATOGameMode::PlayerAbandonMatch()
{
	if (State == ETOGMState::Ended)
	{
		return;
	}
	if (Mode == ETOMatchMode::Operations)
	{
		FinishRaid(false, TEXT("RAID ABANDONED"));
	}
	else if (Mode == ETOMatchMode::Warfare)
	{
		State = ETOGMState::Ended;
		Result = FTOMatchResult();
		Result.bValid = true;
		Result.Mode = ETOMatchMode::Warfare;
		Result.Title = TEXT("LEFT THE BATTLE");
		if (UTOGameInstance* GI = UTOGameInstance::Get(this))
		{
			GI->LastResult = Result;
		}
		if (ATOPlayerController* PC = Cast<ATOPlayerController>(LocalPC))
		{
			PC->ShowResults();
		}
	}
}

// ---------------------------------------------------------------------------------------------
//  Spawning
// ---------------------------------------------------------------------------------------------

int32 ATOGameMode::NewSquad(int32 Team, ETOAIRole Role, int32 Poi)
{
	FTOSquadInfo S;
	S.Id = Squads.Num();
	S.Team = Team;
	S.Role = Role;
	S.Poi = Poi;
	Squads.Add(S);
	return S.Id;
}

FTOSquadInfo* ATOGameMode::FindSquad(int32 Id)
{
	return Squads.IsValidIndex(Id) ? &Squads[Id] : nullptr;
}

FVector ATOGameMode::FindSpawnPoint(const FVector& Around, float Radius) const
{
	if (!WorldGen)
	{
		return Around;
	}
	for (int32 Attempt = 0; Attempt < 12; ++Attempt)
	{
		const FVector2D Off = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * Radius * (Attempt == 0 ? 0.2f : 1.f);
		const FVector P = WorldGen->GroundPoint(Around.X + Off.X, Around.Y + Off.Y, FMath::Max(Around.Z + 2000.f, 30000.f));
		float WaterZ = 0.f;
		if (WorldGen->GetWaterHeight(P, WaterZ) && P.Z < WaterZ)
		{
			continue;
		}
		return P + FVector(0.f, 0.f, 100.f);
	}
	return Around + FVector(0.f, 0.f, 150.f);
}

ATOCharacter* ATOGameMode::SpawnSoldier(const FVector& Location, float Yaw, int32 Team, bool bPlayer)
{
	UWorld* World = GetWorld();
	const FTransform T(FRotator(0.f, Yaw, 0.f), Location + FVector(0.f, 0.f, 20.f));
	ATOCharacter* C = World->SpawnActorDeferred<ATOCharacter>(ATOCharacter::StaticClass(), T, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!C)
	{
		return nullptr;
	}
	if (bPlayer)
	{
		C->AutoPossessAI = EAutoPossessAI::Disabled;
	}
	C->TeamId = Team;
	C->FinishSpawning(T);
	return C;
}

ATOCharacter* ATOGameMode::SpawnAI(const FVector& Location, float Yaw, int32 Team, ETOAIRole Role, int32 Tier, int32 SquadId, const TArray<FVector>& Patrol, const FString& Name)
{
	ATOCharacter* C = SpawnSoldier(Location, Yaw, Team, false);
	if (!C)
	{
		return nullptr;
	}
	const ETOOperator Op = (ETOOperator)FMath::RandRange(0, TODB::NumOperators() - 1);
	ETOMat Suit = ETOMat::SuitGrey;
	switch (Role)
	{
	case ETOAIRole::Guard: Suit = (FMath::RandBool() ? ETOMat::SuitBlue : ETOMat::SuitGrey); break;
	case ETOAIRole::Boss: Suit = ETOMat::SuitRed; break;
	case ETOAIRole::Rival: Suit = (FMath::RandBool() ? ETOMat::SuitBlack : ETOMat::SuitUrban); break;
	case ETOAIRole::Teammate: Suit = TODB::GetOperator(Op).SuitMat; break;
	case ETOAIRole::WarfareBot: Suit = Team == 0 ? (FMath::RandBool() ? ETOMat::SuitGreen : ETOMat::SuitTan) : (FMath::RandBool() ? ETOMat::SuitUrban : ETOMat::SuitBlack); break;
	default: break;
	}
	C->bBoss = Role == ETOAIRole::Boss;
	C->InitCharacter(Team, Name, Op, false, Suit);
	C->SquadId = SquadId;
	FRandomStream Rng(Session.Seed * 13 + Characters.Num() * 977 + 17);
	C->ApplyAIKit(Tier, Role, Rng, Mode);

	ATOAIController* AI = Cast<ATOAIController>(C->GetController());
	if (!AI)
	{
		C->SpawnDefaultController();
		AI = Cast<ATOAIController>(C->GetController());
	}
	if (AI)
	{
		float Skill = 0.3f + 0.15f * Tier;
		if (Role == ETOAIRole::Boss) Skill = 0.9f;
		if (Role == ETOAIRole::Rival) Skill = 0.7f + 0.1f * Session.Difficulty;
		if (Role == ETOAIRole::Teammate) Skill = 0.75f;
		if (Role == ETOAIRole::WarfareBot) Skill = FMath::FRandRange(0.4f, 0.75f);
		AI->Configure(Role, SquadId, Location, Patrol, FMath::Clamp(Skill, 0.f, 1.f));
	}
	if (FTOSquadInfo* Squad = FindSquad(SquadId))
	{
		Squad->Members.Add(C);
	}
	return C;
}

ATOCharacter* ATOGameMode::SpawnPlayer(const FVector& Location, float Yaw)
{
	ATOCharacter* C = SpawnSoldier(Location, Yaw, PlayerTeam, true);
	if (!C || !LocalPC)
	{
		return C;
	}
	UTOGameInstance* GI = UTOGameInstance::Get(this);
	UTOSaveGame* Save = GI ? GI->GetSave() : nullptr;
	FTOLoadout L = Session.Loadout;
	if (Mode == ETOMatchMode::Warfare)
	{
		L.ArmorLevel = 4;
		L.HelmetLevel = 3;
		L.RigTier = 3;
		L.BackpackTier = 1;
		L.MedTier = 1;
		L.Frags = 2;
		L.Smokes = 1;
		L.Flashes = 1;
	}
	const FTOOperatorDef& Op = TODB::GetOperator(L.Operator);
	C->InitCharacter(PlayerTeam, TEXT("YOU"), L.Operator, true, Op.SuitMat);
	LocalPC->Possess(C);
	LocalPC->SetControlRotation(FRotator(0.f, Yaw, 0.f));
	C->ApplyLoadout(L, Mode);
	C->GetHealth()->bCanBeDowned = (Mode == ETOMatchMode::Warfare) || Session.bSquad;
	if (Save)
	{
		C->SetBaseFOV(Save->FieldOfView);
		if (Mode == ETOMatchMode::Operations)
		{
			UTOInventoryComponent* Inv = C->GetInventory();
			Inv->SafeBox.Items.Reset();
			for (const FTOItemInstance& It : Save->SafeBoxItems)
			{
				Inv->SafeBox.TryAdd(It);
			}
			Inv->MarkDirty();
		}
	}
	PlayerChar = C;
	if (ATOPlayerController* PC = Cast<ATOPlayerController>(LocalPC))
	{
		PC->OnPlayerSpawned(C);
	}
	return C;
}

// ---------------------------------------------------------------------------------------------
//  Tick
// ---------------------------------------------------------------------------------------------

void ATOGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);

	switch (State)
	{
	case ETOGMState::Loading:
	{
		const float Elapsed = Now - LoadingStart;
		LoadingProgress = FMath::Clamp(Elapsed / 4.f, 0.f, 0.95f);
		if ((WorldGen && WorldGen->IsTerrainReady() && Elapsed > 0.75f) || Elapsed > 25.f)
		{
			FinishLoading();
		}
		break;
	}
	case ETOGMState::Menu:
		TickMenuCamera(Dt);
		break;
	case ETOGMState::Playing:
		if (Mode == ETOMatchMode::Operations)
		{
			TickOperations(Dt, Now);
		}
		else if (Mode == ETOMatchMode::Warfare)
		{
			TickWarfare(Dt, Now);
		}
		break;
	default:
		break;
	}

	KillFeed.RemoveAll([Now](const FTOKillFeedEntry& E) { return Now - E.Time > 9.f; });
	Messages.RemoveAll([Now](const FTOMessage& M) { return Now - M.Time > 7.f; });
	if (Now > NextNoiseCleanup)
	{
		NextNoiseCleanup = Now + 2.f;
		Characters.RemoveAll([](const TWeakObjectPtr<ATOCharacter>& P) { return !P.IsValid(); });
	}
}
