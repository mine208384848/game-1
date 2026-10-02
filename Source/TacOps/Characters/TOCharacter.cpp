// TAC-OPS - soldier character

#include "Characters/TOCharacter.h"
#include "Characters/TOBodyRigComponent.h"
#include "Characters/TOHealthComponent.h"
#include "Characters/TOInventoryComponent.h"
#include "Weapons/TOWeaponComponent.h"
#include "Weapons/TOCombatManager.h"
#include "Weapons/TOGrenade.h"
#include "AI/TOAIController.h"
#include "Core/TOGameMode.h"
#include "Core/TODatabase.h"
#include "Audio/TOAudio.h"
#include "World/TOLootContainer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationInvokerComponent.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

ATOCharacter::ATOCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.f, 88.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(TO_TRACE_BULLET, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// The procedural body replaces the skeletal mesh.
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		Skel->SetVisibility(false);
		Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Skel->SetComponentTickEnabled(false);
	}

	FPCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FPCamera"));
	FPCamera->SetupAttachment(GetCapsuleComponent());
	FPCamera->SetRelativeLocation(FVector(6.f, 0.f, 76.f));
	FPCamera->bUsePawnControlRotation = false;
	FPCamera->SetUsingAbsoluteRotation(true);
	FPCamera->SetFieldOfView(90.f);
	FPCamera->PostProcessBlendWeight = 1.f;

	ViewmodelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ViewmodelRoot"));
	ViewmodelRoot->SetupAttachment(FPCamera);

	Body = CreateDefaultSubobject<UTOBodyRigComponent>(TEXT("Body"));
	Body->SetupAttachment(GetCapsuleComponent());
	Body->SetRelativeLocation(FVector(0.f, 0.f, -88.f));

	Health = CreateDefaultSubobject<UTOHealthComponent>(TEXT("Health"));
	Inventory = CreateDefaultSubobject<UTOInventoryComponent>(TEXT("Inventory"));
	Weapons = CreateDefaultSubobject<UTOWeaponComponent>(TEXT("Weapons"));

	NavInvoker = CreateDefaultSubobject<UNavigationInvokerComponent>(TEXT("NavInvoker"));
	NavInvoker->SetGenerationRadii(5000.f, 7000.f);

	HeadLamp = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeadLamp"));
	HeadLamp->SetupAttachment(FPCamera);
	HeadLamp->SetRelativeLocation(FVector(0.f, 8.f, 6.f));
	HeadLamp->SetIntensityUnits(ELightUnits::Candelas);
	HeadLamp->SetIntensity(700.f);
	HeadLamp->SetAttenuationRadius(2600.f);
	HeadLamp->SetInnerConeAngle(16.f);
	HeadLamp->SetOuterConeAngle(34.f);
	HeadLamp->SetCastShadows(false);
	HeadLamp->SetVisibility(false);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = 390.f;
	Move->MaxWalkSpeedCrouched = 220.f;
	Move->JumpZVelocity = 420.f;
	Move->AirControl = 0.25f;
	Move->GetNavAgentPropertiesRef().bCanCrouch = true;
	Move->SetCrouchedHalfHeight(58.f);
	Move->bCanWalkOffLedgesWhenCrouching = true;
	Move->GroundFriction = 8.f;
	Move->BrakingDecelerationWalking = 2048.f;
	Move->MaxAcceleration = 2400.f;
	Move->RotationRate = FRotator(0.f, 540.f, 0.f);
	Move->bOrientRotationToMovement = false;
	Move->bUseControllerDesiredRotation = false;

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	BaseEyeHeight = 76.f;
	CrouchedEyeHeight = 52.f;

	AIControllerClass = ATOAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

void ATOCharacter::BeginPlay()
{
	Super::BeginPlay();
	Health->SetInventory(Inventory);
	SavedGroundFriction = GetCharacterMovement()->GroundFriction;
	SavedBraking = GetCharacterMovement()->BrakingDecelerationWalking;
	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		MatchMode = GM->GetMatchMode();
		GM->RegisterCharacter(this);
	}
}

void ATOCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		GM->UnregisterCharacter(this);
	}
	Super::EndPlay(EndPlayReason);
}

void ATOCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
}

void ATOCharacter::InitCharacter(int32 InTeam, const FString& InName, ETOOperator InOperator, bool bPlayer, ETOMat InSuit)
{
	TeamId = InTeam;
	DisplayName = InName;
	Operator = InOperator;
	bIsPlayerControlled = bPlayer;
	SuitMat = InSuit;

	Body->BuildRig(SuitMat, bPlayer, bBoss);
	Weapons->Init(this, bPlayer ? ViewmodelRoot.Get() : nullptr, bPlayer);
	Health->SetInventory(Inventory);

	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (bPlayer)
	{
		bUseControllerRotationYaw = true;
		Move->bUseControllerDesiredRotation = false;
		FPCamera->SetActive(true);
	}
	else
	{
		bUseControllerRotationYaw = false;
		Move->bUseControllerDesiredRotation = true;
		Move->RotationRate = FRotator(0.f, 420.f, 0.f);
		FPCamera->SetActive(false);
	}
}

void ATOCharacter::RefreshGearVisuals()
{
	Body->UpdateGear(Inventory->GetHelmetLevel(), Inventory->GetArmorLevel(), Inventory->RigItem.IsValid(), Inventory->HasBackpack());
}

void ATOCharacter::OnActiveWeaponChanged()
{
}

void ATOCharacter::ApplyLoadout(const FTOLoadout& L, ETOMatchMode Mode)
{
	MatchMode = Mode;
	Operator = L.Operator;
	TArray<FTOItemInstance> Displaced;
	Inventory->ClearAll(true);
	if (L.RigTier > 0) Inventory->EquipGear(TODB::MakeItem(TODB::RigItemId(L.RigTier)), Displaced);
	if (L.BackpackTier > 0) Inventory->EquipGear(TODB::MakeItem(TODB::BackpackItemId(L.BackpackTier)), Displaced);
	if (L.ArmorLevel > 0) Inventory->EquipGear(TODB::MakeItem(TODB::ArmorItemId(L.ArmorLevel)), Displaced);
	if (L.HelmetLevel > 0) Inventory->EquipGear(TODB::MakeItem(TODB::HelmetItemId(L.HelmetLevel)), Displaced);

	if (L.Primary.IsValid())
	{
		Weapons->SetWeapon(UTOWeaponComponent::SlotPrimary, L.Primary, true);
		const FTOWeaponStats S = TODB::ComputeStats(L.Primary);
		if (S.Def)
		{
			const int32 Mags = (Mode == ETOMatchMode::Warfare) ? 7 : L.SpareMags;
			Inventory->AddItem(TODB::MakeItem(TODB::AmmoItemId(S.Def->Caliber, L.Primary.AmmoTier), S.MagSize * Mags));
		}
	}
	if (L.Sidearm.IsValid())
	{
		Weapons->SetWeapon(UTOWeaponComponent::SlotSidearm, L.Sidearm, true);
		const FTOWeaponStats S = TODB::ComputeStats(L.Sidearm);
		if (S.Def)
		{
			Inventory->AddItem(TODB::MakeItem(TODB::AmmoItemId(S.Def->Caliber, L.Sidearm.AmmoTier), S.MagSize * 3));
		}
	}

	TArray<FName> Meds;
	TODB::GetMedKit(L.MedTier, Meds);
	for (const FName& M : Meds)
	{
		Inventory->AddItem(TODB::MakeItem(M));
	}
	for (int32 i = 0; i < L.Frags; ++i) Inventory->AddItem(TODB::MakeItem(TODB::GrenadeItemId(ETOGrenadeType::Frag)));
	for (int32 i = 0; i < L.Smokes; ++i) Inventory->AddItem(TODB::MakeItem(TODB::GrenadeItemId(ETOGrenadeType::Smoke)));
	for (int32 i = 0; i < L.Flashes; ++i) Inventory->AddItem(TODB::MakeItem(TODB::GrenadeItemId(ETOGrenadeType::Flash)));

	Health->bPassiveRegen = (Mode == ETOMatchMode::Warfare);
	Health->ResetFull();
	Weapons->EquipSlot(L.Primary.IsValid() ? UTOWeaponComponent::SlotPrimary : UTOWeaponComponent::SlotSidearm, true);
	RefreshGearVisuals();
}

