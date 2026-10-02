// TAC-OPS - weapon handling

#include "Weapons/TOWeaponComponent.h"
#include "Weapons/TOCombatManager.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOBodyRigComponent.h"
#include "Characters/TOInventoryComponent.h"
#include "Characters/TOHealthComponent.h"
#include "Core/TODatabase.h"
#include "Core/TOGameMode.h"
#include "Audio/TOAudio.h"
#include "World/TOMaterialLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

namespace
{
	ETOSound ShotSoundFor(const FTOWeaponStats& S)
	{
		if (S.bSuppressed)
		{
			return ETOSound::SuppressedShot;
		}
		if (!S.Def)
		{
			return ETOSound::RifleShot;
		}
		switch (S.Def->Class)
		{
		case ETOWeaponClass::Pistol: return ETOSound::PistolShot;
		case ETOWeaponClass::SMG: return ETOSound::SMGShot;
		case ETOWeaponClass::Sniper: return ETOSound::SniperShot;
		case ETOWeaponClass::Shotgun: return ETOSound::ShotgunShot;
		case ETOWeaponClass::DMR:
		case ETOWeaponClass::LMG: return ETOSound::HeavyShot;
		default:
			return (S.Def->Caliber == FName(TEXT("762x51")) || S.Def->Caliber == FName(TEXT("762x39"))) ? ETOSound::HeavyShot : ETOSound::RifleShot;
		}
	}

	FTOWeaponConfig KnifeConfig()
	{
		FTOWeaponConfig C;
		C.WeaponId = FName(TEXT("Knife"));
		C.Attachments.SetNum((int32)ETOAttachSlot::Count);
		return C;
	}

	float EaseInOut(float A)
	{
		A = FMath::Clamp(A, 0.f, 1.f);
		return A * A * (3.f - 2.f * A);
	}
}

UTOWeaponComponent::UTOWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	Slots.SetNum(NumSlots);
}

void UTOWeaponComponent::BeginPlay()
{
	Super::BeginPlay();
	if (Slots.Num() != NumSlots)
	{
		Slots.SetNum(NumSlots);
	}
	if (!Slots[SlotMelee].bHas)
	{
		Slots[SlotMelee].bHas = true;
		Slots[SlotMelee].Config = KnifeConfig();
	}
}

void UTOWeaponComponent::Init(ATOCharacter* InOwner, USceneComponent* InViewRoot, bool bInFirstPerson)
{
	OwnerChar = InOwner;
	ViewRoot = InViewRoot;
	bFirstPerson = bInFirstPerson && InViewRoot != nullptr;
	if (Slots.Num() != NumSlots)
	{
		Slots.SetNum(NumSlots);
	}
	Slots[SlotMelee].bHas = true;
	Slots[SlotMelee].Config = KnifeConfig();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (bFirstPerson && !GunRoot)
	{
		GunRoot = NewObject<USceneComponent>(Owner, TEXT("TO_GunRoot"));
		GunRoot->SetMobility(EComponentMobility::Movable);
		GunRoot->SetupAttachment(ViewRoot);
		GunRoot->RegisterComponent();

		MuzzleLight = NewObject<UPointLightComponent>(Owner, TEXT("TO_MuzzleLight"));
		MuzzleLight->SetMobility(EComponentMobility::Movable);
		MuzzleLight->SetupAttachment(GunRoot);
		MuzzleLight->SetIntensity(0.f);
		MuzzleLight->SetAttenuationRadius(900.f);
		MuzzleLight->SetLightColor(FLinearColor(1.f, 0.72f, 0.38f));
		MuzzleLight->SetCastShadows(false);
		MuzzleLight->RegisterComponent();

		MuzzleFlashMesh = TOWeaponVisuals::AddPart(Owner, GunRoot, ETOShape::Sphere, ETOMat::Flash, FVector(60.f, 0.f, 0.f), FVector(10.f, 5.f, 5.f), FRotator::ZeroRotator, true, false);
		if (MuzzleFlashMesh)
		{
			MuzzleFlashMesh->SetVisibility(false);
		}
	}

	if (!WeaponLight)
	{
		WeaponLight = NewObject<USpotLightComponent>(Owner, TEXT("TO_WeaponLight"));
		WeaponLight->SetMobility(EComponentMobility::Movable);
		WeaponLight->SetupAttachment(bFirstPerson ? GunRoot.Get() : Owner->GetRootComponent());
		WeaponLight->SetIntensityUnits(ELightUnits::Candelas);
		WeaponLight->SetIntensity(1800.f);
		WeaponLight->SetAttenuationRadius(4500.f);
		WeaponLight->SetInnerConeAngle(10.f);
		WeaponLight->SetOuterConeAngle(26.f);
		WeaponLight->SetLightColor(FLinearColor(1.f, 0.95f, 0.88f));
		WeaponLight->SetCastShadows(bFirstPerson);
		WeaponLight->SetVolumetricScatteringIntensity(1.5f);
		WeaponLight->SetVisibility(false);
		WeaponLight->RegisterComponent();
	}
}

// ---------------------------------------------------------------------------------------------
//  Loadout
// ---------------------------------------------------------------------------------------------

