// TAC-OPS - soldier character (player operators, AI guards, rival squads, warfare bots)
//
// Unity port mapping:
//   PlayerMovement.cs / FirstPersonController.cs -> ATOCharacter (movement, stance, lean, vault, slide, stamina)
//   MouseLook.cs                                 -> ATOPlayerController::OnLook + camera code here
//   PlayerHealth.cs                              -> UTOHealthComponent
//   Gun.cs / WeaponController.cs                 -> UTOWeaponComponent
//   Inventory.cs                                 -> UTOInventoryComponent

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/TOTypes.h"
#include "Core/TOInterfaces.h"
#include "TOCharacter.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UNavigationInvokerComponent;
class UTOBodyRigComponent;
class UTOHealthComponent;
class UTOInventoryComponent;
class UTOWeaponComponent;
class ATOLootContainer;
class ATOVehicle;

struct FTODamageIndicator
{
	FVector From = FVector::ZeroVector;
	float Time = 0.f;
	float Strength = 1.f;
};

UCLASS()
class TACOPS_API ATOCharacter : public ACharacter, public ITODamageable
{
	GENERATED_BODY()

public:
	ATOCharacter();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual FVector GetPawnViewLocation() const override;

	// ITODamageable
	virtual void ReceiveTODamage(const FTODamageInfo& Info) override;

	// Components ---------------------------------------------------------------------------
	UCameraComponent* GetCamera() const { return FPCamera; }
	UTOBodyRigComponent* GetBody() const { return Body; }
	UTOHealthComponent* GetHealth() const { return Health; }
	UTOInventoryComponent* GetInventory() const { return Inventory; }
	UTOWeaponComponent* GetWeapons() const { return Weapons; }
	USceneComponent* GetViewmodelRoot() const { return ViewmodelRoot; }

	// Setup --------------------------------------------------------------------------------
	void InitCharacter(int32 InTeam, const FString& InName, ETOOperator InOperator, bool bPlayer, ETOMat InSuit);
	void ApplyLoadout(const FTOLoadout& Loadout, ETOMatchMode Mode);
	void ApplyAIKit(int32 Tier, ETOAIRole Role, FRandomStream& Rng, ETOMatchMode Mode);
	void RefreshGearVisuals();
	void OnActiveWeaponChanged();

	// Identity -----------------------------------------------------------------------------
	int32 TeamId = 0;
	FString DisplayName;
	ETOOperator Operator = ETOOperator::Viper;
	ETOAIRole AIRole = ETOAIRole::Guard;
	int32 SquadId = -1;
	bool bBoss = false;
	bool bIsPlayerControlled = false;
	ETOMat SuitMat = ETOMat::SuitGreen;
	int32 Kills = 0;
	int32 Headshots = 0;
	int32 ZoneTier = 0;

	bool IsPlayerCharacter() const { return bIsPlayerControlled; }
	bool IsAlive() const;
	bool IsDowned() const;
	bool IsDeadState() const;
	bool IsHostileTo(const ATOCharacter* Other) const;

	// Player input ---------------------------------------------------------------------------
	void InputMove(const FVector2D& Axis) { MoveInput = Axis; }
	void InputJump();
	void SetSprint(bool bOn) { bWantsSprint = bOn; }
	void ToggleCrouch();
	void ToggleProne();
	void SetLeanInput(float Value) { LeanInput = FMath::Clamp(Value, -1.f, 1.f); }
	void SetFireInput(bool bPressed);
	void SetAimInput(bool bPressed);
	void InputReload();
	void InputSelectWeapon(int32 Slot);
	void InputCycleWeapon(int32 Direction);
	void InputFireMode();
	void InputGrenade();
	void InputCycleGrenade();
	void InputAbility();
	void InputQuickHeal();
	void InputLight();
	void InputNVG();
	void SetInteractInput(bool bPressed);
	void AddLookInput(float YawDelta, float PitchDelta);
	void CancelActions();

	// Movement / stance (AI + player) ---------------------------------------------------------
	void SetStance(ETOStance NewStance);
	ETOStance GetStance() const { return Stance; }
	void SetWantsSprint(bool bOn) { bWantsSprint = bOn; }
	bool IsSprinting() const { return bSprinting; }
	float GetLean() const { return LeanAlpha; }
	float GetStamina01() const { return Stamina / 100.f; }
	bool IsInWater() const { return bInWater; }
	bool IsSliding() const { return bSliding; }

	// Combat feedback ------------------------------------------------------------------------
	void AddRecoil(float Pitch, float Yaw);
	void AddCameraShake(float Amount) { ShakeAlpha = FMath::Clamp(ShakeAlpha + Amount, 0.f, 1.5f); }
	void ApplySuppression(float Amount, const FVector& From);
	void ApplyFlash(float Amount, const FVector& From);
	void OnDealtDamage(ATOCharacter* Victim, const FTODamageResult& Result);
	void Reveal(float Seconds);
	bool IsRevealed() const;
	void Revive(ATOCharacter* By);
	void Kill(const FTODamageInfo& Info);