void ATOCharacter::ApplyAIKit(int32 Tier, ETOAIRole Role, FRandomStream& Rng, ETOMatchMode Mode)
{
	MatchMode = Mode;
	AIRole = Role;
	ZoneTier = Tier;
	Inventory->ClearAll(false);
	TArray<FTOItemInstance> Displaced;

	int32 ArmorLvl = 0;
	int32 HelmetLvl = 0;
	switch (FMath::Clamp(Tier, 0, 3))
	{
	case 0: ArmorLvl = Rng.RandRange(0, 2); HelmetLvl = Rng.RandRange(0, 1); break;
	case 1: ArmorLvl = Rng.RandRange(2, 3); HelmetLvl = Rng.RandRange(1, 3); break;
	case 2: ArmorLvl = Rng.RandRange(3, 4); HelmetLvl = Rng.RandRange(3, 4); break;
	default: ArmorLvl = Rng.RandRange(4, 5); HelmetLvl = Rng.RandRange(4, 5); break;
	}
	if (Role == ETOAIRole::Boss)
	{
		ArmorLvl = 5;
		HelmetLvl = 5;
	}
	if (Role == ETOAIRole::WarfareBot || Role == ETOAIRole::Teammate)
	{
		ArmorLvl = FMath::Max(ArmorLvl, 3);
		HelmetLvl = FMath::Max(HelmetLvl, 3);
	}

	Inventory->EquipGear(TODB::MakeItem(TODB::RigItemId(Rng.RandRange(1, 2))), Displaced);
	if (Role == ETOAIRole::Rival || Rng.FRand() < 0.35f)
	{
		Inventory->EquipGear(TODB::MakeItem(TODB::BackpackItemId(Rng.RandRange(1, 3))), Displaced);
	}
	if (ArmorLvl > 0) Inventory->EquipGear(TODB::MakeItem(TODB::ArmorItemId(ArmorLvl)), Displaced);
	if (HelmetLvl > 0) Inventory->EquipGear(TODB::MakeItem(TODB::HelmetItemId(HelmetLvl)), Displaced);

	FTOWeaponConfig Primary = TODB::MakeRandomConfig(Tier, Rng, Role != ETOAIRole::Teammate);
	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		if (GM->IsNight())
		{
			// Night patrols carry flashlights (and give their position away).
			Primary.SetAttachment(ETOAttachSlot::Tactical, FName(TEXT("Tac_Light")));
		}
	}
	Weapons->SetWeapon(UTOWeaponComponent::SlotPrimary, Primary, true);
	if (Rng.FRand() < 0.5f)
	{
		const TArray<FName> Pistols = TODB::SidearmIds();
		if (Pistols.Num() > 0)
		{
			Weapons->SetWeapon(UTOWeaponComponent::SlotSidearm, TODB::MakeDefaultConfig(Pistols[Rng.RandRange(0, Pistols.Num() - 1)], 2), true);
		}
	}
	Weapons->bInfiniteReserve = true;
	Weapons->AIDispersion = FMath::Lerp(1.6f, 0.8f, FMath::Clamp(Tier / 3.f, 0.f, 1.f));

	// Lootable supplies & valuables on the body
	const FTOWeaponStats S = TODB::ComputeStats(Primary);
	if (S.Def)
	{
		Inventory->AddItem(TODB::MakeItem(TODB::AmmoItemId(S.Def->Caliber, Primary.AmmoTier), S.MagSize * Rng.RandRange(1, 2)));
	}
	Inventory->AddItem(TODB::MakeItem(FName(TEXT("Med_Bandage"))));
	if (Rng.FRand() < 0.4f) Inventory->AddItem(TODB::MakeItem(FName(TEXT("Med_Medkit"))));
	if (Rng.FRand() < 0.5f) Inventory->AddItem(TODB::MakeItem(TODB::GrenadeItemId(ETOGrenadeType::Frag)));
	if (Mode == ETOMatchMode::Operations)
	{
		const int32 Valuables = Rng.RandRange(0, 1 + Tier);
		for (int32 i = 0; i < Valuables; ++i)
		{
			Inventory->AddItem(TODB::RollValuable(TODB::RollRarity(Tier, Rng), false, Rng), true);
		}
	}

	if (Role == ETOAIRole::Boss)
	{
		bBoss = true;
		Health->MaxHealth = 260.f;
		Inventory->AddItem(TODB::MakeItem(FName(TEXT("Key_AdminBlue"))), true);
		Inventory->AddItem(TODB::MakeItem(FName(TEXT("Val_Ledger"))), true);
		Inventory->AddItem(TODB::MakeItem(FName(TEXT("Val_GoldBar"))), true);
	}
	Health->bCanBeDowned = (Role == ETOAIRole::Teammate || Role == ETOAIRole::WarfareBot);
	Health->bPassiveRegen = (Mode == ETOMatchMode::Warfare);
	Health->ResetFull();
	Weapons->EquipSlot(UTOWeaponComponent::SlotPrimary, true);
	RefreshGearVisuals();
}

// ---------------------------------------------------------------------------------------------
//  State queries
// ---------------------------------------------------------------------------------------------

bool ATOCharacter::IsAlive() const
{
	return Health && !Health->IsDead() && !bDeathHandled;
}

bool ATOCharacter::IsDowned() const
{
	return Health && Health->IsDowned() && !bDeathHandled;
}

bool ATOCharacter::IsDeadState() const
{
	return bDeathHandled || (Health && Health->IsDead());
}

bool ATOCharacter::IsHostileTo(const ATOCharacter* Other) const
{
	return Other && TOUtil::IsHostile(TeamId, Other->TeamId);
}

FVector ATOCharacter::GetEyeLocation() const
{
	if (bIsPlayerControlled && FPCamera)
	{
		return FPCamera->GetComponentLocation();
	}
	return Body ? Body->GetHeadWorld() + GetActorForwardVector() * 8.f : GetActorLocation() + FVector(0.f, 0.f, 70.f);
}

FVector ATOCharacter::GetPawnViewLocation() const
{
	return GetEyeLocation();
}

FVector ATOCharacter::GetHeadLocation() const
{
	return Body ? Body->GetHeadWorld() : GetActorLocation() + FVector(0.f, 0.f, 75.f);
}

FVector ATOCharacter::GetChestLocation() const
{
	return Body ? Body->GetChestWorld() : GetActorLocation() + FVector(0.f, 0.f, 40.f);
}

FVector ATOCharacter::GetAimDirection() const
{
	if (bIsPlayerControlled && FPCamera)
	{
		return FPCamera->GetForwardVector();
	}
	return GetControlRotation().Vector();
}

FVector ATOCharacter::GetMuzzleLocation() const
{
	return Body ? Body->GetMuzzleWorld() : GetEyeLocation();
}

