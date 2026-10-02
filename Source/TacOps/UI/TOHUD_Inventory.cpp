// TAC-OPS - grid inventory screen: equipment, pockets / rig / backpack / secure container,
// loot containers with progressive search, context menu and quick transfer.

#include "UI/TOHUD.h"
#include "UI/TOHUDStyle.h"
#include "Core/TOGameMode.h"
#include "Core/TODatabase.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOInventoryComponent.h"
#include "Weapons/TOWeaponComponent.h"
#include "Weapons/TOCombatManager.h"
#include "Audio/TOAudio.h"
#include "World/TOLootContainer.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

namespace
{
	bool IsGear(ETOItemCategory Cat)
	{
		return Cat == ETOItemCategory::Helmet || Cat == ETOItemCategory::Armor || Cat == ETOItemCategory::Rig || Cat == ETOItemCategory::Backpack;
	}

	void PlayInv(const UObject* Ctx, ETOSound Sound, float Volume = 0.5f)
	{
		if (ATOCombatManager* CM = ATOCombatManager::Get(Ctx))
		{
			if (UTOAudio* A = CM->GetAudio())
			{
				A->Play2D(Sound, Volume);
			}
		}
	}
}

// ---------------------------------------------------------------------------------------------
//  Screen
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawInventory()
{
	ATOCharacter* C = GetPlayerChar();
	ATOPlayerController* PC = GetPC();
	if (!C || !PC)
	{
		return;
	}
	UTOInventoryComponent* Inv = C->GetInventory();
	ATOLootContainer* Loot = PC->GetLootTarget();
	Rect(0.f, 0.f, RefW, RefH, FLinearColor(0.01f, 0.012f, 0.015f, 0.84f));

	const float Total = FMath::Min(RefW - 60.f, 1860.f);
	const float X0 = (RefW - Total) * 0.5f;
	const float Top = 92.f;
	const float WA = 400.f;
	const float WB = 520.f;
	const float WC = Total - WA - WB - 40.f;
	const float XA = X0;
	const float XB = XA + WA + 20.f;
	const float XC = XB + WB + 20.f;

	Text(TEXT("INVENTORY"), X0, 30.f, 34.f, TOStyle::Text);
	Text(FString::Printf(TEXT("Carried value  $%s"), *TOUtil::FormatMoney(Inv->GetTotalValue(false, true))), X0 + 250.f, 42.f, 18.f, TOStyle::Money);
	Text(TEXT("[LMB] actions    [RMB] quick move    [Tab] close"), X0 + Total, 42.f, 15.f, TOStyle::Dim, ETOAlign::Right);

	// --- Equipment ---------------------------------------------------------------------------
	Panel(XA, Top, WA, RefH - Top - 30.f);
	Header(TEXT("Equipment"), XA + 16.f, Top + 12.f, WA - 32.f);
	float Y = Top + 54.f;
	DrawGearSlot(XA + 16.f, Y, (WA - 42.f) * 0.5f, 92.f, ETOItemCategory::Helmet, TEXT("HELMET"));
	DrawGearSlot(XA + 26.f + (WA - 42.f) * 0.5f, Y, (WA - 42.f) * 0.5f, 92.f, ETOItemCategory::Armor, TEXT("BODY ARMOR"));
	Y += 102.f;
	DrawGearSlot(XA + 16.f, Y, (WA - 42.f) * 0.5f, 92.f, ETOItemCategory::Rig, TEXT("CHEST RIG"));
	DrawGearSlot(XA + 26.f + (WA - 42.f) * 0.5f, Y, (WA - 42.f) * 0.5f, 92.f, ETOItemCategory::Backpack, TEXT("BACKPACK"));
	Y += 112.f;
	Header(TEXT("Weapons"), XA + 16.f, Y, WA - 32.f);
	Y += 42.f;
	DrawWeaponSlot(XA + 16.f, Y, WA - 32.f, 84.f, UTOWeaponComponent::SlotPrimary, TEXT("PRIMARY"));
	Y += 92.f;
	DrawWeaponSlot(XA + 16.f, Y, WA - 32.f, 84.f, UTOWeaponComponent::SlotSecondary, TEXT("SECONDARY"));
	Y += 92.f;
	DrawWeaponSlot(XA + 16.f, Y, WA - 32.f, 84.f, UTOWeaponComponent::SlotSidearm, TEXT("SIDEARM"));
	Y += 100.f;
	// Status summary
	const FTOOperatorDef& Op = TODB::GetOperator(C->Operator);
	Text(FString::Printf(TEXT("%s  -  %s"), *Op.Name, *Op.Role), XA + 16.f, Y, 16.f, Op.UIColor);
	Text(Fit(FString::Printf(TEXT("Keys: %s%s%s%s%s"),
		Inv->HasKey(TEXT("AdminBlue")) ? TEXT("Blue  ") : TEXT(""),
		Inv->HasKey(TEXT("ServerRed")) ? TEXT("Red  ") : TEXT(""),
		Inv->HasKey(TEXT("Armory")) ? TEXT("Armory  ") : TEXT(""),
		Inv->HasKey(TEXT("DamControl")) ? TEXT("Dam  ") : TEXT(""),
		Inv->HasKey(TEXT("Warehouse")) ? TEXT("Warehouse") : TEXT("")), 14.f, WA - 32.f), XA + 16.f, Y + 26.f, 14.f, TOStyle::Dim);

	// --- Player grids ------------------------------------------------------------------------
	Panel(XB, Top, WB, RefH - Top - 30.f);
	float GY = Top + 14.f;
	int32 TotalRows = 0;
	for (int32 g = 0; g < UTOInventoryComponent::NumGrids; ++g)
	{
		const FTOGrid* G = Inv->GetGrid(g);
		TotalRows += (G && G->IsEnabled()) ? G->H : 1;
	}
	const float Avail = RefH - Top - 30.f - 28.f - UTOInventoryComponent::NumGrids * 46.f;
	const float Cell = FMath::Clamp(Avail / FMath::Max(1, TotalRows), 30.f, 52.f);
	for (int32 g = 0; g < UTOInventoryComponent::NumGrids; ++g)
	{
		const FTOGrid* G = Inv->GetGrid(g);
		if (!G)
		{
			continue;
		}
		const FString Label = UTOInventoryComponent::GetGridLabel(g);
		Header(Label, XB + 16.f, GY, WB - 32.f, 17.f);
		if (G->IsEnabled())
		{
			Text(FString::Printf(TEXT("%dx%d"), G->W, G->H), XB + WB - 16.f, GY + 1.f, 13.f, TOStyle::Dim, ETOAlign::Right);
			GY += 34.f;
			DrawGrid(*G, XB + 16.f, GY, Cell, FTOItemRef::PlayerGrid, g, false);
			GY += G->H * Cell + 12.f;
		}
		else
		{
			GY += 34.f;
			Text(g == UTOInventoryComponent::GridRig ? TEXT("No chest rig equipped") : TEXT("No backpack equipped"), XB + 16.f, GY, 14.f, TOStyle::Dim);
			GY += Cell * 0.6f + 12.f;
		}
	}

	// --- Loot container ----------------------------------------------------------------------
	Panel(XC, Top, WC, RefH - Top - 30.f);
	if (Loot)
	{
		const FTOGrid& LG = Loot->GetGrid();
		Header(Loot->GetDisplayName(), XC + 16.f, Top + 12.f, WC - 32.f);
		float LY = Top + 56.f;
		if (Loot->HasUnrevealed())
		{
			Text(TEXT("SEARCHING..."), XC + 16.f, LY, 15.f, TOStyle::Accent);
			const float T = FMath::Fmod(WorldNow() * 1.2f, 1.f);
			Ring(XC + 150.f, LY + 9.f, 6.f, 9.f, TOStyle::Accent, T, T + 0.35f, 16);
			LY += 26.f;
		}
		else
		{
			Text(FString::Printf(TEXT("%d items  -  $%s"), LG.Items.Num(), *TOUtil::FormatMoney(LG.TotalValue())), XC + 16.f, LY, 15.f, TOStyle::Dim);
			LY += 26.f;
		}
		const float LCell = FMath::Clamp(FMath::Min((WC - 32.f) / FMath::Max(1, LG.W), (RefH - LY - 60.f) / FMath::Max(1, LG.H)), 30.f, 56.f);
		DrawGrid(LG, XC + 16.f, LY, LCell, FTOItemRef::Loot, 0, true);
		const float BY = LY + LG.H * LCell + 16.f;
		if (!Loot->HasUnrevealed() && LG.Items.Num() > 0)
		{
			Button(XC + 16.f, BY, 220.f, 40.f, TEXT("TAKE ALL"), [this]()
			{
				ATOPlayerController* P = GetPC();
				ATOLootContainer* L = P ? P->GetLootTarget() : nullptr;
				if (!L)
				{
					return;
				}
				TArray<int32> Uids;
				for (const FTOItemInstance& It : L->GetGrid().Items)
				{
					Uids.Add(It.Uid);
				}
				for (int32 Uid : Uids)
				{
					FTOItemRef R;
					R.Source = FTOItemRef::Loot;
					R.Uid = Uid;
					ItemTake(R);
				}
			}, true, false, 16.f);
		}
	}
	else
	{
		Header(TEXT("Nearby"), XC + 16.f, Top + 12.f, WC - 32.f);
		Text(TEXT("Open a container, body or crate with [F] to loot it here."), XC + 16.f, Top + 58.f, 15.f, TOStyle::Dim);
		Text(TEXT("Dropped items appear on the ground in front of you."), XC + 16.f, Top + 84.f, 15.f, TOStyle::Dim);
		// Extraction reminder
		if (const ATOGameMode* GM = GetGM())
		{
			if (GM->GetMatchMode() == ETOMatchMode::Operations)
			{
				Text(FString::Printf(TEXT("Raid time left: %s"), *TOUtil::FormatTime(GM->GetRaidTimeLeft())), XC + 16.f, Top + 130.f, 17.f, TOStyle::Text);
				if (Inv->HasBackpack())
				{
					Text(TEXT("Rail Tunnel extraction requires dropping your backpack."), XC + 16.f, Top + 158.f, 14.f, TOStyle::Dim);
				}
			}
		}
	}

	if (RealNow() - InvMessageTime < 2.5f)
	{
		const float TW = TextWidth(InvMessage, 17.f) + 30.f;
		Rect(RefW * 0.5f - TW * 0.5f, RefH - 74.f, TW, 32.f, FLinearColor(0.f, 0.f, 0.f, 0.75f));
		Text(InvMessage, RefW * 0.5f, RefH - 68.f, 17.f, InvMessageColor, ETOAlign::Center);
	}

	if (bMenuOpen)
	{
		DrawContextMenu();
	}
	else if (HoverItem.IsSet())
	{
		DrawTooltip();
	}
}

