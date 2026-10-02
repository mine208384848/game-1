// TAC-OPS - game mode: Operations (Hazard extraction raid)

#include "Core/TOGameMode.h"
#include "Core/TOGameInstance.h"
#include "Core/TOSaveGame.h"
#include "Core/TOPlayerController.h"
#include "Core/TODatabase.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOHealthComponent.h"
#include "Characters/TOInventoryComponent.h"
#include "Weapons/TOWeaponComponent.h"
#include "Weapons/TOCombatManager.h"
#include "AI/TOAIController.h"
#include "World/TOWorldGenerator.h"
#include "World/TOLootContainer.h"
#include "World/TOExtractionZone.h"
#include "Audio/TOAudio.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"

void ATOGameMode::SetupOperations()
{
	UWorld* World = GetWorld();
	if (!WorldGen)
	{
		return;
	}
	FRandomStream Rng(Session.Seed);
	RaidDuration = 25.f * 60.f;
	Result = FTOMatchResult();
	Result.Mode = ETOMatchMode::Operations;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Extraction points (some are closed each raid)
	int32 AlwaysOpen = 0;
	for (const FTOExtractDef& E : WorldGen->Extracts)
	{
		ATOExtractionZone* Z = World->SpawnActor<ATOExtractionZone>(ATOExtractionZone::StaticClass(), E.Location, FRotator::ZeroRotator, P);
		if (!Z)
		{
			continue;
		}
		Z->Setup(E.Name, E.Rule, E.Radius, E.Cost);
		const bool bAvail = Rng.FRand() <= E.Chance;
		Z->SetAvailable(bAvail);
		if (bAvail && E.Rule == ETOExtractRule::Always)
		{
			++AlwaysOpen;
		}
		ExtractionZones.Add(Z);
		if (E.Rule == ETOExtractRule::Radio || E.Rule == ETOExtractRule::Paid)
		{
			if (ATOObjectiveSwitch* S = World->SpawnActor<ATOObjectiveSwitch>(ATOObjectiveSwitch::StaticClass(), E.SwitchLocation, FRotator(0.f, E.SwitchYaw, 0.f), P))
			{
				S->Setup(E.Rule == ETOExtractRule::Radio ? 0 : 1, Z);
			}
		}
	}
	for (const TWeakObjectPtr<ATOExtractionZone>& Z : ExtractionZones)
	{
		if (AlwaysOpen >= 2)
		{
			break;
		}
		if (Z.IsValid() && !Z->IsAvailable() && Z->GetRule() == ETOExtractRule::Always)
		{
			Z->SetAvailable(true);
			++AlwaysOpen;
		}
	}

	// Loot
	for (int32 i = 0; i < WorldGen->LootSpots.Num(); ++i)
	{
		const FTOLootSpot& L = WorldGen->LootSpots[i];
		if (ATOLootContainer* C = World->SpawnActor<ATOLootContainer>(ATOLootContainer::StaticClass(), L.Location, FRotator(0.f, L.Yaw, 0.f), P))
		{
			C->InitRandom(L.Type, FMath::Clamp(L.Tier + Session.Difficulty, 0, 4), Session.Seed * 31 + i * 7919);
		}
	}

	// Guards, grouped into one squad per point of interest
	TMap<int32, TArray<int32>> PostsByPoi;
	for (int32 i = 0; i < WorldGen->GuardPosts.Num(); ++i)
	{
		PostsByPoi.FindOrAdd(WorldGen->GuardPosts[i].Poi).Add(i);
	}
	for (TPair<int32, TArray<int32>>& Pair : PostsByPoi)
	{
		TArray<int32>& Posts = Pair.Value;
		// Shuffle
		for (int32 i = Posts.Num() - 1; i > 0; --i)
		{
			Posts.Swap(i, Rng.RandRange(0, i));
		}
		const float Fraction = Session.Difficulty > 0 ? 1.f : 0.7f;
		int32 Count = FMath::Max(1, FMath::CeilToInt(Posts.Num() * Fraction));
		const int32 SquadId = NewSquad(1, ETOAIRole::Guard, Pair.Key);
		const FVector PoiCenter = WorldGen->POIs.IsValidIndex(Pair.Key) ? WorldGen->POIs[Pair.Key].Location : FVector::ZeroVector;
		for (int32 k = 0; k < Posts.Num(); ++k)
		{
			const FTOGuardPost& G = WorldGen->GuardPosts[Posts[k]];
			if (k >= Count && !G.bBoss)
			{
				continue;
			}
			TArray<FVector> Patrol;
			if (!G.bOverwatch && !G.bBoss)
			{
				Patrol.Add(G.Location);
				for (int32 m = 0; m < 2; ++m)
				{
					const FTOGuardPost& Other = WorldGen->GuardPosts[Posts[Rng.RandRange(0, Posts.Num() - 1)]];
					if (FMath::Abs(Other.Location.Z - G.Location.Z) < 200.f)
					{
						Patrol.Add(Other.Location);
					}
				}
				if (!PoiCenter.IsZero() && Rng.FRand() < 0.5f)
				{
					Patrol.Add(FVector(PoiCenter.X + Rng.FRandRange(-2000.f, 2000.f), PoiCenter.Y + Rng.FRandRange(-2000.f, 2000.f), G.Location.Z));
				}
			}
			const int32 Tier = FMath::Clamp(G.Tier + Session.Difficulty, 0, 3);
			const ETOAIRole Role = G.bBoss ? ETOAIRole::Boss : ETOAIRole::Guard;
			SpawnAI(G.Location, G.Yaw, 1, Role, G.bBoss ? 3 : Tier, SquadId, Patrol, G.bBoss ? FString(TEXT("Commander Varga")) : FString(TEXT("Haven Guard")));
		}
	}

	// Player squad
	const int32 InsIdx = WorldGen->Insertions.Num() > 0 ? Rng.RandRange(0, WorldGen->Insertions.Num() - 1) : -1;
	FVector InsLoc = FVector(-90000.f, -60000.f, 3000.f);
	float InsYaw = 0.f;
	if (InsIdx >= 0)
	{
		InsLoc = WorldGen->Insertions[InsIdx].Location;
		InsYaw = WorldGen->Insertions[InsIdx].Yaw;
	}
	PlayerSquadId = NewSquad(0, ETOAIRole::Teammate, -1);
	ATOCharacter* Player = SpawnPlayer(FindSpawnPoint(InsLoc, 300.f), InsYaw);
	if (FTOSquadInfo* Squad = FindSquad(PlayerSquadId))
	{
		Squad->Members.Add(Player);
	}
	if (Player)
	{
		Player->SquadId = PlayerSquadId;
	}
	if (Session.bSquad && Player)
	{
		static const TCHAR* Names[] = { TEXT("Raven"), TEXT("Kestrel") };
		for (int32 i = 0; i < 2; ++i)
		{
			if (ATOCharacter* Mate = SpawnAI(FindSpawnPoint(InsLoc, 600.f), InsYaw, 0, ETOAIRole::Teammate, 2, PlayerSquadId, TArray<FVector>(), Names[i]))
			{
				if (ATOAIController* AI = Cast<ATOAIController>(Mate->GetController()))
				{
					AI->SetFollowLeader(Player, i);
				}
			}
		}
	}

	// Rival operator squads enter over time
	RivalSpawnTimes = { 40.f, 150.f, 300.f };
	if (Session.Difficulty > 0)
	{
		RivalSpawnTimes.Add(480.f);
	}
	RivalsSpawned = 0;

	PushMessage(FString::Printf(TEXT("Raid started at %s - collect valuables and extract"), InsIdx >= 0 ? *WorldGen->Insertions[InsIdx].Name : TEXT("insertion")), FLinearColor(0.9f, 0.9f, 0.6f));
	int32 Open = 0;
	for (const TWeakObjectPtr<ATOExtractionZone>& Z : ExtractionZones)
	{
		Open += (Z.IsValid() && Z->IsAvailable()) ? 1 : 0;
	}
	PushMessage(FString::Printf(TEXT("%d extraction points active this raid (check the map: M)"), Open), FLinearColor(0.5f, 1.f, 0.6f));
}