void UTOWeaponComponent::SetWeapon(int32 Slot, const FTOWeaponConfig& Config, bool bFullMag)
{
	if (!Slots.IsValidIndex(Slot))
	{
		return;
	}
	FTOWeaponSlot& S = Slots[Slot];
	S.bHas = Config.IsValid();
	S.Config = Config;
	if (S.Config.Attachments.Num() < (int32)ETOAttachSlot::Count)
	{
		S.Config.Attachments.SetNum((int32)ETOAttachSlot::Count);
	}
	const FTOWeaponStats NewStats = TODB::ComputeStats(S.Config);
	S.MagAmmo = bFullMag ? NewStats.MagSize : 0;
	S.LoadedTier = Config.AmmoTier;
	S.FireModeIndex = 0;
	if (Slot == ActiveSlot)
	{
		Stats = NewStats;
		RebuildVisuals();
	}
}

void UTOWeaponComponent::ClearWeapon(int32 Slot)
{
	if (!Slots.IsValidIndex(Slot) || Slot == SlotMelee)
	{
		return;
	}
	Slots[Slot] = FTOWeaponSlot();
	if (Slot == ActiveSlot)
	{
		// Fall back to the next available weapon.
		for (int32 i = 0; i < NumSlots; ++i)
		{
			if (Slots[i].bHas)
			{
				EquipSlot(i, true);
				return;
			}
		}
	}
}

float UTOWeaponComponent::GetEquipTime() const
{
	if (ActiveSlot == SlotMelee)
	{
		return 0.25f;
	}
	if (!Stats.Def)
	{
		return 0.4f;
	}
	switch (Stats.Def->Class)
	{
	case ETOWeaponClass::Pistol: return 0.35f;
	case ETOWeaponClass::SMG: return 0.45f;
	case ETOWeaponClass::DMR: return 0.6f;
	case ETOWeaponClass::Sniper: return 0.7f;
	case ETOWeaponClass::LMG: return 0.8f;
	default: return 0.55f;
	}
}

void UTOWeaponComponent::EquipSlot(int32 Slot, bool bInstant)
{
	if (!Slots.IsValidIndex(Slot) || !Slots[Slot].bHas)
	{
		return;
	}
	if (Slot == ActiveSlot && !bInstant && Stats.Def)
	{
		return;
	}
	CancelReload();
	bTriggerHeld = false;
	bTriggerPending = false;
	BurstRemaining = 0;
	bMeleeActive = false;
	ActiveSlot = Slot;
	Stats = TODB::ComputeStats(Slots[Slot].Config);

	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	EquipStart = Now;
	EquipEnd = Now + (bInstant ? 0.f : GetEquipTime());
	RebuildVisuals();

	if (OwnerChar)
	{
		OwnerChar->OnActiveWeaponChanged();
		if (!bInstant && OwnerChar->IsPlayerCharacter())
		{
			if (ATOCombatManager* CM = ATOCombatManager::Get(this))
			{
				if (CM->GetAudio())
				{
					CM->GetAudio()->Play2D(ETOSound::Pickup, 0.5f);
				}
			}
		}
	}
}

void UTOWeaponComponent::CycleWeapon(int32 Direction)
{
	for (int32 Step = 1; Step <= NumSlots; ++Step)
	{
		const int32 Idx = ((ActiveSlot + Direction * Step) % NumSlots + NumSlots) % NumSlots;
		if (Slots[Idx].bHas)
		{
			EquipSlot(Idx);
			return;
		}
	}
}

void UTOWeaponComponent::HideAllVisuals()
{
	for (UStaticMeshComponent* P : FPParts)
	{
		if (IsValid(P)) P->SetVisibility(false);
	}
	for (UStaticMeshComponent* P : ArmParts)
	{
		if (IsValid(P)) P->SetVisibility(false);
	}
	if (WeaponLight)
	{
		WeaponLight->SetVisibility(false);
	}
}

void UTOWeaponComponent::RebuildVisuals()
{
	if (!OwnerChar)
	{
		return;
	}
	const FTOWeaponSlot& S = Slots[ActiveSlot];

	// Third person (everyone has a body)
	if (UTOBodyRigComponent* Body = OwnerChar->GetBody())
	{
		Body->SetWeapon(S.bHas ? &S.Config : nullptr);
		if (!bFirstPerson && WeaponLight && Body->GetWeaponSocket())
		{
			WeaponLight->AttachToComponent(Body->GetWeaponSocket(), FAttachmentTransformRules::KeepRelativeTransform);
			WeaponLight->SetRelativeLocationAndRotation(Body->GetGunVisual().LightLocal, FRotator::ZeroRotator);
		}
	}

	if (bFirstPerson)
	{
		BuildFirstPerson();
	}
	ApplyLightState();
}

