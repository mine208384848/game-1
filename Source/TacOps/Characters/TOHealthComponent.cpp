// TAC-OPS - health, armor and injury model

#include "Characters/TOHealthComponent.h"
#include "Characters/TOInventoryComponent.h"
#include "Core/TODatabase.h"
#include "Engine/World.h"

UTOHealthComponent::UTOHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

float UTOHealthComponent::GetTimeSinceDamage() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() - LastDamageTime : 1000.f;
}

void UTOHealthComponent::ResetFull()
{
	Health = MaxHealth;
	bDead = false;
	bDowned = false;
	DownedTime = 0.f;
	bBleeding = false;
	bLegFracture = false;
	bArmFracture = false;
	PainkillerTime = 0.f;
	HoTRemaining = 0.f;
	HoTRate = 0.f;
	bPendingDeathFromStatus = false;
	bPendingDownedFromStatus = false;
}

FTODamageResult UTOHealthComponent::ApplyDamage(const FTODamageInfo& Info)
{
	FTODamageResult R;
	if (bDead)
	{
		return R;
	}

	float Dmg = Info.Damage;
	const ETOBodyPart Part = Info.Part;

	// Hit zone multipliers (head shots are brutal, limbs forgiving)
	if (!Info.bExplosive && !Info.bFall && !Info.bBleed)
	{
		switch (Part)
		{
		case ETOBodyPart::Head: Dmg *= Info.bMelee ? 1.5f : 2.1f; break;
		case ETOBodyPart::Thorax: Dmg *= 1.0f; break;
		case ETOBodyPart::Stomach: Dmg *= 0.9f; break;
		case ETOBodyPart::LeftArm:
		case ETOBodyPart::RightArm: Dmg *= 0.55f; break;
		case ETOBodyPart::LeftLeg:
		case ETOBodyPart::RightLeg: Dmg *= 0.65f; break;
		default: break;
		}
	}

	// Armor --------------------------------------------------------------------------------
	FTOItemInstance* Armor = nullptr;
	if (Inventory && !Info.bFall && !Info.bBleed)
	{
		if (Info.bExplosive)
		{
			Armor = &Inventory->ArmorItem;
		}
		else if (Part == ETOBodyPart::Head)
		{
			Armor = &Inventory->HelmetItem;
		}
		else if (Part == ETOBodyPart::Thorax || Part == ETOBodyPart::Stomach)
		{
			Armor = &Inventory->ArmorItem;
		}
	}

	if (Armor && Armor->IsValid() && Armor->Durability > 0.f)
	{
		const FTOItemDef* AD = TODB::FindItem(Armor->ItemId);
		const int32 Level = AD ? AD->Level : 1;
		const int32 Diff = Info.PenLevel - Level;
		float BodyFactor = 1.f;
		float DurFactor = 1.f;
		if (Info.bExplosive)
		{
			BodyFactor = FMath::Clamp(1.f - Level * 0.08f, 0.45f, 1.f);
			DurFactor = 0.5f;
		}
		else if (Info.bMelee)
		{
			BodyFactor = 0.55f;
			DurFactor = 0.4f;
		}
		else if (Diff >= 2) { BodyFactor = 0.95f; DurFactor = 0.8f; }
		else if (Diff == 1) { BodyFactor = 0.85f; DurFactor = 0.9f; }
		else if (Diff == 0) { BodyFactor = 0.6f; DurFactor = 1.0f; }
		else if (Diff == -1) { BodyFactor = 0.32f; DurFactor = 0.85f; }
		else if (Diff == -2) { BodyFactor = 0.16f; DurFactor = 0.7f; }
		else { BodyFactor = 0.08f; DurFactor = 0.6f; }

		const float DurLoss = Info.Damage * Info.ArmorDamage * DurFactor * 0.55f;
		Armor->Durability = FMath::Max(0.f, Armor->Durability - DurLoss);
		R.bArmorHit = true;
		R.bArmorBroken = Armor->Durability <= 0.f;
		Dmg *= BodyFactor;
		if (Inventory)
		{
			Inventory->MarkDirty();
		}
	}

	Dmg *= DamageTakenMult;
	Dmg = FMath::Max(0.f, Dmg);

	R.bHeadshot = (Part == ETOBodyPart::Head) && !Info.bExplosive && !Info.bFall && !Info.bBleed;
	R.Applied = Dmg;

	if (const UWorld* World = GetWorld())
	{
		if (!Info.bBleed)
		{
			LastDamageTime = World->GetTimeSeconds();
		}
	}

	// Already downed: any further damage finishes the character.
	if (bDowned)
	{
		if (Dmg > 0.f && !Info.bBleed)
		{
			bDead = true;
			bDowned = false;
			Health = 0.f;
			R.bKilled = true;
		}
		return R;
	}

	Health -= Dmg;

	// Status effects ----------------------------------------------------------------------
	if (!Info.bExplosive && !Info.bFall && !Info.bBleed && Dmg > 6.f)
	{
		const bool bLimbLeg = Part == ETOBodyPart::LeftLeg || Part == ETOBodyPart::RightLeg;
		const bool bLimbArm = Part == ETOBodyPart::LeftArm || Part == ETOBodyPart::RightArm;
		if (!bBleeding && Part != ETOBodyPart::Head && FMath::FRand() < (R.bArmorHit ? 0.06f : 0.26f))
		{
			bBleeding = true;
		}
		if (bLimbLeg && FMath::FRand() < 0.22f)
		{
			bLegFracture = true;
		}
		if (bLimbArm && FMath::FRand() < 0.18f)
		{
			bArmFracture = true;
		}
	}
	if (Info.bFall && Dmg > 12.f)
	{
		bLegFracture = true;
	}
	if (Info.bExplosive && Dmg > 20.f && FMath::FRand() < 0.45f)
	{
		bBleeding = true;
	}

	if (Health <= 0.f)
	{
		Health = 0.f;
		if (bCanBeDowned && !R.bHeadshot)
		{
			bDowned = true;
			DownedTime = 0.f;
			R.bDowned = true;
			HoTRemaining = 0.f;
		}
		else
		{
			bDead = true;
			R.bKilled = true;
		}
	}
	return R;
}

