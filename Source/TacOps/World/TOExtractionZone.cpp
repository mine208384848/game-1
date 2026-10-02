// TAC-OPS - extraction points, switches, capture points, explosive barrels

#include "World/TOExtractionZone.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOInventoryComponent.h"
#include "Core/TOGameMode.h"
#include "Core/TODatabase.h"
#include "Weapons/TOCombatManager.h"
#include "Audio/TOAudio.h"
#include "World/TOMaterialLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"

namespace
{
	UStaticMeshComponent* MakeMesh(AActor* Owner, USceneComponent* Parent, ETOShape Shape, ETOMat Mat, const FVector& Loc, const FVector& Size, const FRotator& Rot, bool bSolid)
	{
		UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(Owner);
		if (!Lib)
		{
			return nullptr;
		}
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Lib->GetMesh(Shape));
		C->SetMaterial(0, Lib->GetMaterial(Mat));
		C->SetupAttachment(Parent);
		C->SetRelativeLocation(Loc);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(Size / 100.f);
		C->SetCanEverAffectNavigation(false);
		if (bSolid)
		{
			C->SetCollisionProfileName(TEXT("BlockAll"));
			C->ComponentTags.Add(UTOMaterialLibrary::GetSurfaceTag(Mat));
		}
		else
		{
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		C->RegisterComponent();
		return C;
	}

	float WorldNow(const UObject* Ctx)
	{
		const UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
		return W ? W->GetTimeSeconds() : 0.f;
	}
}

// =============================================================================================
//  Extraction zone
// =============================================================================================

ATOExtractionZone::ATOExtractionZone()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ATOExtractionZone::BeginPlay()
{
	Super::BeginPlay();
	if (!bBuilt)
	{
		Build();
	}
}

void ATOExtractionZone::Setup(const FString& InName, ETOExtractRule InRule, float InRadius, int32 InCost)
{
	ZoneName = InName;
	Rule = InRule;
	Radius = InRadius;
	Cost = InCost;
	if (HasActorBegunPlay())
	{
		Build();
	}
}

void ATOExtractionZone::Build()
{
	bBuilt = true;
	for (UStaticMeshComponent* P : Parts)
	{
		if (IsValid(P)) P->DestroyComponent();
	}
	Parts.Reset();

	const int32 Markers = 16;
	for (int32 i = 0; i < Markers; ++i)
	{
		const float A = 2.f * PI * i / Markers;
		const FVector P(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, 4.f);
		Parts.Add(MakeMesh(this, Root, ETOShape::Cylinder, ETOMat::LampGreen, P, FVector(18.f, 18.f, 8.f), FRotator::ZeroRotator, false));
	}
	Parts.Add(MakeMesh(this, Root, ETOShape::Cylinder, ETOMat::MetalGrey, FVector(0.f, 0.f, 300.f), FVector(10.f, 10.f, 600.f), FRotator::ZeroRotator, true));
	Parts.Add(MakeMesh(this, Root, ETOShape::Cube, ETOMat::LampGreen, FVector(0.f, 60.f, 560.f), FVector(4.f, 110.f, 70.f), FRotator::ZeroRotator, false));

	Beacon = NewObject<UPointLightComponent>(this);
	Beacon->SetupAttachment(Root);
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, 620.f));
	Beacon->SetLightColor(FLinearColor(0.2f, 1.f, 0.35f));
	Beacon->SetIntensity(20000.f);
	Beacon->SetAttenuationRadius(1600.f);
	Beacon->SetCastShadows(false);
	Beacon->RegisterComponent();

	// Helicopter for the radio extraction (hidden until called)
	if (Rule == ETOExtractRule::Radio)
	{
		HeliRoot = NewObject<USceneComponent>(this);
		HeliRoot->SetupAttachment(Root);
		HeliRoot->SetRelativeLocation(FVector(0.f, 0.f, 9000.f));
		HeliRoot->RegisterComponent();
		HeliParts.Add(MakeMesh(this, HeliRoot, ETOShape::Cube, ETOMat::MetalGreen, FVector(0.f, 0.f, 150.f), FVector(800.f, 260.f, 260.f), FRotator::ZeroRotator, false));
		HeliParts.Add(MakeMesh(this, HeliRoot, ETOShape::Cube, ETOMat::Glass, FVector(380.f, 0.f, 170.f), FVector(80.f, 220.f, 160.f), FRotator::ZeroRotator, false));
		HeliParts.Add(MakeMesh(this, HeliRoot, ETOShape::Cube, ETOMat::MetalGreen, FVector(-750.f, 0.f, 220.f), FVector(800.f, 70.f, 90.f), FRotator::ZeroRotator, false));
		HeliParts.Add(MakeMesh(this, HeliRoot, ETOShape::Cube, ETOMat::MetalGreen, FVector(-1120.f, 0.f, 330.f), FVector(60.f, 20.f, 200.f), FRotator::ZeroRotator, false));
		HeliParts.Add(MakeMesh(this, HeliRoot, ETOShape::Cube, ETOMat::MetalDark, FVector(0.f, 0.f, 330.f), FVector(1500.f, 40.f, 8.f), FRotator::ZeroRotator, false));
		HeliParts.Add(MakeMesh(this, HeliRoot, ETOShape::Cube, ETOMat::MetalDark, FVector(0.f, 0.f, 330.f), FVector(40.f, 1500.f, 8.f), FRotator::ZeroRotator, false));
		HeliParts.Add(MakeMesh(this, HeliRoot, ETOShape::Cube, ETOMat::MetalDark, FVector(0.f, -150.f, 0.f), FVector(600.f, 12.f, 12.f), FRotator::ZeroRotator, false));
		HeliParts.Add(MakeMesh(this, HeliRoot, ETOShape::Cube, ETOMat::MetalDark, FVector(0.f, 150.f, 0.f), FVector(600.f, 12.f, 12.f), FRotator::ZeroRotator, false));
		HeliRoot->SetVisibility(false, true);
	}
	SetAvailable(bAvailable);
}

