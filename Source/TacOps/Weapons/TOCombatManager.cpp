// TAC-OPS - combat services

#include "Weapons/TOCombatManager.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOBodyRigComponent.h"
#include "Core/TOGameMode.h"
#include "Core/TOInterfaces.h"
#include "Audio/TOAudio.h"
#include "World/TOMaterialLibrary.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"

namespace
{
	enum EPool : int32
	{
		PoolTracer = 0,
		PoolHole = 1,
		PoolDust = 2,
		PoolSpark = 3,
		PoolBlood = 4,
		PoolMuzzle = 5,
		PoolFireball = 6,
		PoolCount = 7
	};

	const FVector HiddenLocation(0.f, 0.f, -200000.f);
	const FVector HiddenScale(0.001f, 0.001f, 0.001f);

	TWeakObjectPtr<ATOCombatManager> GCachedManager;
}

ATOCombatManager::ATOCombatManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

ATOCombatManager* ATOCombatManager::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	if (GCachedManager.IsValid() && GCachedManager->GetWorld() == World)
	{
		return GCachedManager.Get();
	}
	for (TActorIterator<ATOCombatManager> It(World); It; ++It)
	{
		GCachedManager = *It;
		return *It;
	}
	return nullptr;
}

UInstancedStaticMeshComponent* ATOCombatManager::MakePool(int32 Count, int32 ShapeIndex, int32 MaterialIndex, bool bShadow)
{
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	UInstancedStaticMeshComponent* ISM = NewObject<UInstancedStaticMeshComponent>(this);
	ISM->SetMobility(EComponentMobility::Movable);
	ISM->SetupAttachment(RootComponent);
	if (Lib)
	{
		ISM->SetStaticMesh(Lib->GetMesh((ETOShape)ShapeIndex));
		ISM->SetMaterial(0, Lib->GetMaterial((ETOMat)MaterialIndex));
	}
	ISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM->SetCastShadow(bShadow);
	ISM->SetCanEverAffectNavigation(false);
	ISM->SetGenerateOverlapEvents(false);
	ISM->RegisterComponent();
	TArray<FTransform> Transforms;
	Transforms.Init(FTransform(FRotator::ZeroRotator, HiddenLocation, HiddenScale), Count);
	ISM->AddInstances(Transforms, false);
	return ISM;
}

void ATOCombatManager::BeginPlay()
{
	Super::BeginPlay();
	GCachedManager = this;
	SetActorLocation(FVector::ZeroVector);

	Audio = NewObject<UTOAudio>(this);
	Audio->Init(GetWorld());

	struct FPoolDef { int32 Count; ETOShape Shape; ETOMat Mat; bool bShadow; };
	const FPoolDef Defs[PoolCount] =
	{
		{ 160, ETOShape::Cylinder, ETOMat::TracerOrange, false },
		{ 300, ETOShape::Cube, ETOMat::Black, false },
		{ 80, ETOShape::Sphere, ETOMat::Sand, false },
		{ 64, ETOShape::Sphere, ETOMat::LampWarm, false },
		{ 64, ETOShape::Sphere, ETOMat::Blood, false },
		{ 40, ETOShape::Sphere, ETOMat::Flash, false },
		{ 10, ETOShape::Sphere, ETOMat::Flash, false }
	};
	int32 Total = 0;
	for (int32 p = 0; p < PoolCount; ++p)
	{
		Pools.Add(MakePool(Defs[p].Count, (int32)Defs[p].Shape, (int32)Defs[p].Mat, Defs[p].bShadow));
		PoolSizes.Add(Defs[p].Count);
		PoolCursor.Add(0);
		PoolDirty.Add(false);
		Total += Defs[p].Count;
	}
	Fx.SetNum(Total);
	int32 Offset = 0;
	for (int32 p = 0; p < PoolCount; ++p)
	{
		for (int32 i = 0; i < PoolSizes[p]; ++i)
		{
			Fx[Offset + i].Pool = p;
			Fx[Offset + i].Index = i;
		}
		Offset += PoolSizes[p];
	}

	// Smoke puffs (block AI line of sight on the Visibility channel)
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	for (int32 i = 0; i < 72; ++i)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
		M->SetMobility(EComponentMobility::Movable);
		M->SetupAttachment(RootComponent);
		if (Lib)
		{
			M->SetStaticMesh(Lib->GetMesh(ETOShape::Sphere));
			M->SetMaterial(0, Lib->GetMaterial(ETOMat::Smoke));
		}
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->SetCollisionResponseToAllChannels(ECR_Ignore);
		M->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		M->SetCastShadow(true);
		M->SetCanEverAffectNavigation(false);
		M->SetVisibility(false);
		M->SetWorldLocation(HiddenLocation);
		M->RegisterComponent();
		SmokeMeshes.Add(M);
		FTOSmokePuff P;
		P.Mesh = M;
		P.Life = -1.f;
		Smoke.Add(P);
	}

	for (int32 i = 0; i < 10; ++i)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetMobility(EComponentMobility::Movable);
		L->SetupAttachment(RootComponent);
		L->SetIntensity(0.f);
		L->SetCastShadows(false);
		L->SetVisibility(false);
		L->RegisterComponent();
		Lights.Add(L);
		FTOLightFlash F;
		F.Light = L;
		F.Life = -1.f;
		LightFlashes.Add(F);
	}
}

void ATOCombatManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const float Dt = FMath::Min(DeltaTime, 0.1f);
	Time += Dt;
	SimulateBullets(Dt);
	UpdateFx(Dt);
	if (Audio)
	{
		Audio->Tick(DeltaTime);
	}
}

// ---------------------------------------------------------------------------------------------
//  Ballistics
// ---------------------------------------------------------------------------------------------

void ATOCombatManager::FireBullet(const FTOBulletSpawn& Spawn)
{
	FTOBullet B;
	B.Spawn = Spawn;
	B.Pos = Spawn.Origin;
	B.Vel = Spawn.Velocity;
	B.VisualOffset = Spawn.VisualOrigin - Spawn.Origin;
	if (Spawn.bTracer)
	{
		B.TracerIndex = TracerCursor;
		TracerCursor = (TracerCursor + 1) % PoolSizes[PoolTracer];
		// If another bullet owned this tracer slot, steal it.
		for (FTOBullet& Other : Bullets)
		{
			if (Other.TracerIndex == B.TracerIndex)
			{
				Other.TracerIndex = INDEX_NONE;
			}
		}
	}
	Bullets.Add(B);
}

FName ATOCombatManager::SurfaceOf(const FHitResult& Hit) const
{
	const UPrimitiveComponent* C = Hit.GetComponent();
	if (C && C->ComponentTags.Num() > 0)
	{
		return C->ComponentTags[0];
	}
	return FName(TEXT("Concrete"));
}