float ATOCharacter::GetVisibilityFactor() const
{
	float V = 1.f;
	if (Stance == ETOStance::Crouch) V *= 0.75f;
	if (Stance == ETOStance::Prone) V *= 0.45f;
	V *= 1.f + FMath::Clamp(GetVelocity().Size2D() / 600.f, 0.f, 1.f) * 0.3f;
	if (Weapons && Weapons->IsFiringRecently()) V *= 1.7f;
	const ATOGameMode* GM = ATOGameMode::Get(this);
	if (GM && GM->IsNight())
	{
		V *= 0.45f;
		if ((Weapons && Weapons->IsLightOn()) || (HeadLamp && HeadLamp->IsVisible()))
		{
			V *= 3.2f;
		}
	}
	return V;
}

// ---------------------------------------------------------------------------------------------
//  Input
// ---------------------------------------------------------------------------------------------

void ATOCharacter::InputJump()
{
	if (!IsAlive() || IsDowned() || CurrentVehicle.IsValid())
	{
		return;
	}
	if (Stance == ETOStance::Prone)
	{
		SetStance(ETOStance::Crouch);
		return;
	}
	if (Stance == ETOStance::Crouch)
	{
		SetStance(ETOStance::Stand);
		return;
	}
	if (TryVault())
	{
		return;
	}
	if (Stamina < 10.f || !GetCharacterMovement()->IsMovingOnGround())
	{
		return;
	}
	Stamina -= 12.f;
	StaminaRegenDelay = 1.f;
	Jump();
}

bool ATOCharacter::TryVault()
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move->IsMovingOnGround())
	{
		return false;
	}
	UWorld* World = GetWorld();
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, Half);
	const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TOVault), false, this);

	FHitResult Front;
	if (!World->LineTraceSingleByChannel(Front, Feet + FVector(0.f, 0.f, 55.f), Feet + FVector(0.f, 0.f, 55.f) + Fwd * 95.f, ECC_Visibility, Params))
	{
		return false;
	}
	const FVector TopProbe = Front.ImpactPoint + Fwd * 25.f;
	FHitResult Top;
	if (!World->LineTraceSingleByChannel(Top, TopProbe + FVector(0.f, 0.f, 160.f), TopProbe - FVector(0.f, 0.f, 10.f), ECC_Visibility, Params))
	{
		return false;
	}
	const float H = Top.ImpactPoint.Z - Feet.Z;
	if (H < 45.f || H > 150.f || Top.bStartPenetrating)
	{
		return false;
	}
	FHitResult Clear;
	if (World->LineTraceSingleByChannel(Clear, Top.ImpactPoint + FVector(0.f, 0.f, 5.f), Top.ImpactPoint + FVector(0.f, 0.f, 120.f), ECC_Visibility, Params))
	{
		return false;
	}
	LaunchCharacter(Fwd * 380.f + FVector(0.f, 0.f, 330.f + H * 2.4f), true, true);
	Stamina = FMath::Max(0.f, Stamina - 8.f);
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::Land, GetActorLocation(), 0.5f, 1.2f);
	}
	return true;
}

void ATOCharacter::SetStance(ETOStance NewStance)
{
	if (NewStance == Stance || IsDowned() || !IsAlive())
	{
		return;
	}
	if (NewStance == ETOStance::Stand)
	{
		UnCrouch();
	}
	else
	{
		Crouch();
	}
	Stance = NewStance;
}

void ATOCharacter::ToggleCrouch()
{
	if (!IsAlive() || IsDowned())
	{
		return;
	}
	if (bSprinting && GetVelocity().Size2D() > 450.f && GetCharacterMovement()->IsMovingOnGround())
	{
		StartSlide();
		return;
	}
	SetStance(Stance == ETOStance::Crouch ? ETOStance::Stand : ETOStance::Crouch);
}

void ATOCharacter::ToggleProne()
{
	if (!IsAlive() || IsDowned() || bInWater)
	{
		return;
	}
	SetStance(Stance == ETOStance::Prone ? ETOStance::Crouch : ETOStance::Prone);
}

void ATOCharacter::StartSlide()
{
	UWorld* World = GetWorld();
	UCharacterMovementComponent* Move = GetCharacterMovement();
	bSliding = true;
	SlideEnd = World->GetTimeSeconds() + 0.8f;
	Move->GroundFriction = 0.5f;
	Move->BrakingDecelerationWalking = 320.f;
	Stance = ETOStance::Stand;
	SetStance(ETOStance::Crouch);
	LaunchCharacter(GetVelocity().GetSafeNormal2D() * 420.f, false, false);
	Stamina = FMath::Max(0.f, Stamina - 10.f);
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::FootstepWater, GetActorLocation(), 0.5f, 0.6f);
	}
}

void ATOCharacter::EndSlide()
{
	bSliding = false;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->GroundFriction = SavedGroundFriction;
	Move->BrakingDecelerationWalking = SavedBraking;
}

void ATOCharacter::SetFireInput(bool bPressed)
{
	if (bPressed && (!IsAlive() || IsDowned() || bUsingItem || CurrentVehicle.IsValid()))
	{
		return;
	}
	if (bPressed && bSprinting)
	{
		bWantsSprint = false;
	}
	Weapons->SetTrigger(bPressed);
}

void ATOCharacter::SetAimInput(bool bPressed)
{
	Weapons->SetAiming(bPressed);
}

void ATOCharacter::InputReload()
{
	if (IsAlive() && !IsDowned() && !bUsingItem)
	{
		Weapons->Reload();
	}
}

void ATOCharacter::InputSelectWeapon(int32 Slot)
{
	if (IsAlive() && !IsDowned())
	{
		CancelItemUse();
		Weapons->EquipSlot(Slot);
	}
}

void ATOCharacter::InputCycleWeapon(int32 Direction)
{
	if (IsAlive() && !IsDowned())
	{
		CancelItemUse();
		Weapons->CycleWeapon(Direction);
	}
}

void ATOCharacter::InputFireMode()
{
	Weapons->CycleFireMode();
}

void ATOCharacter::InputCycleGrenade()
{
	for (int32 Step = 1; Step <= 3; ++Step)
	{
		const ETOGrenadeType T = (ETOGrenadeType)(((int32)SelectedGrenade + Step) % 3);
		if (Inventory->CountGrenades(T) > 0)
		{
			SelectedGrenade = T;
			return;
		}
	}
}

void ATOCharacter::InputGrenade()
{
	if (!IsAlive() || IsDowned() || bUsingItem || IsThrowing() || CurrentVehicle.IsValid())
	{
		return;
	}
	if (Inventory->CountGrenades(SelectedGrenade) <= 0)
	{
		InputCycleGrenade();
		if (Inventory->CountGrenades(SelectedGrenade) <= 0)
		{
			return;
		}
	}
	ThrowStart = GetWorld()->GetTimeSeconds();
	bThrowPending = true;
	PendingThrowType = SelectedGrenade;
	bPendingThrowHasTarget = false;
	Weapons->SetTrigger(false);
	Weapons->CancelReload();
}

bool ATOCharacter::ThrowGrenadeAt(ETOGrenadeType Type, const FVector& Target)
{
	if (!IsAlive() || IsDowned() || IsThrowing() || Inventory->CountGrenades(Type) <= 0)
	{
		return false;
	}
	ThrowStart = GetWorld()->GetTimeSeconds();
	bThrowPending = true;
	PendingThrowType = Type;
	PendingThrowTarget = Target;
	bPendingThrowHasTarget = true;
	Weapons->SetTrigger(false);
	return true;
}

bool ATOCharacter::IsThrowing() const
{
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - ThrowStart) < 0.7f;
}