void ATOExtractionZone::SetAvailable(bool bInAvailable)
{
	bAvailable = bInAvailable;
	SetActorHiddenInGame(false);
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	for (int32 i = 0; i < Parts.Num(); ++i)
	{
		if (Parts[i] && Lib && i < 16)
		{
			Parts[i]->SetMaterial(0, Lib->GetMaterial(bAvailable ? ETOMat::LampGreen : ETOMat::LampRed));
		}
	}
	if (Beacon)
	{
		Beacon->SetLightColor(bAvailable ? FLinearColor(0.2f, 1.f, 0.35f) : FLinearColor(1.f, 0.15f, 0.1f));
		Beacon->SetVisibility(bAvailable);
	}
}

bool ATOExtractionZone::IsReady() const
{
	if (!bAvailable)
	{
		return false;
	}
	const float Now = WorldNow(this);
	switch (Rule)
	{
	case ETOExtractRule::Radio: return HeliArrival > 0.f && Now >= HeliArrival && Now <= HeliLeave;
	case ETOExtractRule::Paid: return bPaid;
	default: return true;
	}
}

bool ATOExtractionZone::CanExtract(const ATOCharacter* Character, FString& OutReason) const
{
	if (!bAvailable)
	{
		OutReason = TEXT("Extraction unavailable this raid");
		return false;
	}
	if (!IsReady())
	{
		OutReason = GetStatusText();
		return false;
	}
	if (Rule == ETOExtractRule::NoBackpack && Character && Character->GetInventory() && Character->GetInventory()->HasBackpack())
	{
		OutReason = TEXT("Too narrow: drop your backpack to extract here");
		return false;
	}
	return true;
}

bool ATOExtractionZone::IsInside(const FVector& Location) const
{
	const FVector L = GetActorLocation();
	return FVector::Dist2D(L, Location) < Radius && FMath::Abs(L.Z - Location.Z) < 800.f;
}

void ATOExtractionZone::CallHelicopter()
{
	if (Rule != ETOExtractRule::Radio || HeliArrival > 0.f)
	{
		return;
	}
	const float Now = WorldNow(this);
	HeliArrival = Now + 60.f;
	HeliLeave = HeliArrival + 90.f;
	if (HeliRoot)
	{
		HeliRoot->SetVisibility(true, true);
	}
}

float ATOExtractionZone::GetHelicopterETA() const
{
	return HeliArrival > 0.f ? FMath::Max(0.f, HeliArrival - WorldNow(this)) : -1.f;
}