bool ATOCombatManager::HandleHit(FTOBullet& Bullet, const FHitResult& Hit)
{
	AActor* HitActor = Hit.GetActor();
	const float DistCm = FVector::Dist(Bullet.Spawn.Origin, Hit.ImpactPoint);
	float Damage = Bullet.Spawn.Damage;
	if (DistCm > Bullet.Spawn.EffectiveRange)
	{
		const float Over = (DistCm - Bullet.Spawn.EffectiveRange) / FMath::Max(1.f, Bullet.Spawn.EffectiveRange);
		Damage *= FMath::Clamp(1.f - 0.4f * Over, 0.5f, 1.f);
	}

	FTODamageInfo Info;
	Info.Damage = Damage;
	Info.PenLevel = Bullet.Spawn.PenLevel;
	Info.ArmorDamage = Bullet.Spawn.ArmorDamage;
	Info.HitLocation = Hit.ImpactPoint;
	Info.HitDirection = Bullet.Vel.GetSafeNormal();
	Info.Instigator = Bullet.Spawn.Instigator;
	Info.InstigatorTeam = Bullet.Spawn.Team;
	Info.WeaponId = Bullet.Spawn.WeaponId;
	Info.Distance = DistCm / 100.f;

	if (ATOCharacter* Char = Cast<ATOCharacter>(HitActor))
	{
		ETOBodyPart Part = Char->GetBody() ? Char->GetBody()->GetPartFromComponent(Hit.GetComponent()) : ETOBodyPart::Thorax;
		if (Part == ETOBodyPart::None)
		{
			Part = ETOBodyPart::Thorax;
		}
		Info.Part = Part;
		Char->ReceiveTODamage(Info);
		BloodEffect(Hit.ImpactPoint, Info.HitDirection);
		return true;
	}

	if (HitActor && HitActor->GetClass()->ImplementsInterface(UTODamageable::StaticClass()))
	{
		if (ITODamageable* D = Cast<ITODamageable>(HitActor))
		{
			D->ReceiveTODamage(Info);
		}
	}

	const FName Surface = SurfaceOf(Hit);
	ImpactEffect(Hit.ImpactPoint, Hit.ImpactNormal, Surface);
	return true;
}

void ATOCombatManager::SimulateBullets(float Dt)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Gravity(0.f, 0.f, -980.f);

	ATOCharacter* LocalPlayer = Cast<ATOCharacter>(UGameplayStatics::GetPlayerPawn(World, 0));
	FVector ListenerLoc = FVector::ZeroVector;
	if (LocalPlayer && LocalPlayer->GetCamera())
	{
		ListenerLoc = LocalPlayer->GetCamera()->GetComponentLocation();
	}

	UInstancedStaticMeshComponent* TracerPool = Pools.IsValidIndex(PoolTracer) ? Pools[PoolTracer].Get() : nullptr;
	bool bTracersDirty = false;

	for (int32 i = Bullets.Num() - 1; i >= 0; --i)
	{
		FTOBullet& B = Bullets[i];
		const FVector NewVel = B.Vel + Gravity * Dt;
		const FVector NewPos = B.Pos + (B.Vel + NewVel) * 0.5f * Dt;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(TOBullet), false);
		if (AActor* Inst = B.Spawn.Instigator.Get())
		{
			Params.AddIgnoredActor(Inst);
		}
		FHitResult Hit;
		bool bDone = false;
		if (World->LineTraceSingleByChannel(Hit, B.Pos, NewPos, TO_TRACE_BULLET, Params))
		{
			bDone = HandleHit(B, Hit);
		}

		// Near miss: whiz + suppression on the local player
		if (!bDone && LocalPlayer && !B.bWhizzed && LocalPlayer->IsAlive() && B.Spawn.Instigator.Get() != LocalPlayer && TOUtil::IsHostile(B.Spawn.Team, LocalPlayer->TeamId))
		{
			const FVector Closest = FMath::ClosestPointOnSegment(ListenerLoc, B.Pos, NewPos);
			const float D = FVector::Dist(Closest, ListenerLoc);
			if (D < 260.f)
			{
				B.bWhizzed = true;
				if (Audio)
				{
					Audio->Play(ETOSound::Whiz, Closest, 0.8f, FMath::FRandRange(0.9f, 1.15f));
				}
				LocalPlayer->ApplySuppression(FMath::Clamp(1.f - D / 260.f, 0.25f, 1.f) * 0.35f, B.Spawn.Origin);
			}
		}

		B.Travelled += FVector::Dist(B.Pos, NewPos);
		B.Pos = NewPos;
		B.Vel = NewVel;
		B.Age += Dt;
		B.VisualOffset *= FMath::Exp(-Dt * 14.f);

		if (bDone || B.Age > 3.5f || B.Travelled > 180000.f || B.Pos.Z < -50000.f)
		{
			if (B.TracerIndex != INDEX_NONE && TracerPool)
			{
				TracerPool->UpdateInstanceTransform(B.TracerIndex, FTransform(FRotator::ZeroRotator, HiddenLocation, HiddenScale), false, false, true);
				bTracersDirty = true;
			}
			Bullets.RemoveAtSwap(i);
			continue;
		}

		if (B.TracerIndex != INDEX_NONE && TracerPool)
		{
			const FVector Dir = B.Vel.GetSafeNormal();
			const float Len = FMath::Clamp(B.Vel.Size() * 0.018f, 80.f, 320.f);
			const FVector Center = B.Pos + B.VisualOffset - Dir * Len * 0.5f;
			const FQuat Q = FRotationMatrix::MakeFromZ(Dir).ToQuat();
			// Tracers grow a little with distance so they stay readable.
			const float Thick = 0.012f + FMath::Min(B.Travelled / 100000.f, 1.f) * 0.03f;
			TracerPool->UpdateInstanceTransform(B.TracerIndex, FTransform(Q, Center, FVector(Thick, Thick, Len / 100.f)), false, false, true);
			bTracersDirty = true;
		}
	}

	if (bTracersDirty && TracerPool)
	{
		TracerPool->MarkRenderStateDirty();
	}
}