	// Items & abilities ----------------------------------------------------------------------
	bool UseItemByUid(int32 Uid);
	bool IsUsingItem() const { return bUsingItem; }
	float GetUseProgress() const;
	FString GetUsingItemName() const;
	void CancelItemUse();
	float GetAbilityCooldownRemaining() const;
	float GetAbilityCooldownTotal() const;
	bool IsAdrenalineActive() const;
	ETOGrenadeType GetSelectedGrenade() const { return SelectedGrenade; }
	bool ThrowGrenadeAt(ETOGrenadeType Type, const FVector& Target);
	bool IsThrowing() const;
	bool IsNVGOn() const { return bNVG; }

	// Interaction (player) ---------------------------------------------------------------------
	AActor* GetFocusActor() const { return FocusActor.Get(); }
	ATOCharacter* GetReviveTarget() const { return ReviveTarget.Get(); }
	float GetInteractProgress() const;
	FString GetFocusLabel() const;
	FString GetFocusHint() const;

	// Queries --------------------------------------------------------------------------------
	FVector GetEyeLocation() const;
	FVector GetHeadLocation() const;
	FVector GetChestLocation() const;
	FVector GetAimDirection() const;
	FVector GetMuzzleLocation() const;
	float GetVisibilityFactor() const;
	float GetFOVForView() const { return CurrentFOV; }
	float GetBaseFOV() const { return BaseFOV; }
	void SetBaseFOV(float InFOV) { BaseFOV = InFOV; }

	// Player HUD feedback ----------------------------------------------------------------------
	float LastHitMarkerTime = -10.f;
	bool bLastHitKill = false;
	bool bLastHitHead = false;
	bool bLastHitArmor = false;
	TArray<FTODamageIndicator> DamageIndicators;
	float SuppressionAlpha = 0.f;
	float FlashAlpha = 0.f;
	float LowHealthPulse = 0.f;
	FString KilledByName;
	FString KilledByWeapon;
	float KilledByDistance = 0.f;
	TWeakObjectPtr<ATOCharacter> LastAttacker;
	float LastDamagedTime = -100.f;
	float DeathTime = -1.f;

	/** Vehicle the character currently drives (if any). */
	TWeakObjectPtr<ATOVehicle> CurrentVehicle;

protected:
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UCameraComponent> FPCamera;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USceneComponent> ViewmodelRoot;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UTOBodyRigComponent> Body;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UTOHealthComponent> Health;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UTOInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UTOWeaponComponent> Weapons;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UNavigationInvokerComponent> NavInvoker;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USpotLightComponent> HeadLamp;

private:
	void UpdateMovement(float Dt, float Now);
	void UpdateCamera(float Dt, float Now);
	void UpdateFocus(float Now);
	void UpdateItemUse(float Now);
	void UpdateStatusChecks(float Dt, float Now);
	void UpdateFootsteps(float Dt);
	void UpdatePostProcess(float Dt);
	bool TryVault();
	void StartSlide();
	void EndSlide();
	void HandleDowned(const FTODamageInfo& Info);
	void HandleDeath(const FTODamageInfo& Info);
	void SpawnCorpseLoot();
	void DoThrowGrenade();
	float ComputeMaxSpeed() const;
	float EyeHeightForStance() const;
	void ApplyAbilityEffect();

	// Movement state
	FVector2D MoveInput = FVector2D::ZeroVector;
	ETOStance Stance = ETOStance::Stand;
	bool bWantsSprint = false;
	bool bSprinting = false;
	float Stamina = 100.f;
	float StaminaRegenDelay = 0.f;
	float LeanInput = 0.f;
	float LeanAlpha = 0.f;
	bool bSliding = false;
	float SlideEnd = 0.f;
	float SavedGroundFriction = 8.f;
	float SavedBraking = 2048.f;
	bool bInWater = false;
	bool bHeadUnderWater = false;
	float WaterCheckTimer = 0.f;
	float FootstepDistance = 0.f;
	float EyeHeight = 164.f;

	// Camera
	float BaseFOV = 90.f;
	float CurrentFOV = 90.f;
	float PendingRecoilPitch = 0.f;
	float PendingRecoilYaw = 0.f;
	float RecoilAccumPitch = 0.f;
	float LastRecoilTime = -10.f;
	float ShakeAlpha = 0.f;
	float ShakeTime = 0.f;
	bool bNVG = false;

	// Interaction
	TWeakObjectPtr<AActor> FocusActor;
	TWeakObjectPtr<ATOCharacter> ReviveTarget;
	bool bInteractHeld = false;
	float InteractStart = 0.f;
	bool bInteractConsumed = false;

	// Item use
	bool bUsingItem = false;
	int32 UsingUid = 0;
	FName UsingItemId;
	float UseStart = 0.f;
	float UseEnd = 0.f;

	// Abilities / gadgets
	float AbilityReadyTime = 0.f;
	float AdrenalineEnd = -1.f;
	float RevealedUntil = -1.f;
	ETOGrenadeType SelectedGrenade = ETOGrenadeType::Frag;
	float ThrowStart = -10.f;
	bool bThrowPending = false;
	ETOGrenadeType PendingThrowType = ETOGrenadeType::Frag;
	FVector PendingThrowTarget = FVector::ZeroVector;
	bool bPendingThrowHasTarget = false;

	float HeartbeatTimer = 0.f;
	bool bDeathHandled = false;
	ETOMatchMode MatchMode = ETOMatchMode::Operations;
};
