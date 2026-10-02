// TAC-OPS - extraction points, extraction switches (radio / pay terminal), warfare capture points,
// explosive barrels

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TOTypes.h"
#include "Core/TOInterfaces.h"
#include "TOExtractionZone.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class TACOPS_API ATOExtractionZone : public AActor
{
	GENERATED_BODY()

public:
	ATOExtractionZone();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	void Setup(const FString& InName, ETOExtractRule InRule, float InRadius, int32 InCost);
	void SetAvailable(bool bInAvailable);

	bool IsAvailable() const { return bAvailable; }
	/** Conditions met right now (helicopter on site, paid...). Backpack rule is checked per character. */
	bool IsReady() const;
	bool CanExtract(const ATOCharacter* Character, FString& OutReason) const;
	bool IsInside(const FVector& Location) const;

	void CallHelicopter();
	void MarkPaid() { bPaid = true; }
	bool IsHelicopterCalled() const { return HeliArrival > 0.f; }
	float GetHelicopterETA() const;
	float GetHelicopterTimeLeft() const;

	FString GetZoneName() const { return ZoneName; }
	ETOExtractRule GetRule() const { return Rule; }
	float GetRadius() const { return Radius; }
	int32 GetCost() const { return Cost; }
	FString GetStatusText() const;

protected:
	void Build();

	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> HeliParts;
	UPROPERTY() TObjectPtr<UPointLightComponent> Beacon;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BeaconMID;
	UPROPERTY() TObjectPtr<USceneComponent> HeliRoot;

	FString ZoneName;
	ETOExtractRule Rule = ETOExtractRule::Always;
	float Radius = 700.f;
	int32 Cost = 0;
	bool bAvailable = true;
	bool bPaid = false;
	float HeliArrival = -1.f;
	float HeliLeave = -1.f;
	float NextSmoke = 0.f;
	bool bBuilt = false;
};

UCLASS()
class TACOPS_API ATOObjectiveSwitch : public AActor, public ITOInteractable
{
	GENERATED_BODY()

public:
	ATOObjectiveSwitch();

	virtual void BeginPlay() override;

	/** Kind 0 = radio (calls the helicopter), 1 = pay terminal. */
	void Setup(int32 InKind, ATOExtractionZone* InZone);

	virtual FString GetInteractLabel(const ATOCharacter* User) const override;
	virtual FString GetInteractHint(const ATOCharacter* User) const override;
	virtual bool CanInteract(const ATOCharacter* User) const override;
	virtual float GetInteractDuration(const ATOCharacter* User) const override { return Kind == 0 ? 4.f : 1.5f; }
	virtual void Interact(ATOCharacter* User) override;

protected:
	void Build();

	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	TWeakObjectPtr<ATOExtractionZone> Zone;
	int32 Kind = 0;
	bool bUsed = false;
	bool bBuilt = false;
};

UCLASS()
class TACOPS_API ATOCapturePoint : public AActor
{
	GENERATED_BODY()

public:
	ATOCapturePoint();

	virtual void BeginPlay() override;

	void Setup(const FString& InLabel, int32 InSector, float InRadius, int32 InOwnerTeam);
	void SetOwnerTeam(int32 Team);
	void UpdateVisual();

	FString Label;
	int32 Sector = 0;
	float Radius = 1500.f;
	int32 OwnerTeam = 1;
	/** 0..1 progress of the attacking team. */
	float Capture = 0.f;
	bool bActive = false;
	int32 CountTeam0 = 0;
	int32 CountTeam1 = 0;

protected:
	void Build();

	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Flag;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FlagMID;
	UPROPERTY() TObjectPtr<UPointLightComponent> Light;
	bool bBuilt = false;
};

UCLASS()
class TACOPS_API ATOExplosiveBarrel : public AActor, public ITODamageable
{
	GENERATED_BODY()

public:
	ATOExplosiveBarrel();

	virtual void BeginPlay() override;
	virtual void ReceiveTODamage(const FTODamageInfo& Info) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	float HP = 45.f;
	bool bExploded = false;
};