// ---------------------------------------------------------------------------------------------
//  Effects
// ---------------------------------------------------------------------------------------------

int32 ATOCombatManager::AllocFx(int32 Pool)
{
	if (!PoolSizes.IsValidIndex(Pool))
	{
		return INDEX_NONE;
	}
	int32 Offset = 0;
	for (int32 p = 0; p < Pool; ++p)
	{
		Offset += PoolSizes[p];
	}
	const int32 Idx = PoolCursor[Pool];
	PoolCursor[Pool] = (Idx + 1) % PoolSizes[Pool];
	return Offset + Idx;
}

void ATOCombatManager::SetInstance(int32 Pool, int32 Index, const FTransform& T)
{
	if (Pools.IsValidIndex(Pool) && Pools[Pool])
	{
		Pools[Pool]->UpdateInstanceTransform(Index, T, false, false, true);
		PoolDirty[Pool] = true;
	}
}

void ATOCombatManager::ImpactEffect(const FVector& Location, const FVector& Normal, FName Surface)
{
	const bool bWater = Surface == FName(TEXT("Water"));
	const bool bMetal = Surface == FName(TEXT("Metal"));
	const bool bFoliage = Surface == FName(TEXT("Foliage"));

	// Bullet hole
	if (!bWater && !bFoliage)
	{
		const int32 H = AllocFx(PoolHole);
		if (Fx.IsValidIndex(H))
		{
			FTOFxInstance& F = Fx[H];
			F.bActive = true;
			F.Start = Time;
			F.Life = 40.f;
			F.Location = Location + Normal * 0.4f;
			F.Rotation = FRotationMatrix::MakeFromZ(Normal).Rotator();
			F.Scale0 = F.Scale1 = FMath::FRandRange(0.05f, 0.08f);
			SetInstance(PoolHole, F.Index, FTransform(F.Rotation, F.Location, FVector(F.Scale0, F.Scale0, 0.004f)));
		}
	}

	// Dust / splash puffs
	const int32 Count = bWater ? 3 : 2;
	for (int32 k = 0; k < Count; ++k)
	{
		const int32 D = AllocFx(PoolDust);
		if (Fx.IsValidIndex(D))
		{
			FTOFxInstance& F = Fx[D];
			F.bActive = true;
			F.Start = Time;
			F.Life = bWater ? 0.6f : 0.45f;
			F.Location = Location + Normal * 4.f;
			F.Velocity = (Normal + FMath::VRand() * 0.5f).GetSafeNormal() * (bWater ? 260.f : 140.f) + FVector(0.f, 0.f, bWater ? 200.f : 20.f);
			F.Scale0 = 0.06f;
			F.Scale1 = bWater ? 0.35f : 0.45f;
		}
	}

	if (bMetal)
	{
		for (int32 k = 0; k < 4; ++k)
		{
			const int32 S = AllocFx(PoolSpark);
			if (Fx.IsValidIndex(S))
			{
				FTOFxInstance& F = Fx[S];
				F.bActive = true;
				F.Start = Time;
				F.Life = 0.18f;
				F.Location = Location + Normal * 2.f;
				F.Velocity = (Normal + FMath::VRand() * 0.8f).GetSafeNormal() * FMath::FRandRange(400.f, 900.f);
				F.Scale0 = 0.035f;
				F.Scale1 = 0.01f;
			}
		}
	}

	if (Audio && FMath::FRand() < 0.6f)
	{
		Audio->Play(ETOSound::Impact, Location, bMetal ? 0.7f : 0.45f, bMetal ? 1.4f : 1.f);
	}
}

