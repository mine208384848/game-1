// TAC-OPS - game mode: Warfare (large scale attack & defend with tickets)

#include "Core/TOGameMode.h"
#include "Core/TOGameInstance.h"
#include "Core/TOSaveGame.h"
#include "Core/TOPlayerController.h"
#include "Core/TODatabase.h"
#include "Characters/TOCharacter.h"
#include "AI/TOAIController.h"
#include "Weapons/TOCombatManager.h"
#include "World/TOWorldGenerator.h"
#include "World/TOExtractionZone.h"
#include "Audio/TOAudio.h"
#include "Engine/World.h"

namespace
{
	constexpr int32 GBotsPerTeam = 12;
}

void ATOGameMode::SetupWarfare()
{
	UWorld* World = GetWorld();
	if (!WorldGen || WorldGen->Sectors.Num() == 0)
	{
		return;
	}
	Result = FTOMatchResult();
	Result.Mode = ETOMatchMode::Warfare;
	PlayerTeam = 0;
	AttackerTeam = (Session.WarfareSide == 0) ? 0 : 1;
	MaxTickets = 300;
	Tickets = MaxTickets;
	TeamKills.Init(0, 2);

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const int32 DefenderTeam = 1 - AttackerTeam;
	for (int32 s = 0; s < WorldGen->Sectors.Num(); ++s)
	{
		const FTOSectorDef& Sec = WorldGen->Sectors[s];
		for (int32 p = 0; p < Sec.Points.Num(); ++p)
		{
			if (ATOCapturePoint* CP = World->SpawnActor<ATOCapturePoint>(ATOCapturePoint::StaticClass(), Sec.Points[p], FRotator::ZeroRotator, P))
			{
				CP->Setup(Sec.Labels.IsValidIndex(p) ? Sec.Labels[p] : FString::Printf(TEXT("%c%d"), TCHAR('A' + p), s + 1), s, 1600.f, DefenderTeam);
				CapturePoints.Add(CP);
			}
		}
	}

	WarfareSquads.Init(-1, 2);
	WarfareSquads[0] = NewSquad(0, ETOAIRole::WarfareBot, -1);
	WarfareSquads[1] = NewSquad(1, ETOAIRole::WarfareBot, -1);
	CurrentSector = 0;
	ActivateSector(0);

	for (int32 i = 0; i < GBotsPerTeam - 1; ++i)
	{
		SpawnWarfareBot(0, (ETOOperator)(i % TODB::NumOperators()), WarfareSquads[0]);
	}
	for (int32 i = 0; i < GBotsPerTeam; ++i)
	{
		SpawnWarfareBot(1, (ETOOperator)(i % TODB::NumOperators()), WarfareSquads[1]);
	}

	const FVector Spawn = FindSpawnPoint(GetTeamSpawn(PlayerTeam), 600.f);
	const FVector Target = WorldGen->Sectors[0].Points.Num() > 0 ? WorldGen->Sectors[0].Points[0] : FVector::ZeroVector;
	ATOCharacter* Player = SpawnPlayer(Spawn, (Target - Spawn).Rotation().Yaw);
	if (Player)
	{
		Player->SquadId = WarfareSquads[0];
		if (FTOSquadInfo* Squad = FindSquad(WarfareSquads[0]))
		{
			Squad->Members.Add(Player);
		}
	}

	PushMessage(PlayerTeam == AttackerTeam ? TEXT("ATTACK - capture both objectives of every sector before your tickets run out")
		: TEXT("DEFEND - hold the objectives and drain the attackers' tickets"), FLinearColor(0.9f, 0.85f, 0.5f));
}

FVector ATOGameMode::GetTeamSpawn(int32 Team) const
{
	if (!WorldGen || !WorldGen->Sectors.IsValidIndex(CurrentSector))
	{
		return FVector::ZeroVector;
	}
	const FTOSectorDef& S = WorldGen->Sectors[CurrentSector];
	return Team == AttackerTeam ? S.AttackerSpawn : S.DefenderSpawn;
}

void ATOGameMode::ActivateSector(int32 Index)
{
	CurrentSector = Index;
	for (const TWeakObjectPtr<ATOCapturePoint>& Ptr : CapturePoints)
	{
		if (ATOCapturePoint* CP = Ptr.Get())
		{
			CP->bActive = (CP->Sector == Index);
			if (CP->bActive)
			{
				CP->Capture = 0.f;
			}
			CP->UpdateVisual();
		}
	}
	PushMessage(FString::Printf(TEXT("SECTOR %d / %d: %s"), Index + 1, GetNumSectors(), *GetSectorName()), FLinearColor(1.f, 0.8f, 0.3f));
	for (const TWeakObjectPtr<ATOCharacter>& Ptr : Characters)
	{
		if (ATOCharacter* C = Ptr.Get())
		{
			if (ATOAIController* AI = Cast<ATOAIController>(C->GetController()))
			{
				if (AI->GetRole() == ETOAIRole::WarfareBot)
				{
					AssignWarfareObjective(AI);
				}
			}
		}
	}
}