float ATOExtractionZone::GetHelicopterTimeLeft() const
{
	return HeliLeave > 0.f ? FMath::Max(0.f, HeliLeave - WorldNow(this)) : -1.f;
}

FString ATOExtractionZone::GetStatusText() const
{
	if (!bAvailable)
	{
		return TEXT("Closed");
	}
	switch (Rule)
	{
	case ETOExtractRule::Radio:
	{
		if (HeliArrival < 0.f) return TEXT("Call the helicopter at the radio");
		const float ETA = GetHelicopterETA();
		if (ETA > 0.f) return FString::Printf(TEXT("Helicopter ETA %s"), *TOUtil::FormatTime(ETA));
		const float Left = GetHelicopterTimeLeft();
		if (Left > 0.f) return FString::Printf(TEXT("Helicopter leaving in %s"), *TOUtil::FormatTime(Left));
		return TEXT("Helicopter has left");
	}
	case ETOExtractRule::Paid:
		return bPaid ? TEXT("Paid - ready") : FString::Printf(TEXT("Pay %s at the terminal"), *TOUtil::FormatMoney(Cost));
	case ETOExtractRule::NoBackpack:
		return TEXT("No backpacks allowed");
	default:
		return TEXT("Available");
	}
}

void ATOExtractionZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const float Now = WorldNow(this);
	if (Beacon && bAvailable)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(Now * (IsReady() ? 5.f : 1.5f));
		Beacon->SetIntensity(IsReady() ? 8000.f + 22000.f * Pulse : 3000.f * Pulse);
	}

	if (bAvailable && IsReady() && Now > NextSmoke)
	{
		NextSmoke = Now + 24.f;
		if (ATOCombatManager* CM = ATOCombatManager::Get(this))
		{
			CM->SpawnSignalSmoke(GetActorLocation(), 24.f);
		}
	}

	// Helicopter fly-in / hover / leave
	if (HeliRoot && HeliArrival > 0.f)
	{
		float Z = 600.f;
		if (Now < HeliArrival)
		{
			const float T = FMath::Clamp(1.f - (HeliArrival - Now) / 20.f, 0.f, 1.f);
			Z = FMath::Lerp(9000.f, 600.f, T * T * (3.f - 2.f * T));
		}
		else if (Now > HeliLeave)
		{
			Z = 600.f + (Now - HeliLeave) * 900.f;
			if (Z > 20000.f)
			{
				HeliRoot->SetVisibility(false, true);
			}
		}
		Z += FMath::Sin(Now * 1.3f) * 25.f;
		HeliRoot->SetRelativeLocationAndRotation(FVector(0.f, -Radius * 0.3f, Z), FRotator(FMath::Sin(Now * 0.7f) * 2.f, 30.f, FMath::Sin(Now) * 2.f));
		// Rotor blades spin
		for (int32 i = 4; i <= 5 && i < HeliParts.Num(); ++i)
		{
			if (HeliParts[i])
			{
				HeliParts[i]->SetRelativeRotation(FRotator(0.f, Now * 900.f, 0.f));
			}
		}
		if (Now - HeliArrival > -20.f && Now < HeliLeave + 10.f && FMath::Fmod(Now, 0.5f) < DeltaTime)
		{
			if (ATOCombatManager* CM = ATOCombatManager::Get(this))
			{
				if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::DistantShot, HeliRoot->GetComponentLocation(), 0.5f, 0.35f);
			}
		}
	}
}

// =============================================================================================
//  Objective switch (radio / pay terminal)
// =============================================================================================

ATOObjectiveSwitch::ATOObjectiveSwitch()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ATOObjectiveSwitch::BeginPlay()
{
	Super::BeginPlay();
	if (!bBuilt)
	{
		Build();
	}
}

void ATOObjectiveSwitch::Setup(int32 InKind, ATOExtractionZone* InZone)
{
	Kind = InKind;
	Zone = InZone;
	if (HasActorBegunPlay())
	{
		Build();
	}
}