void UTOWeaponComponent::BuildFirstPerson()
{
	for (UStaticMeshComponent* P : FPParts)
	{
		if (IsValid(P)) P->DestroyComponent();
	}
	FPParts.Reset();
	FPGun = FTOGunVisual();
	if (!GunRoot || !Slots[ActiveSlot].bHas)
	{
		return;
	}
	TOWeaponVisuals::BuildGun(GetOwner(), GunRoot, Slots[ActiveSlot].Config, true, false, FPGun);
	for (UStaticMeshComponent* P : FPGun.Parts)
	{
		FPParts.Add(P);
	}
	if (MuzzleFlashMesh)
	{
		MuzzleFlashMesh->SetRelativeLocation(FPGun.MuzzleLocal + FVector(4.f, 0.f, 0.f));
	}
	if (MuzzleLight)
	{
		MuzzleLight->SetRelativeLocation(FPGun.MuzzleLocal + FVector(10.f, 0.f, 0.f));
	}
	if (WeaponLight)
	{
		WeaponLight->SetRelativeLocationAndRotation(FPGun.LightLocal, FRotator::ZeroRotator);
	}
	BuildArms();
}

void UTOWeaponComponent::BuildArms()
{
	for (UStaticMeshComponent* P : ArmParts)
	{
		if (IsValid(P)) P->DestroyComponent();
	}
	ArmParts.Reset();
	if (!GunRoot || !OwnerChar)
	{
		return;
	}
	AActor* Owner = GetOwner();
	const ETOMat Sleeve = OwnerChar->SuitMat;

	auto Limb = [&](const FVector& From, const FVector& To, float Thick, ETOMat Mat)
	{
		const FVector Dir = To - From;
		UStaticMeshComponent* C = TOWeaponVisuals::AddPart(Owner, GunRoot, ETOShape::Cube, Mat, (From + To) * 0.5f,
			FVector(Dir.Size(), Thick, Thick), Dir.Rotation(), true, false);
		if (C) ArmParts.Add(C);
	};

	const FVector RH = FPGun.RightHandLocal;
	const FVector LH = FPGun.LeftHandLocal;
	const bool bKnife = ActiveSlot == SlotMelee;
	Limb(RH + FVector(-4.f, 2.f, -2.f), RH + FVector(-38.f, 12.f, -18.f), 8.f, Sleeve);
	if (!bKnife)
	{
		Limb(LH + FVector(-3.f, -2.f, -2.f), LH + FVector(-40.f, -14.f, -16.f), 8.f, Sleeve);
	}
	if (UStaticMeshComponent* G = TOWeaponVisuals::AddPart(Owner, GunRoot, ETOShape::Cube, ETOMat::Black, RH + FVector(0.f, 0.f, -1.f), FVector(8.f, 6.f, 5.f), FRotator(-15.f, 0.f, 0.f), true, false))
	{
		ArmParts.Add(G);
	}
	if (!bKnife)
	{
		if (UStaticMeshComponent* G = TOWeaponVisuals::AddPart(Owner, GunRoot, ETOShape::Cube, ETOMat::Black, LH + FVector(0.f, 0.f, -1.5f), FVector(8.f, 6.f, 5.f), FRotator(10.f, 0.f, 0.f), true, false))
		{
			ArmParts.Add(G);
		}
	}
}

// ---------------------------------------------------------------------------------------------
//  Input
// ---------------------------------------------------------------------------------------------

void UTOWeaponComponent::SetTrigger(bool bPressed)
{
	if (bPressed && !bTriggerHeld)
	{
		bTriggerPending = true;
		bDryFired = false;
		ConsecutiveShots = 0;
		if (GetFireMode() == ETOFireMode::Burst && BurstRemaining <= 0)
		{
			BurstRemaining = 3;
		}
	}
	bTriggerHeld = bPressed;
}

ETOFireMode UTOWeaponComponent::GetFireMode() const
{
	if (!Stats.Def || Stats.Def->Modes.Num() == 0)
	{
		return ETOFireMode::Semi;
	}
	const int32 Idx = FMath::Clamp(Slots[ActiveSlot].FireModeIndex, 0, Stats.Def->Modes.Num() - 1);
	return Stats.Def->Modes[Idx];
}

void UTOWeaponComponent::CycleFireMode()
{
	if (!Stats.Def || Stats.Def->Modes.Num() <= 1)
	{
		return;
	}
	FTOWeaponSlot& S = Slots[ActiveSlot];
	S.FireModeIndex = (S.FireModeIndex + 1) % Stats.Def->Modes.Num();
	BurstRemaining = 0;
	if (OwnerChar && OwnerChar->IsPlayerCharacter())
	{
		if (ATOCombatManager* CM = ATOCombatManager::Get(this))
		{
			if (CM->GetAudio()) CM->GetAudio()->Play2D(ETOSound::DryFire, 0.6f, 1.3f);
		}
	}
}

void UTOWeaponComponent::ToggleLight()
{
	SetLightOn(!bLightOn);
}

void UTOWeaponComponent::SetLightOn(bool bOn)
{
	bLightOn = bOn;
	ApplyLightState();
}

void UTOWeaponComponent::ApplyLightState()
{
	if (!WeaponLight)
	{
		return;
	}
	const bool bHasLight = Stats.bLight && ActiveSlot != SlotMelee;
	WeaponLight->SetVisibility(bLightOn && bHasLight);
}

