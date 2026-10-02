// TAC-OPS - weapon handling (shared by players and AI)
//
// Fire modes, rate of fire, magazine / reserve ammo, tactical & empty reloads, ADS with
// optic zoom, spread & bloom, Delta Force style recoil impulses, procedural first-person
// view model (sway, bob, sprint pose, reload animation, wall avoidance) and the weapon light.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TOTypes.h"
#include "Weapons/TOWeaponVisuals.h"
#include "TOWeaponComponent.generated.h"

class ATOCharacter;
class UStaticMeshComponent;
class USpotLightComponent;
class UPointLightComponent;
class USceneComponent;

USTRUCT()
struct FTOWeaponSlot
{
	GENERATED_BODY()

	UPROPERTY() bool bHas = false;
	UPROPERTY() FTOWeaponConfig Config;
	UPROPERTY() int32 MagAmmo = 0;
	UPROPERTY() int32 LoadedTier = 3;
	UPROPERTY() int32 FireModeIndex = 0;
};

UCLASS(ClassGroup = (TacOps), meta = (BlueprintSpawnableComponent))
class TACOPS_API UTOWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTOWeaponComponent();

	static constexpr int32 SlotPrimary = 0;
	static constexpr int32 SlotSecondary = 1;
	static constexpr int32 SlotSidearm = 2;
	static constexpr int32 SlotMelee = 3;
	static constexpr int32 NumSlots = 4;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Init(ATOCharacter* InOwner, USceneComponent* InViewRoot, bool bInFirstPerson);

	// Loadout ------------------------------------------------------------------------------
	void SetWeapon(int32 Slot, const FTOWeaponConfig& Config, bool bFullMag = true);
	void ClearWeapon(int32 Slot);
	bool HasWeapon(int32 Slot) const { return Slots.IsValidIndex(Slot) && Slots[Slot].bHas; }
	const FTOWeaponSlot& GetSlot(int32 Slot) const { return Slots[FMath::Clamp(Slot, 0, NumSlots - 1)]; }
	FTOWeaponSlot& GetSlotMutable(int32 Slot) { return Slots[FMath::Clamp(Slot, 0, NumSlots - 1)]; }
	int32 GetActiveSlotIndex() const { return ActiveSlot; }
	void EquipSlot(int32 Slot, bool bInstant = false);
	void CycleWeapon(int32 Direction);
	void RebuildVisuals();
	void HideAllVisuals();

	// Input --------------------------------------------------------------------------------
	void SetTrigger(bool bPressed);
	void SetAiming(bool bAim) { bWantsAim = bAim; }
	void Reload();
	void CancelReload();
	void CycleFireMode();
	void ToggleLight();
	void SetLightOn(bool bOn);

	/** AI: where the soldier is trying to put rounds (already includes aim error). */
	void SetAIAimPoint(const FVector& Point) { AIAimPoint = Point; bHasAIAim = true; }
	void ClearAIAim() { bHasAIAim = false; }

	// State --------------------------------------------------------------------------------
	bool IsReloading() const { return bReloading; }
	float GetReloadProgress() const;
	bool IsEquipping() const;
	bool IsBusy() const { return bReloading || IsEquipping() || bMeleeActive; }
	bool IsFiringRecently() const;
	bool CanAim() const;
	float GetADSAlpha() const { return ADSAlpha; }
	bool IsScoped() const;
	int32 GetMagAmmo() const;
	int32 GetMagSize() const { return Stats.MagSize; }
	int32 GetReserveAmmo() const;
	ETOFireMode GetFireMode() const;
	const FTOWeaponStats& GetStats() const { return Stats; }
	const FTOWeaponConfig* GetActiveConfig() const;
	FString GetActiveName() const;
	bool IsMeleeActive() const { return ActiveSlot == SlotMelee; }
	float GetSpreadDegrees() const { return CurrentSpread; }
	bool IsLightOn() const { return bLightOn; }
	float GetShotsFiredRecently() const { return RecentShots; }

	bool bInfiniteReserve = false;
	/** Multiplier on AI shot dispersion (difficulty). */
	float AIDispersion = 1.f;
	float WallBlockAlpha = 0.f;

	/** View-model look sway input from the controller (degrees this frame). */
	void AddLookDelta(float Yaw, float Pitch) { LookSway.X += Yaw; LookSway.Y += Pitch; }

private:
	void TryFire(float Now);
	void FireShot(float Now);
	void DoMelee(float Now);
	void FinishReload();
	void UpdateSpread(float Dt);
	void UpdateViewModel(float Dt, float Now);
	void BuildFirstPerson();
	void BuildArms();
	void ApplyLightState();
	float GetEquipTime() const;

	UPROPERTY() TArray<FTOWeaponSlot> Slots;
	UPROPERTY() TObjectPtr<ATOCharacter> OwnerChar;
	UPROPERTY() TObjectPtr<USceneComponent> ViewRoot;
	UPROPERTY() TObjectPtr<USceneComponent> GunRoot;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FPParts;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ArmParts;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> MuzzleFlashMesh;
	UPROPERTY() TObjectPtr<UPointLightComponent> MuzzleLight;
	UPROPERTY() TObjectPtr<USpotLightComponent> WeaponLight;

	FTOGunVisual FPGun;
	FTOWeaponStats Stats;
	int32 ActiveSlot = 0;
	bool bFirstPerson = false;

	bool bTriggerHeld = false;
	bool bTriggerPending = false;
	int32 BurstRemaining = 0;
	float NextFireTime = 0.f;
	float LastShotTime = -100.f;
	bool bDryFired = false;
	int32 ConsecutiveShots = 0;
	float RecentShots = 0.f;

	bool bReloading = false;
	float ReloadStart = 0.f;
	float ReloadEnd = 0.f;
	float EquipStart = -100.f;
	float EquipEnd = -100.f;

	bool bMeleeActive = false;
	float MeleeStart = 0.f;
	bool bMeleeHitDone = false;

	bool bWantsAim = false;
	float ADSAlpha = 0.f;
	float Bloom = 0.f;
	float CurrentSpread = 2.f;
	bool bLightOn = false;

	FVector AIAimPoint = FVector::ZeroVector;
	bool bHasAIAim = false;

	// View model dynamics
	FVector2D LookSway = FVector2D::ZeroVector;
	FVector2D SwayOffset = FVector2D::ZeroVector;
	float BobPhase = 0.f;
	float KickBack = 0.f;
	float KickPitch = 0.f;
	float SprintAlpha = 0.f;
	float LowerAlpha = 0.f;
	float FlashTimer = 0.f;
};