void ATOCombatManager::BloodEffect(const FVector& Location, const FVector& Direction)
{
	for (int32 k = 0; k < 3; ++k)
	{
		const int32 B = AllocFx(PoolBlood);
		if (Fx.IsValidIndex(B))
		{
			FTOFxInstance& F = Fx[B];
			F.bActive = true;
			F.Start = Time;
			F.Life = 0.35f;
			F.Location = Location;
			F.Velocity = (Direction + FMath::VRand() * 0.6f).GetSafeNormal() * FMath::FRandRange(150.f, 350.f);
			F.Scale0 = 0.05f;
			F.Scale1 = 0.18f;
		}
	}
}

void ATOCombatManager::MuzzleFlash(const FVector& Location, const FRotator& Rotation, bool bSuppressed)
{
	if (bSuppressed)
	{
		return;
	}
	const int32 M = AllocFx(PoolMuzzle);
	if (Fx.IsValidIndex(M))
	{
		FTOFxInstance& F = Fx[M];
		F.bActive = true;
		F.Start = Time;
		F.Life = 0.05f;
		F.Location = Location + Rotation.Vector() * 8.f;
		F.Rotation = Rotation;
		F.Scale0 = 0.18f;
		F.Scale1 = 0.12f;
	}
	LightFlash(Location, FLinearColor(1.f, 0.7f, 0.35f), 6000.f, 800.f, 0.05f);
}

void ATOCombatManager::LightFlash(const FVector& Location, const FLinearColor& Color, float Intensity, float Radius, float Duration)
{
	if (LightFlashes.Num() == 0)
	{
		return;
	}
	// Only spend dynamic lights near the player.
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
	{
		if (FVector::DistSquared(Cam->GetCameraLocation(), Location) > FMath::Square(9000.f) && Intensity < 20000.f)
		{
			return;
		}
	}
	FTOLightFlash& F = LightFlashes[LightCursor];
	LightCursor = (LightCursor + 1) % LightFlashes.Num();
	if (UPointLightComponent* L = F.Light.Get())
	{
		L->SetWorldLocation(Location);
		L->SetLightColor(Color);
		L->SetAttenuationRadius(Radius);
		L->SetIntensity(Intensity);
		L->SetVisibility(true);
	}
	F.Start = Time;
	F.Life = Duration;
	F.Intensity = Intensity;
}