// ---------------------------------------------------------------------------------------------
//  Drawing helpers
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawGrid(const FTOGrid& Grid, float X, float Y, float Cell, FTOItemRef::ESource Source, int32 GridIndex, bool bShowSearch)
{
	for (int32 GY = 0; GY < Grid.H; ++GY)
	{
		for (int32 GX = 0; GX < Grid.W; ++GX)
		{
			Rect(X + GX * Cell + 1.f, Y + GY * Cell + 1.f, Cell - 2.f, Cell - 2.f, FLinearColor(1.f, 1.f, 1.f, 0.045f));
		}
	}
	Frame(X, Y, Grid.W * Cell, Grid.H * Cell, FLinearColor(1.f, 1.f, 1.f, 0.12f));

	bool bFirstHidden = true;
	for (const FTOItemInstance& It : Grid.Items)
	{
		int32 FW = 1, FH = 1;
		FTOGrid::GetFootprint(It, FW, FH);
		const float IX = X + It.X * Cell;
		const float IY = Y + It.Y * Cell;
		const float IW = FW * Cell;
		const float IH = FH * Cell;
		if (!It.bRevealed)
		{
			Rect(IX + 2.f, IY + 2.f, IW - 4.f, IH - 4.f, FLinearColor(0.1f, 0.1f, 0.11f, 0.95f));
			Text(TEXT("?"), IX + IW * 0.5f, IY + IH * 0.5f - 12.f, 22.f, TOStyle::Dim, ETOAlign::Center);
			if (bShowSearch && bFirstHidden)
			{
				const ATOPlayerController* PC = GetPC();
				const ATOLootContainer* L = PC ? PC->GetLootTarget() : nullptr;
				const float P = L ? L->GetRevealProgress() : 0.f;
				Ring(IX + IW * 0.5f, IY + IH * 0.5f, FMath::Min(IW, IH) * 0.3f, FMath::Min(IW, IH) * 0.36f, TOStyle::Accent, 0.f, P, 24);
			}
			bFirstHidden = false;
			continue;
		}
		FTOItemRef Ref;
		Ref.Source = Source;
		Ref.Index = GridIndex;
		Ref.Uid = It.Uid;
		const bool bHover = Hover(IX, IY, IW, IH);
		if (bHover)
		{
			HoverItem = Ref;
		}
		DrawItemCell(It, IX, IY, IW, IH, bHover, bMenuOpen && MenuItem == Ref);
		Region(IX, IY, IW, IH, [this, Ref]() { OpenItemMenu(Ref); }, [this, Ref]() { ItemQuickMove(Ref); });
	}
}

