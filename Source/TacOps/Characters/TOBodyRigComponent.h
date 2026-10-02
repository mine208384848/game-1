// TAC-OPS - procedural soldier body
//
// A soldier assembled from primitives with procedural animation (walk cycle, crouch, prone,
// lean, aim pitch, downed / death poses).  Every body part mesh doubles as a precise hitbox
// on the "Bullet" trace channel, which gives Delta Force style per-zone damage.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Core/TOTypes.h"
#include "Weapons/TOWeaponVisuals.h"
#include "TOBodyRigComponent.generated.h"

class UStaticMeshComponent;
class UPrimitiveComponent;

UCLASS(ClassGroup = (TacOps), meta = (BlueprintSpawnableComponent))
class TACOPS_API UTOBodyRigComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UTOBodyRigComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void BuildRig(ETOMat SuitMat, bool bHideFromOwner, bool bBoss);
	void UpdateGear(int32 HelmetLevel, int32 ArmorLevel, bool bHasRig, bool bHasBackpack);
	void SetWeapon(const FTOWeaponConfig* Config);

	ETOBodyPart GetPartFromComponent(const UPrimitiveComponent* Component) const;
	bool IsRigComponent(const UPrimitiveComponent* Component) const;

	FVector GetHeadWorld() const;
	FVector GetChestWorld() const;
	FVector GetMuzzleWorld() const;
	FVector GetWeaponForward() const;
	FTransform GetWeaponLightTransform() const;
	USceneComponent* GetWeaponSocket() const { return WeaponSocket; }
	const FTOGunVisual& GetGunVisual() const { return Gun; }

	void SetHitboxesEnabled(bool bEnabled);
	void AddFlinch(float Amount) { Flinch = FMath::Min(Flinch + Amount, 1.f); }
	void AddFireKick() { FireKick = 1.f; }

	// Animation inputs (set by the owning character every frame)
	float AimPitch = 0.f;
	float Speed = 0.f;
	float Lean = 0.f;
	ETOStance Stance = ETOStance::Stand;
	bool bSprinting = false;
	bool bDowned = false;
	bool bDead = false;
	bool bReloading = false;
	bool bThrowing = false;
	bool bInVehicle = false;

private:
	UStaticMeshComponent* MakePart(USceneComponent* Parent, ETOShape Shape, ETOMat Mat, const FVector& Loc, const FVector& Size, ETOBodyPart Part, bool bHitbox, const FRotator& Rot = FRotator::ZeroRotator);
	USceneComponent* MakePivot(USceneComponent* Parent, const FVector& Loc, FName Name);
	void PlaceLimb(UStaticMeshComponent* Limb, const FVector& From, const FVector& To, float Thickness);
	void UpdateArms();

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> WeaponParts;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Pivots;

	UPROPERTY() TObjectPtr<USceneComponent> Hips;
	UPROPERTY() TObjectPtr<USceneComponent> Spine;
	UPROPERTY() TObjectPtr<USceneComponent> HeadPivot;
	UPROPERTY() TObjectPtr<USceneComponent> ThighL;
	UPROPERTY() TObjectPtr<USceneComponent> ThighR;
	UPROPERTY() TObjectPtr<USceneComponent> ShinL;
	UPROPERTY() TObjectPtr<USceneComponent> ShinR;
	UPROPERTY() TObjectPtr<USceneComponent> WeaponSocket;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> HeadMesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> HelmetMesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> TorsoMesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> VestMesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> RigMesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> BackpackMesh;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> UpperArmL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> UpperArmR;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ForeArmL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> ForeArmR;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> HandL;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> HandR;

	TArray<TPair<TWeakObjectPtr<UPrimitiveComponent>, ETOBodyPart>> PartMap;

	FTOGunVisual Gun;
	bool bHideOwner = false;
	bool bBuilt = false;

	// Smoothed pose
	float WalkPhase = 0.f;
	float HipsZ = 94.f;
	float HipsPitch = 0.f;
	float HipsRoll = 0.f;
	float ThighLP = 0.f;
	float ThighRP = 0.f;
	float ShinLP = 0.f;
	float ShinRP = 0.f;
	float SpinePitch = 0.f;
	float SpineRoll = 0.f;
	float HeadPitch = 0.f;
	float SocketPitch = 0.f;
	float SocketYaw = 0.f;
	float SocketRoll = 0.f;
	float Flinch = 0.f;
	float FireKick = 0.f;
	float DeathRoll = 0.f;
};
