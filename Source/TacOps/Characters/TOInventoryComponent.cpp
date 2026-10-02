// TAC-OPS - grid inventory

#include "Characters/TOInventoryComponent.h"
#include "Core/TODatabase.h"

UTOInventoryComponent::UTOInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	InitDefaults();
}

void UTOInventoryComponent::InitDefaults(int32 SafeW, int32 SafeH)
{
	Pockets.Init(FName(TEXT("Pockets")), 5, 1);
	Rig.Init(FName(TEXT("Chest Rig")), 0, 0);
	Backpack.Init(FName(TEXT("Backpack")), 0, 0);
	SafeBox.Init(FName(TEXT("Safe Box")), SafeW, SafeH);
	HelmetItem = FTOItemInstance();
	ArmorItem = FTOItemInstance();
	RigItem = FTOItemInstance();
	BackpackItem = FTOItemInstance();
	MarkDirty();
}

void UTOInventoryComponent::ClearAll(bool bKeepSafe)
{
	const TArray<FTOItemInstance> SafeItems = SafeBox.Items;
	const int32 SW = SafeBox.W;
	const int32 SH = SafeBox.H;
	InitDefaults(SW, SH);
	if (bKeepSafe)
	{
		SafeBox.Items = SafeItems;
	}
	MarkDirty();
}

FTOGrid* UTOInventoryComponent::GetGrid(int32 Index)
{
	switch (Index)
	{
	case GridPockets: return &Pockets;
	case GridRig: return &Rig;
	case GridBackpack: return &Backpack;
	case GridSafe: return &SafeBox;
	default: return nullptr;
	}
}

const FTOGrid* UTOInventoryComponent::GetGrid(int32 Index) const
{
	return const_cast<UTOInventoryComponent*>(this)->GetGrid(Index);
}

const TCHAR* UTOInventoryComponent::GetGridLabel(int32 Index)
{
	switch (Index)
	{
	case GridPockets: return TEXT("POCKETS");
	case GridRig: return TEXT("CHEST RIG");
	case GridBackpack: return TEXT("BACKPACK");
	case GridSafe: return TEXT("SAFE BOX");
	default: return TEXT("");
	}
}

int32 UTOInventoryComponent::AddItem(const FTOItemInstance& Item, bool bAllowSafe)
{
	if (!Item.IsValid())
	{
		return 0;
	}
	FTOItemInstance Remaining = Item;
	static const int32 Order[] = { GridRig, GridPockets, GridBackpack, GridSafe };
	for (int32 GridIdx : Order)
	{
		if (GridIdx == GridSafe && !bAllowSafe)
		{
			continue;
		}
		FTOGrid* G = GetGrid(GridIdx);
		if (!G || !G->IsEnabled())
		{
			continue;
		}
		const int32 Left = G->TryAdd(Remaining);
		if (Left <= 0)
		{
			MarkDirty();
			return 0;
		}
		// Partial stack placement for ammo
		if (Left < Remaining.Count)
		{
			Remaining.Count = Left;
			Remaining.Uid = TODB::NewUid();
		}
	}
	MarkDirty();
	return FMath::Max(1, Remaining.Count);
}

bool UTOInventoryComponent::AddItemToGrid(int32 GridIndex, const FTOItemInstance& Item)
{
	FTOGrid* G = GetGrid(GridIndex);
	if (!G || !G->IsEnabled())
	{
		return false;
	}
	const bool bOk = G->TryAdd(Item) == 0;
	MarkDirty();
	return bOk;
}

bool UTOInventoryComponent::RemoveByUid(int32 Uid, FTOItemInstance* OutItem)
{
	for (int32 g = 0; g < NumGrids; ++g)
	{
		FTOGrid* G = GetGrid(g);
		const int32 Idx = G ? G->IndexOfUid(Uid) : INDEX_NONE;
		if (Idx != INDEX_NONE)
		{
			if (OutItem)
			{
				*OutItem = G->Items[Idx];
			}
			G->Items.RemoveAt(Idx);
			MarkDirty();
			return true;
		}
	}
	return false;
}

FTOItemInstance* UTOInventoryComponent::FindByUid(int32 Uid, int32* OutGridIndex)
{
	for (int32 g = 0; g < NumGrids; ++g)
	{
		FTOGrid* G = GetGrid(g);
		const int32 Idx = G ? G->IndexOfUid(Uid) : INDEX_NONE;
		if (Idx != INDEX_NONE)
		{
			if (OutGridIndex)
			{
				*OutGridIndex = g;
			}
			return &G->Items[Idx];
		}
	}
	return nullptr;
}

int32 UTOInventoryComponent::CountItem(FName ItemId) const
{
	int32 Sum = 0;
	for (int32 g = 0; g < NumGrids; ++g)
	{
		if (const FTOGrid* G = GetGrid(g))
		{
			Sum += G->CountItem(ItemId);
		}
	}
	return Sum;
}