void ATOHUD::DrawItemCell(const FTOItemInstance& Item, float X, float Y, float W, float H, bool bHovered, bool bSelected)
{
	const FTOItemDef* D = TODB::FindItem(Item.ItemId);
	const FLinearColor RC = D ? TODB::RarityColor(D->Rarity) : TOStyle::Text;
	Rect(X + 2.f, Y + 2.f, W - 4.f, H - 4.f, FLinearColor(RC.R * 0.22f, RC.G * 0.22f, RC.B * 0.22f, 0.95f));
	Rect(X + 2.f, Y + H - 5.f, W - 4.f, 3.f, RC.CopyWithNewOpacity(0.9f));
	if (bHovered || bSelected)
	{
		Frame(X + 1.f, Y + 1.f, W - 2.f, H - 2.f, bSelected ? TOStyle::Accent : FLinearColor(1.f, 1.f, 1.f, 0.6f), bSelected ? 2.f : 1.f);
	}
	if (!D)
	{
		return;
	}
	// Category glyph (simple shapes so no icon textures are needed)
	const float CX = X + W * 0.5f;
	const float CY = Y + H * 0.5f;
	const float G = FMath::Min(W, H) * 0.22f;
	const FLinearColor GC = RC.CopyWithNewOpacity(0.35f);
	switch (D->Category)
	{
	case ETOItemCategory::Medical:
		Rect(CX - G * 0.25f, CY - G, G * 0.5f, G * 2.f, GC);
		Rect(CX - G, CY - G * 0.25f, G * 2.f, G * 0.5f, GC);
		break;
	case ETOItemCategory::Ammo:
		for (int32 i = -1; i <= 1; ++i)
		{
			Rect(CX + i * G * 0.6f - G * 0.2f, CY - G * 0.6f, G * 0.4f, G * 1.4f, GC);
		}
		break;
	case ETOItemCategory::Grenade:
		Disc(CX, CY + G * 0.2f, G * 0.8f, GC, 14);
		break;
	case ETOItemCategory::Key:
		Rect(CX - G, CY - G * 0.6f, G * 2.f, G * 1.2f, GC);
		break;
	case ETOItemCategory::Weapon:
		Rect(X + W * 0.15f, CY - G * 0.3f, W * 0.7f, G * 0.6f, GC);
		Rect(X + W * 0.35f, CY, G * 0.5f, G * 1.2f, GC);
		break;
	case ETOItemCategory::Armor:
	case ETOItemCategory::Helmet:
	case ETOItemCategory::Rig:
	case ETOItemCategory::Backpack:
		Rect(CX - G, CY - G, G * 2.f, G * 2.f, GC);
		break;
	default:
		Diamond(CX, CY, G, GC);
		break;
	}

	const float FS = FMath::Clamp(FMath::Min(W, H) * 0.27f, 11.f, 15.f);
	Text(Fit(D->ShortName.IsEmpty() ? D->Name : D->ShortName, FS, W - 8.f), X + 5.f, Y + 3.f, FS, TOStyle::Text);
	FString Corner;
	switch (D->Category)
	{
	case ETOItemCategory::Ammo: Corner = FString::Printf(TEXT("%d"), Item.Count); break;
	case ETOItemCategory::Medical: Corner = D->Uses > 1 ? FString::Printf(TEXT("%d/%d"), Item.Count, D->Uses) : FString(); break;
	case ETOItemCategory::Armor:
	case ETOItemCategory::Helmet: Corner = FString::Printf(TEXT("Lv%d %.0f"), D->Level, Item.Durability); break;
	case ETOItemCategory::Weapon:
		if (const FTOWeaponDef* WD = TODB::FindWeapon(Item.Weapon.WeaponId.IsNone() ? D->WeaponId : Item.Weapon.WeaponId))
		{
			Corner = TODB::CaliberName(WD->Caliber);
		}
		break;
	default:
		if (D->Category == ETOItemCategory::Valuable && W >= 80.f)
		{
			Corner = FString::Printf(TEXT("$%s"), *TOUtil::FormatMoney(TODB::ItemValue(Item)));
		}
		break;
	}
	if (!Corner.IsEmpty())
	{
		Text(Corner, X + W - 5.f, Y + H - 8.f - FS, FS * 0.9f, TOStyle::Dim, ETOAlign::Right);
	}
}

void ATOHUD::DrawGearSlot(float X, float Y, float W, float H, ETOItemCategory Category, const FString& Label)
{
	ATOCharacter* C = GetPlayerChar();
	UTOInventoryComponent* Inv = C ? C->GetInventory() : nullptr;
	FTOItemInstance* Slot = Inv ? Inv->GetGearSlot(Category) : nullptr;
	Rect(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, 0.04f));
	Frame(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, 0.12f));
	Text(Label, X + 6.f, Y + 4.f, 11.f, TOStyle::Dim);
	if (!Slot || !Slot->IsValid())
	{
		Text(TEXT("EMPTY"), X + W * 0.5f, Y + H * 0.5f - 6.f, 14.f, FLinearColor(1.f, 1.f, 1.f, 0.25f), ETOAlign::Center);
		return;
	}
	FTOItemRef Ref;
	Ref.Source = FTOItemRef::Gear;
	Ref.Index = (int32)Category;
	Ref.Uid = Slot->Uid;
	const bool bHover = Hover(X, Y, W, H);
	if (bHover)
	{
		HoverItem = Ref;
	}
	DrawItemCell(*Slot, X, Y + 16.f, W, H - 16.f, bHover, bMenuOpen && MenuItem == Ref);
	if (Category == ETOItemCategory::Armor || Category == ETOItemCategory::Helmet)
	{
		const FTOItemDef* D = TODB::FindItem(Slot->ItemId);
		const float Pct = D && D->Durability > 0.f ? Slot->Durability / D->Durability : 0.f;
		Bar(X + 6.f, Y + H - 12.f, W - 12.f, 4.f, Pct, Pct > 0.3f ? TOStyle::Armor : TOStyle::Danger, 0.5f);
	}
	Region(X, Y, W, H, [this, Ref]() { OpenItemMenu(Ref); }, [this, Ref]() { ItemQuickMove(Ref); });
}