void ATOGameMode::SpawnWarfareBot(int32 Team, ETOOperator Op, int32 SquadId)
{
	const FVector Spawn = FindSpawnPoint(GetTeamSpawn(Team), 1500.f);
	++BotCounter;
	const FString Name = FString::Printf(TEXT("%s-%02d"), Team == PlayerTeam ? TEXT("Ally") : TEXT("Enemy"), BotCounter);
	if (ATOCharacter* C = SpawnAI(Spawn, 0.f, Team, ETOAIRole::WarfareBot, 2, SquadId, TArray<FVector>(), Name))
	{
		C->Operator = Op;
		if (ATOAIController* AI = Cast<ATOAIController>(C->GetController()))
		{
			AssignWarfareObjective(AI);
		}
	}
}

void ATOGameMode::AssignWarfareObjective(ATOAIController* AI)
{
	ATOCharacter* C = AI ? Cast<ATOCharacter>(AI->GetPawn()) : nullptr;
	if (!C)
	{
		return;
	}
	TArray<ATOCapturePoint*> Candidates;
	for (const TWeakObjectPtr<ATOCapturePoint>& Ptr : CapturePoints)
	{
		ATOCapturePoint* CP = Ptr.Get();
		if (!CP || !CP->bActive)
		{
			continue;
		}
		if (C->TeamId == AttackerTeam && CP->OwnerTeam == AttackerTeam)
		{
			continue;
		}
		Candidates.Add(CP);
	}
	if (Candidates.Num() == 0)
	{
		for (const TWeakObjectPtr<ATOCapturePoint>& Ptr : CapturePoints)
		{
			if (Ptr.IsValid() && Ptr->bActive)
			{
				Candidates.Add(Ptr.Get());
			}
		}
	}
	if (Candidates.Num() == 0)
	{
		return;
	}
	ATOCapturePoint* CP = Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
	AI->SetObjective(CP->GetActorLocation(), CP->Radius * 0.7f, false);
}

void ATOGameMode::TickWarfare(float Dt, float Now)
{
	if (Now >= NextCaptureTick)
	{
		const float Step = 0.25f;
		NextCaptureTick = Now + Step;
		bool bAllCaptured = true;
		bool bAnyActive = false;
		for (const TWeakObjectPtr<ATOCapturePoint>& Ptr : CapturePoints)
		{
			ATOCapturePoint* CP = Ptr.Get();
			if (!CP || !CP->bActive)
			{
				continue;
			}
			bAnyActive = true;
			int32 Counts[2] = { 0, 0 };
			bool bPlayerInside = false;
			for (const TWeakObjectPtr<ATOCharacter>& CPtr : Characters)
			{
				const ATOCharacter* C = CPtr.Get();
				if (!C || !C->IsAlive() || C->IsDowned() || C->TeamId < 0 || C->TeamId > 1)
				{
					continue;
				}
				const FVector D = C->GetActorLocation() - CP->GetActorLocation();
				if (D.Size2D() < CP->Radius && FMath::Abs(D.Z) < 1200.f)
				{
					++Counts[C->TeamId];
					bPlayerInside |= (C == PlayerChar.Get());
				}
			}
			CP->CountTeam0 = Counts[0];
			CP->CountTeam1 = Counts[1];
			const int32 Att = Counts[AttackerTeam];
			const int32 Def = Counts[1 - AttackerTeam];
			if (CP->OwnerTeam != AttackerTeam)
			{
				if (Att > 0 && Def == 0)
				{
					CP->Capture += Step * (0.05f + 0.02f * FMath::Min(Att, 4));
				}
				else if (Def > 0 && Att == 0)
				{
					CP->Capture = FMath::Max(0.f, CP->Capture - Step * 0.1f);
				}
				if (CP->Capture >= 1.f)
				{
					CP->Capture = 1.f;
					CP->SetOwnerTeam(AttackerTeam);
					PushMessage(FString::Printf(TEXT("Objective %s captured by %s"), *CP->Label, AttackerTeam == PlayerTeam ? TEXT("your team") : TEXT("the enemy")),
						AttackerTeam == PlayerTeam ? FLinearColor(0.3f, 0.6f, 1.f) : FLinearColor(1.f, 0.35f, 0.3f));
					if (bPlayerInside)
					{
						Result.Captures += 1;
					}
					if (Combat && Combat->GetAudio())
					{
						Combat->GetAudio()->Play2D(ETOSound::Capture, 0.8f);
					}
				}
				CP->UpdateVisual();
			}
			if (CP->OwnerTeam != AttackerTeam)
			{
				bAllCaptured = false;
			}
		}
		if (bAnyActive && bAllCaptured)
		{
			if (CurrentSector + 1 >= GetNumSectors())
			{
				FinishWarfare(true);
				return;
			}
			Tickets = FMath::Min(MaxTickets, Tickets + 75);
			ActivateSector(CurrentSector + 1);
		}
	}

	// Bot reinforcements
	for (int32 i = PendingRespawns.Num() - 1; i >= 0; --i)
	{
		const FTOPendingRespawn R = PendingRespawns[i];
		if (Now < R.Time)
		{
			continue;
		}
		PendingRespawns.RemoveAt(i);
		if (R.Team == AttackerTeam && Tickets <= 0)
		{
			continue;
		}
		SpawnWarfareBot(R.Team, R.Operator, WarfareSquads.IsValidIndex(R.Team) ? WarfareSquads[R.Team] : -1);
	}

	if (Tickets <= 0)
	{
		// Attackers lose once they are out of tickets and have nobody left alive near the objectives.
		bool bAttackersAlive = false;
		for (const TWeakObjectPtr<ATOCharacter>& CPtr : Characters)
		{
			if (CPtr.IsValid() && CPtr->IsAlive() && CPtr->TeamId == AttackerTeam)
			{
				bAttackersAlive = true;
				break;
			}
		}
		if (!bAttackersAlive)
		{
			FinishWarfare(false);
		}
	}
}

