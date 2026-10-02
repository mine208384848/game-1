// TAC-OPS - procedural soldier body

#include "Characters/TOBodyRigComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "World/TOMaterialLibrary.h"

UTOBodyRigComponent::UTOBodyRigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetMobility(EComponentMobility::Movable);
}

USceneComponent* UTOBodyRigComponent::MakePivot(USceneComponent* Parent, const FVector& Loc, FName Name)
{
	USceneComponent* P = NewObject<USceneComponent>(GetOwner(), Name);
	P->SetMobility(EComponentMobility::Movable);
	P->SetupAttachment(Parent);
	P->SetRelativeLocation(Loc);
	P->RegisterComponent();
	Pivots.Add(P);
	return P;
}

UStaticMeshComponent* UTOBodyRigComponent::MakePart(USceneComponent* Parent, ETOShape Shape, ETOMat Mat, const FVector& Loc, const FVector& Size, ETOBodyPart Part, bool bHitbox, const FRotator& Rot)
{
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	if (!Lib)
	{
		return nullptr;
	}
	UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(GetOwner());
	M->SetStaticMesh(Lib->GetMesh(Shape));
	M->SetMaterial(0, Lib->GetMaterial(Mat));
	M->SetMobility(EComponentMobility::Movable);
	M->SetGenerateOverlapEvents(false);
	M->SetCanEverAffectNavigation(false);
	M->bReceivesDecals = false;
	M->SetCastShadow(true);
	M->bCastHiddenShadow = true;
	M->SetOwnerNoSee(bHideOwner);
	M->SetCullDistance(25000.f);
	if (bHitbox)
	{
		M->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		M->SetCollisionObjectType(ECC_Pawn);
		M->SetCollisionResponseToAllChannels(ECR_Ignore);
		M->SetCollisionResponseToChannel(TO_TRACE_BULLET, ECR_Block);
		PartMap.Add(TPair<TWeakObjectPtr<UPrimitiveComponent>, ETOBodyPart>(M, Part));
	}
	else
	{
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	M->SetupAttachment(Parent);
	M->SetRelativeLocation(Loc);
	M->SetRelativeRotation(Rot);
	M->SetRelativeScale3D(Size / 100.f);
	M->RegisterComponent();
	Parts.Add(M);
	return M;
}

void UTOBodyRigComponent::BuildRig(ETOMat SuitMat, bool bHideFromOwner, bool bBoss)
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	bHideOwner = bHideFromOwner;

	const float Bulk = bBoss ? 1.15f : 1.f;

	Hips = MakePivot(this, FVector(0.f, 0.f, 94.f), TEXT("Rig_Hips"));
	MakePart(Hips, ETOShape::Cube, SuitMat, FVector(0.f, 0.f, 4.f), FVector(22.f, 32.f, 20.f) * Bulk, ETOBodyPart::Stomach, true);

	ThighL = MakePivot(Hips, FVector(0.f, -10.f, 0.f), TEXT("Rig_ThighL"));
	ThighR = MakePivot(Hips, FVector(0.f, 10.f, 0.f), TEXT("Rig_ThighR"));
	MakePart(ThighL, ETOShape::Cube, SuitMat, FVector(0.f, 0.f, -23.f), FVector(15.f, 15.f, 46.f) * Bulk, ETOBodyPart::LeftLeg, true);
	MakePart(ThighR, ETOShape::Cube, SuitMat, FVector(0.f, 0.f, -23.f), FVector(15.f, 15.f, 46.f) * Bulk, ETOBodyPart::RightLeg, true);
	ShinL = MakePivot(ThighL, FVector(0.f, 0.f, -46.f), TEXT("Rig_ShinL"));
	ShinR = MakePivot(ThighR, FVector(0.f, 0.f, -46.f), TEXT("Rig_ShinR"));
	MakePart(ShinL, ETOShape::Cube, SuitMat, FVector(0.f, 0.f, -21.f), FVector(13.f, 13.f, 42.f), ETOBodyPart::LeftLeg, true);
	MakePart(ShinR, ETOShape::Cube, SuitMat, FVector(0.f, 0.f, -21.f), FVector(13.f, 13.f, 42.f), ETOBodyPart::RightLeg, true);
	MakePart(ShinL, ETOShape::Cube, ETOMat::Rubber, FVector(4.f, 0.f, -44.f), FVector(26.f, 12.f, 8.f), ETOBodyPart::LeftLeg, false);
	MakePart(ShinR, ETOShape::Cube, ETOMat::Rubber, FVector(4.f, 0.f, -44.f), FVector(26.f, 12.f, 8.f), ETOBodyPart::RightLeg, false);

	Spine = MakePivot(Hips, FVector(0.f, 0.f, 14.f), TEXT("Rig_Spine"));
	TorsoMesh = MakePart(Spine, ETOShape::Cube, SuitMat, FVector(0.f, 0.f, 24.f), FVector(24.f, 38.f, 46.f) * Bulk, ETOBodyPart::Thorax, true);
	VestMesh = MakePart(Spine, ETOShape::Cube, ETOMat::MetalGreen, FVector(1.f, 0.f, 26.f), FVector(30.f, 42.f, 38.f) * Bulk, ETOBodyPart::Thorax, true);
	RigMesh = MakePart(Spine, ETOShape::Cube, ETOMat::Canvas, FVector(15.f, 0.f, 16.f), FVector(10.f, 34.f, 14.f), ETOBodyPart::Stomach, false);
	BackpackMesh = MakePart(Spine, ETOShape::Cube, ETOMat::Canvas, FVector(-22.f, 0.f, 24.f), FVector(18.f, 32.f, 42.f), ETOBodyPart::Thorax, true);

	HeadPivot = MakePivot(Spine, FVector(0.f, 0.f, 50.f), TEXT("Rig_Head"));
	MakePart(HeadPivot, ETOShape::Cylinder, SuitMat, FVector(0.f, 0.f, 0.f), FVector(12.f, 12.f, 10.f), ETOBodyPart::Head, false);
	HeadMesh = MakePart(HeadPivot, ETOShape::Sphere, ETOMat::Skin, FVector(1.f, 0.f, 11.f), FVector(21.f, 20.f, 24.f), ETOBodyPart::Head, true);
	MakePart(HeadPivot, ETOShape::Cube, ETOMat::Black, FVector(9.5f, 0.f, 12.f), FVector(4.f, 15.f, 5.f), ETOBodyPart::Head, false);
	MakePart(HeadPivot, ETOShape::Cube, bBoss ? ETOMat::SuitRed : ETOMat::SuitBlack, FVector(6.f, 0.f, 5.f), FVector(10.f, 18.f, 8.f), ETOBodyPart::Head, false);
	HelmetMesh = MakePart(HeadPivot, ETOShape::Sphere, ETOMat::GunOlive, FVector(0.f, 0.f, 16.f), FVector(27.f, 26.f, 19.f), ETOBodyPart::Head, true);

	// Arms (placed every frame from shoulders to the weapon hand positions)
	UpperArmL = MakePart(Spine, ETOShape::Cube, SuitMat, FVector::ZeroVector, FVector(30.f, 11.f, 11.f), ETOBodyPart::LeftArm, true);
	UpperArmR = MakePart(Spine, ETOShape::Cube, SuitMat, FVector::ZeroVector, FVector(30.f, 11.f, 11.f), ETOBodyPart::RightArm, true);
	ForeArmL = MakePart(Spine, ETOShape::Cube, SuitMat, FVector::ZeroVector, FVector(28.f, 9.f, 9.f), ETOBodyPart::LeftArm, true);
	ForeArmR = MakePart(Spine, ETOShape::Cube, SuitMat, FVector::ZeroVector, FVector(28.f, 9.f, 9.f), ETOBodyPart::RightArm, true);
	HandL = MakePart(Spine, ETOShape::Cube, ETOMat::Black, FVector::ZeroVector, FVector(9.f, 8.f, 7.f), ETOBodyPart::LeftArm, false);
	HandR = MakePart(Spine, ETOShape::Cube, ETOMat::Black, FVector::ZeroVector, FVector(9.f, 8.f, 7.f), ETOBodyPart::RightArm, false);

	WeaponSocket = MakePivot(Spine, FVector(16.f, 8.f, 36.f), TEXT("Rig_WeaponSocket"));

	UpdateGear(0, 0, false, false);
}