void ATOHUD::DrawWeaponSlot(float X, float Y, float W, float H, int32 Slot, const FString& Label)
{
	ATOCharacter* C = GetPlayerChar();
	UTOWeaponComponent* Wc = C ? C->GetWeapons() : nullptr;
	Rect(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, 0.04f));
	Frame(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, 0.12f));
	Text(Label, X + 8.f, Y + 6.f, 12.f, TOStyle::Dim);
	if (!Wc || !Wc->HasWeapon(Slot))
	{
		Text(TEXT("EMPTY"), X + W * 0.5f, Y + H * 0.5f - 6.f, 15.f, FLinearColor(1.f, 1.f, 1.f, 0.25f), ETOAlign::Center);
		return;
	}
	const FTOWeaponSlot& WS = Wc->GetSlot(Slot);
	const FTOWeaponDef* D = TODB::FindWeapon(WS.Config.WeaponId);
	FTOItemRef Ref;
	Ref.Source = FTOItemRef::WeaponSlot;
	Ref.Index = Slot;
	Ref.Uid = Slot + 1;
	const bool bHover = Hover(X, Y, W, H);
	if (bHover || (bMenuOpen && MenuItem == Ref))
	{
		Frame(X, Y, W, H, bHover ? FLinearColor(1.f, 1.f, 1.f, 0.6f) : TOStyle::Accent, 1.f);
	}
	const bool bActive = Wc->GetActiveSlotIndex() == Slot;
	Text(D ? D->Name : FString(TEXT("?")), X + 8.f, Y + 24.f, 22.f, bActive ? TOStyle::Accent : TOStyle::Text);
	int32 NumAtt = 0;
	for (const FName& A : WS.Config.Attachments)
	{
		NumAtt += A.IsNone() ? 0 : 1;
	}
	Text(FString::Printf(TEXT("%s  -  %d / %d  T%d  -  %d attachments"), D ? TODB::CaliberName(D->Caliber) : TEXT("-"), WS.MagAmmo, TODB::ComputeStats(WS.Config).MagSize, WS.LoadedTier, NumAtt),
		X + 8.f, Y + 56.f, 13.f, TOStyle::Dim);
	Region(X, Y, W, H, [this, Ref]() { OpenItemMenu(Ref); }, [this, Ref]() { ItemQuickMove(Ref); });
}

// ---------------------------------------------------------------------------------------------
//  Tooltip & context menu
// ---------------------------------------------------------------------------------------------

const FTOItemInstance* ATOHUD::ResolveItem(const FTOItemRef& Ref) const
{
	ATOCharacter* C = GetPlayerChar();
	ATOPlayerController* PC = GetPC();
	if (!C || !PC)
	{
		return nullptr;
	}
	UTOInventoryComponent* Inv = C->GetInventory();
	switch (Ref.Source)
	{
	case FTOItemRef::PlayerGrid:
		return Inv->FindByUid(Ref.Uid);
	case FTOItemRef::Loot:
		if (ATOLootContainer* L = PC->GetLootTarget())
		{
			const int32 Idx = L->GetGrid().IndexOfUid(Ref.Uid);
			return Idx != INDEX_NONE ? &L->GetGrid().Items[Idx] : nullptr;
		}
		return nullptr;
	case FTOItemRef::Gear:
	{
		const FTOItemInstance* G = Inv->GetGearSlot((ETOItemCategory)Ref.Index);
		return (G && G->IsValid()) ? G : nullptr;
	}
	default:
		return nullptr;
	}
}