void UTOHealthComponent::Heal(float Amount)
{
	if (bDead || bDowned)
	{
		return;
	}
	Health = FMath::Clamp(Health + Amount, 0.f, MaxHealth);
}

void UTOHealthComponent::AddHealOverTime(float Amount, float Duration)
{
	if (bDead || bDowned || Duration <= 0.f)
	{
		return;
	}
	HoTRemaining += Amount;
	HoTRate = FMath::Max(HoTRate, Amount / Duration);
}

void UTOHealthComponent::ReviveFromDowned(float NewHealth)
{
	if (bDead)
	{
		return;
	}
	bDowned = false;
	DownedTime = 0.f;
	Health = FMath::Clamp(NewHealth, 1.f, MaxHealth);
	bBleeding = false;
}

void UTOHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDead)
	{
		return;
	}

	if (PainkillerTime > 0.f)
	{
		PainkillerTime = FMath::Max(0.f, PainkillerTime - DeltaTime);
	}

	if (bDowned)
	{
		DownedTime += DeltaTime;
		if (DownedTime >= BleedOutTime)
		{
			bPendingDeathFromStatus = true;
		}
		return;
	}

	// Bleeding drains HP until bandaged.
	if (bBleeding)
	{
		Health -= 1.1f * DeltaTime;
		if (Health <= 0.f)
		{
			Health = 0.f;
			if (bCanBeDowned)
			{
				bDowned = true;
				DownedTime = 0.f;
				bPendingDownedFromStatus = true;
			}
			else
			{
				bPendingDeathFromStatus = true;
			}
			return;
		}
	}

	if (HoTRemaining > 0.f)
	{
		const float Step = FMath::Min(HoTRemaining, HoTRate * DeltaTime);
		HoTRemaining -= Step;
		Health = FMath::Min(MaxHealth, Health + Step);
		if (HoTRemaining <= 0.f)
		{
			HoTRate = 0.f;
		}
	}

	// Warfare style passive regeneration
	if (bPassiveRegen && !bBleeding && GetTimeSinceDamage() > 6.f && Health < MaxHealth)
	{
		Health = FMath::Min(MaxHealth, Health + 9.f * DeltaTime);
	}
}