void ATOObjectiveSwitch::Build()
{
	bBuilt = true;
	for (UStaticMeshComponent* P : Parts)
	{
		if (IsValid(P)) P->DestroyComponent();
	}
	Parts.Reset();
	if (Kind == 0)
	{
		Parts.Add(MakeMesh(this, Root, ETOShape::Cube, ETOMat::Wood, FVector(0.f, 0.f, 37.f), FVector(120.f, 60.f, 74.f), FRotator::ZeroRotator, true));
		Parts.Add(MakeMesh(this, Root, ETOShape::Cube, ETOMat::MetalGreen, FVector(0.f, 0.f, 86.f), FVector(45.f, 28.f, 24.f), FRotator::ZeroRotator, true));
		Parts.Add(MakeMesh(this, Root, ETOShape::Cylinder, ETOMat::MetalDark, FVector(15.f, 8.f, 150.f), FVector(2.f, 2.f, 110.f), FRotator::ZeroRotator, false));
		Parts.Add(MakeMesh(this, Root, ETOShape::Cube, ETOMat::LampRed, FVector(-23.f, 0.f, 92.f), FVector(1.f, 4.f, 4.f), FRotator::ZeroRotator, false));
	}
	else
	{
		Parts.Add(MakeMesh(this, Root, ETOShape::Cube, ETOMat::MetalBlue, FVector(0.f, 0.f, 70.f), FVector(50.f, 40.f, 140.f), FRotator::ZeroRotator, true));
		Parts.Add(MakeMesh(this, Root, ETOShape::Cube, ETOMat::Screen, FVector(-25.5f, 0.f, 105.f), FVector(1.f, 30.f, 22.f), FRotator::ZeroRotator, false));
		Parts.Add(MakeMesh(this, Root, ETOShape::Cube, ETOMat::Gold, FVector(-25.5f, 0.f, 75.f), FVector(1.f, 18.f, 6.f), FRotator::ZeroRotator, false));
	}
}

FString ATOObjectiveSwitch::GetInteractLabel(const ATOCharacter* User) const
{
	const ATOExtractionZone* Z = Zone.Get();
	if (Kind == 0)
	{
		return FString::Printf(TEXT("Call extraction helicopter (%s)"), Z ? *Z->GetZoneName() : TEXT("?"));
	}
	return FString::Printf(TEXT("Pay %s for %s"), *TOUtil::FormatMoney(Z ? Z->GetCost() : 0), Z ? *Z->GetZoneName() : TEXT("?"));
}

FString ATOObjectiveSwitch::GetInteractHint(const ATOCharacter* User) const
{
	return Kind == 0 ? TEXT("Warning: the radio call will alert nearby enemies") : TEXT("Deducted from your credits on extraction");
}

bool ATOObjectiveSwitch::CanInteract(const ATOCharacter* User) const
{
	const ATOExtractionZone* Z = Zone.Get();
	return !bUsed && Z && Z->IsAvailable();
}

void ATOObjectiveSwitch::Interact(ATOCharacter* User)
{
	ATOExtractionZone* Z = Zone.Get();
	ATOGameMode* GM = ATOGameMode::Get(this);
	if (!Z || !GM || bUsed)
	{
		return;
	}
	if (Kind == 0)
	{
		bUsed = true;
		Z->CallHelicopter();
		GM->ReportNoise(GetActorLocation(), 45000.f, User, true);
		GM->PushMessage(FString::Printf(TEXT("Helicopter inbound to %s - ETA 60 s"), *Z->GetZoneName()), FLinearColor(0.4f, 1.f, 0.5f));
	}
	else
	{
		if (GM->TryPayExtraction(User, Z->GetCost()))
		{
			bUsed = true;
			Z->MarkPaid();
			GM->PushMessage(FString::Printf(TEXT("%s unlocked"), *Z->GetZoneName()), FLinearColor(1.f, 0.85f, 0.3f));
		}
		else
		{
			GM->PushMessage(TEXT("Not enough credits"), FLinearColor(1.f, 0.3f, 0.3f));
		}
	}
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::Capture, GetActorLocation(), 0.9f);
	}
}

// =============================================================================================
//  Capture point
// =============================================================================================

ATOCapturePoint::ATOCapturePoint()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ATOCapturePoint::BeginPlay()
{
	Super::BeginPlay();
	if (!bBuilt)
	{
		Build();
	}
}