void ATOHUD::DrawTooltip()
{
	FString Title;
	FLinearColor TitleC = TOStyle::Text;
	TArray<FString> Lines;
	if (HoverItem.Source == FTOItemRef::WeaponSlot)
	{
		const ATOCharacter* C = GetPlayerChar();
		const UTOWeaponComponent* W = C ? C->GetWeapons() : nullptr;
		if (!W || !W->HasWeapon(HoverItem.Index))
		{
			return;
		}
		const FTOWeaponConfig& Cfg = W->GetSlot(HoverItem.Index).Config;
		const FTOWeaponDef* D = TODB::FindWeapon(Cfg.WeaponId);
		Title = D ? D->Name : FString(TEXT("Weapon"));
		for (int32 s = 0; s < (int32)ETOAttachSlot::Count; ++s)
		{
			if (const FTOAttachmentDef* A = TODB::FindAttachment(Cfg.GetAttachment((ETOAttachSlot)s)))
			{
				Lines.Add(FString::Printf(TEXT("%s: %s"), TODB::SlotName((ETOAttachSlot)s), *A->Name));
			}
		}
		Lines.Add(FString::Printf(TEXT("Value $%s"), *TOUtil::FormatMoney(TODB::WeaponConfigPrice(Cfg))));
	}
	else
	{
		const FTOItemInstance* It = ResolveItem(HoverItem);
		const FTOItemDef* D = It ? TODB::FindItem(It->ItemId) : nullptr;
		if (!D)
		{
			return;
		}
		Title = TODB::ItemName(*It);
		TitleC = TODB::RarityColor(D->Rarity);
		Lines.Add(FString::Printf(TEXT("%s  -  %s  -  %dx%d"), TODB::RarityName(D->Rarity), TODB::CategoryName(D->Category), D->W, D->H));
		switch (D->Category)
		{
		case ETOItemCategory::Armor:
		case ETOItemCategory::Helmet:
			Lines.Add(FString::Printf(TEXT("Protection level %d  -  durability %.0f / %.0f"), D->Level, It->Durability, D->Durability));
			break;
		case ETOItemCategory::Ammo:
			Lines.Add(FString::Printf(TEXT("%s  -  penetration tier %d  -  %d rounds"), TODB::CaliberName(D->Caliber), D->Level, It->Count));
			break;
		case ETOItemCategory::Medical:
		{
			FString Fx;
			if (D->Heal > 0.f) Fx += FString::Printf(TEXT("+%.0f HP  "), D->Heal);
			if (D->bStopBleed) Fx += TEXT("stops bleeding  ");
			if (D->bFixFracture) Fx += TEXT("fixes fractures  ");
			if (D->Painkiller > 0.f) Fx += FString::Printf(TEXT("painkiller %.0fs  "), D->Painkiller);
			Lines.Add(Fx);
			Lines.Add(FString::Printf(TEXT("Use time %.1fs  -  uses %d"), D->UseTime, It->Count));
			break;
		}
		case ETOItemCategory::Rig:
		case ETOItemCategory::Backpack:
			Lines.Add(FString::Printf(TEXT("Storage %dx%d"), D->GridW, D->GridH));
			break;
		case ETOItemCategory::Weapon:
		{
			const FTOWeaponStats St = TODB::ComputeStats(It->Weapon.IsValid() ? It->Weapon : TODB::MakeDefaultConfig(D->WeaponId));
			if (St.Def)
			{
				Lines.Add(FString::Printf(TEXT("%s  -  %s  -  %.0f dmg  %.0f rpm"), TODB::WeaponClassName(St.Def->Class), TODB::CaliberName(St.Def->Caliber), St.Damage, St.RPM));
			}
			break;
		}
		default:
			break;
		}
		if (!D->Description.IsEmpty())
		{
			Lines.Add(D->Description);
		}
		Lines.Add(FString::Printf(TEXT("Value $%s"), *TOUtil::FormatMoney(TODB::ItemValue(*It))));
	}

	float W = TextWidth(Title, 18.f) + 24.f;
	for (const FString& L : Lines)
	{
		W = FMath::Max(W, TextWidth(L, 14.f) + 24.f);
	}
	W = FMath::Min(W, 520.f);
	const float H = 38.f + Lines.Num() * 20.f + 8.f;
	float X = Mouse.X + 18.f;
	float Y = Mouse.Y + 18.f;
	if (X + W > RefW - 8.f) X = Mouse.X - W - 12.f;
	if (Y + H > RefH - 8.f) Y = RefH - H - 8.f;
	Rect(X, Y, W, H, FLinearColor(0.02f, 0.025f, 0.03f, 0.96f));
	Rect(X, Y, W, 2.f, TitleC);
	Text(Fit(Title, 18.f, W - 24.f), X + 12.f, Y + 8.f, 18.f, TitleC);
	float LY = Y + 36.f;
	for (const FString& L : Lines)
	{
		Text(Fit(L, 14.f, W - 24.f), X + 12.f, LY, 14.f, TOStyle::Dim);
		LY += 20.f;
	}
}

void ATOHUD::OpenItemMenu(const FTOItemRef& Ref)
{
	MenuItem = Ref;
	MenuPos = ClickPos;
	bMenuOpen = true;
	PlayUISound(false);
}

void ATOHUD::DrawContextMenu()
{
	ATOCharacter* C = GetPlayerChar();
	ATOPlayerController* PC = GetPC();
	if (!C || !PC)
	{
		bMenuOpen = false;
		return;
	}
	const bool bLootOpen = PC->GetLootTarget() != nullptr;
	const FTOItemRef Ref = MenuItem;

	struct FEntry { FString Label; TFunction<void()> Fn; };
	TArray<FEntry> Entries;
	FString Title;

	if (Ref.Source == FTOItemRef::WeaponSlot)
	{
		const UTOWeaponComponent* W = C->GetWeapons();
		if (!W->HasWeapon(Ref.Index))
		{
			bMenuOpen = false;
			return;
		}
		const FTOWeaponDef* D = TODB::FindWeapon(W->GetSlot(Ref.Index).Config.WeaponId);
		Title = D ? D->Name : FString(TEXT("Weapon"));
		TWeakObjectPtr<ATOCharacter> WeakChar = C;
		Entries.Add({ TEXT("Equip (hold in hands)"), [WeakChar, Ref]() { if (ATOCharacter* Ch = WeakChar.Get()) { Ch->InputSelectWeapon(Ref.Index); } } });
		Entries.Add({ bLootOpen ? TEXT("Put into container") : TEXT("Unequip to bag"), [this, Ref, bLootOpen]() { if (bLootOpen) ItemStore(Ref); else ItemTake(Ref); } });
		Entries.Add({ TEXT("Drop"), [this, Ref]() { ItemDrop(Ref); } });
	}
	else
	{
		const FTOItemInstance* It = ResolveItem(Ref);
		const FTOItemDef* D = It ? TODB::FindItem(It->ItemId) : nullptr;
		if (!D)
		{
			bMenuOpen = false;
			return;
		}
		Title = TODB::ItemName(*It);
		const bool bGear = IsGear(D->Category);
		if (Ref.Source == FTOItemRef::Loot)
		{
			Entries.Add({ TEXT("Take"), [this, Ref]() { ItemTake(Ref); } });
			if (bGear || D->Category == ETOItemCategory::Weapon)
			{
				Entries.Add({ TEXT("Equip"), [this, Ref]() { ItemEquip(Ref); } });
			}
			if (D->Category == ETOItemCategory::Medical)
			{
				Entries.Add({ TEXT("Use"), [this, Ref]() { ItemUse(Ref); } });
			}
			Entries.Add({ TEXT("Take into secure container"), [this, Ref]() { ItemToSafe(Ref, true); } });
		}
		else if (Ref.Source == FTOItemRef::PlayerGrid)
		{
			int32 GridIdx = 0;
			C->GetInventory()->FindByUid(Ref.Uid, &GridIdx);
			if (D->Category == ETOItemCategory::Medical)
			{
				Entries.Add({ TEXT("Use"), [this, Ref]() { ItemUse(Ref); } });
			}
			if (D->Category == ETOItemCategory::Grenade)
			{
				Entries.Add({ TEXT("Select as throwable"), [this, Ref]() { ItemUse(Ref); } });
			}
			if (bGear || D->Category == ETOItemCategory::Weapon)
			{
				Entries.Add({ TEXT("Equip"), [this, Ref]() { ItemEquip(Ref); } });
			}
			if (bLootOpen)
			{
				Entries.Add({ TEXT("Put into container"), [this, Ref]() { ItemStore(Ref); } });
			}
			if (GridIdx == UTOInventoryComponent::GridSafe)
			{
				Entries.Add({ TEXT("Move out of secure container"), [this, Ref]() { ItemToSafe(Ref, false); } });
			}
			else
			{
				Entries.Add({ TEXT("Move to secure container"), [this, Ref]() { ItemToSafe(Ref, true); } });
			}
			Entries.Add({ TEXT("Drop"), [this, Ref]() { ItemDrop(Ref); } });
		}
		else if (Ref.Source == FTOItemRef::Gear)
		{
			const bool bContainerGear = D->Category == ETOItemCategory::Rig || D->Category == ETOItemCategory::Backpack;
			if (bLootOpen)
			{
				Entries.Add({ bContainerGear ? TEXT("Put into container (with contents)") : TEXT("Put into container"), [this, Ref]() { ItemStore(Ref); } });
			}
			if (!bContainerGear)
			{
				Entries.Add({ TEXT("Unequip to bag"), [this, Ref]() { ItemTake(Ref); } });
			}
			Entries.Add({ bContainerGear ? TEXT("Drop (with contents)") : TEXT("Drop"), [this, Ref]() { ItemDrop(Ref); } });
		}
	}

	const float W = 300.f;
	const float RowH = 34.f;
	const float H = 34.f + Entries.Num() * RowH + 6.f;
	float X = MenuPos.X + 6.f;
	float Y = MenuPos.Y + 6.f;
	if (X + W > RefW - 8.f) X = MenuPos.X - W - 6.f;
	if (Y + H > RefH - 8.f) Y = RefH - H - 8.f;

	// Clicking anywhere else closes the menu.
	Region(0.f, 0.f, RefW, RefH, [this]() { bMenuOpen = false; }, [this]() { bMenuOpen = false; });
	Rect(X, Y, W, H, FLinearColor(0.03f, 0.035f, 0.04f, 0.98f));
	Rect(X, Y, W, 2.f, TOStyle::Accent);
	Text(Fit(Title, 15.f, W - 20.f), X + 10.f, Y + 8.f, 15.f, TOStyle::Dim);
	float RY = Y + 34.f;
	for (FEntry& E : Entries)
	{
		const bool bHover = Hover(X + 4.f, RY, W - 8.f, RowH - 2.f);
		Rect(X + 4.f, RY, W - 8.f, RowH - 2.f, bHover ? TOStyle::Accent.CopyWithNewOpacity(0.9f) : FLinearColor(1.f, 1.f, 1.f, 0.04f));
		Text(E.Label, X + 16.f, RY + 7.f, 16.f, bHover ? TOStyle::TextOnAccent : TOStyle::Text, ETOAlign::Left, !bHover);
		Region(X + 4.f, RY, W - 8.f, RowH - 2.f, [this, Fn = MoveTemp(E.Fn)]()
		{
			bMenuOpen = false;
			Fn();
		});
		RY += RowH;
	}
}