void ATOGameMode::SpawnRivalSquad(int32 Index)
{
	if (!WorldGen || WorldGen->Insertions.Num() == 0)
	{
		return;
	}
	FRandomStream Rng(Session.Seed + 777 + Index * 131);
	// Enter from the insertion point farthest from the player.
	FVector Best = WorldGen->Insertions[0].Location;
	float BestD = -1.f;
	const ATOCharacter* P = PlayerChar.Get();
	for (const FTOSpawnDef& S : WorldGen->Insertions)
	{
		const float D = P ? FVector::Dist2D(S.Location, P->GetActorLocation()) : Rng.FRand();
		const float Score = D + Rng.FRandRange(0.f, 30000.f);
		if (Score > BestD)
		{
			BestD = Score;
			Best = S.Location;
		}
	}
	const int32 Team = 2 + Index;
	const int32 SquadId = NewSquad(Team, ETOAIRole::Rival, -1);
	const int32 Size = 3 + (Session.Difficulty > 0 ? 1 : 0);
	for (int32 i = 0; i < Size; ++i)
	{
		SpawnAI(FindSpawnPoint(Best, 700.f), 0.f, Team, ETOAIRole::Rival, FMath::Clamp(2 + Session.Difficulty, 0, 3), SquadId, TArray<FVector>(), FString::Printf(TEXT("Hostile Operator %c%d"), TCHAR('A' + Index), i + 1));
	}
	if (FTOSquadInfo* Squad = FindSquad(SquadId))
	{
		AssignRivalObjective(*Squad);
	}
}

