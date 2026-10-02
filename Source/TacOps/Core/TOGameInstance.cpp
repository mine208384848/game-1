// TAC-OPS - game instance

#include "Core/TOGameInstance.h"
#include "Core/TOSaveGame.h"
#include "Core/TODatabase.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void UTOGameInstance::Init()
{
	Super::Init();
	GetSave();
	if (Save)
	{
		Session.Loadout = Save->Loadout;
		Session.Time = Save->LastTime;
		Session.Difficulty = Save->LastDifficulty;
		Session.bSquad = Save->bLastSquad;
		Session.WarfareSide = Save->LastWarfareSide;
	}
	if (!Session.Loadout.Primary.IsValid())
	{
		Session.Loadout.Primary = TODB::MakeDefaultConfig(FName(TEXT("M4A1")), 3);
	}
	if (!Session.Loadout.Sidearm.IsValid())
	{
		Session.Loadout.Sidearm = TODB::MakeDefaultConfig(FName(TEXT("G17")), 2);
	}
	Session.bDeploy = false;
	Session.Mode = ETOMatchMode::Menu;
}

UTOGameInstance* UTOGameInstance::Get(const UObject* WorldContext)
{
	const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	return World ? Cast<UTOGameInstance>(World->GetGameInstance()) : nullptr;
}

UTOSaveGame* UTOGameInstance::GetSave()
{
	if (!Save)
	{
		if (UGameplayStatics::DoesSaveGameExist(SlotName(), 0))
		{
			Save = Cast<UTOSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName(), 0));
		}
		if (!Save)
		{
			Save = Cast<UTOSaveGame>(UGameplayStatics::CreateSaveGameObject(UTOSaveGame::StaticClass()));
			if (Save)
			{
				Save->Loadout.Primary = TODB::MakeDefaultConfig(FName(TEXT("M4A1")), 3);
				Save->Loadout.Sidearm = TODB::MakeDefaultConfig(FName(TEXT("G17")), 2);
			}
		}
	}
	return Save;
}

void UTOGameInstance::WriteSave()
{
	if (UTOSaveGame* S = GetSave())
	{
		S->Loadout = Session.Loadout;
		S->LastTime = Session.Time;
		S->LastDifficulty = Session.Difficulty;
		S->bLastSquad = Session.bSquad;
		S->LastWarfareSide = Session.WarfareSide;
		UGameplayStatics::SaveGameToSlot(S, SlotName(), 0);
	}
}

int64 UTOGameInstance::GetCredits()
{
	UTOSaveGame* S = GetSave();
	return S ? S->Credits : 0;
}

bool UTOGameInstance::SpendCredits(int64 Amount)
{
	UTOSaveGame* S = GetSave();
	if (!S || S->Credits < Amount)
	{
		return false;
	}
	S->Credits -= Amount;
	return true;
}

void UTOGameInstance::AddCredits(int64 Amount)
{
	if (UTOSaveGame* S = GetSave())
	{
		S->Credits += Amount;
	}
}

void UTOGameInstance::StartMatch(ETOMatchMode Mode)
{
	Session.Mode = Mode;
	Session.bDeploy = true;
	Session.Seed = FMath::RandRange(1, 1000000);
	WriteSave();
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true);
}

void UTOGameInstance::ReturnToLobby()
{
	Session.bDeploy = false;
	Session.Mode = ETOMatchMode::Menu;
	PaidLoadoutCost = 0;
	WriteSave();
	UGameplayStatics::OpenLevel(this, FName(TEXT("/Engine/Maps/Entry")), true);
}

void UTOGameInstance::ApplyGraphicsQuality(int32 Quality)
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		Settings->SetOverallScalabilityLevel(FMath::Clamp(Quality, 0, 4));
		Settings->ApplySettings(false);
	}
}
