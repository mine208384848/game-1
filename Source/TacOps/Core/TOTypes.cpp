// TAC-OPS - shared gameplay types (grid inventory implementation)

#include "Core/TOTypes.h"
#include "Core/TODatabase.h"

void FTOGrid::GetFootprint(const FTOItemInstance& Item, int32& OutW, int32& OutH)
{
	const FTOItemDef* Def = TODB::FindItem(Item.ItemId);
	int32 W0 = Def ? Def->W : 1;
	int32 H0 = Def ? Def->H : 1;
	if (Item.bRotated)
	{
		Swap(W0, H0);
	}
	OutW = W0;
	OutH = H0;
}

bool FTOGrid::CanPlaceAt(int32 IW, int32 IH, int32 PX, int32 PY, int32 IgnoreUid) const
{
	if (PX < 0 || PY < 0 || PX + IW > W || PY + IH > H)
	{
		return false;
	}
	for (const FTOItemInstance& It : Items)
	{
		if (It.Uid == IgnoreUid)
		{
			continue;
		}
		int32 OW = 1, OH = 1;
		GetFootprint(It, OW, OH);
		const bool bOverlap = PX < It.X + OW && PX + IW > It.X && PY < It.Y + OH && PY + IH > It.Y;
		if (bOverlap)
		{
			return false;
		}
	}
	return true;
}

bool FTOGrid::FindSpot(int32 IW, int32 IH, int32& OutX, int32& OutY, bool& bOutRotated) const
{
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		if (Pass == 1 && IW == IH)
		{
			break;
		}
		const int32 TW = (Pass == 0) ? IW : IH;
		const int32 TH = (Pass == 0) ? IH : IW;
		for (int32 PY = 0; PY <= H - TH; ++PY)
		{
			for (int32 PX = 0; PX <= W - TW; ++PX)
			{
				if (CanPlaceAt(TW, TH, PX, PY))
				{
					OutX = PX;
					OutY = PY;
					bOutRotated = (Pass == 1);
					return true;
				}
			}
		}
	}
	return false;
}

int32 FTOGrid::TryAdd(const FTOItemInstance& InItem)
{
	if (!IsEnabled() || !InItem.IsValid())
	{
		return FMath::Max(1, InItem.Count);
	}
	const FTOItemDef* Def = TODB::FindItem(InItem.ItemId);
	if (!Def)
	{
		return FMath::Max(1, InItem.Count);
	}

	FTOItemInstance Item = InItem;
	const bool bStackable = Def->Category == ETOItemCategory::Ammo && Def->StackMax > 1;

	if (bStackable)
	{
		// Top up existing stacks first.
		for (FTOItemInstance& Existing : Items)
		{
			if (Existing.ItemId == Item.ItemId && Existing.Count < Def->StackMax)
			{
				const int32 Move = FMath::Min(Def->StackMax - Existing.Count, Item.Count);
				Existing.Count += Move;
				Item.Count -= Move;
				if (Item.Count <= 0)
				{
					return 0;
				}
			}
		}

		bool bFirst = true;
		while (Item.Count > 0)
		{
			const int32 Chunk = FMath::Min(Item.Count, Def->StackMax);
			int32 PX = 0, PY = 0;
			bool bRot = false;
			if (!FindSpot(Def->W, Def->H, PX, PY, bRot))
			{
				return Item.Count;
			}
			FTOItemInstance Piece = Item;
			Piece.Count = Chunk;
			Piece.X = PX;
			Piece.Y = PY;
			Piece.bRotated = bRot;
			if (!bFirst || Piece.Uid == 0)
			{
				Piece.Uid = TODB::NewUid();
			}
			bFirst = false;
			Items.Add(Piece);
			Item.Count -= Chunk;
		}
		return 0;
	}

	int32 PX = 0, PY = 0;
	bool bRot = false;
	if (FindSpot(Def->W, Def->H, PX, PY, bRot))
	{
		Item.X = PX;
		Item.Y = PY;
		Item.bRotated = bRot;
		if (Item.Uid == 0)
		{
			Item.Uid = TODB::NewUid();
		}
		Items.Add(Item);
		return 0;
	}
	return FMath::Max(1, Item.Count);
}

int32 FTOGrid::IndexOfUid(int32 Uid) const
{
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (Items[i].Uid == Uid)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

int64 FTOGrid::TotalValue() const
{
	int64 Sum = 0;
	for (const FTOItemInstance& It : Items)
	{
		Sum += TODB::ItemValue(It);
	}
	return Sum;
}

int32 FTOGrid::CountItem(FName ItemId) const
{
	int32 Sum = 0;
	for (const FTOItemInstance& It : Items)
	{
		if (It.ItemId == ItemId)
		{
			Sum += FMath::Max(1, It.Count);
		}
	}
	return Sum;
}