void ATOGameMode::AssignRivalObjective(FTOSquadInfo& Squad)
{
	if (!WorldGen)
	{
		return;
	}
	const bool bLeave = Squad.Visits >= 3 || GetRaidTimeLeft() < 420.f;
	if (bLeave)
	{
		// Head for the nearest open extraction
		FVector Ref = FVector::ZeroVector;
		for (const TWeakObjectPtr<ATOCharacter>& M : Squad.Members)
		{
			if (M.IsValid())
			{
				Ref = M->GetActorLocation();
				break;
			}
		}
		float BestD = TNumericLimits<float>::Max();
		FVector BestLoc = FVector::ZeroVector;
		for (const TWeakObjectPtr<ATOExtractionZone>& Z : ExtractionZones)
		{
			if (Z.IsValid() && Z->IsAvailable() && Z->GetRule() == ETOExtractRule::Always)
			{
				const float D = FVector::Dist2D(Z->GetActorLocation(), Ref);
				if (D < BestD)
				{
					BestD = D;
					BestLoc = Z->GetActorLocation();
				}
			}
		}
		Squad.bExtracting = true;
		Squad.ExtractLocation = BestLoc;
		for (int32 i = 0; i < Squad.Members.Num(); ++i)
		{
			if (ATOCharacter* M = Squad.Members[i].Get())
			{
				if (ATOAIController* AI = Cast<ATOAIController>(M->GetController()))
				{
					AI->SetExtractTarget(BestLoc + FVector(i * 150.f, 0.f, 0.f));
				}
			}
		}
		return;
	}

	// Pick a point of interest, weighted towards valuable (higher tier) areas.
	TArray<float> Weights;
	for (int32 i = 0; i < WorldGen->POIs.Num(); ++i)
	{
		float W = 1.f + WorldGen->POIs[i].Tier * 1.5f;
		if (Squad.VisitedPois.Contains(i)) W *= 0.1f;
		Weights.Add(W);
	}
	float Total = 0.f;
	for (float W : Weights) Total += W;
	float Pick = FMath::FRand() * Total;
	int32 Chosen = 0;
	for (int32 i = 0; i < Weights.Num(); ++i)
	{
		Pick -= Weights[i];
		if (Pick <= 0.f)
		{
			Chosen = i;
			break;
		}
	}
	Squad.VisitedPois.Add(Chosen);
	Squad.Poi = Chosen;
	const FTOPOI& Poi = WorldGen->POIs[Chosen];
	Squad.Objective = Poi.Location + FVector(FMath::FRandRange(-1500.f, 1500.f), FMath::FRandRange(-1500.f, 1500.f), 0.f);
	Squad.bHasObjective = true;
	Squad.bLoot = true;
	for (int32 i = 0; i < Squad.Members.Num(); ++i)
	{
		if (ATOCharacter* M = Squad.Members[i].Get())
		{
			if (ATOAIController* AI = Cast<ATOAIController>(M->GetController()))
			{
				AI->SetObjective(Squad.Objective + FVector(FMath::FRandRange(-400.f, 400.f), FMath::FRandRange(-400.f, 400.f), 0.f), FMath::Max(1500.f, Poi.Radius * 0.35f), true);
			}
		}
	}
}