void UTOWeaponComponent::Reload()
{
	if (ActiveSlot == SlotMelee || bReloading || IsEquipping() || !OwnerChar)
	{
		return;
	}
	FTOWeaponSlot& S = Slots[ActiveSlot];
	if (!S.bHas || !Stats.Def || S.MagAmmo >= Stats.MagSize)
	{
		return;
	}
	if (!bInfiniteReserve)
	{
		UTOInventoryComponent* Inv = OwnerChar->GetInventory();
		if (!Inv || Inv->CountAmmo(Stats.Def->Caliber, 0) <= 0)
		{
			return;
		}
	}
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	float T = (S.MagAmmo == 0) ? Stats.ReloadEmptyTime : Stats.ReloadTime;
	if (OwnerChar->GetHealth() && OwnerChar->GetHealth()->ArmPenalty())
	{
		T *= 1.25f;
	}
	bReloading = true;
	ReloadStart = Now;
	ReloadEnd = Now + T;
	BurstRemaining = 0;
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio())
		{
			CM->GetAudio()->Play(ETOSound::ReloadStart, OwnerChar->GetActorLocation(), OwnerChar->IsPlayerCharacter() ? 0.7f : 0.5f);
		}
	}
	// Reloading is audible to nearby enemies.
	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		GM->ReportNoise(OwnerChar->GetActorLocation(), 900.f, OwnerChar, false);
	}
}

void UTOWeaponComponent::CancelReload()
{
	bReloading = false;
}

void UTOWeaponComponent::FinishReload()
{
	bReloading = false;
	if (!OwnerChar || !Stats.Def)
	{
		return;
	}
	FTOWeaponSlot& S = Slots[ActiveSlot];
	const int32 Needed = Stats.MagSize - S.MagAmmo;
	if (Needed <= 0)
	{
		return;
	}
	if (bInfiniteReserve)
	{
		S.MagAmmo = Stats.MagSize;
		S.LoadedTier = S.Config.AmmoTier;
	}
	else if (UTOInventoryComponent* Inv = OwnerChar->GetInventory())
	{
		int32 Tier = S.Config.AmmoTier;
		const int32 Taken = Inv->TakeAmmo(Stats.Def->Caliber, S.Config.AmmoTier, Needed, Tier);
		if (Taken > 0)
		{
			if (S.MagAmmo > 0 && Tier != S.LoadedTier)
			{
				// Mixed magazine: the weakest rounds decide the penetration.
				S.LoadedTier = FMath::Min(S.LoadedTier, Tier);
			}
			else
			{
				S.LoadedTier = Tier;
			}
			S.MagAmmo += Taken;
		}
	}
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio())
		{
			CM->GetAudio()->Play(ETOSound::ReloadEnd, OwnerChar->GetActorLocation(), OwnerChar->IsPlayerCharacter() ? 0.7f : 0.5f);
		}
	}
}

// ---------------------------------------------------------------------------------------------
//  State
// ---------------------------------------------------------------------------------------------

float UTOWeaponComponent::GetReloadProgress() const
{
	if (!bReloading)
	{
		return 0.f;
	}
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	return FMath::Clamp((Now - ReloadStart) / FMath::Max(0.01f, ReloadEnd - ReloadStart), 0.f, 1.f);
}

bool UTOWeaponComponent::IsEquipping() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < EquipEnd;
}

bool UTOWeaponComponent::IsFiringRecently() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() - LastShotTime < 0.25f;
}

bool UTOWeaponComponent::CanAim() const
{
	if (!OwnerChar || ActiveSlot == SlotMelee || IsBusy())
	{
		return false;
	}
	if (!OwnerChar->IsAlive() || OwnerChar->IsDowned() || OwnerChar->IsSprinting() || OwnerChar->IsUsingItem() || OwnerChar->IsThrowing())
	{
		return false;
	}
	return WallBlockAlpha < 0.5f;
}

bool UTOWeaponComponent::IsScoped() const
{
	return ActiveSlot != SlotMelee && ADSAlpha > 0.92f && Stats.Zoom >= 2.9f;
}

int32 UTOWeaponComponent::GetMagAmmo() const
{
	return Slots[ActiveSlot].MagAmmo;
}

int32 UTOWeaponComponent::GetReserveAmmo() const
{
	if (bInfiniteReserve)
	{
		return 999;
	}
	if (!OwnerChar || !Stats.Def || ActiveSlot == SlotMelee)
	{
		return 0;
	}
	const UTOInventoryComponent* Inv = OwnerChar->GetInventory();
	return Inv ? Inv->CountAmmo(Stats.Def->Caliber, 0) : 0;
}

const FTOWeaponConfig* UTOWeaponComponent::GetActiveConfig() const
{
	return Slots[ActiveSlot].bHas ? &Slots[ActiveSlot].Config : nullptr;
}

FString UTOWeaponComponent::GetActiveName() const
{
	return Stats.Def ? Stats.Def->Name : FString(TEXT("-"));
}

// ---------------------------------------------------------------------------------------------
//  Firing
// ---------------------------------------------------------------------------------------------

