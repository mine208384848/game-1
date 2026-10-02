// TAC-OPS - static game database (items, weapons, attachments, operators, loot tables)

#pragma once

#include "CoreMinimal.h"
#include "Core/TOTypes.h"

namespace TODB
{
	// Items -------------------------------------------------------------------------------
	const TArray<FTOItemDef>& Items();
	const FTOItemDef* FindItem(FName Id);

	FName AmmoItemId(FName Caliber, int32 Tier);
	FName ArmorItemId(int32 Level);
	FName HelmetItemId(int32 Level);
	FName RigItemId(int32 Tier);
	FName BackpackItemId(int32 Tier);
	FName WeaponItemId(FName WeaponId);
	FName GrenadeItemId(ETOGrenadeType Type);

	int32 NewUid();
	/** Creates a fresh instance with full durability / uses. Count <= 0 means "default". */
	FTOItemInstance MakeItem(FName Id, int32 Count = 0);
	FTOItemInstance MakeWeaponItem(const FTOWeaponConfig& Config);
	int64 ItemValue(const FTOItemInstance& Item);
	FString ItemName(const FTOItemInstance& Item, bool bShort = false);

	FLinearColor RarityColor(ETORarity Rarity);
	const TCHAR* RarityName(ETORarity Rarity);
	const TCHAR* CategoryName(ETOItemCategory Category);

	// Weapons -----------------------------------------------------------------------------
	const TArray<FTOWeaponDef>& Weapons();
	const FTOWeaponDef* FindWeapon(FName Id);
	const TCHAR* WeaponClassName(ETOWeaponClass Class);
	const TCHAR* FireModeName(ETOFireMode Mode);
	const TCHAR* SlotName(ETOAttachSlot Slot);
	const TCHAR* CaliberName(FName Caliber);

	const TArray<FTOAttachmentDef>& Attachments();
	const FTOAttachmentDef* FindAttachment(FName Id);
	bool IsAttachmentCompatible(const FTOAttachmentDef& Att, const FTOWeaponDef& Weapon);
	/** All compatible attachments for a slot (first entry is always NAME_None = stock part). */
	TArray<FName> GetAttachmentOptions(FName WeaponId, ETOAttachSlot Slot);

	FTOWeaponStats ComputeStats(const FTOWeaponConfig& Config);
	int32 WeaponConfigPrice(const FTOWeaponConfig& Config);
	FTOWeaponConfig MakeDefaultConfig(FName WeaponId, int32 AmmoTier = 3);
	FTOWeaponConfig MakeRandomConfig(int32 Tier, FRandomStream& Rng, bool bAllowSniper = true);
	FName RandomWeaponForTier(int32 Tier, FRandomStream& Rng, bool bAllowSniper = true);
	TArray<FName> WeaponsOfClass(ETOWeaponClass Class);
	TArray<FName> PrimaryWeaponIds();
	TArray<FName> SidearmIds();

	// Operators ---------------------------------------------------------------------------
	const FTOOperatorDef& GetOperator(ETOOperator Op);
	int32 NumOperators();

	// Loadout / economy ------------------------------------------------------------------
	int64 LoadoutCost(const FTOLoadout& Loadout, bool bIncludePrimary = true);
	int64 AmmoCost(FName Caliber, int32 Tier, int32 Rounds);
	void GetMedKit(int32 MedTier, TArray<FName>& OutItems);
	const TCHAR* MedKitName(int32 MedTier);

	// Loot ---------------------------------------------------------------------------------
	void RollLoot(ETOContainerType Type, int32 Tier, FRandomStream& Rng, TArray<FTOItemInstance>& Out);
	FTOItemInstance RollValuable(ETORarity Rarity, bool bTechOnly, FRandomStream& Rng);
	ETORarity RollRarity(int32 Tier, FRandomStream& Rng);
	const TCHAR* ContainerName(ETOContainerType Type);
	float ContainerSearchTime(ETOContainerType Type);
	void ContainerGridSize(ETOContainerType Type, int32& OutW, int32& OutH);
}