void ATOGameMode::RequestNextObjective(ATOAIController* AI)
{
	if (!AI)
	{
		return;
	}
	if (AI->GetRole() == ETOAIRole::WarfareBot)
	{
		AssignWarfareObjective(AI);
		return;
	}
	if (AI->GetRole() != ETOAIRole::Rival)
	{
		return;
	}
	FTOSquadInfo* Squad = FindSquad(AI->GetSquadId());
	if (!Squad)
	{
		return;
	}
	// The first member that finishes looting moves the whole squad on.
	++Squad->Visits;
	AssignRivalObjective(*Squad);
}

void ATOGameMode::TickOperations(float Dt, float Now)
{
	ATOCharacter* P = PlayerChar.Get();

	// Raid timer
	if (GetRaidTimeLeft() <= 0.f)
	{
		FinishRaid(false, TEXT("MISSING IN ACTION"));
		return;
	}

	// Rival squads
	while (RivalsSpawned < RivalSpawnTimes.Num() && Now - MatchStart >= RivalSpawnTimes[RivalsSpawned])
	{
		SpawnRivalSquad(RivalsSpawned);
		++RivalsSpawned;
	}
	// Late raid: everyone heads out
	if (Now > NextRivalCheck)
	{
		NextRivalCheck = Now + 10.f;
		for (FTOSquadInfo& S : Squads)
		{
			if (S.Role == ETOAIRole::Rival && !S.bExtracting && GetRaidTimeLeft() < 420.f)
			{
				AssignRivalObjective(S);
			}
		}
	}

	if (!P)
	{
		return;
	}

	// Being downed needs a living squad mate, otherwise it is death.
	bool bMateAlive = false;
	if (FTOSquadInfo* Squad = FindSquad(PlayerSquadId))
	{
		for (const TWeakObjectPtr<ATOCharacter>& M : Squad->Members)
		{
			if (M.IsValid() && M.Get() != P && M->IsAlive() && !M->IsDowned())
			{
				bMateAlive = true;
			}
		}
	}
	P->GetHealth()->bCanBeDowned = bMateAlive;
	if (P->IsDowned() && !bMateAlive)
	{
		PlayerGiveUp();
		return;
	}

	// Extraction
	ATOExtractionZone* InZone = nullptr;
	ExtractBlockReason.Reset();
	if (P->IsAlive() && !P->IsDowned())
	{
		for (const TWeakObjectPtr<ATOExtractionZone>& Z : ExtractionZones)
		{
			if (Z.IsValid() && Z->IsAvailable() && Z->IsInside(P->GetActorLocation()))
			{
				InZone = Z.Get();
				break;
			}
		}
	}
	if (InZone)
	{
		FString Reason;
		if (InZone->CanExtract(P, Reason))
		{
			const float Before = ExtractProgress;
			ExtractProgress += Dt;
			if (FMath::FloorToInt(Before) != FMath::FloorToInt(ExtractProgress))
			{
				if (Combat && Combat->GetAudio())
				{
					Combat->GetAudio()->Play2D(ETOSound::Beep, 0.6f, 1.f + ExtractProgress * 0.05f);
				}
			}
			if (ExtractProgress >= GetExtractDuration())
			{
				Result.ExtractName = InZone->GetZoneName();
				FinishRaid(true, TEXT("EXTRACTED"));
				return;
			}
		}
		else
		{
			ExtractProgress = 0.f;
			ExtractBlockReason = Reason;
		}
	}
	else
	{
		ExtractProgress = 0.f;
	}
	PlayerExtractZone = InZone;
}