void UTOWeaponComponent::TryFire(float Now)
{
	if (!OwnerChar)
	{
		return;
	}
	if (ActiveSlot == SlotMelee)
	{
		if (bTriggerPending)
		{
			DoMelee(Now);
			bTriggerPending = false;
		}
		return;
	}
	if (!OwnerChar->IsAlive() || OwnerChar->IsDowned() || IsBusy() || OwnerChar->IsUsingItem() || OwnerChar->IsThrowing())
	{
		return;
	}
	if (OwnerChar->IsSprinting())
	{
		// Pulling the trigger breaks the sprint (handled by the character).
		OwnerChar->SetSprint(false);
		return;
	}
	if (WallBlockAlpha > 0.6f || Now < NextFireTime || !Stats.Def)
	{
		return;
	}

	FTOWeaponSlot& S = Slots[ActiveSlot];
	const ETOFireMode Mode = GetFireMode();
	if ((Mode == ETOFireMode::Semi || Mode == ETOFireMode::Bolt) && !bTriggerPending)
	{
		return;
	}
	if (Mode == ETOFireMode::Burst && BurstRemaining <= 0)
	{
		return;
	}

	if (S.MagAmmo <= 0)
	{
		if (bTriggerPending && !bDryFired)
		{
			bDryFired = true;
			if (ATOCombatManager* CM = ATOCombatManager::Get(this))
			{
				if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::DryFire, OwnerChar->GetActorLocation(), 0.6f);
			}
		}
		bTriggerPending = false;
		BurstRemaining = 0;
		Reload();
		return;
	}

	FireShot(Now);
	bTriggerPending = false;
	if (Mode == ETOFireMode::Burst)
	{
		--BurstRemaining;
	}
}

void UTOWeaponComponent::FireShot(float Now)
{
	FTOWeaponSlot& S = Slots[ActiveSlot];
	ATOCharacter* C = OwnerChar;
	if (!C || !Stats.Def)
	{
		return;
	}
	S.MagAmmo = FMath::Max(0, S.MagAmmo - 1);
	const float Interval = 60.f / FMath::Max(Stats.RPM, 1.f);
	NextFireTime = FMath::Max(NextFireTime + Interval, Now + Interval * 0.5f);
	if (Now - LastShotTime > Interval * 2.f)
	{
		NextFireTime = Now + Interval;
	}
	LastShotTime = Now;
	++ConsecutiveShots;
	RecentShots += 1.f;

	const bool bPlayer = C->IsPlayerCharacter();
	FVector Origin, Dir, Muzzle;
	if (bPlayer && C->GetCamera())
	{
		Origin = C->GetCamera()->GetComponentLocation();
		Dir = C->GetCamera()->GetForwardVector();
		Muzzle = GunRoot ? GunRoot->GetComponentTransform().TransformPosition(FPGun.MuzzleLocal) : Origin + Dir * 60.f;
	}
	else
	{
		Muzzle = C->GetBody() ? C->GetBody()->GetMuzzleWorld() : C->GetEyeLocation();
		Origin = C->GetEyeLocation();
		const FVector Target = bHasAIAim ? AIAimPoint : Origin + C->GetAimDirection() * 10000.f;
		Dir = (Target - Origin).GetSafeNormal();
		if (Dir.IsNearlyZero())
		{
			Dir = C->GetActorForwardVector();
		}
	}

	ATOCombatManager* CM = ATOCombatManager::Get(this);
	const int32 Pellets = FMath::Max(1, Stats.Def->Pellets);
	const float SpreadDeg = bPlayer ? CurrentSpread : CurrentSpread * 0.35f * AIDispersion;
	const bool bLMG = Stats.Def->Class == ETOWeaponClass::LMG;
	if (CM)
	{
		for (int32 i = 0; i < Pellets; ++i)
		{
			const float Angle = FMath::DegreesToRadians(FMath::Max(0.01f, SpreadDeg)) * (Pellets > 1 ? 1.f : FMath::Sqrt(FMath::FRand()));
			const FVector ShotDir = FMath::VRandCone(Dir, Angle);
			FTOBulletSpawn B;
			B.Origin = Origin;
			B.Velocity = ShotDir * Stats.Velocity * 100.f;
			B.VisualOrigin = Muzzle;
			B.Damage = Stats.Damage;
			B.PenLevel = S.LoadedTier;
			B.ArmorDamage = Stats.Def->ArmorDamage * (0.75f + 0.1f * S.LoadedTier);
			B.EffectiveRange = Stats.Range * 100.f;
			B.Instigator = C;
			B.Team = C->TeamId;
			B.WeaponId = Stats.Def->Id;
			B.bFromPlayer = bPlayer;
			B.bTracer = (i == 0) && (!bPlayer || bLMG || (ConsecutiveShots % 3 == 1));
			CM->FireBullet(B);
		}

		if (UTOAudio* Audio = CM->GetAudio())
		{
			Audio->Play(ShotSoundFor(Stats), Muzzle, bPlayer ? 0.9f : 1.f);
		}
		if (!bPlayer)
		{
			CM->MuzzleFlash(Muzzle, Dir.Rotation(), Stats.bSuppressed);
		}
	}

	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		GM->ReportNoise(C->GetActorLocation(), 15000.f * Stats.Noise, C, true);
	}

	if (bPlayer)
	{
		// Muzzle flash on the view model
		if (!Stats.bSuppressed)
		{
			FlashTimer = 0.045f;
			if (MuzzleFlashMesh)
			{
				MuzzleFlashMesh->SetVisibility(true);
				MuzzleFlashMesh->SetRelativeRotation(FRotator(0.f, 0.f, FMath::FRandRange(0.f, 360.f)));
				const float Sc = FMath::FRandRange(0.8f, 1.3f);
				MuzzleFlashMesh->SetRelativeScale3D(FVector(0.16f, 0.07f, 0.07f) * Sc);
			}
			if (MuzzleLight)
			{
				MuzzleLight->SetIntensity(9000.f);
			}
		}

		// Recoil impulse (Delta Force style: strong vertical, random horizontal with drift)
		float StanceMult = 1.f;
		switch (C->GetStance())
		{
		case ETOStance::Crouch: StanceMult = Stats.bBipod ? 0.55f : 0.85f; break;
		case ETOStance::Prone: StanceMult = Stats.bBipod ? 0.45f : 0.7f; break;
		default: break;
		}
		const bool bArm = C->GetHealth() && C->GetHealth()->ArmPenalty();
		const float First = (ConsecutiveShots <= 1) ? 0.65f : 1.f;
		const float AdsMult = FMath::Lerp(1.15f, 1.f, ADSAlpha);
		const float V = Stats.RecoilV * FMath::FRandRange(0.85f, 1.15f) * StanceMult * AdsMult * (bArm ? 1.3f : 1.f) * First;
		const float H = (Stats.RecoilH * FMath::FRandRange(-1.f, 1.f) + Stats.RecoilH * 0.25f) * StanceMult * (bArm ? 1.3f : 1.f);
		C->AddRecoil(V, H);
		KickBack += 1.0f + V * 1.2f;
		KickPitch += 1.2f + V * 1.6f;
		C->AddCameraShake(0.04f + V * 0.04f);
	}
	else if (C->GetBody())
	{
		C->GetBody()->AddFireKick();
	}

	Bloom = FMath::Min(Bloom + FMath::Lerp(0.22f * Stats.HipSpread / 2.5f, 0.03f, ADSAlpha), 3.f);
}