void ATOCharacter::DoThrowGrenade()
{
	bThrowPending = false;
	if (!IsAlive() || IsDowned())
	{
		return;
	}
	if (!Inventory->ConsumeOne(TODB::GrenadeItemId(PendingThrowType)))
	{
		return;
	}
	const FVector Aim = GetAimDirection();
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Aim).GetSafeNormal();
	const FVector Start = GetEyeLocation() + Aim * 45.f + Right * 15.f - FVector(0.f, 0.f, 10.f);
	FVector Vel;
	if (bPendingThrowHasTarget)
	{
		const FVector Delta = PendingThrowTarget - Start;
		const float T = FMath::Clamp(Delta.Size2D() / 1100.f, 0.6f, 1.7f);
		Vel = Delta / T - FVector(0.f, 0.f, -980.f) * 0.5f * T;
		Vel = Vel.GetClampedToMaxSize(2000.f);
	}
	else
	{
		Vel = Aim * 1350.f + FVector(0.f, 0.f, 220.f) + GetVelocity() * 0.5f;
	}
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.Instigator = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ATOGrenade* G = GetWorld()->SpawnActor<ATOGrenade>(ATOGrenade::StaticClass(), Start, Aim.Rotation(), Params))
	{
		G->Launch(PendingThrowType, Vel, this);
	}
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::Whiz, GetActorLocation(), 0.35f, 0.7f);
	}
}

void ATOCharacter::InputAbility()
{
	if (!IsAlive() || IsDowned())
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < AbilityReadyTime)
	{
		return;
	}
	AbilityReadyTime = Now + TODB::GetOperator(Operator).Cooldown;
	ApplyAbilityEffect();
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::Ability, GetActorLocation(), 0.8f);
	}
}

void ATOCharacter::ApplyAbilityEffect()
{
	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();
	ATOGameMode* GM = ATOGameMode::Get(this);
	ATOCombatManager* CM = ATOCombatManager::Get(this);
	switch (Operator)
	{
	case ETOOperator::Viper:
		AdrenalineEnd = Now + 8.f;
		Stamina = 100.f;
		Health->AddPainkiller(8.f);
		if (CM) CM->LightFlash(GetActorLocation(), FLinearColor(1.f, 0.5f, 0.1f), 3000.f, 400.f, 0.4f);
		break;
	case ETOOperator::Halo:
		if (GM)
		{
			for (const TWeakObjectPtr<ATOCharacter>& Ptr : GM->GetCharacters())
			{
				ATOCharacter* C = Ptr.Get();
				if (!C || C->IsDeadState() || IsHostileTo(C))
				{
					continue;
				}
				const float Dist = FVector::Dist(C->GetActorLocation(), GetActorLocation());
				if (Dist > 1000.f)
				{
					continue;
				}
				if (C->IsDowned() && Dist < 450.f)
				{
					C->Revive(this);
				}
				C->GetHealth()->StopBleeding();
				C->GetHealth()->AddHealOverTime(40.f, 4.f);
			}
		}
		if (CM) CM->LightFlash(GetActorLocation() + FVector(0.f, 0.f, 50.f), FLinearColor(0.3f, 1.f, 0.4f), 8000.f, 1000.f, 0.8f);
		break;
	case ETOOperator::Bastion:
	{
		const FVector Fwd = GetActorForwardVector().GetSafeNormal2D();
		FVector Loc = GetActorLocation() + Fwd * 160.f;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TOShield), false, this);
		if (World->LineTraceSingleByChannel(Hit, Loc + FVector(0.f, 0.f, 100.f), Loc - FVector(0.f, 0.f, 400.f), ECC_Visibility, Params))
		{
			Loc = Hit.ImpactPoint;
		}
		FActorSpawnParameters SP;
		SP.Owner = this;
		SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ATODeployableShield* Shield = World->SpawnActor<ATODeployableShield>(ATODeployableShield::StaticClass(), Loc, Fwd.Rotation(), SP))
		{
			Shield->Team = TeamId;
		}
		break;
	}
	case ETOOperator::Echo:
		if (GM)
		{
			GM->RevealEnemiesAround(this, 7000.f, 8.f);
		}
		if (CM) CM->LightFlash(GetActorLocation() + FVector(0.f, 0.f, 80.f), FLinearColor(0.6f, 0.3f, 1.f), 6000.f, 900.f, 0.5f);
		break;
	}
}

bool ATOCharacter::IsAdrenalineActive() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < AdrenalineEnd;
}

float ATOCharacter::GetAbilityCooldownRemaining() const
{
	const UWorld* World = GetWorld();
	return World ? FMath::Max(0.f, AbilityReadyTime - World->GetTimeSeconds()) : 0.f;
}

float ATOCharacter::GetAbilityCooldownTotal() const
{
	return TODB::GetOperator(Operator).Cooldown;
}

void ATOCharacter::InputQuickHeal()
{
	if (!IsAlive() || IsDowned() || bUsingItem)
	{
		return;
	}
	const bool bHurt = Health->GetHealth() < Health->GetMaxHealth() - 1.f;
	const bool bFracture = Health->HasLegFracture() || Health->HasArmFracture();
	if (FTOItemInstance* Med = Inventory->FindBestMedical(Health->IsBleeding(), bFracture, bHurt))
	{
		UseItemByUid(Med->Uid);
	}
}

void ATOCharacter::InputLight()
{
	if (!IsAlive())
	{
		return;
	}
	if (Weapons->GetStats().bLight && !Weapons->IsMeleeActive())
	{
		Weapons->ToggleLight();
		HeadLamp->SetVisibility(false);
	}
	else
	{
		HeadLamp->SetVisibility(!HeadLamp->IsVisible());
	}
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play2D(ETOSound::UIClick, 0.5f, 0.8f);
	}
}

void ATOCharacter::InputNVG()
{
	bNVG = !bNVG;
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play2D(ETOSound::Beep, 0.35f, 1.6f);
	}
}

void ATOCharacter::SetInteractInput(bool bPressed)
{
	bInteractHeld = bPressed;
	if (bPressed)
	{
		InteractStart = GetWorld()->GetTimeSeconds();
		bInteractConsumed = false;
	}
}

void ATOCharacter::AddLookInput(float YawDelta, float PitchDelta)
{
	AController* C = GetController();
	if (!C)
	{
		return;
	}
	FRotator R = C->GetControlRotation();
	R.Pitch = FMath::Clamp(FRotator::NormalizeAxis(R.Pitch + PitchDelta), -88.f, 88.f);
	R.Yaw = FRotator::NormalizeAxis(R.Yaw + YawDelta);
	R.Roll = 0.f;
	C->SetControlRotation(R);
	// Pulling down compensates recoil, so it is not "recovered" twice.
	if (PitchDelta < 0.f)
	{
		RecoilAccumPitch = FMath::Max(0.f, RecoilAccumPitch + PitchDelta);
	}
	Weapons->AddLookDelta(YawDelta, PitchDelta);
}

void ATOCharacter::CancelActions()
{
	Weapons->SetTrigger(false);
	Weapons->SetAiming(false);
	bWantsSprint = false;
	LeanInput = 0.f;
	MoveInput = FVector2D::ZeroVector;
	bInteractHeld = false;
}

// ---------------------------------------------------------------------------------------------
//  Items
// ---------------------------------------------------------------------------------------------