bool UTOInventoryComponent::ConsumeOne(FName ItemId)
{
	for (int32 g = 0; g < NumGrids; ++g)
	{
		FTOGrid* G = GetGrid(g);
		if (!G)
		{
			continue;
		}
		for (int32 i = 0; i < G->Items.Num(); ++i)
		{
			FTOItemInstance& It = G->Items[i];
			if (It.ItemId == ItemId)
			{
				It.Count -= 1;
				if (It.Count <= 0)
				{
					G->Items.RemoveAt(i);
				}
				MarkDirty();
				return true;
			}
		}
	}
	return false;
}

int32 UTOInventoryComponent::CountAmmo(FName Caliber, int32 Tier) const
{
	int32 Sum = 0;
	for (int32 g = 0; g < NumGrids; ++g)
	{
		const FTOGrid* G = GetGrid(g);
		if (!G)
		{
			continue;
		}
		for (const FTOItemInstance& It : G->Items)
		{
			const FTOItemDef* D = TODB::FindItem(It.ItemId);
			if (D && D->Category == ETOItemCategory::Ammo && D->Caliber == Caliber && (Tier <= 0 || D->Level == Tier))
			{
				Sum += It.Count;
			}
		}
	}
	return Sum;
}

int32 UTOInventoryComponent::TakeAmmo(FName Caliber, int32 PreferredTier, int32 Amount, int32& OutTier)
{
	OutTier = PreferredTier;
	int32 Taken = 0;
	// Pass 0: preferred tier, pass 1: highest other tier available.
	for (int32 Pass = 0; Pass < 2 && Taken < Amount; ++Pass)
	{
		int32 TierToUse = PreferredTier;
		if (Pass == 1)
		{
			if (Taken > 0)
			{
				break; // don't mix ammo types in one magazine
			}
			TierToUse = -1;
			for (int32 T = 6; T >= 1; --T)
			{
				if (T != PreferredTier && CountAmmo(Caliber, T) > 0)
				{
					TierToUse = T;
					break;
				}
			}
			if (TierToUse < 0)
			{
				break;
			}
		}
		// Safe box ammo is never auto-used.
		for (int32 g = 0; g < GridSafe && Taken < Amount; ++g)
		{
			FTOGrid* G = GetGrid(g);
			if (!G)
			{
				continue;
			}
			for (int32 i = G->Items.Num() - 1; i >= 0 && Taken < Amount; --i)
			{
				FTOItemInstance& It = G->Items[i];
				const FTOItemDef* D = TODB::FindItem(It.ItemId);
				if (D && D->Category == ETOItemCategory::Ammo && D->Caliber == Caliber && D->Level == TierToUse)
				{
					const int32 Take = FMath::Min(It.Count, Amount - Taken);
					It.Count -= Take;
					Taken += Take;
					OutTier = TierToUse;
					if (It.Count <= 0)
					{
						G->Items.RemoveAt(i);
					}
				}
			}
		}
	}
	if (Taken > 0)
	{
		MarkDirty();
	}
	return Taken;
}

int32 UTOInventoryComponent::CountGrenades(ETOGrenadeType Type) const
{
	return CountItem(TODB::GrenadeItemId(Type));
}

bool UTOInventoryComponent::HasKey(FName KeyId) const
{
	for (int32 g = 0; g < NumGrids; ++g)
	{
		const FTOGrid* G = GetGrid(g);
		if (!G)
		{
			continue;
		}
		for (const FTOItemInstance& It : G->Items)
		{
			const FTOItemDef* D = TODB::FindItem(It.ItemId);
			if (D && D->Category == ETOItemCategory::Key && D->KeyId == KeyId)
			{
				return true;
			}
		}
	}
	return false;
}

FTOItemInstance* UTOInventoryComponent::FindBestMedical(bool bBleeding, bool bFracture, bool bHurt)
{
	FTOItemInstance* Best = nullptr;
	float BestScore = 0.f;
	for (int32 g = 0; g < NumGrids; ++g)
	{
		FTOGrid* G = GetGrid(g);
		if (!G)
		{
			continue;
		}
		for (FTOItemInstance& It : G->Items)
		{
			const FTOItemDef* D = TODB::FindItem(It.ItemId);
			if (!D || D->Category != ETOItemCategory::Medical)
			{
				continue;
			}
			float Score = 0.f;
			if (bBleeding && D->bStopBleed) Score += 10.f;
			if (bFracture && D->bFixFracture) Score += 8.f;
			if (bHurt && D->Heal > 0.f) Score += D->Heal * 0.1f;
			// Prefer cheap items for simple problems.
			Score -= D->UseTime * 0.3f;
			if (Score > BestScore)
			{
				BestScore = Score;
				Best = &It;
			}
		}
	}
	return Best;
}

