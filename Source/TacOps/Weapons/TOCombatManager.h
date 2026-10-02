// TAC-OPS - combat services: ballistics simulation, impact / muzzle / explosion effects, audio
//
// Bullets are simulated as projectiles with real muzzle velocity, gravity drop and damage
// fall-off (like Delta Force's ballistics).  Every frame each round sweeps a segment on the
// custom "Bullet" trace channel; body part hitboxes resolve per-zone damage.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TOTypes.h"
#include "TOCombatManager.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UTOAudio;
class ATOCharacter;

struct FTOBulletSpawn
{
	FVector Origin = FVector::ZeroVector;
	FVector Velocity = FVector::ForwardVector;     // cm/s
	FVector VisualOrigin = FVector::ZeroVector;    // muzzle (tracer start)
	float Damage = 30.f;
	int32 PenLevel = 3;
	float ArmorDamage = 1.f;
	float EffectiveRange = 6000.f;                 // cm
	TWeakObjectPtr<AActor> Instigator;
	int32 Team = -1;
	FName WeaponId;
	bool bTracer = true;
	bool bFromPlayer = false;
};

struct FTOBullet
{
	FTOBulletSpawn Spawn;
	FVector Pos = FVector::ZeroVector;
	FVector Vel = FVector::ZeroVector;
	FVector VisualOffset = FVector::ZeroVector;
	float Age = 0.f;
	float Travelled = 0.f;
	int32 TracerIndex = INDEX_NONE;
	bool bWhizzed = false;
};

struct FTOFxInstance
{
	int32 Pool = 0;
	int32 Index = 0;
	float Start = 0.f;
	float Life = 0.f;
	FVector Location = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	float Scale0 = 1.f;
	float Scale1 = 1.f;
	bool bActive = false;
};

struct FTOSmokePuff
{
	TWeakObjectPtr<UStaticMeshComponent> Mesh;
	FVector Center = FVector::ZeroVector;
	float Start = 0.f;
	float Life = 25.f;
	float MaxScale = 4.f;
	float Rise = 0.f;
};

struct FTOLightFlash
{
	TWeakObjectPtr<UPointLightComponent> Light;
	float Start = 0.f;
	float Life = 0.1f;
	float Intensity = 1.f;
};

UCLASS()
class TACOPS_API ATOCombatManager : public AActor
{
	GENERATED_BODY()

public:
	ATOCombatManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	static ATOCombatManager* Get(const UObject* WorldContext);

	void FireBullet(const FTOBulletSpawn& Spawn);
	void Explode(const FVector& Location, float Radius, float Damage, AActor* Instigator, int32 Team, FName WeaponId);
	void SpawnSmoke(const FVector& Location, bool bGreen = false, float Duration = 25.f, int32 Puffs = 12);
	void SpawnSignalSmoke(const FVector& Location, float Duration);
	void Flashbang(const FVector& Location, AActor* Instigator);
	void MuzzleFlash(const FVector& Location, const FRotator& Rotation, bool bSuppressed);
	void ImpactEffect(const FVector& Location, const FVector& Normal, FName Surface);
	void BloodEffect(const FVector& Location, const FVector& Direction);
	void LightFlash(const FVector& Location, const FLinearColor& Color, float Intensity, float Radius, float Duration);

	UTOAudio* GetAudio() const { return Audio; }

	/** Debug / stats */
	int32 GetActiveBulletCount() const { return Bullets.Num(); }

private:
	void SimulateBullets(float Dt);
	bool HandleHit(FTOBullet& Bullet, const FHitResult& Hit);
	void UpdateFx(float Dt);
	int32 AllocFx(int32 Pool);
	UInstancedStaticMeshComponent* MakePool(int32 Count, int32 ShapeIndex, int32 MaterialIndex, bool bShadow);
	void SetInstance(int32 Pool, int32 Index, const FTransform& T);
	FName SurfaceOf(const FHitResult& Hit) const;

	UPROPERTY() TObjectPtr<UTOAudio> Audio;

	// Pools: 0 tracers, 1 holes, 2 dust, 3 sparks, 4 blood, 5 muzzle flashes, 6 fireballs
	UPROPERTY() TArray<TObjectPtr<UInstancedStaticMeshComponent>> Pools;
	TArray<int32> PoolSizes;
	TArray<int32> PoolCursor;
	TArray<FTOFxInstance> Fx;
	TArray<bool> PoolDirty;

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> SmokeMeshes;
	TArray<FTOSmokePuff> Smoke;
	int32 SmokeCursor = 0;

	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
	TArray<FTOLightFlash> LightFlashes;
	int32 LightCursor = 0;

	TArray<FTOBullet> Bullets;
	int32 TracerCursor = 0;
	float Time = 0.f;
};