void UTOBodyRigComponent::UpdateGear(int32 HelmetLevel, int32 ArmorLevel, bool bHasRig, bool bHasBackpack)
{
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	if (HelmetMesh)
	{
		HelmetMesh->SetVisibility(HelmetLevel > 0);
		HelmetMesh->SetCollisionEnabled(HelmetLevel > 0 ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		if (Lib && HelmetLevel > 0)
		{
			const ETOMat M = HelmetLevel >= 5 ? ETOMat::MetalDark : (HelmetLevel >= 3 ? ETOMat::GunOlive : ETOMat::PlasticTan);
			HelmetMesh->SetMaterial(0, Lib->GetMaterial(M));
		}
	}
	if (VestMesh)
	{
		VestMesh->SetVisibility(ArmorLevel > 0);
		VestMesh->SetCollisionEnabled(ArmorLevel > 0 ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		if (Lib && ArmorLevel > 0)
		{
			const ETOMat M = ArmorLevel >= 5 ? ETOMat::MetalDark : (ArmorLevel >= 3 ? ETOMat::MetalGreen : ETOMat::Canvas);
			VestMesh->SetMaterial(0, Lib->GetMaterial(M));
			const float Thick = 26.f + ArmorLevel * 1.5f;
			VestMesh->SetRelativeScale3D(FVector(Thick, 41.f + ArmorLevel, 36.f + ArmorLevel) / 100.f);
		}
	}
	if (RigMesh)
	{
		RigMesh->SetVisibility(bHasRig);
	}
	if (BackpackMesh)
	{
		BackpackMesh->SetVisibility(bHasBackpack);
		BackpackMesh->SetCollisionEnabled(bHasBackpack ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void UTOBodyRigComponent::SetWeapon(const FTOWeaponConfig* Config)
{
	for (UStaticMeshComponent* P : WeaponParts)
	{
		if (IsValid(P))
		{
			P->DestroyComponent();
		}
	}
	WeaponParts.Reset();
	Gun = FTOGunVisual();
	if (!Config || !Config->IsValid() || !WeaponSocket)
	{
		return;
	}
	TOWeaponVisuals::BuildGun(GetOwner(), WeaponSocket, *Config, false, bHideOwner, Gun);
	for (UStaticMeshComponent* P : Gun.Parts)
	{
		WeaponParts.Add(P);
	}
}

ETOBodyPart UTOBodyRigComponent::GetPartFromComponent(const UPrimitiveComponent* Component) const
{
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, ETOBodyPart>& Pair : PartMap)
	{
		if (Pair.Key.Get() == Component)
		{
			return Pair.Value;
		}
	}
	return ETOBodyPart::None;
}

bool UTOBodyRigComponent::IsRigComponent(const UPrimitiveComponent* Component) const
{
	return GetPartFromComponent(Component) != ETOBodyPart::None;
}

FVector UTOBodyRigComponent::GetHeadWorld() const
{
	return HeadMesh ? HeadMesh->GetComponentLocation() : GetComponentLocation() + FVector(0.f, 0.f, 168.f);
}

FVector UTOBodyRigComponent::GetChestWorld() const
{
	return TorsoMesh ? TorsoMesh->GetComponentLocation() : GetComponentLocation() + FVector(0.f, 0.f, 130.f);
}

FVector UTOBodyRigComponent::GetMuzzleWorld() const
{
	if (WeaponSocket)
	{
		return WeaponSocket->GetComponentTransform().TransformPosition(Gun.MuzzleLocal);
	}
	return GetChestWorld();
}

FVector UTOBodyRigComponent::GetWeaponForward() const
{
	return WeaponSocket ? WeaponSocket->GetForwardVector() : GetForwardVector();
}

FTransform UTOBodyRigComponent::GetWeaponLightTransform() const
{
	if (WeaponSocket)
	{
		const FTransform T = WeaponSocket->GetComponentTransform();
		return FTransform(T.GetRotation(), T.TransformPosition(Gun.LightLocal));
	}
	return GetComponentTransform();
}

void UTOBodyRigComponent::SetHitboxesEnabled(bool bEnabled)
{
	for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, ETOBodyPart>& Pair : PartMap)
	{
		if (UPrimitiveComponent* C = Pair.Key.Get())
		{
			if (!bEnabled)
			{
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			else if (C->IsVisible())
			{
				C->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			}
		}
	}
}

void UTOBodyRigComponent::PlaceLimb(UStaticMeshComponent* Limb, const FVector& From, const FVector& To, float Thickness)
{
	if (!Limb)
	{
		return;
	}
	const FVector Dir = To - From;
	const float Len = FMath::Max(Dir.Size(), 1.f);
	Limb->SetRelativeLocationAndRotation((From + To) * 0.5f, Dir.Rotation());
	Limb->SetRelativeScale3D(FVector(Len, Thickness, Thickness) / 100.f);
}

void UTOBodyRigComponent::UpdateArms()
{
	if (!Spine || !WeaponSocket)
	{
		return;
	}
	const FTransform SocketRel = WeaponSocket->GetRelativeTransform();
	FVector HandLLocal = FVector(34.f, -4.f, 30.f);
	FVector HandRLocal = FVector(14.f, 10.f, 28.f);
	if (Gun.Parts.Num() > 0)
	{
		HandLLocal = SocketRel.TransformPosition(Gun.LeftHandLocal);
		HandRLocal = SocketRel.TransformPosition(Gun.RightHandLocal);
	}
	const FVector ShoulderL(2.f, -20.f, 42.f);
	const FVector ShoulderR(2.f, 20.f, 42.f);
	const FVector ElbowL = (ShoulderL + HandLLocal) * 0.5f + FVector(-3.f, -9.f, -12.f);
	const FVector ElbowR = (ShoulderR + HandRLocal) * 0.5f + FVector(-3.f, 10.f, -12.f);
	PlaceLimb(UpperArmL, ShoulderL, ElbowL, 11.f);
	PlaceLimb(UpperArmR, ShoulderR, ElbowR, 11.f);
	PlaceLimb(ForeArmL, ElbowL, HandLLocal, 9.f);
	PlaceLimb(ForeArmR, ElbowR, HandRLocal, 9.f);
	if (HandL) HandL->SetRelativeLocation(HandLLocal);
	if (HandR) HandR->SetRelativeLocation(HandRLocal);
}

void UTOBodyRigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bBuilt || !Hips)
	{
		return;
	}

	// Cheap LOD: far away soldiers animate at a lower rate.
	if (const UWorld* World = GetWorld())
	{
		if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(World, 0))
		{
			const float Dist = FVector::Dist(Cam->GetCameraLocation(), GetComponentLocation());
			const float Interval = Dist > 15000.f ? 0.25f : (Dist > 6000.f ? 0.1f : 0.f);
			if (!FMath::IsNearlyEqual(PrimaryComponentTick.TickInterval, Interval))
			{
				SetComponentTickInterval(Interval);
			}
		}
	}

	const float Dt = FMath::Min(DeltaTime, 0.25f);
	const float Blend = FMath::Clamp(Dt * 10.f, 0.f, 1.f);

	// Targets ---------------------------------------------------------------------------------
	float tHipsZ = 94.f, tHipsPitch = 0.f, tHipsRoll = 0.f;
	float tThighL = 0.f, tThighR = 0.f, tShinL = 0.f, tShinR = 0.f;
	float tSpinePitch = 0.f, tSpineRoll = 0.f, tHeadPitch = 0.f;
	float tSockPitch = 0.f, tSockYaw = 0.f, tSockRoll = 0.f;

	const float SpeedN = FMath::Clamp(Speed / 600.f, 0.f, 1.3f);
	WalkPhase += Dt * (4.f + 7.f * SpeedN) * (Speed > 20.f ? 1.f : 0.f);
	const float Swing = FMath::Sin(WalkPhase);
	const float Swing2 = FMath::Sin(WalkPhase + PI * 0.5f);

	if (bDead)
	{
		tHipsZ = 14.f;
		tHipsPitch = 84.f;
		tHipsRoll = DeathRoll;
		tThighL = -12.f; tThighR = 8.f; tShinL = -10.f; tShinR = -25.f;
		tSpinePitch = 4.f;
		tHeadPitch = 20.f;
		tSockPitch = 30.f;
		tSockYaw = 50.f;
	}
	else if (bDowned)
	{
		tHipsZ = 22.f;
		tHipsPitch = -72.f;
		tHipsRoll = 25.f;
		tThighL = -10.f; tThighR = 15.f; tShinL = -30.f; tShinR = -40.f;
		tSpinePitch = -8.f;
		tHeadPitch = 55.f;
		tSockPitch = 80.f;
		tSockYaw = 25.f;
	}
	else if (bInVehicle)
	{
		tHipsZ = 60.f;
		tThighL = 80.f; tThighR = 80.f; tShinL = -80.f; tShinR = -80.f;
	}
	else
	{
		switch (Stance)
		{
		case ETOStance::Crouch:
			tHipsZ = 60.f;
			tThighL = 72.f + Swing * 12.f * SpeedN;
			tThighR = 40.f - Swing * 12.f * SpeedN;
			tShinL = -78.f;
			tShinR = -95.f;
			tSpinePitch = AimPitch * 0.4f + 8.f;
			break;
		case ETOStance::Prone:
			tHipsZ = 16.f;
			tHipsPitch = -82.f;
			tThighL = Swing * 8.f * SpeedN;
			tThighR = -Swing * 8.f * SpeedN;
			tShinL = -5.f;
			tShinR = -5.f;
			tSpinePitch = 6.f + AimPitch * 0.2f;
			tHeadPitch = 60.f;
			break;
		default:
			tHipsZ = 94.f - (bSprinting ? 4.f : 0.f) + FMath::Abs(Swing) * 2.f * SpeedN;
			tThighL = Swing * 30.f * SpeedN;
			tThighR = -Swing * 30.f * SpeedN;
			tShinL = -FMath::Max(0.f, Swing2) * 40.f * SpeedN;
			tShinR = -FMath::Max(0.f, -Swing2) * 40.f * SpeedN;
			tSpinePitch = AimPitch * 0.45f - (bSprinting ? 10.f : 0.f);
			break;
		}
		tSpineRoll = Lean * 18.f;
		if (Stance != ETOStance::Prone)
		{
			tHeadPitch = AimPitch * 0.3f;
		}

		// Weapon socket keeps the gun on the aim line regardless of body pose.
		tSockPitch = AimPitch - (tHipsPitch + tSpinePitch);
		if (bSprinting)
		{
			tSockPitch = -35.f - tHipsPitch;
			tSockYaw = 30.f;
		}
		if (bReloading)
		{
			tSockPitch -= 20.f;
			tSockRoll = -25.f;
		}
		if (bThrowing)
		{
			tSockPitch = -50.f;
		}
	}

	// Flinch & recoil
	tSpinePitch -= Flinch * 10.f;
	tSpineRoll += Flinch * 6.f;
	Flinch = FMath::Max(0.f, Flinch - Dt * 4.f);
	tSockPitch += FireKick * 3.f;
	FireKick = FMath::Max(0.f, FireKick - Dt * 12.f);

	// Smooth -----------------------------------------------------------------------------------
	const float PoseBlend = (bDead || bDowned) ? FMath::Clamp(Dt * 5.f, 0.f, 1.f) : Blend;
	HipsZ = FMath::Lerp(HipsZ, tHipsZ, PoseBlend);
	HipsPitch = FMath::Lerp(HipsPitch, tHipsPitch, PoseBlend);
	HipsRoll = FMath::Lerp(HipsRoll, tHipsRoll, PoseBlend);
	ThighLP = FMath::Lerp(ThighLP, tThighL, Blend);
	ThighRP = FMath::Lerp(ThighRP, tThighR, Blend);
	ShinLP = FMath::Lerp(ShinLP, tShinL, Blend);
	ShinRP = FMath::Lerp(ShinRP, tShinR, Blend);
	SpinePitch = FMath::Lerp(SpinePitch, tSpinePitch, Blend);
	SpineRoll = FMath::Lerp(SpineRoll, tSpineRoll, Blend);
	HeadPitch = FMath::Lerp(HeadPitch, tHeadPitch, Blend);
	SocketPitch = FMath::Lerp(SocketPitch, tSockPitch, FMath::Clamp(Dt * 18.f, 0.f, 1.f));
	SocketYaw = FMath::Lerp(SocketYaw, tSockYaw, Blend);
	SocketRoll = FMath::Lerp(SocketRoll, tSockRoll, Blend);

	if (bDead && FMath::IsNearlyZero(DeathRoll))
	{
		DeathRoll = FMath::FRandRange(-25.f, 25.f) + 0.01f;
	}

	// Apply ------------------------------------------------------------------------------------
	Hips->SetRelativeLocationAndRotation(FVector(0.f, 0.f, HipsZ), FRotator(HipsPitch, 0.f, HipsRoll));
	ThighL->SetRelativeRotation(FRotator(ThighLP, 0.f, bDead ? -8.f : 0.f));
	ThighR->SetRelativeRotation(FRotator(ThighRP, 0.f, bDead ? 8.f : 0.f));
	ShinL->SetRelativeRotation(FRotator(ShinLP, 0.f, 0.f));
	ShinR->SetRelativeRotation(FRotator(ShinRP, 0.f, 0.f));
	Spine->SetRelativeRotation(FRotator(SpinePitch, 0.f, SpineRoll));
	HeadPivot->SetRelativeRotation(FRotator(HeadPitch, 0.f, 0.f));
	WeaponSocket->SetRelativeLocationAndRotation(FVector(16.f - FireKick * 2.f, 8.f, 36.f), FRotator(SocketPitch, SocketYaw, SocketRoll));

	UpdateArms();
}
