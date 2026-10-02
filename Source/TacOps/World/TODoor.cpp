// TAC-OPS - hinged doors, keycard rooms

#include "World/TODoor.h"
#include "Core/TODatabase.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOInventoryComponent.h"
#include "Weapons/TOCombatManager.h"
#include "Audio/TOAudio.h"
#include "World/TOMaterialLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

ATODoor::ATODoor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
	Hinge = CreateDefaultSubobject<USceneComponent>(TEXT("Hinge"));
	Hinge->SetupAttachment(Root);
}

void ATODoor::BeginPlay()
{
	Super::BeginPlay();
	if (!bBuilt)
	{
		Build();
	}
}

void ATODoor::Setup(float InWidth, float InHeight, ETOMat InMat, FName InKeyId)
{
	Width = InWidth;
	Height = InHeight;
	Mat = InMat;
	KeyId = InKeyId;
	if (HasActorBegunPlay())
	{
		Build();
	}
}

void ATODoor::Build()
{
	bBuilt = true;
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
	Hinge->SetRelativeLocation(FVector(-Width * 0.5f, 0.f, 0.f));

	auto Make = [&](USceneComponent* Parent, ETOMat M, const FVector& Loc, const FVector& Size, bool bSolid)
	{
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
		C->SetStaticMesh(Lib->GetMesh(ETOShape::Cube));
		C->SetMaterial(0, Lib->GetMaterial(M));
		C->SetupAttachment(Parent);
		C->SetRelativeLocation(Loc);
		C->SetRelativeScale3D(Size / 100.f);
		C->SetCanEverAffectNavigation(false);
		if (bSolid)
		{
			C->SetCollisionProfileName(TEXT("BlockAll"));
			C->ComponentTags.Add(UTOMaterialLibrary::GetSurfaceTag(M));
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		C->RegisterComponent();
		Parts.Add(C);
		return C;
	};

	const bool bKeyDoor = !KeyId.IsNone();
	const ETOMat PanelMat = bKeyDoor ? ETOMat::MetalGrey : Mat;
	Panel = Make(Hinge, PanelMat, FVector(Width * 0.5f, 0.f, Height * 0.5f), FVector(Width - 4.f, 6.f, Height - 3.f), true);
	Make(Hinge, ETOMat::Aluminium, FVector(Width - 14.f, -6.f, 100.f), FVector(12.f, 4.f, 3.f), false);
	Make(Hinge, ETOMat::Aluminium, FVector(Width - 14.f, 6.f, 100.f), FVector(12.f, 4.f, 3.f), false);
	if (bKeyDoor)
	{
		// Hazard stripe and keycard reader with status light
		Make(Hinge, ETOMat::MetalYellow, FVector(Width * 0.5f, -3.5f, 30.f), FVector(Width - 10.f, 1.f, 10.f), false);
		Make(Root, ETOMat::MetalDark, FVector(Width * 0.5f + 18.f, -16.f, 125.f), FVector(10.f, 4.f, 16.f), false);
		Reader = Make(Root, ETOMat::LampRed, FVector(Width * 0.5f + 18.f, -18.5f, 130.f), FVector(4.f, 1.f, 4.f), false);
	}
}

FString ATODoor::GetInteractLabel(const ATOCharacter* User) const
{
	if (IsLocked())
	{
		const UTOInventoryComponent* Inv = User ? User->GetInventory() : nullptr;
		if (Inv && Inv->HasKey(KeyId))
		{
			return TEXT("Unlock door");
		}
		return TEXT("Locked");
	}
	return bOpen ? TEXT("Close door") : TEXT("Open door");
}

FString ATODoor::GetInteractHint(const ATOCharacter* User) const
{
	if (!IsLocked())
	{
		return FString();
	}
	for (const FTOItemDef& D : TODB::Items())
	{
		if (D.Category == ETOItemCategory::Key && D.KeyId == KeyId)
		{
			return FString::Printf(TEXT("Requires: %s"), *D.Name);
		}
	}
	return TEXT("Requires a key");
}

float ATODoor::GetInteractDuration(const ATOCharacter* User) const
{
	if (IsLocked())
	{
		const UTOInventoryComponent* Inv = User ? User->GetInventory() : nullptr;
		return (Inv && Inv->HasKey(KeyId)) ? 1.5f : 0.f;
	}
	return 0.f;
}

void ATODoor::Interact(ATOCharacter* User)
{
	if (!User)
	{
		return;
	}
	if (IsLocked())
	{
		UTOInventoryComponent* Inv = User->GetInventory();
		if (!Inv || !Inv->HasKey(KeyId))
		{
			if (ATOCombatManager* CM = ATOCombatManager::Get(this))
			{
				if (CM->GetAudio()) CM->GetAudio()->Play2D(ETOSound::DryFire, 0.6f, 0.6f);
			}
			return;
		}
		bUnlocked = true;
		if (Reader)
		{
			if (UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this))
			{
				Reader->SetMaterial(0, Lib->GetMaterial(ETOMat::LampGreen));
			}
		}
		if (ATOCombatManager* CM = ATOCombatManager::Get(this))
		{
			if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::Beep, GetActorLocation(), 0.8f, 1.4f);
		}
	}
	if (bOpen)
	{
		SetOpen(false, 1.f);
	}
	else
	{
		OpenFor(User);
	}
}

void ATODoor::OpenFor(const AActor* By)
{
	if (IsLocked() || bOpen)
	{
		return;
	}
	// Swing away from whoever opens the door.
	float Dir = 1.f;
	if (By)
	{
		const FVector Local = GetActorTransform().InverseTransformPosition(By->GetActorLocation());
		Dir = Local.Y < 0.f ? 1.f : -1.f;
	}
	SetOpen(true, Dir);
}

void ATODoor::SetOpen(bool bInOpen, float Direction)
{
	bOpen = bInOpen;
	TargetAngle = bInOpen ? 100.f * Direction : 0.f;
	SetActorTickEnabled(true);
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::Door, GetActorLocation() + FVector(0.f, 0.f, 100.f), 0.6f, FMath::FRandRange(0.9f, 1.1f));
	}
}

void ATODoor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	Angle = FMath::FInterpConstantTo(Angle, TargetAngle, DeltaTime, 260.f);
	Hinge->SetRelativeRotation(FRotator(0.f, Angle, 0.f));
	if (FMath::IsNearlyEqual(Angle, TargetAngle, 0.1f))
	{
		SetActorTickEnabled(false);
	}
}