void ATOCombatManager::Explode(const FVector& Location, float Radius, float Damage, AActor* Instigator, int32 Team, FName WeaponId)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Damage characters with line of sight falloff
	for (TActorIterator<ATOCharacter> It(World); It; ++It)
	{
		ATOCharacter* C = *It;
		if (!C || C->IsDeadState())
		{
			continue;
		}
		const FVector Chest = C->GetChestLocation();
		const float Dist = FVector::Dist(Chest, Location);
		if (Dist > Radius)
		{
			// Still shake the player's camera from afar.
			if (C->IsPlayerCharacter() && Dist < Radius * 5.f)
			{
				C->AddCameraShake(0.6f * (1.f - Dist / (Radius * 5.f)));
			}
			continue;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TOExplosionLOS), false);
		Params.AddIgnoredActor(C);
		if (Instigator)
		{
			Params.AddIgnoredActor(Instigator);
		}
		FHitResult Hit;
		const bool bBlocked = World->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 25.f), Chest, ECC_WorldStatic, Params);
		float Dmg = Damage * FMath::Pow(1.f - FMath::Clamp(Dist / Radius, 0.f, 1.f), 1.2f);
		if (bBlocked)
		{
			Dmg *= 0.2f;
		}
		if (Dmg < 1.f)
		{
			continue;
		}
		FTODamageInfo Info;
		Info.Damage = Dmg;
		Info.bExplosive = true;
		Info.Part = ETOBodyPart::Thorax;
		Info.HitLocation = Chest;
		Info.HitDirection = (Chest - Location).GetSafeNormal();
		Info.Instigator = Instigator;
		Info.InstigatorTeam = Team;
		Info.WeaponId = WeaponId;
		Info.PenLevel = 4;
		Info.ArmorDamage = 1.f;
		Info.Distance = Dist / 100.f;
		C->ReceiveTODamage(Info);
		if (C->IsPlayerCharacter())
		{
			C->AddCameraShake(1.2f);
		}
	}

	// Other damageables (shields, barrels, vehicles)
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* A = *It;
		if (!A || A->IsA(ATOCharacter::StaticClass()) || !A->GetClass()->ImplementsInterface(UTODamageable::StaticClass()))
		{
			continue;
		}
		const float Dist = FVector::Dist(A->GetActorLocation(), Location);
		if (Dist > Radius * 1.2f)
		{
			continue;
		}
		if (ITODamageable* D = Cast<ITODamageable>(A))
		{
			FTODamageInfo Info;
			Info.Damage = Damage * (1.f - FMath::Clamp(Dist / (Radius * 1.2f), 0.f, 1.f));
			Info.bExplosive = true;
			Info.Instigator = Instigator;
			Info.InstigatorTeam = Team;
			Info.WeaponId = WeaponId;
			Info.HitLocation = A->GetActorLocation();
			D->ReceiveTODamage(Info);
		}
	}

	// Visuals
	const int32 FB = AllocFx(PoolFireball);
	if (Fx.IsValidIndex(FB))
	{
		FTOFxInstance& F = Fx[FB];
		F.bActive = true;
		F.Start = Time;
		F.Life = 0.45f;
		F.Location = Location + FVector(0.f, 0.f, 60.f);
		F.Scale0 = 0.6f;
		F.Scale1 = Radius / 90.f;
	}
	for (int32 k = 0; k < 10; ++k)
	{
		const int32 S = AllocFx(PoolSpark);
		if (Fx.IsValidIndex(S))
		{
			FTOFxInstance& F = Fx[S];
			F.bActive = true;
			F.Start = Time;
			F.Life = 0.5f;
			F.Location = Location + FVector(0.f, 0.f, 30.f);
			F.Velocity = (FMath::VRand() + FVector(0.f, 0.f, 0.8f)).GetSafeNormal() * FMath::FRandRange(600.f, 1400.f);
			F.Scale0 = 0.08f;
			F.Scale1 = 0.02f;
		}
	}
	for (int32 k = 0; k < 6; ++k)
	{
		const int32 D = AllocFx(PoolDust);
		if (Fx.IsValidIndex(D))
		{
			FTOFxInstance& F = Fx[D];
			F.bActive = true;
			F.Start = Time;
			F.Life = 1.2f;
			F.Location = Location + FVector(0.f, 0.f, 40.f);
			F.Velocity = (FMath::VRand() + FVector(0.f, 0.f, 1.f)).GetSafeNormal() * FMath::FRandRange(150.f, 400.f);
			F.Scale0 = 0.4f;
			F.Scale1 = 2.2f;
		}
	}
	SpawnSmoke(Location + FVector(0.f, 0.f, 100.f), false, 5.f, 3);
	LightFlash(Location + FVector(0.f, 0.f, 120.f), FLinearColor(1.f, 0.6f, 0.25f), 120000.f, Radius * 4.f, 0.35f);

	if (Audio)
	{
		Audio->Play(ETOSound::Explosion, Location, 1.f);
	}
	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		GM->ReportNoise(Location, 30000.f, Cast<ATOCharacter>(Instigator), true);
	}
}

