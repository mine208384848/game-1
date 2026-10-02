// TAC-OPS - thrown grenades (frag / smoke / flash) and the Engineer's deployable shield

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TOTypes.h"
#include "Core/TOInterfaces.h"
#include "TOGrenade.generated.h"

class UStaticMeshComponent;
class ATOCharacter;

UCLASS()
class TACOPS_API ATOGrenade : public AActor
{
	GENERATED_BODY()

public:
	ATOGrenade();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	void Launch(ETOGrenadeType InType, const FVector& Velocity, ATOCharacter* InThrower);

	ETOGrenadeType GetGrenadeType() const { return Type; }
	int32 GetTeam() const { return Team; }

protected:
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void Detonate();

	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UStaticMeshComponent> Mesh;

	ETOGrenadeType Type = ETOGrenadeType::Frag;
	TWeakObjectPtr<ATOCharacter> Thrower;
	int32 Team = -1;
	float FuseEnd = 0.f;
	float LastBounce = -10.f;
	float LastWarn = -10.f;
	bool bDetonated = false;
	bool bLaunched = false;
};

UCLASS()
class TACOPS_API ATODeployableShield : public AActor, public ITODamageable
{
	GENERATED_BODY()

public:
	ATODeployableShield();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void ReceiveTODamage(const FTODamageInfo& Info) override;

	int32 Team = -1;
	float HP = 800.f;
	float LifeTime = 60.f;

protected:
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	float SpawnTime = 0.f;
};
