// TAC-OPS - grid inventory (pockets, chest rig, backpack, secure container) and gear slots

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/TOTypes.h"
#include "TOInventoryComponent.generated.h"

UCLASS(ClassGroup = (TacOps), meta = (BlueprintSpawnableComponent))
class TACOPS_API UTOInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTOInventoryComponent();

	static constexpr int32 GridPockets = 0;
	static constexpr int32 GridRig = 1;
	static constexpr int32 GridBackpack = 2;
	static constexpr int32 GridSafe = 3;
	static constexpr int32 NumGrids = 4;

	UPROPERTY() FTOGrid Pockets;
	UPROPERTY() FTOGrid Rig;
	UPROPERTY() FTOGrid Backpack;
	UPROPERTY() FTOGrid SafeBox;

	UPROPERTY() FTOItemInstance HelmetItem;
	UPROPERTY() FTOItemInstance ArmorItem;
	UPROPERTY() FTOItemInstance RigItem;
	UPROPERTY() FTOItemInstance BackpackItem;

	/** Bumped on every change so UI / visuals can refresh lazily. */
	int32 Revision = 0;
	void MarkDirty() { ++Revision; }

	void InitDefaults(int32 SafeW = 3, int32 SafeH = 2);
	void ClearAll(bool bKeepSafe);

	FTOGrid* GetGrid(int32 Index);
	const FTOGrid* GetGrid(int32 Index) const;
	static const TCHAR* GetGridLabel(int32 Index);

	/** Auto-places the item (rig -> pockets -> backpack [-> safe]). Returns remaining units (0 = fully added). */
	int32 AddItem(const FTOItemInstance& Item, bool bAllowSafe = false);
	bool AddItemToGrid(int32 GridIndex, const FTOItemInstance& Item);
	bool RemoveByUid(int32 Uid, FTOItemInstance* OutItem = nullptr);
	FTOItemInstance* FindByUid(int32 Uid, int32* OutGridIndex = nullptr);
	int32 CountItem(FName ItemId) const;
	bool ConsumeOne(FName ItemId);

	int32 CountAmmo(FName Caliber, int32 Tier) const;
	/** Takes up to Amount rounds, preferring PreferredTier. Returns rounds taken and the tier used. */
	int32 TakeAmmo(FName Caliber, int32 PreferredTier, int32 Amount, int32& OutTier);
	int32 CountGrenades(ETOGrenadeType Type) const;
	bool HasKey(FName KeyId) const;

	/** Best medical item for the current state, or nullptr. */
	FTOItemInstance* FindBestMedical(bool bBleeding, bool bFracture, bool bHurt);

	/** Equips armor / helmet / rig / backpack. Previously equipped gear and overflowing items go to OutDisplaced. */
	bool EquipGear(const FTOItemInstance& Item, TArray<FTOItemInstance>& OutDisplaced);
	FTOItemInstance* GetGearSlot(ETOItemCategory Category);

	int64 GetTotalValue(bool bIncludeSafe, bool bIncludeGear) const;
	void CollectAll(TArray<FTOItemInstance>& Out, bool bIncludeSafe, bool bIncludeGear) const;

	int32 GetArmorLevel() const;
	int32 GetHelmetLevel() const;
	float GetArmor01() const;
	float GetHelmet01() const;
	bool HasBackpack() const { return BackpackItem.IsValid(); }

private:
	void ResizeGridFromItem(FTOGrid& Grid, const FTOItemInstance& Item, FName GridName, TArray<FTOItemInstance>& OutOverflow);
};