void ATOCombatManager::SpawnSmoke(const FVector& Location, bool bGreen, float Duration, int32 Puffs)
{
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	for (int32 k = 0; k < Puffs; ++k)
	{
		FTOSmokePuff& P = Smoke[SmokeCursor];
		SmokeCursor = (SmokeCursor + 1) % Smoke.Num();
		UStaticMeshComponent* M = P.Mesh.Get();
		if (!M)
		{
			continue;
		}
		const float Ring = (Puffs > 4) ? 280.f : 80.f;
		P.Center = Location + FVector(FMath::FRandRange(-Ring, Ring), FMath::FRandRange(-Ring, Ring), FMath::FRandRange(0.f, Puffs > 4 ? 220.f : 60.f));
		P.Start = Time + k * 0.08f;
		P.Life = Duration * FMath::FRandRange(0.85f, 1.1f);
		P.MaxScale = (Puffs > 4) ? FMath::FRandRange(4.f, 6.5f) : FMath::FRandRange(1.5f, 2.5f);
		P.Rise = (Puffs > 4) ? 8.f : 40.f;
		if (Lib)
		{
			M->SetMaterial(0, Lib->GetMaterial(bGreen ? ETOMat::SmokeGreen : ETOMat::Smoke));
		}
		M->SetWorldLocation(P.Center);
		M->SetWorldScale3D(FVector(0.2f));
		M->SetVisibility(true);
		M->SetCollisionEnabled(Puffs > 4 ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
}

void ATOCombatManager::SpawnSignalSmoke(const FVector& Location, float Duration)
{
	// A tall column of colored smoke (extraction / helicopter landing zone marker).
	for (int32 k = 0; k < 5; ++k)
	{
		FTOSmokePuff& P = Smoke[SmokeCursor];
		SmokeCursor = (SmokeCursor + 1) % Smoke.Num();
		UStaticMeshComponent* M = P.Mesh.Get();
		if (!M)
		{
			continue;
		}
		P.Center = Location + FVector(0.f, 0.f, 150.f + k * 280.f);
		P.Start = Time + k * 0.3f;
		P.Life = Duration;
		P.MaxScale = 2.2f + k * 0.5f;
		P.Rise = 5.f;
		if (UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this))
		{
			M->SetMaterial(0, Lib->GetMaterial(ETOMat::SmokeGreen));
		}
		M->SetWorldLocation(P.Center);
		M->SetWorldScale3D(FVector(0.2f));
		M->SetVisibility(true);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void ATOCombatManager::Flashbang(const FVector& Location, AActor* Instigator)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	LightFlash(Location + FVector(0.f, 0.f, 50.f), FLinearColor::White, 400000.f, 3500.f, 0.25f);
	if (Audio)
	{
		Audio->Play(ETOSound::Explosion, Location, 0.6f, 1.6f);
	}
	for (TActorIterator<ATOCharacter> It(World); It; ++It)
	{
		ATOCharacter* C = *It;
		if (!C || !C->IsAlive())
		{
			continue;
		}
		const FVector Eye = C->GetEyeLocation();
		const float Dist = FVector::Dist(Eye, Location);
		if (Dist > 2200.f)
		{
			continue;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TOFlashLOS), false, C);
		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Location + FVector(0.f, 0.f, 20.f), Eye, ECC_Visibility, Params))
		{
			continue;
		}
		const float Facing = FVector::DotProduct(C->GetAimDirection(), (Location - Eye).GetSafeNormal());
		const float Amount = FMath::Clamp(1.f - Dist / 2200.f, 0.f, 1.f) * FMath::Lerp(0.35f, 1.f, FMath::Clamp(Facing * 0.5f + 0.5f, 0.f, 1.f));
		C->ApplyFlash(Amount, Location);
	}
	if (ATOGameMode* GM = ATOGameMode::Get(this))
	{
		GM->ReportNoise(Location, 12000.f, Cast<ATOCharacter>(Instigator), true);
	}
}