void UTOWeaponComponent::DoMelee(float Now)
{
	if (bMeleeActive || !OwnerChar || !OwnerChar->IsAlive() || OwnerChar->IsDowned())
	{
		return;
	}
	bMeleeActive = true;
	MeleeStart = Now;
	bMeleeHitDone = false;
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::Whiz, OwnerChar->GetActorLocation(), 0.4f, 1.6f);
	}
}

// ---------------------------------------------------------------------------------------------
//  Tick
// ---------------------------------------------------------------------------------------------

void UTOWeaponComponent::UpdateSpread(float Dt)
{
	if (!OwnerChar || !Stats.Def)
	{
		CurrentSpread = 2.f;
		return;
	}
	float Base = FMath::Lerp(Stats.HipSpread, Stats.AdsSpread, ADSAlpha);
	const float Speed = OwnerChar->GetVelocity().Size2D();
	const float MoveF = FMath::Clamp(Speed / 500.f, 0.f, 1.5f);
	Base += MoveF * FMath::Lerp(1.6f, 0.25f, ADSAlpha);
	const UCharacterMovementComponent* Move = OwnerChar->GetCharacterMovement();
	if (Move && Move->IsFalling())
	{
		Base += FMath::Lerp(4.f, 1.5f, ADSAlpha);
	}
	float StanceMult = 1.f;
	if (OwnerChar->GetStance() == ETOStance::Crouch) StanceMult = 0.8f;
	if (OwnerChar->GetStance() == ETOStance::Prone) StanceMult = 0.65f;
	if (OwnerChar->GetHealth() && OwnerChar->GetHealth()->ArmPenalty()) StanceMult *= 1.3f;
	Bloom = FMath::Max(0.f, Bloom - Dt * 4.f);
	CurrentSpread = Base * StanceMult + Bloom;
}

void UTOWeaponComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const UWorld* World = GetWorld();
	if (!World || !OwnerChar)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	const float Dt = FMath::Min(DeltaTime, 0.1f);

	if (bReloading && Now >= ReloadEnd)
	{
		FinishReload();
	}

	// Knife swing hit detection
	if (bMeleeActive)
	{
		const float T = Now - MeleeStart;
		if (!bMeleeHitDone && T > 0.15f)
		{
			bMeleeHitDone = true;
			const FVector Start = OwnerChar->GetEyeLocation();
			const FVector End = Start + OwnerChar->GetAimDirection() * 170.f;
			FCollisionQueryParams Params(SCENE_QUERY_STAT(TOMelee), false, OwnerChar.Get());
			FHitResult Hit;
			if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, TO_TRACE_BULLET, FCollisionShape::MakeSphere(22.f), Params))
			{
				if (ATOCharacter* Victim = Cast<ATOCharacter>(Hit.GetActor()))
				{
					FTODamageInfo Info;
					Info.Damage = 65.f;
					const FVector VictimFwd = Victim->GetActorForwardVector();
					const FVector AttackDir = (Victim->GetActorLocation() - OwnerChar->GetActorLocation()).GetSafeNormal2D();
					if (FVector::DotProduct(VictimFwd, AttackDir) > 0.5f)
					{
						Info.Damage *= 2.5f; // back stab
					}
					Info.bMelee = true;
					Info.Part = Victim->GetBody() ? Victim->GetBody()->GetPartFromComponent(Hit.GetComponent()) : ETOBodyPart::Thorax;
					if (Info.Part == ETOBodyPart::None) Info.Part = ETOBodyPart::Thorax;
					Info.HitLocation = Hit.ImpactPoint;
					Info.HitDirection = AttackDir;
					Info.Instigator = OwnerChar;
					Info.InstigatorTeam = OwnerChar->TeamId;
					Info.WeaponId = FName(TEXT("Knife"));
					Info.PenLevel = 2;
					Victim->ReceiveTODamage(Info);
					if (ATOCombatManager* CM = ATOCombatManager::Get(this))
					{
						CM->BloodEffect(Hit.ImpactPoint, AttackDir);
					}
				}
				else if (ATOCombatManager* CM = ATOCombatManager::Get(this))
				{
					CM->ImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal, FName(TEXT("Metal")));
				}
			}
		}
		if (T > 0.55f)
		{
			bMeleeActive = false;
		}
	}

	// ADS
	{
		const bool bCan = CanAim();
		const float Target = (bWantsAim && bCan) ? 1.f : 0.f;
		float Time = FMath::Max(0.05f, Stats.AdsTime);
		if (OwnerChar->GetHealth() && OwnerChar->GetHealth()->ArmPenalty()) Time *= 1.3f;
		if (OwnerChar->IsAdrenalineActive()) Time *= 0.7f;
		ADSAlpha = FMath::FInterpConstantTo(ADSAlpha, Target, Dt, 1.f / Time);
	}

	UpdateSpread(Dt);

	if (bTriggerHeld || BurstRemaining > 0 || (bTriggerPending && ActiveSlot == SlotMelee))
	{
		TryFire(Now);
	}

	RecentShots = FMath::Max(0.f, RecentShots - Dt * 2.f);

	if (bFirstPerson)
	{
		UpdateViewModel(Dt, Now);
	}
}