// ---------------------------------------------------------------------------------------------
//  Item operations
// ---------------------------------------------------------------------------------------------

void ATOHUD::InventoryMessage(const FString& Msg, const FLinearColor& Color)
{
	InvMessage = Msg;
	InvMessageColor = Color;
	InvMessageTime = RealNow();
}

bool ATOHUD::ExtractItem(const FTOItemRef& Ref, FTOItemInstance& Out)
{
	ATOCharacter* C = GetPlayerChar();
	ATOPlayerController* PC = GetPC();
	if (!C || !PC)
	{
		return false;
	}
	UTOInventoryComponent* Inv = C->GetInventory();
	switch (Ref.Source)
	{
	case FTOItemRef::PlayerGrid:
		return Inv->RemoveByUid(Ref.Uid, &Out);
	case FTOItemRef::Loot:
	{
		ATOLootContainer* L = PC->GetLootTarget();
		if (!L)
		{
			return false;
		}
		FTOGrid& G = L->GetGrid();
		const int32 Idx = G.IndexOfUid(Ref.Uid);
		if (Idx == INDEX_NONE || !G.Items[Idx].bRevealed)
		{
			return false;
		}
		Out = G.Items[Idx];
		G.Items.RemoveAt(Idx);
		++L->Revision;
		return true;
	}
	case FTOItemRef::Gear:
	{
		TArray<FTOItemInstance> Contents;
		if (!Inv->UnequipGear((ETOItemCategory)Ref.Index, Out, Contents))
		{
			return false;
		}
		for (const FTOItemInstance& It : Contents)
		{
			GiveOrDrop(It, false);
		}
		C->RefreshGearVisuals();
		return true;
	}
	case FTOItemRef::WeaponSlot:
	{
		UTOWeaponComponent* W = C->GetWeapons();
		if (Ref.Index == UTOWeaponComponent::SlotMelee || !W->HasWeapon(Ref.Index))
		{
			return false;
		}
		const FTOWeaponSlot Slot = W->GetSlot(Ref.Index);
		Out = TODB::MakeWeaponItem(Slot.Config);
		// Rounds still in the magazine go back to the bag.
		const FTOWeaponDef* D = TODB::FindWeapon(Slot.Config.WeaponId);
		if (D && Slot.MagAmmo > 0)
		{
			GiveOrDrop(TODB::MakeItem(TODB::AmmoItemId(D->Caliber, Slot.LoadedTier), Slot.MagAmmo), false);
		}
		W->ClearWeapon(Ref.Index);
		return true;
	}
	default:
		return false;
	}
}

void ATOHUD::GiveOrDrop(const FTOItemInstance& InItem, bool bPreferContainer)
{
	ATOCharacter* C = GetPlayerChar();
	ATOPlayerController* PC = GetPC();
	if (!C || !PC || !InItem.IsValid())
	{
		return;
	}
	UTOInventoryComponent* Inv = C->GetInventory();
	ATOLootContainer* L = PC->GetLootTarget();
	FTOItemInstance Item = InItem;
	Item.bRevealed = true;

	auto TryContainer = [&]() -> bool
	{
		if (!L)
		{
			return false;
		}
		const int32 Left = L->GetGrid().TryAdd(Item);
		++L->Revision;
		if (Left <= 0)
		{
			return true;
		}
		Item.Count = FMath::Min(Item.Count, Left);
		return false;
	};
	auto TryBag = [&]() -> bool
	{
		const int32 Left = Inv->AddItem(Item, false);
		if (Left <= 0)
		{
			return true;
		}
		Item.Count = FMath::Min(Item.Count, Left);
		return false;
	};

	if (bPreferContainer ? (TryContainer() || TryBag()) : (TryBag() || TryContainer()))
	{
		return;
	}
	DropToWorld({ Item }, TODB::ItemName(Item), false);
}

