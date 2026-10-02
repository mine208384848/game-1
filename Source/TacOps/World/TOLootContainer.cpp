// TAC-OPS - lootable containers

#include "World/TOLootContainer.h"
#include "Core/TODatabase.h"
#include "Core/TOPlayerController.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOInventoryComponent.h"
#include "Weapons/TOCombatManager.h"
#include "Audio/TOAudio.h"
#include "World/TOMaterialLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

ATOLootContainer::ATOLootContainer()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	InteractBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractBox"));
	InteractBox->SetupAttachment(Root);
	InteractBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	InteractBox->SetCanEverAffectNavigation(false);
	InteractBox->SetBoxExtent(FVector(40.f, 40.f, 30.f));
	InteractBox->SetRelativeLocation(FVector(0.f, 0.f, 30.f));
}

void ATOLootContainer::BeginPlay()
{
	Super::BeginPlay();
	if (!bVisualBuilt)
	{
		BuildVisual();
	}
}

void ATOLootContainer::InitRandom(ETOContainerType InType, int32 InTier, int32 Seed)
{
	Type = InType;
	Tier = InTier;
	DisplayName = TODB::ContainerName(Type);
	int32 W = 5, H = 4;
	TODB::ContainerGridSize(Type, W, H);
	Grid.Init(FName(*DisplayName), W, H);
	FRandomStream Rng(Seed);
	TArray<FTOItemInstance> Items;
	TODB::RollLoot(Type, Tier, Rng, Items);
	for (FTOItemInstance& It : Items)
	{
		It.bRevealed = false;
		Grid.TryAdd(It);
	}
	if (HasActorBegunPlay())
	{
		BuildVisual();
	}
}

void ATOLootContainer::InitWithItems(ETOContainerType InType, const FString& InName, const TArray<FTOItemInstance>& Items)
{
	Type = InType;
	DisplayName = InName;
	int32 W = 8, H = 9;
	TODB::ContainerGridSize(Type, W, H);
	// Grow the grid until everything fits (bodies can carry a lot).
	for (int32 Attempt = 0; Attempt < 12; ++Attempt)
	{
		Grid.Init(FName(*DisplayName), W, H);
		bool bAll = true;
		for (FTOItemInstance It : Items)
		{
			It.bRevealed = (Type == ETOContainerType::LooseItem);
			It.bRotated = false;
			if (Grid.TryAdd(It) != 0)
			{
				bAll = false;
				break;
			}
		}
		if (bAll)
		{
			break;
		}
		H += 2;
	}
	if (HasActorBegunPlay())
	{
		BuildVisual();
	}
}