bool ATOCharacter::UseItemByUid(int32 Uid)
{
	if (!IsAlive() || IsDowned() || bUsingItem)
	{
		return false;
	}
	FTOItemInstance* It = Inventory->FindByUid(Uid);
	const FTOItemDef* D = It ? TODB::FindItem(It->ItemId) : nullptr;
	if (!D)
	{
		return false;
	}
	if (D->Category == ETOItemCategory::Grenade)
	{
		SelectedGrenade = D->GrenadeType;
		return true;
	}
	if (D->Category != ETOItemCategory::Medical)
	{
		return false;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	bUsingItem = true;
	UsingUid = Uid;
	UsingItemId = It->ItemId;
	UseStart = Now;
	UseEnd = Now + D->UseTime * (Operator == ETOOperator::Halo ? 0.7f : 1.f);
	Weapons->SetTrigger(false);
	Weapons->CancelReload();
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::Pickup, GetActorLocation(), 0.5f, 0.8f);
	}
	return true;
}

void ATOCharacter::CancelItemUse()
{
	bUsingItem = false;
}

float ATOCharacter::GetUseProgress() const
{
	if (!bUsingItem)
	{
		return 0.f;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	return FMath::Clamp((Now - UseStart) / FMath::Max(0.01f, UseEnd - UseStart), 0.f, 1.f);
}

FString ATOCharacter::GetUsingItemName() const
{
	const FTOItemDef* D = TODB::FindItem(UsingItemId);
	return D ? D->Name : FString();
}

void ATOCharacter::UpdateItemUse(float Now)
{
	if (!bUsingItem)
	{
		return;
	}
	if (!IsAlive() || IsDowned())
	{
		bUsingItem = false;
		return;
	}
	if (Now < UseEnd)
	{
		return;
	}
	bUsingItem = false;
	FTOItemInstance* It = Inventory->FindByUid(UsingUid);
	const FTOItemDef* D = TODB::FindItem(UsingItemId);
	if (!It || !D)
	{
		return;
	}
	if (D->bStopBleed) Health->StopBleeding();
	if (D->bFixFracture) Health->FixFractures();
	if (D->Heal > 0.f)
	{
		if (D->Heal >= 20.f) Health->AddHealOverTime(D->Heal, 2.5f);
		else Health->Heal(D->Heal);
	}
	if (D->Painkiller > 0.f) Health->AddPainkiller(D->Painkiller);
	if (D->Id == FName(TEXT("Med_Adrenaline")))
	{
		Stamina = 100.f;
	}
	It->Count -= 1;
	if (It->Count <= 0)
	{
		Inventory->RemoveByUid(UsingUid);
	}
	Inventory->MarkDirty();
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::Heal, GetActorLocation(), 0.6f);
	}
}

// ---------------------------------------------------------------------------------------------
//  Interaction
// ---------------------------------------------------------------------------------------------

void ATOCharacter::UpdateFocus(float Now)
{
	AActor* PrevFocus = FocusActor.Get();
	ATOCharacter* PrevRevive = ReviveTarget.Get();
	FocusActor = nullptr;
	ReviveTarget = nullptr;
	if (!IsAlive() || IsDowned() || !FPCamera || CurrentVehicle.IsValid())
	{
		return;
	}
	UWorld* World = GetWorld();
	const FVector Start = FPCamera->GetComponentLocation();
	const FVector Fwd = FPCamera->GetForwardVector();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TOInteract), false, this);
	FHitResult Hit;
	if (World->SweepSingleByChannel(Hit, Start, Start + Fwd * 260.f, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(10.f), Params))
	{
		AActor* A = Hit.GetActor();
		if (A && A->GetClass()->ImplementsInterface(UTOInteractable::StaticClass()))
		{
			if (ITOInteractable* I = Cast<ITOInteractable>(A))
			{
				if (I->CanInteract(this))
				{
					FocusActor = A;
				}
			}
		}
	}
	if (!FocusActor.IsValid())
	{
		if (ATOGameMode* GM = ATOGameMode::Get(this))
		{
			float Best = 240.f;
			for (const TWeakObjectPtr<ATOCharacter>& Ptr : GM->GetCharacters())
			{
				ATOCharacter* C = Ptr.Get();
				if (!C || C == this || !C->IsDowned() || IsHostileTo(C))
				{
					continue;
				}
				const FVector To = C->GetActorLocation() - Start;
				const float Dist = To.Size();
				if (Dist < Best && FVector::DotProduct(To.GetSafeNormal(), Fwd) > 0.4f)
				{
					Best = Dist;
					ReviveTarget = C;
				}
			}
		}
	}

	if (FocusActor.Get() != PrevFocus || ReviveTarget.Get() != PrevRevive)
	{
		InteractStart = Now;
		bInteractConsumed = false;
	}

	if (!bInteractHeld || bInteractConsumed)
	{
		return;
	}
	if (AActor* A = FocusActor.Get())
	{
		ITOInteractable* I = Cast<ITOInteractable>(A);
		const float Dur = I ? I->GetInteractDuration(this) : 0.f;
		if (I && (Dur <= 0.f || Now - InteractStart >= Dur))
		{
			bInteractConsumed = true;
			I->Interact(this);
		}
	}
	else if (ATOCharacter* T = ReviveTarget.Get())
	{
		const float Dur = Operator == ETOOperator::Halo ? 2.5f : 5.f;
		if (Now - InteractStart >= Dur)
		{
			bInteractConsumed = true;
			T->Revive(this);
		}
	}
}

float ATOCharacter::GetInteractProgress() const
{
	if (!bInteractHeld || bInteractConsumed)
	{
		return 0.f;
	}
	const float Now = GetWorld()->GetTimeSeconds();
	float Dur = 0.f;
	if (AActor* A = FocusActor.Get())
	{
		if (ITOInteractable* I = Cast<ITOInteractable>(A))
		{
			Dur = I->GetInteractDuration(this);
		}
	}
	else if (ReviveTarget.IsValid())
	{
		Dur = Operator == ETOOperator::Halo ? 2.5f : 5.f;
	}
	return Dur > 0.f ? FMath::Clamp((Now - InteractStart) / Dur, 0.f, 1.f) : 0.f;
}

FString ATOCharacter::GetFocusLabel() const
{
	if (AActor* A = FocusActor.Get())
	{
		if (ITOInteractable* I = Cast<ITOInteractable>(A))
		{
			return I->GetInteractLabel(this);
		}
	}
	if (ATOCharacter* T = ReviveTarget.Get())
	{
		return FString::Printf(TEXT("Revive %s"), *T->DisplayName);
	}
	return FString();
}

FString ATOCharacter::GetFocusHint() const
{
	if (AActor* A = FocusActor.Get())
	{
		if (ITOInteractable* I = Cast<ITOInteractable>(A))
		{
			return I->GetInteractHint(this);
		}
	}
	return FString();
}

// ---------------------------------------------------------------------------------------------
//  Damage / death
// ---------------------------------------------------------------------------------------------

void ATOCharacter::ReceiveTODamage(const FTODamageInfo& Info)
{
	if (bDeathHandled || Health->IsDead())
	{
		return;
	}
	ATOCharacter* Inst = Cast<ATOCharacter>(Info.Instigator.Get());
	// No friendly fire (except your own grenades).
	if (Inst && Inst != this && !TOUtil::IsHostile(Inst->TeamId, TeamId))
	{
		return;
	}
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	const FTODamageResult R = Health->ApplyDamage(Info);
	LastDamagedTime = Now;
	if (Inst && Inst != this)
	{
		LastAttacker = Inst;
	}
	Body->AddFlinch(FMath::Clamp(R.Applied / 30.f, 0.2f, 1.f));

	if (bIsPlayerControlled)
	{
		FTODamageIndicator D;
		D.From = Inst ? Inst->GetActorLocation() : Info.HitLocation - Info.HitDirection * 800.f;
		D.Time = Now;
		D.Strength = FMath::Clamp(R.Applied / 25.f, 0.3f, 1.f);
		DamageIndicators.Add(D);
		if (DamageIndicators.Num() > 8)
		{
			DamageIndicators.RemoveAt(0);
		}
		AddCameraShake(0.15f + R.Applied / 80.f);
		if (R.bArmorHit)
		{
			if (ATOCombatManager* CM = ATOCombatManager::Get(this))
			{
				if (CM->GetAudio()) CM->GetAudio()->Play2D(ETOSound::Impact, 0.8f, 0.7f);
			}
		}
	}

	if (Inst && Inst != this)
	{
		Inst->OnDealtDamage(this, R);
	}

	if (ATOAIController* AI = Cast<ATOAIController>(GetController()))
	{
		AI->OnDamaged(Info, Inst);
	}

	if (R.bKilled)
	{
		HandleDeath(Info);
	}
	else if (R.bDowned)
	{
		HandleDowned(Info);
	}
}