void ATOHUD::DropToWorld(const TArray<FTOItemInstance>& Items, const FString& Name, bool bBag)
{
	ATOCharacter* C = GetPlayerChar();
	if (!C || Items.Num() == 0)
	{
		return;
	}
	UWorld* World = GetWorld();
	const float Half = C->GetCapsuleComponent() ? C->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 88.f;
	const FVector Fwd = FRotator(0.f, C->GetActorRotation().Yaw, 0.f).Vector();
	FVector Loc = C->GetActorLocation() + Fwd * 70.f - FVector(0.f, 0.f, Half - 15.f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TODrop), false, C);
	if (World->LineTraceSingleByChannel(Hit, Loc + FVector(0.f, 0.f, 80.f), Loc - FVector(0.f, 0.f, 300.f), ECC_Visibility, Params))
	{
		Loc = Hit.ImpactPoint + FVector(0.f, 0.f, 2.f);
	}
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (ATOLootContainer* Box = World->SpawnActor<ATOLootContainer>(ATOLootContainer::StaticClass(), Loc, FRotator(0.f, C->GetActorRotation().Yaw, 0.f), SP))
	{
		const bool bLoose = !bBag && Items.Num() == 1;
		Box->InitWithItems(bLoose ? ETOContainerType::LooseItem : ETOContainerType::Duffel, Name, Items);
	}
	PlayInv(this, ETOSound::Pickup, 0.4f);
}

void ATOHUD::ItemTake(const FTOItemRef& Ref)
{
	ATOCharacter* C = GetPlayerChar();
	ATOPlayerController* PC = GetPC();
	if (!C || !PC)
	{
		return;
	}
	UTOInventoryComponent* Inv = C->GetInventory();
	if (Ref.Source == FTOItemRef::Gear || Ref.Source == FTOItemRef::WeaponSlot)
	{
		// "Unequip to bag"
		FTOItemInstance Item;
		if (ExtractItem(Ref, Item))
		{
			GiveOrDrop(Item, false);
			PlayInv(this, ETOSound::Pickup);
		}
		return;
	}
	if (Ref.Source != FTOItemRef::Loot)
	{
		return;
	}
	ATOLootContainer* L = PC->GetLootTarget();
	FTOItemInstance Item;
	if (!L || !ExtractItem(Ref, Item))
	{
		return;
	}
	const int32 Left = Inv->AddItem(Item, false);
	if (Left > 0)
	{
		Item.Count = FMath::Min(Item.Count, Left);
		L->GetGrid().TryAdd(Item);
		++L->Revision;
		InventoryMessage(TEXT("Not enough space"), TOStyle::Danger);
		PlayInv(this, ETOSound::DryFire, 0.4f);
		return;
	}
	PlayInv(this, ETOSound::Pickup);
}

void ATOHUD::ItemStore(const FTOItemRef& Ref)
{
	ATOCharacter* C = GetPlayerChar();
	ATOPlayerController* PC = GetPC();
	ATOLootContainer* L = PC ? PC->GetLootTarget() : nullptr;
	if (!C || !L)
	{
		return;
	}
	UTOInventoryComponent* Inv = C->GetInventory();
	if (Ref.Source == FTOItemRef::Gear)
	{
		// Rig / backpack go into the container together with their contents.
		FTOItemInstance Item;
		TArray<FTOItemInstance> Contents;
		if (!Inv->UnequipGear((ETOItemCategory)Ref.Index, Item, Contents))
		{
			return;
		}
		C->RefreshGearVisuals();
		Contents.Insert(Item, 0);
		for (const FTOItemInstance& It : Contents)
		{
			GiveOrDrop(It, true);
		}
		PlayInv(this, ETOSound::Pickup);
		return;
	}
	int32 FromGrid = UTOInventoryComponent::GridPockets;
	if (Ref.Source == FTOItemRef::PlayerGrid)
	{
		Inv->FindByUid(Ref.Uid, &FromGrid);
	}
	FTOItemInstance Item;
	if (!ExtractItem(Ref, Item))
	{
		return;
	}
	Item.bRevealed = true;
	const int32 Left = L->GetGrid().TryAdd(Item);
	++L->Revision;
	if (Left > 0)
	{
		Item.Count = FMath::Min(Item.Count, Left);
		if (Ref.Source == FTOItemRef::WeaponSlot || !Inv->AddItemToGrid(FromGrid, Item))
		{
			GiveOrDrop(Item, false);
		}
		InventoryMessage(TEXT("Container is full"), TOStyle::Danger);
		return;
	}
	PlayInv(this, ETOSound::Pickup);
}

void ATOHUD::ItemToSafe(const FTOItemRef& Ref, bool bIntoSafe)
{
	ATOCharacter* C = GetPlayerChar();
	ATOPlayerController* PC = GetPC();
	if (!C || !PC)
	{
		return;
	}
	UTOInventoryComponent* Inv = C->GetInventory();
	int32 FromGrid = UTOInventoryComponent::GridPockets;
	if (Ref.Source == FTOItemRef::PlayerGrid)
	{
		Inv->FindByUid(Ref.Uid, &FromGrid);
	}
	FTOItemInstance Item;
	if (!ExtractItem(Ref, Item))
	{
		return;
	}
	if (bIntoSafe)
	{
		if (Inv->AddItemToGrid(UTOInventoryComponent::GridSafe, Item))
		{
			PlayInv(this, ETOSound::Pickup);
			return;
		}
		InventoryMessage(TEXT("Secure container is full"), TOStyle::Danger);
	}
	else
	{
		if (Inv->AddItem(Item, false) == 0)
		{
			PlayInv(this, ETOSound::Pickup);
			return;
		}
		InventoryMessage(TEXT("Not enough space"), TOStyle::Danger);
	}
	// Put it back where it came from.
	if (Ref.Source == FTOItemRef::Loot)
	{
		if (ATOLootContainer* L = PC->GetLootTarget())
		{
			L->GetGrid().TryAdd(Item);
			++L->Revision;
		}
	}
	else if (!Inv->AddItemToGrid(FromGrid, Item))
	{
		GiveOrDrop(Item, false);
	}
}