FTOItemInstance* UTOInventoryComponent::GetGearSlot(ETOItemCategory Category)
{
	switch (Category)
	{
	case ETOItemCategory::Helmet: return &HelmetItem;
	case ETOItemCategory::Armor: return &ArmorItem;
	case ETOItemCategory::Rig: return &RigItem;
	case ETOItemCategory::Backpack: return &BackpackItem;
	default: return nullptr;
	}
}

void UTOInventoryComponent::ResizeGridFromItem(FTOGrid& Grid, const FTOItemInstance& Item, FName GridName, TArray<FTOItemInstance>& OutOverflow)
{
	const TArray<FTOItemInstance> OldItems = Grid.Items;
	const FTOItemDef* D = Item.IsValid() ? TODB::FindItem(Item.ItemId) : nullptr;
	Grid.Init(GridName, D ? D->GridW : 0, D ? D->GridH : 0);
	for (const FTOItemInstance& Old : OldItems)
	{
		FTOItemInstance Copy = Old;
		Copy.bRotated = false;
		if (Grid.TryAdd(Copy) != 0)
		{
			// Try any other grid before giving up.
			if (AddItem(Copy, false) != 0)
			{
				OutOverflow.Add(Old);
			}
		}
	}
}

bool UTOInventoryComponent::EquipGear(const FTOItemInstance& Item, TArray<FTOItemInstance>& OutDisplaced)
{
	const FTOItemDef* D = TODB::FindItem(Item.ItemId);
	if (!D)
	{
		return false;
	}
	FTOItemInstance* Slot = GetGearSlot(D->Category);
	if (!Slot)
	{
		return false;
	}
	if (Slot->IsValid())
	{
		OutDisplaced.Add(*Slot);
	}
	*Slot = Item;
	Slot->bRotated = false;
	Slot->bRevealed = true;

	if (D->Category == ETOItemCategory::Rig)
	{
		ResizeGridFromItem(Rig, RigItem, FName(TEXT("Chest Rig")), OutDisplaced);
	}
	else if (D->Category == ETOItemCategory::Backpack)
	{
		ResizeGridFromItem(Backpack, BackpackItem, FName(TEXT("Backpack")), OutDisplaced);
	}
	MarkDirty();
	return true;
}

int64 UTOInventoryComponent::GetTotalValue(bool bIncludeSafe, bool bIncludeGear) const
{
	int64 Sum = 0;
	for (int32 g = 0; g < NumGrids; ++g)
	{
		if (g == GridSafe && !bIncludeSafe)
		{
			continue;
		}
		if (const FTOGrid* G = GetGrid(g))
		{
			Sum += G->TotalValue();
		}
	}
	if (bIncludeGear)
	{
		Sum += TODB::ItemValue(HelmetItem) + TODB::ItemValue(ArmorItem) + TODB::ItemValue(RigItem) + TODB::ItemValue(BackpackItem);
	}
	return Sum;
}

void UTOInventoryComponent::CollectAll(TArray<FTOItemInstance>& Out, bool bIncludeSafe, bool bIncludeGear) const
{
	if (bIncludeGear)
	{
		if (HelmetItem.IsValid()) Out.Add(HelmetItem);
		if (ArmorItem.IsValid()) Out.Add(ArmorItem);
		if (RigItem.IsValid()) Out.Add(RigItem);
		if (BackpackItem.IsValid()) Out.Add(BackpackItem);
	}
	for (int32 g = 0; g < NumGrids; ++g)
	{
		if (g == GridSafe && !bIncludeSafe)
		{
			continue;
		}
		if (const FTOGrid* G = GetGrid(g))
		{
			Out.Append(G->Items);
		}
	}
}

int32 UTOInventoryComponent::GetArmorLevel() const
{
	const FTOItemDef* D = ArmorItem.IsValid() ? TODB::FindItem(ArmorItem.ItemId) : nullptr;
	return D ? D->Level : 0;
}

int32 UTOInventoryComponent::GetHelmetLevel() const
{
	const FTOItemDef* D = HelmetItem.IsValid() ? TODB::FindItem(HelmetItem.ItemId) : nullptr;
	return D ? D->Level : 0;
}

float UTOInventoryComponent::GetArmor01() const
{
	const FTOItemDef* D = ArmorItem.IsValid() ? TODB::FindItem(ArmorItem.ItemId) : nullptr;
	return (D && D->Durability > 0.f) ? FMath::Clamp(ArmorItem.Durability / D->Durability, 0.f, 1.f) : 0.f;
}

float UTOInventoryComponent::GetHelmet01() const
{
	const FTOItemDef* D = HelmetItem.IsValid() ? TODB::FindItem(HelmetItem.ItemId) : nullptr;
	return (D && D->Durability > 0.f) ? FMath::Clamp(HelmetItem.Durability / D->Durability, 0.f, 1.f) : 0.f;
}