void ATOCapturePoint::Setup(const FString& InLabel, int32 InSector, float InRadius, int32 InOwnerTeam)
{
	Label = InLabel;
	Sector = InSector;
	Radius = InRadius;
	OwnerTeam = InOwnerTeam;
	if (HasActorBegunPlay())
	{
		Build();
	}
}

void ATOCapturePoint::Build()
{
	bBuilt = true;
	for (UStaticMeshComponent* P : Parts)
	{
		if (IsValid(P)) P->DestroyComponent();
	}
	Parts.Reset();
	Parts.Add(MakeMesh(this, Root, ETOShape::Cylinder, ETOMat::Aluminium, FVector(0.f, 0.f, 450.f), FVector(10.f, 10.f, 900.f), FRotator::ZeroRotator, true));
	Flag = MakeMesh(this, Root, ETOShape::Cube, ETOMat::MetalWhite, FVector(0.f, 75.f, 820.f), FVector(4.f, 140.f, 90.f), FRotator::ZeroRotator, false);
	Parts.Add(Flag);
	if (UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this))
	{
		FlagMID = Lib->MakeDynamic(ETOMat::LampCold);
		if (Flag && FlagMID)
		{
			Flag->SetMaterial(0, FlagMID);
		}
	}
	const int32 Markers = 20;
	for (int32 i = 0; i < Markers; ++i)
	{
		const float A = 2.f * PI * i / Markers;
		Parts.Add(MakeMesh(this, Root, ETOShape::Cylinder, ETOMat::MetalYellow, FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, 40.f), FVector(10.f, 10.f, 80.f), FRotator::ZeroRotator, false));
	}
	Light = NewObject<UPointLightComponent>(this);
	Light->SetupAttachment(Root);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 700.f));
	Light->SetIntensity(15000.f);
	Light->SetAttenuationRadius(1800.f);
	Light->SetCastShadows(false);
	Light->RegisterComponent();
	UpdateVisual();
}

void ATOCapturePoint::SetOwnerTeam(int32 Team)
{
	OwnerTeam = Team;
	UpdateVisual();
}

void ATOCapturePoint::UpdateVisual()
{
	const FLinearColor C = OwnerTeam == 0 ? FLinearColor(0.15f, 0.45f, 1.f) : (OwnerTeam == 1 ? FLinearColor(1.f, 0.2f, 0.12f) : FLinearColor(0.9f, 0.9f, 0.9f));
	if (FlagMID)
	{
		UTOMaterialLibrary::SetDynamicColor(FlagMID, C, 6.f);
	}
	if (Light)
	{
		Light->SetLightColor(C);
		Light->SetVisibility(bActive);
	}
	// Flag height shows capture progress
	if (Flag)
	{
		Flag->SetRelativeLocation(FVector(0.f, 75.f, FMath::Lerp(820.f, 150.f, FMath::Clamp(Capture, 0.f, 1.f))));
	}
}

// =============================================================================================
//  Explosive barrel
// =============================================================================================

ATOExplosiveBarrel::ATOExplosiveBarrel()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ATOExplosiveBarrel::BeginPlay()
{
	Super::BeginPlay();
	Parts.Add(MakeMesh(this, Root, ETOShape::Cylinder, ETOMat::MetalRed, FVector(0.f, 0.f, 45.f), FVector(58.f, 58.f, 90.f), FRotator::ZeroRotator, true));
	Parts.Add(MakeMesh(this, Root, ETOShape::Cylinder, ETOMat::MetalYellow, FVector(0.f, 0.f, 62.f), FVector(59.f, 59.f, 10.f), FRotator::ZeroRotator, false));
	Parts.Add(MakeMesh(this, Root, ETOShape::Cylinder, ETOMat::MetalDark, FVector(0.f, 0.f, 90.5f), FVector(56.f, 56.f, 2.f), FRotator::ZeroRotator, false));
}

void ATOExplosiveBarrel::ReceiveTODamage(const FTODamageInfo& Info)
{
	if (bExploded)
	{
		return;
	}
	HP -= Info.Damage;
	if (HP > 0.f)
	{
		return;
	}
	bExploded = true;
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		CM->Explode(GetActorLocation() + FVector(0.f, 0.f, 50.f), 650.f, 130.f, Info.Instigator.Get(), Info.InstigatorTeam, FName(TEXT("Explosive Barrel")));
	}
	Destroy();
}