void UTOWeaponComponent::UpdateViewModel(float Dt, float Now)
{
	if (!GunRoot || !OwnerChar)
	{
		return;
	}

	// Muzzle flash fade
	if (FlashTimer > 0.f)
	{
		FlashTimer -= Dt;
		if (FlashTimer <= 0.f)
		{
			if (MuzzleFlashMesh) MuzzleFlashMesh->SetVisibility(false);
			if (MuzzleLight) MuzzleLight->SetIntensity(0.f);
		}
	}

	const bool bKnife = ActiveSlot == SlotMelee;
	const bool bPistol = Stats.Def && Stats.Def->Class == ETOWeaponClass::Pistol;

	FVector Hip = bKnife ? FVector(26.f, 13.f, -15.f) : (bPistol ? FVector(30.f, 9.f, -10.5f) : FVector(22.f, 10.f, -12.f));
	float AdsX = bPistol ? 24.f : 13.f;
	switch (Stats.Optic)
	{
	case ETOOpticType::RedDot: AdsX = bPistol ? 24.f : 13.f; break;
	case ETOOpticType::Holo: AdsX = 12.f; break;
	case ETOOpticType::Prism: AdsX = 15.f; break;
	case ETOOpticType::ACOG: AdsX = 17.f; break;
	case ETOOpticType::Sniper: AdsX = 22.f; break;
	default: break;
	}
	const FVector Ads(AdsX, 0.f, -FPGun.SightHeight);
	const float A = EaseInOut(ADSAlpha);
	FVector Loc = FMath::Lerp(Hip, Ads, A);
	FRotator Rot = FRotator::ZeroRotator;

	// Look sway (lags behind mouse movement)
	const FVector2D TargetSway(FMath::Clamp(-LookSway.X * 0.8f, -6.f, 6.f), FMath::Clamp(LookSway.Y * 0.8f, -6.f, 6.f));
	LookSway = FVector2D::ZeroVector;
	SwayOffset = FMath::Lerp(SwayOffset, TargetSway, FMath::Clamp(Dt * 9.f, 0.f, 1.f));
	const float SwayMul = FMath::Lerp(1.f, 0.25f, A);
	Loc.Y += SwayOffset.X * 0.15f * SwayMul;
	Loc.Z += SwayOffset.Y * 0.15f * SwayMul;
	Rot.Yaw += SwayOffset.X * 0.6f * SwayMul;
	Rot.Pitch += SwayOffset.Y * 0.6f * SwayMul;

	// Movement bob
	const float Speed = OwnerChar->GetVelocity().Size2D();
	const float SpeedN = FMath::Clamp(Speed / 600.f, 0.f, 1.2f);
	const UCharacterMovementComponent* Move = OwnerChar->GetCharacterMovement();
	const bool bGrounded = Move && Move->IsMovingOnGround();
	BobPhase += Dt * (bGrounded && SpeedN > 0.05f ? (8.f + 5.f * SpeedN) : 0.f);
	const float BobAmt = SpeedN * FMath::Lerp(1.f, 0.2f, A) * (OwnerChar->IsSprinting() ? 1.6f : 1.f);
	Loc.Z += FMath::Sin(BobPhase * 2.f) * 0.45f * BobAmt;
	Loc.Y += FMath::Sin(BobPhase) * 0.6f * BobAmt;
	Rot.Roll += FMath::Sin(BobPhase) * 1.2f * BobAmt;

	// Idle breathing (scopes hold steadier while not breathing)
	const float Breath = 1.f - A * 0.75f;
	Loc.Z += FMath::Sin(Now * 1.6f) * 0.12f * Breath;
	Rot.Pitch += FMath::Sin(Now * 1.1f) * 0.25f * Breath;

	// Sprint pose
	SprintAlpha = FMath::FInterpTo(SprintAlpha, OwnerChar->IsSprinting() ? 1.f : 0.f, Dt, 10.f);
	Loc += FVector(-3.f, -2.f, -4.f) * SprintAlpha;
	Rot += FRotator(-22.f, 32.f, -12.f) * SprintAlpha;

	// Reload animation (gun tilts, magazine drops out and comes back)
	if (bReloading)
	{
		const float P = GetReloadProgress();
		const float Arc = FMath::Sin(P * PI);
		Rot.Roll += -28.f * Arc;
		Rot.Pitch += -12.f * Arc;
		Loc.Z += -3.f * Arc;
		if (FPGun.Magazine)
		{
			float Drop = 0.f;
			if (P < 0.2f) Drop = P / 0.2f;
			else if (P < 0.6f) Drop = 1.f;
			else Drop = FMath::Max(0.f, 1.f - (P - 0.6f) / 0.2f);
			FPGun.Magazine->SetRelativeLocation(FPGun.MagazineRest + FVector(0.f, 0.f, -24.f) * Drop);
		}
	}
	else if (FPGun.Magazine)
	{
		FPGun.Magazine->SetRelativeLocation(FPGun.MagazineRest);
	}

	// Equip
	const float EquipP = IsEquipping() ? FMath::Clamp((Now - EquipStart) / FMath::Max(0.01f, EquipEnd - EquipStart), 0.f, 1.f) : 1.f;
	const float E = (1.f - EquipP) * (1.f - EquipP);
	Loc.Z -= 20.f * E;
	Rot.Pitch -= 35.f * E;

	// Lowered while healing / throwing / downed
	const bool bLower = OwnerChar->IsUsingItem() || OwnerChar->IsThrowing() || OwnerChar->IsDowned();
	LowerAlpha = FMath::FInterpTo(LowerAlpha, bLower ? 1.f : 0.f, Dt, 8.f);
	Loc.Z -= 26.f * LowerAlpha;
	Rot.Pitch -= 40.f * LowerAlpha;

	// Knife swing
	if (bMeleeActive)
	{
		const float T = FMath::Clamp((Now - MeleeStart) / 0.45f, 0.f, 1.f);
		const float S = FMath::Sin(T * PI);
		Rot.Yaw += -55.f * S;
		Rot.Pitch += -15.f * S;
		Loc.X += 12.f * S;
		Loc.Y -= 8.f * S;
	}

	// Wall avoidance: pull the gun back / up instead of clipping into geometry
	if (UCameraComponent* Cam = OwnerChar->GetCamera())
	{
		const FVector Start = Cam->GetComponentLocation();
		const float Reach = FMath::Max(40.f, FPGun.MuzzleLocal.X + Hip.X);
		const FVector End = Start + Cam->GetForwardVector() * Reach;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TOWallBlock), false, OwnerChar.Get());
		FHitResult Hit;
		float Block = 0.f;
		if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(4.f), Params))
		{
			Block = FMath::Clamp(1.f - Hit.Distance / Reach, 0.f, 1.f) * 1.4f;
		}
		WallBlockAlpha = FMath::FInterpTo(WallBlockAlpha, FMath::Min(Block, 1.f), Dt, 10.f);
	}
	Loc.X -= 12.f * WallBlockAlpha;
	Loc.Z -= 3.f * WallBlockAlpha;
	Rot.Pitch += 38.f * WallBlockAlpha;
	Rot.Yaw -= 10.f * WallBlockAlpha;

	// Recoil kick
	KickBack = FMath::FInterpTo(KickBack, 0.f, Dt, 18.f);
	KickPitch = FMath::FInterpTo(KickPitch, 0.f, Dt, 14.f);
	Loc.X -= KickBack * FMath::Lerp(1.f, 0.6f, A);
	Rot.Pitch += KickPitch * FMath::Lerp(1.f, 0.35f, A);

	GunRoot->SetRelativeLocationAndRotation(Loc, Rot);

	// Hide the model while looking through a magnified scope (HUD draws the scope).
	const bool bHide = IsScoped() || !OwnerChar->IsAlive() || (OwnerChar->CurrentVehicle.IsValid());
	for (UStaticMeshComponent* P : FPParts)
	{
		if (IsValid(P) && P->IsVisible() == bHide) P->SetVisibility(!bHide);
	}
	for (UStaticMeshComponent* P : ArmParts)
	{
		if (IsValid(P) && P->IsVisible() == bHide) P->SetVisibility(!bHide);
	}
}