void ATOLootContainer::BuildVisual()
{
	bVisualBuilt = true;
	for (UStaticMeshComponent* P : Parts)
	{
		if (IsValid(P)) P->DestroyComponent();
	}
	Parts.Reset();
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	if (!Lib)
	{
		return;
	}

	auto Box = [&](ETOMat Mat, const FVector& Loc, const FVector& Size, bool bSolid)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
		M->SetStaticMesh(Lib->GetMesh(ETOShape::Cube));
		M->SetMaterial(0, Lib->GetMaterial(Mat));
		M->SetupAttachment(Root);
		M->SetRelativeLocation(Loc);
		M->SetRelativeScale3D(Size / 100.f);
		if (bSolid)
		{
			M->SetCollisionProfileName(TEXT("BlockAll"));
			M->ComponentTags.Add(UTOMaterialLibrary::GetSurfaceTag(Mat));
		}
		else
		{
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		M->SetCanEverAffectNavigation(false);
		M->SetCullDistance(15000.f);
		M->RegisterComponent();
		Parts.Add(M);
		return M;
	};

	FVector Ext(40.f, 40.f, 30.f);
	switch (Type)
	{
	case ETOContainerType::WeaponCrate:
		Box(ETOMat::MetalGreen, FVector(0.f, 0.f, 22.f), FVector(120.f, 50.f, 44.f), true);
		Box(ETOMat::MetalDark, FVector(0.f, 0.f, 45.f), FVector(122.f, 52.f, 4.f), false);
		Box(ETOMat::MetalYellow, FVector(0.f, -26.f, 22.f), FVector(20.f, 1.f, 8.f), false);
		Ext = FVector(62.f, 28.f, 26.f);
		break;
	case ETOContainerType::AmmoBox:
		Box(ETOMat::MetalGreen, FVector(0.f, 0.f, 16.f), FVector(60.f, 30.f, 32.f), true);
		Box(ETOMat::MetalYellow, FVector(0.f, -15.5f, 16.f), FVector(40.f, 1.f, 6.f), false);
		Ext = FVector(32.f, 18.f, 20.f);
		break;
	case ETOContainerType::MedicalCase:
		Box(ETOMat::MetalWhite, FVector(0.f, 0.f, 13.f), FVector(50.f, 35.f, 26.f), true);
		Box(ETOMat::MetalRed, FVector(0.f, 0.f, 26.5f), FVector(20.f, 6.f, 1.f), false);
		Box(ETOMat::MetalRed, FVector(0.f, 0.f, 26.5f), FVector(6.f, 20.f, 1.f), false);
		Ext = FVector(27.f, 20.f, 16.f);
		break;
	case ETOContainerType::Toolbox:
		Box(ETOMat::MetalRed, FVector(0.f, 0.f, 13.f), FVector(50.f, 25.f, 26.f), true);
		Box(ETOMat::MetalDark, FVector(0.f, 0.f, 29.f), FVector(30.f, 4.f, 4.f), false);
		Ext = FVector(27.f, 15.f, 17.f);
		break;
	case ETOContainerType::Safe:
		Box(ETOMat::MetalDark, FVector(0.f, 0.f, 42.f), FVector(62.f, 60.f, 84.f), true);
		Box(ETOMat::Aluminium, FVector(0.f, -30.5f, 50.f), FVector(10.f, 2.f, 10.f), false);
		Box(ETOMat::Gold, FVector(-18.f, -30.5f, 50.f), FVector(3.f, 2.f, 14.f), false);
		Ext = FVector(33.f, 32.f, 44.f);
		break;
	case ETOContainerType::ComputerTower:
		Box(ETOMat::Plastic, FVector(0.f, 0.f, 24.f), FVector(22.f, 46.f, 48.f), true);
		Box(ETOMat::LampGreen, FVector(-11.5f, -18.f, 40.f), FVector(1.f, 2.f, 2.f), false);
		Ext = FVector(14.f, 25.f, 26.f);
		break;
	case ETOContainerType::Duffel:
	{
		UStaticMeshComponent* M = Box(ETOMat::Canvas, FVector(0.f, 0.f, 16.f), FVector(70.f, 34.f, 30.f), true);
		if (M)
		{
			M->SetStaticMesh(Lib->GetMesh(ETOShape::Cylinder));
			M->SetRelativeRotation(FRotator(90.f, 0.f, 0.f));
			M->SetRelativeScale3D(FVector(0.34f, 0.32f, 0.72f));
		}
		Box(ETOMat::Black, FVector(0.f, 0.f, 32.f), FVector(30.f, 3.f, 3.f), false);
		Ext = FVector(38.f, 20.f, 18.f);
		break;
	}
	case ETOContainerType::FilingCabinet:
		Box(ETOMat::MetalGrey, FVector(0.f, 0.f, 66.f), FVector(50.f, 46.f, 132.f), true);
		for (int32 i = 0; i < 4; ++i)
		{
			Box(ETOMat::MetalDark, FVector(0.f, -23.5f, 18.f + i * 32.f), FVector(30.f, 1.f, 2.f), false);
		}
		Ext = FVector(27.f, 25.f, 68.f);
		break;
	case ETOContainerType::ServerRack:
		Box(ETOMat::MetalDark, FVector(0.f, 0.f, 100.f), FVector(62.f, 80.f, 200.f), true);
		for (int32 i = 0; i < 8; ++i)
		{
			Box((i % 3 == 0) ? ETOMat::LampCold : ETOMat::LampGreen, FVector(-31.5f, -25.f + (i % 4) * 14.f, 40.f + i * 18.f), FVector(1.f, 3.f, 2.f), false);
		}
		Ext = FVector(33.f, 42.f, 102.f);
		break;
	case ETOContainerType::Locker:
		Box(ETOMat::MetalBlue, FVector(0.f, 0.f, 95.f), FVector(50.f, 50.f, 190.f), true);
		Box(ETOMat::MetalDark, FVector(0.f, -25.5f, 160.f), FVector(30.f, 1.f, 6.f), false);
		Ext = FVector(27.f, 27.f, 97.f);
		break;
	case ETOContainerType::HighValueCrate:
		Box(ETOMat::MetalYellow, FVector(0.f, 0.f, 26.f), FVector(100.f, 62.f, 52.f), true);
		Box(ETOMat::Black, FVector(-30.f, 0.f, 26.f), FVector(10.f, 64.f, 54.f), false);
		Box(ETOMat::Black, FVector(30.f, 0.f, 26.f), FVector(10.f, 64.f, 54.f), false);
		Box(ETOMat::LampWarm, FVector(0.f, -31.5f, 40.f), FVector(8.f, 1.f, 4.f), false);
		Ext = FVector(52.f, 33.f, 28.f);
		break;
	case ETOContainerType::LooseItem:
	{
		ETOMat Mat = ETOMat::Plastic;
		if (Grid.Items.Num() > 0)
		{
			if (const FTOItemDef* D = TODB::FindItem(Grid.Items[0].ItemId))
			{
				Mat = D->Rarity >= ETORarity::Legendary ? ETOMat::Gold : (D->Rarity >= ETORarity::Rare ? ETOMat::MetalBlue : ETOMat::Plastic);
			}
		}
		Box(Mat, FVector(0.f, 0.f, 6.f), FVector(22.f, 16.f, 12.f), false);
		Ext = FVector(25.f, 25.f, 15.f);
		break;
	}
	case ETOContainerType::Corpse:
	default:
		Ext = FVector(70.f, 45.f, 25.f);
		break;
	}
	InteractBox->SetBoxExtent(Ext);
	InteractBox->SetRelativeLocation(FVector(0.f, 0.f, Ext.Z));
}