void ATOCharacter::OnDealtDamage(ATOCharacter* Victim, const FTODamageResult& Result)
{
	if (Result.bKilled)
	{
		++Kills;
		if (Result.bHeadshot)
		{
			++Headshots;
		}
	}
	if (!bIsPlayerControlled)
	{
		return;
	}
	LastHitMarkerTime = GetWorld()->GetTimeSeconds();
	bLastHitKill = Result.bKilled || Result.bDowned;
	bLastHitHead = Result.bHeadshot;
	bLastHitArmor = Result.bArmorHit;
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (UTOAudio* A = CM->GetAudio())
		{
			if (bLastHitKill) A->Play2D(ETOSound::KillMarker, 0.7f);
			else if (Result.bHeadshot) A->Play2D(ETOSound::Headshot, 0.6f);
			else A->Play2D(ETOSound::HitMarker, 0.55f, Result.bArmorHit ? 0.8f : 1.f);
		}
	}
}

void ATOCharacter::HandleDowned(const FTODamageInfo& Info)
{
	if (!Health->bCanBeDowned)
	{
		Health->MarkDead();
		HandleDeath(Info);
		return;
	}
	Weapons->SetTrigger(false);
	Weapons->CancelReload();
	Weapons->SetAiming(false);
	bUsingItem = false;
	bSprinting = false;
	bWantsSprint = false;
	LeanInput = 0.f;
	if (bSliding)
	{
		EndSlide();
	}
	Crouch();
	Stance = ETOStance::Prone;
	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		GM->OnCharacterDowned(this, Info);
	}
}

void ATOCharacter::Revive(ATOCharacter* By)
{
	if (!IsDowned())
	{
		return;
	}
	Health->ReviveFromDowned(35.f);
	Stance = ETOStance::Prone;
	SetStance(ETOStance::Crouch);
	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		GM->OnCharacterRevived(this, By);
	}
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(ETOSound::Heal, GetActorLocation(), 0.8f, 1.2f);
	}
}

void ATOCharacter::Kill(const FTODamageInfo& Info)
{
	if (bDeathHandled)
	{
		return;
	}
	Health->MarkDead();
	HandleDeath(Info);
}

void ATOCharacter::HandleDeath(const FTODamageInfo& Info)
{
	if (bDeathHandled)
	{
		return;
	}
	bDeathHandled = true;
	Health->MarkDead();
	const UWorld* World = GetWorld();
	DeathTime = World ? World->GetTimeSeconds() : 0.f;

	ATOCharacter* Inst = Cast<ATOCharacter>(Info.Instigator.Get());
	if (!Inst)
	{
		Inst = LastAttacker.Get();
	}
	if (Inst && Inst != this)
	{
		KilledByName = Inst->DisplayName;
		const FTOWeaponDef* W = TODB::FindWeapon(Info.WeaponId);
		KilledByWeapon = W ? W->Name : Info.WeaponId.ToString();
		KilledByDistance = Info.Distance;
	}
	else
	{
		KilledByName = Info.bFall ? TEXT("Fall damage") : (Info.bBleed ? TEXT("Blood loss") : TEXT("Unknown"));
		KilledByWeapon = FString();
	}

	Weapons->SetTrigger(false);
	Weapons->CancelReload();
	Weapons->SetLightOn(false);
	HeadLamp->SetVisibility(false);
	bUsingItem = false;
	bSprinting = false;
	if (bSliding)
	{
		EndSlide();
	}

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->StopMovementImmediately();
	Move->DisableMovement();
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Body->SetHitboxesEnabled(false);
	if (bIsPlayerControlled)
	{
		Weapons->HideAllVisuals();
	}
	if (NavInvoker)
	{
		NavInvoker->Deactivate();
	}

	SpawnCorpseLoot();

	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		GM->OnCharacterKilled(this, (Inst && Inst != this) ? Inst : nullptr, Info);
	}
	if (!bIsPlayerControlled)
	{
		SetLifeSpan(240.f);
	}
}

void ATOCharacter::SpawnCorpseLoot()
{
	if (MatchMode != ETOMatchMode::Operations)
	{
		return;
	}
	TArray<FTOItemInstance> Items;
	Inventory->CollectAll(Items, !bIsPlayerControlled, true);
	for (int32 Slot = 0; Slot < UTOWeaponComponent::SlotMelee; ++Slot)
	{
		if (Weapons->HasWeapon(Slot))
		{
			Items.Insert(TODB::MakeWeaponItem(Weapons->GetSlot(Slot).Config), 0);
		}
	}
	if (Items.Num() == 0)
	{
		return;
	}
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Loc = GetActorLocation() - FVector(0.f, 0.f, Half - 20.f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ATOLootContainer* C = GetWorld()->SpawnActor<ATOLootContainer>(ATOLootContainer::StaticClass(), Loc, FRotator(0.f, GetActorRotation().Yaw, 0.f), Params))
	{
		C->InitWithItems(ETOContainerType::Corpse, FString::Printf(TEXT("Body: %s"), *DisplayName), Items);
	}
}

// ---------------------------------------------------------------------------------------------
//  Feedback
// ---------------------------------------------------------------------------------------------

void ATOCharacter::AddRecoil(float Pitch, float Yaw)
{
	PendingRecoilPitch += Pitch;
	PendingRecoilYaw += Yaw;
	LastRecoilTime = GetWorld()->GetTimeSeconds();
}

void ATOCharacter::ApplySuppression(float Amount, const FVector& From)
{
	SuppressionAlpha = FMath::Clamp(SuppressionAlpha + Amount, 0.f, 1.f);
	if (bIsPlayerControlled)
	{
		AddCameraShake(Amount * 0.25f);
	}
	if (ATOAIController* AI = Cast<ATOAIController>(GetController()))
	{
		AI->OnSuppressed(From);
	}
}

void ATOCharacter::ApplyFlash(float Amount, const FVector& From)
{
	if (bIsPlayerControlled)
	{
		FlashAlpha = FMath::Max(FlashAlpha, Amount);
		if (Amount > 0.35f)
		{
			if (ATOCombatManager* CM = ATOCombatManager::Get(this))
			{
				if (CM->GetAudio()) CM->GetAudio()->Play2D(ETOSound::Tinnitus, Amount * 0.7f);
			}
		}
	}
	if (ATOAIController* AI = Cast<ATOAIController>(GetController()))
	{
		AI->Blind(Amount * 5.f);
	}
}

void ATOCharacter::Reveal(float Seconds)
{
	const UWorld* World = GetWorld();
	if (World)
	{
		RevealedUntil = FMath::Max(RevealedUntil, World->GetTimeSeconds() + Seconds);
	}
}

bool ATOCharacter::IsRevealed() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimeSeconds() < RevealedUntil;
}

// ---------------------------------------------------------------------------------------------
//  Tick
// ---------------------------------------------------------------------------------------------