void ATOCombatManager::UpdateFx(float Dt)
{
	for (FTOFxInstance& F : Fx)
	{
		if (!F.bActive)
		{
			continue;
		}
		const float T = (Time - F.Start) / FMath::Max(0.001f, F.Life);
		if (T >= 1.f)
		{
			F.bActive = false;
			SetInstance(F.Pool, F.Index, FTransform(FRotator::ZeroRotator, HiddenLocation, HiddenScale));
			continue;
		}
		switch (F.Pool)
		{
		case PoolHole:
			// static, nothing to animate
			break;
		case PoolMuzzle:
		{
			const float S = FMath::Lerp(F.Scale0, F.Scale1, T);
			SetInstance(F.Pool, F.Index, FTransform(F.Rotation, F.Location, FVector(S * 2.2f, S, S)));
			break;
		}
		case PoolFireball:
		{
			const float Grow = FMath::Sin(FMath::Min(T * 1.4f, 1.f) * PI * 0.5f);
			const float S = FMath::Lerp(F.Scale0, F.Scale1, Grow) * (1.f - FMath::Max(0.f, T - 0.6f) * 2.5f);
			SetInstance(F.Pool, F.Index, FTransform(FRotator::ZeroRotator, F.Location, FVector(FMath::Max(S, 0.01f))));
			break;
		}
		default:
		{
			F.Velocity += FVector(0.f, 0.f, F.Pool == PoolDust ? 60.f : -900.f) * Dt;
			F.Velocity *= FMath::Exp(-Dt * (F.Pool == PoolDust ? 3.f : 1.f));
			F.Location += F.Velocity * Dt;
			const float S = FMath::Lerp(F.Scale0, F.Scale1, T);
			SetInstance(F.Pool, F.Index, FTransform(FRotator::ZeroRotator, F.Location, FVector(S)));
			break;
		}
		}
	}

	for (int32 p = 0; p < Pools.Num(); ++p)
	{
		if (PoolDirty.IsValidIndex(p) && PoolDirty[p] && Pools[p])
		{
			Pools[p]->MarkRenderStateDirty();
			PoolDirty[p] = false;
		}
	}

	// Smoke
	for (FTOSmokePuff& P : Smoke)
	{
		UStaticMeshComponent* M = P.Mesh.Get();
		if (!M || P.Life <= 0.f)
		{
			continue;
		}
		const float Age = Time - P.Start;
		if (Age < 0.f)
		{
			continue;
		}
		if (Age > P.Life)
		{
			P.Life = -1.f;
			M->SetVisibility(false);
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			M->SetWorldLocation(HiddenLocation);
			continue;
		}
		const float Grow = FMath::Clamp(Age / 2.f, 0.f, 1.f);
		const float Fade = FMath::Clamp((P.Life - Age) / 3.f, 0.f, 1.f);
		const float S = FMath::Max(0.05f, P.MaxScale * FMath::Sin(Grow * PI * 0.5f) * Fade);
		M->SetWorldScale3D(FVector(S, S, S * 0.85f));
		M->SetWorldLocation(P.Center + FVector(0.f, 0.f, P.Rise * Age));
	}

	// Lights
	for (FTOLightFlash& F : LightFlashes)
	{
		UPointLightComponent* L = F.Light.Get();
		if (!L || F.Life <= 0.f)
		{
			continue;
		}
		const float T = (Time - F.Start) / F.Life;
		if (T >= 1.f)
		{
			F.Life = -1.f;
			L->SetIntensity(0.f);
			L->SetVisibility(false);
			continue;
		}
		L->SetIntensity(F.Intensity * (1.f - T) * (1.f - T));
	}
}