FString ATOLootContainer::GetInteractLabel(const ATOCharacter* User) const
{
	if (Type == ETOContainerType::LooseItem)
	{
		if (Grid.Items.Num() > 0)
		{
			return FString::Printf(TEXT("Pick up %s"), *TODB::ItemName(Grid.Items[0]));
		}
		return TEXT("Empty");
	}
	return FString::Printf(TEXT("%s %s"), Type == ETOContainerType::Corpse ? TEXT("Loot") : TEXT("Search"), *DisplayName);
}

FString ATOLootContainer::GetInteractHint(const ATOCharacter* User) const
{
	if (bOpened && Grid.Items.Num() == 0)
	{
		return TEXT("Empty");
	}
	if (bOpened && !HasUnrevealed())
	{
		return FString::Printf(TEXT("%d item(s)"), Grid.Items.Num());
	}
	return FString();
}

void ATOLootContainer::Interact(ATOCharacter* User)
{
	if (!User)
	{
		return;
	}
	if (Type == ETOContainerType::LooseItem && Grid.Items.Num() > 0)
	{
		// Loose items go straight into the bag.
		FTOItemInstance Item = Grid.Items[0];
		Item.bRevealed = true;
		if (User->GetInventory() && User->GetInventory()->AddItem(Item) == 0)
		{
			Grid.Items.RemoveAt(0);
			++Revision;
			if (ATOCombatManager* CM = ATOCombatManager::Get(this))
			{
				if (CM->GetAudio()) CM->GetAudio()->Play2D(ETOSound::Pickup, 0.7f);
			}
			Destroy();
			return;
		}
	}
	bOpened = true;
	if (ATOPlayerController* PC = Cast<ATOPlayerController>(User->GetController()))
	{
		PC->OpenLoot(this);
	}
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::Door, GetActorLocation(), 0.4f, 1.6f);
	}
}

void ATOLootContainer::SetBeingSearched(bool bSearched)
{
	bBeingSearched = bSearched;
	SetActorTickEnabled(bSearched);
	if (bSearched)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (NextRevealTime < Now)
		{
			RevealStart = Now;
			RevealDuration = TODB::ContainerSearchTime(Type);
			NextRevealTime = Now + RevealDuration;
		}
	}
}

bool ATOLootContainer::HasUnrevealed() const
{
	for (const FTOItemInstance& It : Grid.Items)
	{
		if (!It.bRevealed)
		{
			return true;
		}
	}
	return false;
}

bool ATOLootContainer::IsFullySearched() const
{
	return bOpened && !HasUnrevealed();
}

float ATOLootContainer::GetRevealProgress() const
{
	if (!bBeingSearched || RevealDuration <= 0.f)
	{
		return 0.f;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	return FMath::Clamp((Now - RevealStart) / RevealDuration, 0.f, 1.f);
}

void ATOLootContainer::RevealNext()
{
	for (FTOItemInstance& It : Grid.Items)
	{
		if (!It.bRevealed)
		{
			It.bRevealed = true;
			++Revision;
			const FTOItemDef* D = TODB::FindItem(It.ItemId);
			if (ATOCombatManager* CM = ATOCombatManager::Get(this))
			{
				if (CM->GetAudio())
				{
					const bool bRare = D && D->Rarity >= ETORarity::Epic;
					CM->GetAudio()->Play2D(bRare ? ETOSound::LootRare : ETOSound::LootReveal, 0.5f, D ? 1.f + 0.06f * (int32)D->Rarity : 1.f);
				}
			}
			break;
		}
	}
	// Time for the next one depends on its rarity.
	const float Now = GetWorld()->GetTimeSeconds();
	float Next = TODB::ContainerSearchTime(Type);
	for (const FTOItemInstance& It : Grid.Items)
	{
		if (!It.bRevealed)
		{
			if (const FTOItemDef* D = TODB::FindItem(It.ItemId))
			{
				Next *= 1.f + 0.35f * (int32)D->Rarity;
			}
			break;
		}
	}
	RevealStart = Now;
	RevealDuration = Next;
	NextRevealTime = Now + Next;
}

void ATOLootContainer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bBeingSearched || !HasUnrevealed())
	{
		return;
	}
	if (GetWorld()->GetTimeSeconds() >= NextRevealTime)
	{
		RevealNext();
	}
}