bool ATOGameMode::CanPlayerRespawn() const
{
	if (Mode != ETOMatchMode::Warfare || !bPlayerAwaitingRespawn || State != ETOGMState::Playing)
	{
		return false;
	}
	if (PlayerTeam == AttackerTeam && Tickets <= 0)
	{
		return false;
	}
	return GetWorld()->GetTimeSeconds() - PlayerDeathTime >= 6.f;
}

float ATOGameMode::GetRespawnWait() const
{
	return FMath::Max(0.f, 6.f - (GetWorld()->GetTimeSeconds() - PlayerDeathTime));
}

void ATOGameMode::PlayerRequestRespawn(ETOOperator Operator, const FTOWeaponConfig& Weapon)
{
	if (!CanPlayerRespawn())
	{
		return;
	}
	Session.Loadout.Operator = Operator;
	if (Weapon.IsValid())
	{
		Session.Loadout.Primary = Weapon;
	}
	if (ATOCharacter* Old = PlayerChar.Get())
	{
		Old->SetLifeSpan(30.f);
	}
	bPlayerAwaitingRespawn = false;
	const FVector Spawn = FindSpawnPoint(GetTeamSpawn(PlayerTeam), 800.f);
	FVector Target = Spawn;
	for (const TWeakObjectPtr<ATOCapturePoint>& Ptr : CapturePoints)
	{
		if (Ptr.IsValid() && Ptr->bActive)
		{
			Target = Ptr->GetActorLocation();
			break;
		}
	}
	SpawnPlayer(Spawn, (Target - Spawn).Rotation().Yaw);
}

void ATOGameMode::FinishWarfare(bool bAttackersWin)
{
	if (State == ETOGMState::Ended)
	{
		return;
	}
	State = ETOGMState::Ended;
	EndTime = GetWorld()->GetTimeSeconds();
	const bool bWin = (PlayerTeam == AttackerTeam) == bAttackersWin;
	ATOCharacter* P = PlayerChar.Get();
	Result.bValid = true;
	Result.Mode = ETOMatchMode::Warfare;
	Result.bVictory = bWin;
	Result.bSurvived = true;
	Result.Title = bWin ? TEXT("VICTORY") : TEXT("DEFEAT");
	Result.Duration = EndTime - MatchStart;
	(void)P;
	const int32 Kills = Result.Kills;
	const int64 Reward = (int64)Kills * 1500 + (int64)Result.Captures * 5000 + (bWin ? 40000 : 15000);
	Result.ValueExtracted = Reward;
	if (UTOGameInstance* GI = UTOGameInstance::Get(this))
	{
		GI->AddCredits(Reward);
		if (UTOSaveGame* Save = GI->GetSave())
		{
			Save->WarfareMatches += 1;
			Save->WarfareWins += bWin ? 1 : 0;
			Save->Kills += Kills;
		}
		GI->LastResult = Result;
		GI->WriteSave();
	}
	if (ATOPlayerController* PC = Cast<ATOPlayerController>(LocalPC))
	{
		PC->ShowResults();
	}
}