void ATOHUD::ItemUse(const FTOItemRef& Ref)
{
	ATOCharacter* C = GetPlayerChar();
	if (!C)
	{
		return;
	}
	if (Ref.Source == FTOItemRef::Loot)
	{
		ItemTake(Ref);
		if (!C->GetInventory()->FindByUid(Ref.Uid))
		{
			return;
		}
	}
	if (!C->UseItemByUid(Ref.Uid))
	{
		InventoryMessage(C->IsUsingItem() ? TEXT("Already using an item") : TEXT("Cannot use that now"));
		return;
	}
	const FTOItemInstance* It = C->GetInventory()->FindByUid(Ref.Uid);
	const FTOItemDef* D = It ? TODB::FindItem(It->ItemId) : nullptr;
	if (D && D->Category == ETOItemCategory::Grenade)
	{
		InventoryMessage(FString::Printf(TEXT("%s selected"), *D->Name), TOStyle::Accent);
	}
}

void ATOHUD::ItemEquip(const FTOItemRef& Ref)
{
	ATOCharacter* C = GetPlayerChar();
	ATOPlayerController* PC = GetPC();
	if (!C || !PC)
	{
		return;
	}
	const FTOItemInstance* Peek = ResolveItem(Ref);
	const FTOItemDef* D = Peek ? TODB::FindItem(Peek->ItemId) : nullptr;
	if (!D)
	{
		return;
	}
	const bool bLootOpen = PC->GetLootTarget() != nullptr;
	UTOInventoryComponent* Inv = C->GetInventory();

	if (IsGear(D->Category))
	{
		FTOItemInstance Item;
		if (!ExtractItem(Ref, Item))
		{
			return;
		}
		TArray<FTOItemInstance> Displaced;
		Inv->EquipGear(Item, Displaced);
		for (const FTOItemInstance& Old : Displaced)
		{
			GiveOrDrop(Old, bLootOpen);
		}
		C->RefreshGearVisuals();
		PlayInv(this, ETOSound::Pickup);
		return;
	}

	if (D->Category == ETOItemCategory::Weapon)
	{
		const FTOWeaponConfig Config = Peek->Weapon.IsValid() ? Peek->Weapon : TODB::MakeDefaultConfig(D->WeaponId);
		const FTOWeaponDef* WD = TODB::FindWeapon(Config.WeaponId);
		if (!WD)
		{
			return;
		}
		UTOWeaponComponent* W = C->GetWeapons();
		int32 Slot = UTOWeaponComponent::SlotSidearm;
		if (WD->Class != ETOWeaponClass::Pistol)
		{
			if (!W->HasWeapon(UTOWeaponComponent::SlotPrimary))
			{
				Slot = UTOWeaponComponent::SlotPrimary;
			}
			else if (!W->HasWeapon(UTOWeaponComponent::SlotSecondary))
			{
				Slot = UTOWeaponComponent::SlotSecondary;
			}
			else
			{
				Slot = W->GetActiveSlotIndex() == UTOWeaponComponent::SlotSecondary ? UTOWeaponComponent::SlotSecondary : UTOWeaponComponent::SlotPrimary;
			}
		}
		FTOItemInstance Item;
		if (!ExtractItem(Ref, Item))
		{
			return;
		}
		if (W->HasWeapon(Slot))
		{
			FTOItemRef OldRef;
			OldRef.Source = FTOItemRef::WeaponSlot;
			OldRef.Index = Slot;
			FTOItemInstance Old;
			if (ExtractItem(OldRef, Old))
			{
				GiveOrDrop(Old, bLootOpen);
			}
		}
		// Picked-up guns come with an empty magazine - reload with matching ammo.
		W->SetWeapon(Slot, Config, false);
		W->EquipSlot(Slot);
		PlayInv(this, ETOSound::ReloadEnd, 0.5f);
	}
}

void ATOHUD::ItemDrop(const FTOItemRef& Ref)
{
	ATOCharacter* C = GetPlayerChar();
	if (!C)
	{
		return;
	}
	UTOInventoryComponent* Inv = C->GetInventory();
	if (Ref.Source == FTOItemRef::Gear)
	{
		FTOItemInstance Item;
		TArray<FTOItemInstance> Contents;
		if (!Inv->UnequipGear((ETOItemCategory)Ref.Index, Item, Contents))
		{
			return;
		}
		C->RefreshGearVisuals();
		const FString Name = TODB::ItemName(Item);
		if (Contents.Num() > 0)
		{
			Contents.Insert(Item, 0);
			DropToWorld(Contents, FString::Printf(TEXT("Dropped %s"), *Name), true);
		}
		else
		{
			DropToWorld({ Item }, Name, false);
		}
		return;
	}
	FTOItemInstance Item;
	if (ExtractItem(Ref, Item))
	{
		DropToWorld({ Item }, TODB::ItemName(Item), false);
	}
}

void ATOHUD::ItemQuickMove(const FTOItemRef& Ref)
{
	ATOPlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	bMenuOpen = false;
	const bool bLootOpen = PC->GetLootTarget() != nullptr;
	switch (Ref.Source)
	{
	case FTOItemRef::Loot:
		ItemTake(Ref);
		break;
	case FTOItemRef::PlayerGrid:
		if (bLootOpen)
		{
			ItemStore(Ref);
		}
		else if (const FTOItemInstance* It = ResolveItem(Ref))
		{
			const FTOItemDef* D = TODB::FindItem(It->ItemId);
			if (D && D->Category == ETOItemCategory::Medical)
			{
				ItemUse(Ref);
			}
			else if (D && (IsGear(D->Category) || D->Category == ETOItemCategory::Weapon))
			{
				ItemEquip(Ref);
			}
		}
		break;
	case FTOItemRef::Gear:
	case FTOItemRef::WeaponSlot:
		if (bLootOpen)
		{
			ItemStore(Ref);
		}
		break;
	default:
		break;
	}
}