void ATOGameMode::FinishRaid(bool bExtracted, const FString& Title)
{
	if (State == ETOGMState::Ended)
	{
		return;
	}
	State = ETOGMState::Ended;
	UWorld* World = GetWorld();
	EndTime = World->GetTimeSeconds();
	UTOGameInstance* GI = UTOGameInstance::Get(this);
	UTOSaveGame* Save = GI ? GI->GetSave() : nullptr;
	ATOCharacter* P = PlayerChar.Get();

	Result.bValid = true;
	Result.Mode = ETOMatchMode::Operations;
	Result.bExtracted = bExtracted;
	Result.bSurvived = bExtracted;
	Result.Title = Title;
	Result.Duration = EndTime - MatchStart;
	Result.LoadoutCost = GI ? GI->PaidLoadoutCost : 0;

	if (P && Save)
	{
		UTOInventoryComponent* Inv = P->GetInventory();
		Save->SafeBoxItems = Inv->SafeBox.Items;
		Save->Raids += 1;
		Save->Kills += Result.Kills;
		if (bExtracted)
		{
			TArray<FTOItemInstance> Items;
			Inv->CollectAll(Items, false, true);
			UTOWeaponComponent* W = P->GetWeapons();
			for (int32 Slot = 0; Slot < UTOWeaponComponent::SlotMelee; ++Slot)
			{
				if (W->HasWeapon(Slot))
				{
					Items.Add(TODB::MakeWeaponItem(W->GetSlot(Slot).Config));
				}
			}
			int64 Value = 0;
			Items.Sort([](const FTOItemInstance& A, const FTOItemInstance& B) { return TODB::ItemValue(A) > TODB::ItemValue(B); });
			for (const FTOItemInstance& It : Items)
			{
				const int64 V = TODB::ItemValue(It);
				Value += V;
				if (Result.TopItems.Num() < 10 && V > 0)
				{
					Result.TopItems.Add(FString::Printf(TEXT("%s|%s|%d"), *TODB::ItemName(It), *TOUtil::FormatMoney(V), (int32)(TODB::FindItem(It.ItemId) ? TODB::FindItem(It.ItemId)->Rarity : ETORarity::Common)));
				}
			}
			Result.ValueExtracted = Value - PendingExtractionCost;
			GI->AddCredits(Result.ValueExtracted);
			Save->Extractions += 1;
			Save->TotalExtracted += Value;
			Save->BestRaid = FMath::Max(Save->BestRaid, Value);

			// Leave the battlefield: hide the operator, watch from above
			P->CancelActions();
			P->SetActorHiddenInGame(true);
			P->SetActorEnableCollision(false);
			FActorSpawnParameters SP;
			SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector Loc = P->GetActorLocation() + FVector(-2500.f, 0.f, 2500.f);
			MenuCamera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Loc, (P->GetActorLocation() - Loc).Rotation(), SP);
			if (LocalPC && MenuCamera)
			{
				LocalPC->SetViewTargetWithBlend(MenuCamera, 1.5f);
			}
		}
		else
		{
			Save->Deaths += 1;
			Result.KilledBy = P->KilledByName.IsEmpty() ? FString(TEXT("-")) : P->KilledByName;
			if (!P->KilledByWeapon.IsEmpty())
			{
				Result.KilledBy += FString::Printf(TEXT(" (%s, %.0f m)"), *P->KilledByWeapon, P->KilledByDistance);
			}
			if (P->IsAlive())
			{
				FTODamageInfo Info;
				Info.bBleed = true;
				P->Kill(Info);
			}
		}
	}
	if (GI)
	{
		GI->LastResult = Result;
		GI->PaidLoadoutCost = 0;
		GI->WriteSave();
	}
	if (ATOPlayerController* PC = Cast<ATOPlayerController>(LocalPC))
	{
		if (bExtracted)
		{
			PC->ShowResults();
		}
		else
		{
			PC->ShowDeath();
		}
	}
}
