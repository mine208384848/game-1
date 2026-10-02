// TAC-OPS - health, armor and injury model
//
// Delta Force style: 100 HP, hit zones with multipliers, armor / helmet levels 1-6 with
// durability vs. ammo penetration levels, bleeding, fractures, painkillers and a downed state.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TOTypes.h"
#include "TOHealthComponent.generated.h"

class UTOInventoryComponent;

UCLASS(ClassGroup = (TacOps), meta = (BlueprintSpawnableComponent))
class TACOPS_API UTOHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTOHealthComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetInventory(UTOInventoryComponent* InInventory) { Inventory = InInventory; }

	/** Resolves armor, zone multipliers and status effects. Does NOT trigger death handling. */
	FTODamageResult ApplyDamage(const FTODamageInfo& Info);

	void Heal(float Amount);
	void AddHealOverTime(float Amount, float Duration);
	void StopBleeding() { bBleeding = false; }
	void FixFractures() { bLegFracture = false; bArmFracture = false; }
	void AddPainkiller(float Seconds) { PainkillerTime = FMath::Max(PainkillerTime, Seconds); }
	void ReviveFromDowned(float NewHealth);
	void ResetFull();

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetHealth01() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }
	bool IsDead() const { return bDead; }
	bool IsDowned() const { return bDowned; }
	bool IsBleeding() const { return bBleeding; }
	bool HasLegFracture() const { return bLegFracture; }
	bool HasArmFracture() const { return bArmFracture; }
	bool IsPainkillerActive() const { return PainkillerTime > 0.f; }
	float GetPainkillerTime() const { return PainkillerTime; }
	float GetDownedTime() const { return DownedTime; }
	float GetBleedOutTime() const { return BleedOutTime; }
	float GetTimeSinceDamage() const;

	/** Effective penalties (respect painkillers). */
	bool LegPenalty() const { return bLegFracture && PainkillerTime <= 0.f; }
	bool ArmPenalty() const { return bArmFracture && PainkillerTime <= 0.f; }

	UPROPERTY(EditAnywhere, Category = "Health") float MaxHealth = 100.f;
	UPROPERTY(EditAnywhere, Category = "Health") bool bCanBeDowned = true;
	UPROPERTY(EditAnywhere, Category = "Health") bool bPassiveRegen = false;
	UPROPERTY(EditAnywhere, Category = "Health") float DamageTakenMult = 1.f;
	UPROPERTY(EditAnywhere, Category = "Health") float BleedOutTime = 45.f;

	/** Set when the downed timer runs out or bleeding kills the character; the owner polls it. */
	bool bPendingDeathFromStatus = false;
	bool bPendingDownedFromStatus = false;

	void MarkDead() { bDead = true; bDowned = false; Health = 0.f; }

private:
	UPROPERTY() TObjectPtr<UTOInventoryComponent> Inventory;

	float Health = 100.f;
	bool bDead = false;
	bool bDowned = false;
	float DownedTime = 0.f;
	bool bBleeding = false;
	bool bLegFracture = false;
	bool bArmFracture = false;
	float PainkillerTime = 0.f;
	float LastDamageTime = -1000.f;
	float HoTRemaining = 0.f;
	float HoTRate = 0.f;
};