float ATOCharacter::ComputeMaxSpeed() const
{
	if (IsDowned())
	{
		return 70.f;
	}
	if (bSliding)
	{
		return 900.f;
	}
	float S = 390.f;
	switch (Stance)
	{
	case ETOStance::Prone: S = 95.f; break;
	case ETOStance::Crouch: S = 220.f; break;
	default: S = bSprinting ? 640.f : 390.f; break;
	}
	if (Weapons->GetADSAlpha() > 0.5f) S *= 0.68f;
	S *= Weapons->GetStats().Def ? Weapons->GetStats().MoveMult : 1.f;
	if (Health->LegPenalty()) S *= 0.65f;
	if (bInWater) S *= 0.6f;
	if (IsAdrenalineActive()) S *= 1.25f;
	if (bUsingItem) S *= 0.5f;
	return S;
}

float ATOCharacter::EyeHeightForStance() const
{
	if (IsDeadState())
	{
		return 25.f;
	}
	if (IsDowned())
	{
		return 40.f;
	}
	if (bSliding)
	{
		return 95.f;
	}
	switch (Stance)
	{
	case ETOStance::Crouch: return 112.f;
	case ETOStance::Prone: return 34.f;
	default: return 164.f;
	}
}

void ATOCharacter::UpdateMovement(float Dt, float Now)
{
	UCharacterMovementComponent* Move = GetCharacterMovement();
	const bool bActive = IsAlive() && !IsDowned();

	// Sprint
	const bool bForward = bIsPlayerControlled ? (MoveInput.Y > 0.3f) : (GetVelocity().Size2D() > 120.f);
	if (bWantsSprint && bIsPlayerControlled && Stance == ETOStance::Crouch && bForward && !bSliding)
	{
		SetStance(ETOStance::Stand);
	}
	const bool bCanSprint = bActive && bWantsSprint && bForward && Stance == ETOStance::Stand && !bInWater
		&& !Health->LegPenalty() && !bUsingItem && Stamina > (bSprinting ? 0.5f : 15.f) && !CurrentVehicle.IsValid()
		&& !(bIsPlayerControlled && Weapons->GetADSAlpha() > 0.3f && Weapons->IsScoped());
	bSprinting = bCanSprint;

	if (bSprinting)
	{
		Stamina = FMath::Max(0.f, Stamina - (IsAdrenalineActive() ? 4.f : 11.f) * Dt);
		StaminaRegenDelay = 0.9f;
	}
	else
	{
		StaminaRegenDelay -= Dt;
		if (StaminaRegenDelay <= 0.f)
		{
			Stamina = FMath::Min(100.f, Stamina + 16.f * Dt);
		}
	}

	const float MaxSpeed = ComputeMaxSpeed();
	Move->MaxWalkSpeed = MaxSpeed;
	Move->MaxWalkSpeedCrouched = MaxSpeed;

	if (bIsPlayerControlled && (bActive || IsDowned()) && !CurrentVehicle.IsValid())
	{
		const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
		const FVector Fwd = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
		AddMovementInput(Fwd, MoveInput.Y);
		AddMovementInput(Right, MoveInput.X);
	}

	// Lean
	const float LeanTarget = (bActive && !bSprinting && Stance != ETOStance::Prone) ? LeanInput : 0.f;
	LeanAlpha = FMath::FInterpTo(LeanAlpha, LeanTarget, Dt, 9.f);

	if (bSliding && (Now > SlideEnd || GetVelocity().Size2D() < 160.f))
	{
		EndSlide();
	}

	// Keep stance in sync if the movement component could not stand up (blocked overhead).
	if (Stance == ETOStance::Stand && bIsCrouched && !bSliding)
	{
		UnCrouch();
	}
}

void ATOCharacter::UpdateCamera(float Dt, float Now)
{
	AController* C = GetController();
	EyeHeight = FMath::FInterpTo(EyeHeight, EyeHeightForStance(), Dt, 10.f);
	const float Half = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// Lean with collision against walls
	float LeanOffset = LeanAlpha * 34.f;
	if (FMath::Abs(LeanOffset) > 1.f)
	{
		const FVector Head = GetActorLocation() + FVector(0.f, 0.f, EyeHeight - Half);
		const FVector Target = Head + GetActorRightVector() * LeanOffset;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TOLean), false, this);
		if (GetWorld()->SweepSingleByChannel(Hit, Head, Target, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(12.f), Params))
		{
			LeanOffset *= Hit.Time;
		}
	}
	FPCamera->SetRelativeLocation(FVector(6.f, LeanOffset, EyeHeight - Half - FMath::Abs(LeanAlpha) * 4.f));

	// Recoil: apply pending impulses smoothly, then recover part of it.
	if (C && IsAlive())
	{
		FRotator R = C->GetControlRotation();
		R.Pitch = FRotator::NormalizeAxis(R.Pitch);
		const float ApplyP = PendingRecoilPitch * FMath::Min(1.f, Dt * 28.f);
		const float ApplyY = PendingRecoilYaw * FMath::Min(1.f, Dt * 28.f);
		PendingRecoilPitch -= ApplyP;
		PendingRecoilYaw -= ApplyY;
		R.Pitch += ApplyP;
		R.Yaw += ApplyY;
		RecoilAccumPitch += ApplyP;
		if (Now - LastRecoilTime > 0.14f && RecoilAccumPitch > 0.f)
		{
			const float Rec = FMath::Min(RecoilAccumPitch, Dt * FMath::Max(3.f, RecoilAccumPitch * 7.f));
			RecoilAccumPitch -= Rec;
			R.Pitch -= Rec * 0.7f;
		}
		R.Pitch = FMath::Clamp(R.Pitch, -88.f, 88.f);
		C->SetControlRotation(R);
	}

	// Camera shake (explosions, suppression, hits)
	ShakeAlpha = FMath::Max(0.f, ShakeAlpha - Dt * 2.5f);
	ShakeTime += Dt;
	const float SA = ShakeAlpha * ShakeAlpha + SuppressionAlpha * 0.15f;
	FRotator Shake(FMath::Sin(ShakeTime * 37.f) * 1.2f * SA, FMath::Sin(ShakeTime * 29.f + 1.f) * 1.0f * SA, FMath::Sin(ShakeTime * 23.f + 2.f) * 0.8f * SA);

	// Scope sway (hold sprint key while scoped to hold breath)
	if (Weapons->IsScoped())
	{
		const float Breath = (bWantsSprint && Stamina > 5.f) ? 0.12f : 1.f;
		if (bWantsSprint)
		{
			Stamina = FMath::Max(0.f, Stamina - 12.f * Dt);
			StaminaRegenDelay = 0.8f;
		}
		const float StanceSteady = Stance == ETOStance::Prone ? 0.35f : (Stance == ETOStance::Crouch ? 0.7f : 1.f);
		Shake.Pitch += FMath::Sin(Now * 0.9f) * 0.09f * Breath * StanceSteady;
		Shake.Yaw += FMath::Sin(Now * 1.3f + 0.7f) * 0.07f * Breath * StanceSteady;
	}

	FRotator View = GetControlRotation() + Shake;
	View.Roll += LeanAlpha * 9.f;

	if (IsDeadState() || IsDowned())
	{
		View.Roll += IsDeadState() ? 70.f : 20.f;
	}
	FPCamera->SetWorldRotation(View);

	// Field of view: optic magnification + sprint kick
	float TargetFOV = BaseFOV;
	const float Ads = Weapons->GetADSAlpha();
	if (Ads > 0.f)
	{
		const float Zoom = FMath::Max(1.f, Weapons->GetStats().Zoom);
		const float HalfRad = FMath::DegreesToRadians(BaseFOV * 0.5f);
		const float ZoomFOV = FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(HalfRad) / Zoom));
		TargetFOV = FMath::Lerp(BaseFOV, ZoomFOV, Ads);
	}
	if (bSprinting)
	{
		TargetFOV += 5.f;
	}
	CurrentFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, Dt, 20.f);
	FPCamera->SetFieldOfView(CurrentFOV);
}

void ATOCharacter::UpdatePostProcess(float Dt)
{
	FPostProcessSettings& PP = FPCamera->PostProcessSettings;
	const float Hp01 = Health->GetHealth01();
	const float LowHP = IsDeadState() ? 1.f : FMath::Clamp((0.45f - Hp01) / 0.45f, 0.f, 1.f);
	LowHealthPulse = LowHP;

	SuppressionAlpha = FMath::Max(0.f, SuppressionAlpha - Dt * 0.6f);
	FlashAlpha = FMath::Max(0.f, FlashAlpha - Dt * 0.32f);

	PP.bOverride_ColorSaturation = true;
	PP.ColorSaturation = FVector4(1.f, 1.f, 1.f, 1.f - 0.7f * LowHP - (IsDowned() ? 0.3f : 0.f));
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.35f + 0.55f * LowHP + 0.45f * SuppressionAlpha + (IsDowned() ? 0.5f : 0.f);
	PP.bOverride_SceneFringeIntensity = true;
	PP.SceneFringeIntensity = 0.15f + 3.5f * SuppressionAlpha;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = FlashAlpha * 7.f + (bNVG ? 2.5f : 0.f);
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.55f + FlashAlpha * 6.f;
	PP.bOverride_SceneColorTint = true;
	PP.SceneColorTint = FLinearColor(1.f, 1.f - 0.25f * LowHP, 1.f - 0.25f * LowHP);

	PP.bOverride_ColorGain = bNVG;
	PP.ColorGain = FVector4(0.35f, 1.f, 0.4f, 1.f);
	PP.bOverride_FilmGrainIntensity = true;
	PP.FilmGrainIntensity = bNVG ? 0.6f : 0.06f;
	PP.bOverride_AutoExposureMinBrightness = bNVG;
	PP.AutoExposureMinBrightness = -6.f;
}

void ATOCharacter::UpdateStatusChecks(float Dt, float Now)
{
	if (Health->bPendingDeathFromStatus)
	{
		Health->bPendingDeathFromStatus = false;
		FTODamageInfo I;
		I.bBleed = true;
		I.Instigator = LastAttacker.Get();
		Kill(I);
		return;
	}
	if (Health->bPendingDownedFromStatus)
	{
		Health->bPendingDownedFromStatus = false;
		FTODamageInfo I;
		I.bBleed = true;
		I.Instigator = LastAttacker.Get();
		HandleDowned(I);
	}

	// Water
	WaterCheckTimer -= Dt;
	if (WaterCheckTimer <= 0.f)
	{
		WaterCheckTimer = 0.2f;
		bInWater = false;
		bHeadUnderWater = false;
		float WaterZ = 0.f;
		ATOGameMode* GM = ATOGameMode::Get(this);
		if (GM && GM->GetWaterHeight(GetActorLocation(), WaterZ))
		{
			const float Feet = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			bInWater = WaterZ > Feet + 15.f;
			bHeadUnderWater = WaterZ > GetHeadLocation().Z;
		}
		if (bHeadUnderWater && IsAlive())
		{
			FTODamageInfo Drown;
			Drown.Damage = 2.f;
			Drown.bBleed = true;
			const FTODamageResult R = Health->ApplyDamage(Drown);
			if (R.bKilled || Health->GetHealth() <= 0.f)
			{
				Kill(Drown);
				return;
			}
		}
	}

	if (bIsPlayerControlled && IsAlive() && Health->GetHealth01() < 0.3f)
	{
		HeartbeatTimer -= Dt;
		if (HeartbeatTimer <= 0.f)
		{
			HeartbeatTimer = 1.1f;
			if (ATOCombatManager* CM = ATOCombatManager::Get(this))
			{
				if (CM->GetAudio()) CM->GetAudio()->Play2D(ETOSound::Heartbeat, 0.6f);
			}
		}
	}
}

void ATOCharacter::UpdateFootsteps(float Dt)
{
	const UCharacterMovementComponent* Move = GetCharacterMovement();
	if (!Move->IsMovingOnGround())
	{
		return;
	}
	const float Speed = GetVelocity().Size2D();
	if (Speed < 50.f)
	{
		return;
	}
	FootstepDistance += Speed * Dt;
	const float Stride = bSprinting ? 190.f : (Stance == ETOStance::Crouch ? 120.f : (Stance == ETOStance::Prone ? 140.f : 160.f));
	if (FootstepDistance < Stride)
	{
		return;
	}
	FootstepDistance = 0.f;
	float Vol = bSprinting ? 0.9f : (Stance == ETOStance::Crouch ? 0.3f : (Stance == ETOStance::Prone ? 0.2f : 0.55f));
	if (bIsPlayerControlled)
	{
		Vol *= 0.55f;
	}
	const FVector Feet = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio()) CM->GetAudio()->Play(bInWater ? ETOSound::FootstepWater : ETOSound::Footstep, Feet, Vol);
	}
	const float NoiseRadius = bSprinting ? 2400.f : (Stance == ETOStance::Stand ? 1100.f : 350.f);
	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		GM->ReportNoise(Feet, NoiseRadius * (bInWater ? 1.5f : 1.f), this, false);
	}
}

void ATOCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	const float FallSpeed = -GetVelocity().Z;
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio() && FallSpeed > 300.f)
		{
			CM->GetAudio()->Play(ETOSound::Land, GetActorLocation(), FMath::Clamp(FallSpeed / 900.f, 0.3f, 1.f));
		}
	}
	if (FallSpeed > 1100.f && IsAlive())
	{
		FTODamageInfo Info;
		Info.Damage = (FallSpeed - 1100.f) * 0.12f;
		Info.bFall = true;
		Info.Part = ETOBodyPart::LeftLeg;
		ReceiveTODamage(Info);
	}
}

void ATOCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	const float Dt = FMath::Min(DeltaTime, 0.1f);

	if (!bDeathHandled)
	{
		UpdateStatusChecks(Dt, Now);
	}
	if (!bDeathHandled)
	{
		UpdateMovement(Dt, Now);
		UpdateItemUse(Now);
		if (bThrowPending && Now - ThrowStart > 0.35f)
		{
			DoThrowGrenade();
		}
		UpdateFootsteps(Dt);
	}

	// Body animation inputs
	const FRotator CR = GetControlRotation();
	Body->AimPitch = FMath::Clamp(FRotator::NormalizeAxis(CR.Pitch), -70.f, 70.f);
	Body->Speed = GetVelocity().Size2D();
	Body->Stance = Stance;
	Body->Lean = LeanAlpha;
	Body->bSprinting = bSprinting;
	Body->bDowned = IsDowned();
	Body->bDead = IsDeadState();
	Body->bReloading = Weapons->IsReloading();
	Body->bThrowing = IsThrowing() || bUsingItem;
	Body->bInVehicle = CurrentVehicle.IsValid();
	Body->SetRelativeLocation(FVector(0.f, 0.f, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));

	if (bIsPlayerControlled)
	{
		UpdateCamera(Dt, Now);
		UpdateFocus(Now);
		UpdatePostProcess(Dt);
		for (int32 i = DamageIndicators.Num() - 1; i >= 0; --i)
		{
			if (Now - DamageIndicators[i].Time > 2.5f)
			{
				DamageIndicators.RemoveAt(i);
			}
		}
	}
}
