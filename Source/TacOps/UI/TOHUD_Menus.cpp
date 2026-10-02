// TAC-OPS - menus: loading, lobby, Operations / Warfare preparation, gunsmith, settings,
// pause, death, results and the Warfare redeploy screen.

#include "UI/TOHUD.h"
#include "UI/TOHUDStyle.h"
#include "Core/TOGameMode.h"
#include "Core/TOGameInstance.h"
#include "Core/TOSaveGame.h"
#include "Core/TODatabase.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOInventoryComponent.h"
#include "World/TOWorldGenerator.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

namespace
{
	const TCHAR* TimeName(ETOTimeOfDay T)
	{
		switch (T)
		{
		case ETOTimeOfDay::Day: return TEXT("Day");
		case ETOTimeOfDay::Sunset: return TEXT("Sunset");
		default: return TEXT("Night");
		}
	}

	FString WeaponName(FName Id)
	{
		const FTOWeaponDef* W = TODB::FindWeapon(Id);
		return W ? W->Name : FString(TEXT("-"));
	}

	FString ItemNameById(FName Id, const TCHAR* Fallback)
	{
		const FTOItemDef* D = Id.IsNone() ? nullptr : TODB::FindItem(Id);
		return D ? D->Name : FString(Fallback);
	}

	int64 ItemPrice(FName Id, int32 Count = 1)
	{
		const FTOItemDef* D = Id.IsNone() ? nullptr : TODB::FindItem(Id);
		return D ? (int64)D->Value * Count : 0;
	}

	FString Price(int64 V)
	{
		return V > 0 ? FString::Printf(TEXT("$%s"), *TOUtil::FormatMoney(V)) : FString(TEXT("free"));
	}

	int32 CycleIndex(const TArray<FName>& List, FName Current, int32 Dir)
	{
		if (List.Num() == 0)
		{
			return INDEX_NONE;
		}
		const int32 Idx = List.IndexOfByKey(Current);
		return ((Idx == INDEX_NONE ? 0 : Idx) + Dir + List.Num()) % List.Num();
	}

	const TArray<ETOWeaponClass>& PrimaryClasses()
	{
		static const TArray<ETOWeaponClass> Classes = {
			ETOWeaponClass::AssaultRifle, ETOWeaponClass::SMG, ETOWeaponClass::DMR,
			ETOWeaponClass::Sniper, ETOWeaponClass::LMG, ETOWeaponClass::Shotgun };
		return Classes;
	}

	/** Weapons offered on the Warfare redeploy screen (index 0 = custom loadout gun). */
	const TArray<FName>& WarfareWeapons()
	{
		static const TArray<FName> Ids = {
			NAME_None, TEXT("M4A1"), TEXT("AKM"), TEXT("K416"), TEXT("AUG"), TEXT("AK12"), TEXT("SCARH"),
			TEXT("MP5"), TEXT("Vector"), TEXT("P90"), TEXT("SR25"), TEXT("SVD"), TEXT("AWM"), TEXT("M249"), TEXT("PKM"), TEXT("M1014") };
		return Ids;
	}

	const TCHAR* Tips[] = {
		TEXT("Armor only stops rounds up to its protection level - bring higher tier ammo for armored targets."),
		TEXT("Put your most valuable find into the secure container - it survives death."),
		TEXT("The Base Helipad extraction needs the radio tower switch first. Calling it is loud."),
		TEXT("Rival operator squads loot the same buildings you do. Listen for footsteps."),
		TEXT("The Administration director's office needs the blue keycard. The vault needs the red one."),
		TEXT("Hold [Shift] while scoped to steady your breath."),
		TEXT("Downed squad mates can be revived - hold [F] next to them."),
		TEXT("Mark targets with the middle mouse button - your squad will engage them."),
		TEXT("Warfare: attackers lose a ticket for every death. Take both objectives to push the front."),
		TEXT("The Rail Tunnel exit is too narrow for backpacks. Drop it from the inventory first.")
	};
}

// ---------------------------------------------------------------------------------------------
//  Loading
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawLoading()
{
	Rect(0.f, 0.f, RefW, RefH, FLinearColor(0.008f, 0.01f, 0.012f, 1.f));
	const ATOGameMode* GM = GetGM();
	const UTOGameInstance* GI = GetGI();
	const ETOMatchMode Mode = GI ? (GI->Session.bDeploy ? GI->Session.Mode : ETOMatchMode::Menu) : ETOMatchMode::Menu;

	// Map preview in the background
	if (GM && GM->GetWorldGen() && GM->GetWorldGen()->GetMapTexture())
	{
		const float Size = 900.f;
		Tex(GM->GetWorldGen()->GetMapTexture(), RefW - Size + 120.f, (RefH - Size) * 0.5f, Size, Size, 0.f, 0.f, 1.f, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.22f));
	}

	Rect(90.f, 380.f, 8.f, 120.f, TOStyle::Accent);
	Text(TEXT("DAM VALLEY"), 120.f, 370.f, 78.f, TOStyle::Text);
	FString ModeText = TEXT("Main menu");
	if (Mode == ETOMatchMode::Operations)
	{
		ModeText = FString::Printf(TEXT("OPERATIONS  -  %s  -  %s"), GI && GI->Session.Difficulty > 0 ? TEXT("HAZARD") : TEXT("NORMAL"), TimeName(GI ? GI->Session.Time : ETOTimeOfDay::Day));
	}
	else if (Mode == ETOMatchMode::Warfare)
	{
		ModeText = FString::Printf(TEXT("WARFARE  -  %s  -  %s"), GI && GI->Session.WarfareSide == 0 ? TEXT("ATTACK") : TEXT("DEFEND"), TimeName(GI ? GI->Session.Time : ETOTimeOfDay::Day));
	}
	Text(ModeText, 124.f, 462.f, 22.f, TOStyle::Accent);

	const float P = GM ? GM->GetLoadingProgress() : 0.f;
	const float BarW = FMath::Min(900.f, RefW - 240.f);
	Bar(120.f, RefH - 150.f, BarW, 6.f, P, TOStyle::Accent, 0.7f);
	Text(FString::Printf(TEXT("BUILDING BATTLEFIELD  %d%%"), FMath::RoundToInt(P * 100.f)), 120.f, RefH - 186.f, 16.f, TOStyle::Dim);

	const int32 TipIdx = (FMath::FloorToInt(RealNow() / 6.f)) % UE_ARRAY_COUNT(Tips);
	Text(TEXT("TIP"), 120.f, RefH - 120.f, 14.f, TOStyle::Accent);
	Text(Tips[TipIdx], 160.f, RefH - 120.f, 16.f, TOStyle::Text);

	// Spinner
	const float T = FMath::Fmod(RealNow() * 0.8f, 1.f);
	Ring(RefW - 90.f, RefH - 90.f, 16.f, 22.f, TOStyle::Accent, T, T + 0.3f, 24);
}

// ---------------------------------------------------------------------------------------------
//  Lobby
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawLobby()
{
	ATOPlayerController* PC = GetPC();
	UTOGameInstance* GI = GetGI();
	if (!PC || !GI)
	{
		return;
	}
	// Left gradient over the orbiting world camera
	for (int32 i = 0; i < 12; ++i)
	{
		Rect(i * 70.f, 0.f, 70.f, RefH, FLinearColor(0.f, 0.f, 0.f, 0.62f * (1.f - i / 12.f)));
	}
	Rect(0.f, 0.f, RefW, 96.f, FLinearColor(0.f, 0.f, 0.f, 0.45f));
	DrawTitle(TEXT("TAC-OPS"), TEXT("Dam Valley  -  tactical extraction & combined arms warfare"));

	const float X = 66.f;
	float Y = 190.f;
	const float CW = 600.f;

	auto ModeCard = [&](const FString& Title, const FString& Sub, const FString& Desc, const FLinearColor& Col, TFunction<void()> OnClick)
	{
		const float CH = 170.f;
		const bool bHover = Hover(X, Y, CW, CH);
		Rect(X, Y, CW, CH, FLinearColor(0.03f, 0.035f, 0.04f, bHover ? 0.95f : 0.82f));
		Rect(X, Y, 6.f, CH, Col);
		if (bHover)
		{
			Frame(X, Y, CW, CH, TOStyle::Accent, 2.f);
			Rect(X + CW - 120.f, Y + CH - 46.f, 104.f, 32.f, TOStyle::Accent);
			Text(TEXT("ENTER"), X + CW - 68.f, Y + CH - 40.f, 17.f, TOStyle::TextOnAccent, ETOAlign::Center, false);
		}
		Text(Title, X + 30.f, Y + 22.f, 42.f, TOStyle::Text);
		Text(Sub, X + 32.f, Y + 74.f, 17.f, Col);
		Text(Desc, X + 32.f, Y + 106.f, 15.f, TOStyle::Dim);
		Region(X, Y, CW, CH, [this, Fn = MoveTemp(OnClick)]()
		{
			PlayUISound(true);
			Fn();
		});
		Y += CH + 18.f;
	};

	ModeCard(TEXT("OPERATIONS"), TEXT("HAZARD EXTRACTION  -  PvPvE"), TEXT("Deploy with your squad, loot Dam Valley, fight guards & rival operators, extract alive."),
		FLinearColor(1.f, 0.62f, 0.2f), [PC]() { PC->SetScreen(ETOScreen::OpsPrep); });
	ModeCard(TEXT("WARFARE"), TEXT("ATTACK & DEFEND  -  12 v 12"), TEXT("Combined arms push through four sectors. Capture both objectives, protect the tickets."),
		FLinearColor(0.35f, 0.7f, 1.f), [PC]() { PC->SetScreen(ETOScreen::WarfarePrep); });

	Y += 10.f;
	Button(X, Y, 290.f, 48.f, TEXT("SETTINGS"), [PC]() { PC->OpenSettings(); });
	Button(X + 310.f, Y, 290.f, 48.f, TEXT("QUIT GAME"), [PC]() { PC->ConsoleCommand(TEXT("quit")); });

	DrawProfilePanel(RefW - 560.f, 150.f, 500.f);

	Text(TEXT("WASD move  -  Mouse aim/fire  -  Shift sprint  -  C crouch  -  Z prone  -  Q/E lean  -  F interact  -  Tab inventory  -  M map"),
		RefW * 0.5f, RefH - 38.f, 14.f, TOStyle::Dim, ETOAlign::Center);
}

void ATOHUD::DrawProfilePanel(float X, float Y, float W)
{
	const UTOSaveGame* Save = GetSaveGame();
	const UTOGameInstance* GI = GetGI();
	if (!Save || !GI)
	{
		return;
	}
	float H = 330.f;
	Panel(X, Y, W, H);
	Header(TEXT("Operator profile"), X + 20.f, Y + 16.f, W - 40.f);
	float RY = Y + 62.f;
	auto Row = [&](const FString& L, const FString& V, const FLinearColor& C)
	{
		Text(L, X + 24.f, RY, 16.f, TOStyle::Dim);
		Text(V, X + W - 24.f, RY, 16.f, C, ETOAlign::Right);
		RY += 28.f;
	};
	const float Survival = Save->Raids > 0 ? 100.f * Save->Extractions / Save->Raids : 0.f;
	const float KD = Save->Deaths > 0 ? (float)Save->Kills / Save->Deaths : (float)Save->Kills;
	Row(TEXT("Credits"), FString::Printf(TEXT("$%s"), *TOUtil::FormatMoney(Save->Credits)), TOStyle::Money);
	Row(TEXT("Raids  /  Extracted"), FString::Printf(TEXT("%d  /  %d  (%.0f%%)"), Save->Raids, Save->Extractions, Survival), TOStyle::Text);
	Row(TEXT("Kills  /  Deaths"), FString::Printf(TEXT("%d  /  %d  (K/D %.2f)"), Save->Kills, Save->Deaths, KD), TOStyle::Text);
	Row(TEXT("Total extracted value"), FString::Printf(TEXT("$%s"), *TOUtil::FormatMoney(Save->TotalExtracted)), TOStyle::Money);
	Row(TEXT("Best raid"), FString::Printf(TEXT("$%s"), *TOUtil::FormatMoney(Save->BestRaid)), TOStyle::Money);
	Row(TEXT("Warfare  wins / matches"), FString::Printf(TEXT("%d / %d"), Save->WarfareWins, Save->WarfareMatches), TOStyle::Text);
	Row(TEXT("Secure container"), FString::Printf(TEXT("%d items  ($%s)"), Save->SafeBoxItems.Num(), *TOUtil::FormatMoney([&]() { int64 V = 0; for (const FTOItemInstance& I : Save->SafeBoxItems) { V += TODB::ItemValue(I); } return V; }())), TOStyle::Text);

	// Last match
	const FTOMatchResult& R = GI->LastResult;
	if (R.bValid)
	{
		const float LY = Y + H + 16.f;
		Panel(X, LY, W, 150.f);
		Header(TEXT("Last deployment"), X + 20.f, LY + 16.f, W - 40.f);
		const bool bGood = R.Mode == ETOMatchMode::Operations ? R.bExtracted : R.bVictory;
		Text(R.Title, X + 24.f, LY + 60.f, 28.f, bGood ? TOStyle::Good : TOStyle::Danger);
		const FString Detail = R.Mode == ETOMatchMode::Operations
			? FString::Printf(TEXT("%s  -  %d kills  -  %s"), R.bExtracted ? *R.ExtractName : *R.KilledBy, R.Kills, *TOUtil::FormatTime(R.Duration))
			: FString::Printf(TEXT("Warfare  -  %d kills  -  %d captures"), R.Kills, R.Captures);
		Text(Fit(Detail, 15.f, W - 48.f), X + 24.f, LY + 98.f, 15.f, TOStyle::Dim);
		const int64 Net = R.ValueExtracted - R.LoadoutCost;
		Text(FString::Printf(TEXT("%s$%s"), Net >= 0 ? TEXT("+") : TEXT(""), *TOUtil::FormatMoney(Net)), X + W - 24.f, LY + 64.f, 24.f, Net >= 0 ? TOStyle::Money : TOStyle::Danger, ETOAlign::Right);
	}
}

// ---------------------------------------------------------------------------------------------
//  Operations preparation
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawOpsPrep()
{
	ATOPlayerController* PC = GetPC();
	UTOGameInstance* GI = GetGI();
	ATOGameMode* GM = GetGM();
	if (!PC || !GI)
	{
		return;
	}
	DrawBackdrop(0.78f);
	DrawTitle(TEXT("Operations"), TEXT("Dam Valley  -  hazard extraction. Everything you bring in can be lost."));
	FTOSession& Ses = GI->Session;
	FTOLoadout& L = Ses.Loadout;
	if (!L.Primary.IsValid())
	{
		L.Primary = TODB::MakeDefaultConfig(TEXT("M4A1"), 3);
	}
	if (!L.Sidearm.IsValid())
	{
		L.Sidearm = TODB::MakeDefaultConfig(TEXT("G17"), 2);
	}

	const float Total = FMath::Min(RefW - 80.f, 1840.f);
	const float X0 = (RefW - Total) * 0.5f;
	const float Top = 120.f;
	const float WA = Total * 0.28f;
	const float WB = Total * 0.42f;
	const float WC = Total - WA - WB - 40.f;
	const float XA = X0;
	const float XB = XA + WA + 20.f;
	const float XC = XB + WB + 20.f;

	// --- Column A: deployment -----------------------------------------------------------
	Panel(XA, Top, WA, 860.f);
	Header(TEXT("Deployment"), XA + 18.f, Top + 14.f, WA - 36.f);
	float Y = Top + 60.f;
	const float MapSize = FMath::Min(WA - 36.f, 380.f);
	if (GM && GM->GetWorldGen() && GM->GetWorldGen()->GetMapTexture())
	{
		Tex(GM->GetWorldGen()->GetMapTexture(), XA + (WA - MapSize) * 0.5f, Y, MapSize, MapSize);
		Frame(XA + (WA - MapSize) * 0.5f, Y, MapSize, MapSize, FLinearColor(1.f, 1.f, 1.f, 0.2f));
		Text(TEXT("DAM VALLEY  -  2.5 x 2.5 km"), XA + WA * 0.5f, Y + MapSize - 26.f, 14.f, TOStyle::Text, ETOAlign::Center);
	}
	Y += MapSize + 18.f;
	Choice(XA + 18.f, Y, WA - 36.f, TEXT("Difficulty"), { TEXT("Normal"), TEXT("Hazard") }, Ses.Difficulty, [GI](int32 I) { GI->Session.Difficulty = I; });
	Y += 48.f;
	Choice(XA + 18.f, Y, WA - 36.f, TEXT("Time"), { TEXT("Day"), TEXT("Sunset"), TEXT("Night") }, (int32)Ses.Time, [GI](int32 I) { GI->Session.Time = (ETOTimeOfDay)I; });
	Y += 48.f;
	Choice(XA + 18.f, Y, WA - 36.f, TEXT("Squad"), { TEXT("Solo"), TEXT("AI Squad (3)") }, Ses.bSquad ? 1 : 0, [GI](int32 I) { GI->Session.bSquad = I == 1; });
	Y += 58.f;
	const FString DiffText = Ses.Difficulty > 0
		? TEXT("HAZARD: elite guards, more rival squads, armored Commander. Higher tier loot.")
		: TEXT("NORMAL: lighter guard presence. Good for learning the valley.");
	Text(Fit(DiffText, 14.f, WA - 36.f), XA + 18.f, Y, 14.f, Ses.Difficulty > 0 ? TOStyle::Warn : TOStyle::Dim);
	Y += 22.f;
	Text(Ses.bSquad ? TEXT("Two AI operators (Raven, Kestrel) deploy with you and can revive you.") : TEXT("Solo: no revives - going down means death."),
		XA + 18.f, Y, 14.f, TOStyle::Dim);

	// --- Column B: operator & loadout --------------------------------------------------
	Panel(XB, Top, WB, 860.f);
	Header(TEXT("Operator"), XB + 18.f, Top + 14.f, WB - 36.f);
	OperatorCards(XB + 18.f, Top + 56.f, WB - 36.f, L.Operator, [GI](ETOOperator Op) { GI->Session.Loadout.Operator = Op; });

	Y = Top + 226.f;
	Header(TEXT("Loadout"), XB + 18.f, Y, WB - 36.f);
	Y += 44.f;
	const float SW = WB - 36.f - 120.f;
	const float SX = XB + 18.f;
	const float RowH = 39.f;

	// Primary weapon
	{
		const TArray<FName> Ids = TODB::PrimaryWeaponIds();
		auto Cycle = [GI, Ids](int32 Dir)
		{
			FTOWeaponConfig& P = GI->Session.Loadout.Primary;
			const int32 N = CycleIndex(Ids, P.WeaponId, Dir);
			if (N != INDEX_NONE)
			{
				P = TODB::MakeDefaultConfig(Ids[N], P.AmmoTier);
			}
		};
		Stepper(SX, Y, SW, TEXT("Primary"), WeaponName(L.Primary.WeaponId), [Cycle]() { Cycle(-1); }, [Cycle]() { Cycle(1); }, Price(TODB::WeaponConfigPrice(L.Primary)));
		Button(SX + SW + 8.f, Y + 2.f, 112.f, 30.f, TEXT("GUNSMITH"), [PC]() { PC->OpenGunsmith(0); }, true, false, 14.f);
		Y += RowH;
	}
	// Ammo tier
	{
		const FTOWeaponStats St = TODB::ComputeStats(L.Primary);
		const FName Cal = St.Def ? St.Def->Caliber : NAME_None;
		const int64 AmmoCost = TODB::AmmoCost(Cal, L.Primary.AmmoTier, St.MagSize * (L.SpareMags + 1));
		Stepper(SX, Y, SW + 120.f, FString::Printf(TEXT("Ammo  (%s)"), TODB::CaliberName(Cal)), FString::Printf(TEXT("Tier %d  -  %d mags"), L.Primary.AmmoTier, L.SpareMags + 1),
			[GI]() { GI->Session.Loadout.Primary.AmmoTier = FMath::Max(1, GI->Session.Loadout.Primary.AmmoTier - 1); },
			[GI]() { GI->Session.Loadout.Primary.AmmoTier = FMath::Min(6, GI->Session.Loadout.Primary.AmmoTier + 1); },
			Price(AmmoCost), L.Primary.AmmoTier > 1, L.Primary.AmmoTier < 6);
		Y += RowH;
		Stepper(SX, Y, SW + 120.f, TEXT("Spare magazines"), FString::Printf(TEXT("%d"), L.SpareMags),
			[GI]() { GI->Session.Loadout.SpareMags = FMath::Max(1, GI->Session.Loadout.SpareMags - 1); },
			[GI]() { GI->Session.Loadout.SpareMags = FMath::Min(8, GI->Session.Loadout.SpareMags + 1); },
			FString(), L.SpareMags > 1, L.SpareMags < 8);
		Y += RowH;
	}
	// Sidearm
	{
		const TArray<FName> Ids = TODB::SidearmIds();
		auto Cycle = [GI, Ids](int32 Dir)
		{
			FTOWeaponConfig& P = GI->Session.Loadout.Sidearm;
			const int32 N = CycleIndex(Ids, P.WeaponId, Dir);
			if (N != INDEX_NONE)
			{
				P = TODB::MakeDefaultConfig(Ids[N], P.AmmoTier);
			}
		};
		Stepper(SX, Y, SW, TEXT("Sidearm"), WeaponName(L.Sidearm.WeaponId), [Cycle]() { Cycle(-1); }, [Cycle]() { Cycle(1); }, Price(TODB::WeaponConfigPrice(L.Sidearm)));
		Button(SX + SW + 8.f, Y + 2.f, 112.f, 30.f, TEXT("GUNSMITH"), [PC]() { PC->OpenGunsmith(1); }, true, false, 14.f);
		Y += RowH;
	}
	// Gear
	auto GearRow = [&](const FString& Label, int32& Value, int32 Max, FName ItemId, const TCHAR* NoneName)
	{
		int32* Ptr = &Value;
		Stepper(SX, Y, SW + 120.f, Label, Value > 0 ? ItemNameById(ItemId, NoneName) : FString(NoneName),
			[Ptr]() { *Ptr = FMath::Max(0, *Ptr - 1); }, [Ptr, Max]() { *Ptr = FMath::Min(Max, *Ptr + 1); },
			Price(ItemPrice(ItemId)), Value > 0, Value < Max);
		Y += RowH;
	};
	GearRow(TEXT("Body armor"), L.ArmorLevel, 6, TODB::ArmorItemId(L.ArmorLevel), TEXT("None"));
	GearRow(TEXT("Helmet"), L.HelmetLevel, 6, TODB::HelmetItemId(L.HelmetLevel), TEXT("None"));
	GearRow(TEXT("Chest rig"), L.RigTier, 4, TODB::RigItemId(L.RigTier), TEXT("None"));
	GearRow(TEXT("Backpack"), L.BackpackTier, 4, TODB::BackpackItemId(L.BackpackTier), TEXT("None"));
	{
		TArray<FName> Meds;
		TODB::GetMedKit(L.MedTier, Meds);
		int64 MedCost = 0;
		for (const FName& M : Meds)
		{
			MedCost += ItemPrice(M);
		}
		Stepper(SX, Y, SW + 120.f, TEXT("Medical"), TODB::MedKitName(L.MedTier),
			[GI]() { GI->Session.Loadout.MedTier = FMath::Max(0, GI->Session.Loadout.MedTier - 1); },
			[GI]() { GI->Session.Loadout.MedTier = FMath::Min(3, GI->Session.Loadout.MedTier + 1); },
			Price(MedCost), L.MedTier > 0, L.MedTier < 3);
		Y += RowH;
	}
	auto CountRow = [&](const FString& Label, int32& Value, FName ItemId)
	{
		int32* Ptr = &Value;
		Stepper(SX, Y, SW + 120.f, Label, FString::Printf(TEXT("x %d"), Value),
			[Ptr]() { *Ptr = FMath::Max(0, *Ptr - 1); }, [Ptr]() { *Ptr = FMath::Min(4, *Ptr + 1); },
			Price(ItemPrice(ItemId, Value)), Value > 0, Value < 4);
		Y += RowH;
	};
	CountRow(TEXT("Frag grenades"), L.Frags, TODB::GrenadeItemId(ETOGrenadeType::Frag));
	CountRow(TEXT("Smoke grenades"), L.Smokes, TODB::GrenadeItemId(ETOGrenadeType::Smoke));
	CountRow(TEXT("Flashbangs"), L.Flashes, TODB::GrenadeItemId(ETOGrenadeType::Flash));

	// --- Column C: secure container & deploy -------------------------------------------
	Panel(XC, Top, WC, 860.f);
	Header(TEXT("Secure container"), XC + 18.f, Top + 14.f, WC - 36.f);
	Y = Top + 58.f;
	if (const UTOSaveGame* Save = GetSaveGame())
	{
		if (Save->SafeBoxItems.Num() == 0)
		{
			Text(TEXT("Empty. Items placed in the secure container survive death."), XC + 18.f, Y, 14.f, TOStyle::Dim);
			Y += 26.f;
		}
		for (const FTOItemInstance& It : Save->SafeBoxItems)
		{
			const FTOItemDef* D = TODB::FindItem(It.ItemId);
			const FLinearColor RC = D ? TODB::RarityColor(D->Rarity) : TOStyle::Text;
			Rect(XC + 18.f, Y + 3.f, 4.f, 16.f, RC);
			Text(Fit(TODB::ItemName(It), 15.f, WC - 160.f), XC + 30.f, Y, 15.f, TOStyle::Text);
			Text(Price(TODB::ItemValue(It)), XC + WC - 18.f, Y, 15.f, TOStyle::Money, ETOAlign::Right);
			Y += 24.f;
			if (Y > Top + 330.f)
			{
				break;
			}
		}
	}

	const int64 Cost = TODB::LoadoutCost(L);
	const int64 Credits = GI->GetCredits();
	float CY = Top + 400.f;
	Header(TEXT("Summary"), XC + 18.f, CY, WC - 36.f);
	CY += 50.f;
	Text(TEXT("Loadout cost"), XC + 18.f, CY, 17.f, TOStyle::Dim);
	Text(FString::Printf(TEXT("$%s"), *TOUtil::FormatMoney(Cost)), XC + WC - 18.f, CY, 17.f, TOStyle::Money, ETOAlign::Right);
	CY += 30.f;
	Text(TEXT("Credits after deploy"), XC + 18.f, CY, 17.f, TOStyle::Dim);
	Text(FString::Printf(TEXT("$%s"), *TOUtil::FormatMoney(Credits - Cost)), XC + WC - 18.f, CY, 17.f, Credits >= Cost ? TOStyle::Text : TOStyle::Danger, ETOAlign::Right);
	CY += 30.f;
	Text(TEXT("Extracting sells everything you carry out."), XC + 18.f, CY, 14.f, TOStyle::Dim);
	CY += 40.f;

	const bool bCanPay = Credits >= Cost;
	Button(XC + 18.f, CY, WC - 36.f, 64.f, bCanPay ? FString::Printf(TEXT("DEPLOY  ($%s)"), *TOUtil::FormatMoney(Cost)) : FString(TEXT("NOT ENOUGH CREDITS")), [this, GI, Cost]()
	{
		if (!GI->SpendCredits(Cost))
		{
			DeployError = TEXT("Not enough credits");
			DeployErrorUntil = RealNow() + 3.f;
			return;
		}
		GI->Session.bFreeKit = false;
		GI->PaidLoadoutCost = Cost;
		GI->StartMatch(ETOMatchMode::Operations);
	}, bCanPay, true, 24.f);
	CY += 76.f;
	Button(XC + 18.f, CY, WC - 36.f, 46.f, TEXT("DEPLOY WITH FREE KIT  (MP5, Lv2 armor)"), [GI]()
	{
		GI->Session.bFreeKit = true;
		GI->PaidLoadoutCost = 0;
		GI->StartMatch(ETOMatchMode::Operations);
	}, true, false, 16.f);
	CY += 58.f;
	Button(XC + 18.f, CY, WC - 36.f, 42.f, TEXT("BACK"), [PC]() { PC->SetScreen(ETOScreen::Lobby); });
	if (RealNow() < DeployErrorUntil)
	{
		Text(DeployError, XC + WC * 0.5f, CY + 56.f, 16.f, TOStyle::Danger, ETOAlign::Center);
	}
}

// ---------------------------------------------------------------------------------------------
//  Warfare preparation
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawWarfarePrep()
{
	ATOPlayerController* PC = GetPC();
	UTOGameInstance* GI = GetGI();
	if (!PC || !GI)
	{
		return;
	}
	DrawBackdrop(0.74f);
	DrawTitle(TEXT("Warfare"), TEXT("Attack & Defend  -  Dam Valley front. Equipment is free."));
	FTOSession& Ses = GI->Session;
	FTOLoadout& L = Ses.Loadout;

	const float W = FMath::Min(RefW - 120.f, 1300.f);
	const float X = (RefW - W) * 0.5f;
	float Y = 130.f;
	Panel(X, Y, W, 820.f);
	Header(TEXT("Battle"), X + 20.f, Y + 14.f, W - 40.f);
	Y += 60.f;
	Choice(X + 20.f, Y, W * 0.5f - 30.f, TEXT("Side"), { TEXT("Attack"), TEXT("Defend") }, Ses.WarfareSide, [GI](int32 I) { GI->Session.WarfareSide = I; });
	Choice(X + W * 0.5f + 10.f, Y, W * 0.5f - 30.f, TEXT("Time"), { TEXT("Day"), TEXT("Sunset"), TEXT("Night") }, (int32)Ses.Time, [GI](int32 I) { GI->Session.Time = (ETOTimeOfDay)I; });
	Y += 52.f;
	Text(Ses.WarfareSide == 0
		? TEXT("ATTACKERS: 250 tickets. Capture both objectives of a sector to push the front and earn +75 tickets.")
		: TEXT("DEFENDERS: hold the line. The attackers lose when their tickets run out."),
		X + 20.f, Y, 15.f, TOStyle::Dim);
	Y += 40.f;

	Header(TEXT("Operator"), X + 20.f, Y, W - 40.f);
	OperatorCards(X + 20.f, Y + 44.f, W - 40.f, L.Operator, [GI](ETOOperator Op) { GI->Session.Loadout.Operator = Op; });
	Y += 214.f;

	Header(TEXT("Weapon"), X + 20.f, Y, W - 40.f);
	Y += 46.f;
	const TArray<FName> Ids = TODB::PrimaryWeaponIds();
	auto Cycle = [GI, Ids](int32 Dir)
	{
		FTOWeaponConfig& P = GI->Session.Loadout.Primary;
		const int32 N = CycleIndex(Ids, P.WeaponId, Dir);
		if (N != INDEX_NONE)
		{
			P = TODB::MakeDefaultConfig(Ids[N], FMath::Max(3, P.AmmoTier));
		}
	};
	Stepper(X + 20.f, Y, W - 200.f, TEXT("Primary"), WeaponName(L.Primary.WeaponId), [Cycle]() { Cycle(-1); }, [Cycle]() { Cycle(1); });
	Button(X + W - 170.f, Y + 2.f, 150.f, 30.f, TEXT("GUNSMITH"), [PC]() { PC->OpenGunsmith(0); }, true, false, 15.f);
	Y += 44.f;
	DrawWeaponStats(X + 20.f, Y, W - 40.f, L.Primary, nullptr);
	Y += 230.f;

	Button(X + 20.f, Y, W * 0.5f - 30.f, 60.f, TEXT("DEPLOY"), [GI]()
	{
		GI->Session.bFreeKit = false;
		GI->PaidLoadoutCost = 0;
		GI->StartMatch(ETOMatchMode::Warfare);
	}, true, true, 26.f);
	Button(X + W * 0.5f + 10.f, Y, W * 0.5f - 30.f, 60.f, TEXT("BACK"), [PC]() { PC->SetScreen(ETOScreen::Lobby); }, true, false, 20.f);
}

// ---------------------------------------------------------------------------------------------
//  Gunsmith
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawWeaponStats(float X, float Y, float W, const FTOWeaponConfig& Config, const FTOWeaponConfig* Compare)
{
	const FTOWeaponStats St = TODB::ComputeStats(Config);
	if (!St.Def)
	{
		return;
	}
	FTOWeaponStats Base;
	if (Compare)
	{
		Base = TODB::ComputeStats(*Compare);
	}
	struct FStatRow { const TCHAR* Name; float Value; float BaseValue; float Max; FString Label; bool bHigherBetter; };
	const float DmgPerShot = St.Damage * FMath::Max(1, St.Def->Pellets);
	TArray<FStatRow> Rows;
	Rows.Add({ TEXT("Damage"), DmgPerShot, Base.Def ? Base.Damage * FMath::Max(1, Base.Def->Pellets) : DmgPerShot, 160.f, FString::Printf(TEXT("%.0f"), DmgPerShot), true });
	Rows.Add({ TEXT("Fire rate"), St.RPM, Base.Def ? Base.RPM : St.RPM, 1200.f, FString::Printf(TEXT("%.0f rpm"), St.RPM), true });
	Rows.Add({ TEXT("Recoil control"), 1.6f - St.RecoilV, Base.Def ? 1.6f - Base.RecoilV : 1.6f - St.RecoilV, 1.6f, FString::Printf(TEXT("%.0f"), 100.f * FMath::Clamp((1.6f - St.RecoilV) / 1.6f, 0.f, 1.f)), true });
	Rows.Add({ TEXT("Handling"), 0.7f - St.AdsTime, Base.Def ? 0.7f - Base.AdsTime : 0.7f - St.AdsTime, 0.6f, FString::Printf(TEXT("ADS %.2fs"), St.AdsTime), true });
	Rows.Add({ TEXT("Hip accuracy"), 6.f - St.HipSpread, Base.Def ? 6.f - Base.HipSpread : 6.f - St.HipSpread, 6.f, FString::Printf(TEXT("%.1f deg"), St.HipSpread), true });
	Rows.Add({ TEXT("Effective range"), St.Range, Base.Def ? Base.Range : St.Range, 150.f, FString::Printf(TEXT("%.0f m"), St.Range), true });
	Rows.Add({ TEXT("Muzzle velocity"), St.Velocity, Base.Def ? Base.Velocity : St.Velocity, 1000.f, FString::Printf(TEXT("%.0f m/s"), St.Velocity), true });
	Rows.Add({ TEXT("Magazine"), (float)St.MagSize, Base.Def ? (float)Base.MagSize : (float)St.MagSize, 100.f, FString::Printf(TEXT("%d"), St.MagSize), true });
	Rows.Add({ TEXT("Mobility"), St.MoveMult, Base.Def ? Base.MoveMult : St.MoveMult, 1.1f, FString::Printf(TEXT("%.0f%%"), St.MoveMult * 100.f), true });

	float RY = Y;
	for (const FStatRow& R : Rows)
	{
		Text(R.Name, X, RY, 15.f, TOStyle::Dim);
		const float BX = X + W * 0.32f;
		const float BW = W * 0.46f;
		const float P = FMath::Clamp(R.Value / R.Max, 0.f, 1.f);
		const float PB = FMath::Clamp(R.BaseValue / R.Max, 0.f, 1.f);
		Rect(BX, RY + 6.f, BW, 8.f, FLinearColor(1.f, 1.f, 1.f, 0.08f));
		Rect(BX, RY + 6.f, BW * FMath::Min(P, PB), 8.f, TOStyle::Text.CopyWithNewOpacity(0.85f));
		if (P > PB + 0.002f)
		{
			Rect(BX + BW * PB, RY + 6.f, BW * (P - PB), 8.f, TOStyle::Good);
		}
		else if (PB > P + 0.002f)
		{
			Rect(BX + BW * P, RY + 6.f, BW * (PB - P), 8.f, TOStyle::Danger);
		}
		Text(R.Label, X + W, RY, 15.f, TOStyle::Text, ETOAlign::Right);
		RY += 24.f;
	}
	FString Flags;
	if (St.bSuppressed) Flags += TEXT("SUPPRESSED   ");
	if (St.bLight) Flags += TEXT("FLASHLIGHT   ");
	if (St.bLaser) Flags += TEXT("LASER   ");
	if (St.bBipod) Flags += TEXT("BIPOD   ");
	if (St.Zoom > 1.01f) Flags += FString::Printf(TEXT("%.1fx OPTIC"), St.Zoom);
	if (!Flags.IsEmpty())
	{
		Text(Flags, X, RY + 2.f, 14.f, TOStyle::Accent);
	}
}

void ATOHUD::DrawGunsmith()
{
	ATOPlayerController* PC = GetPC();
	UTOGameInstance* GI = GetGI();
	if (!PC || !GI)
	{
		return;
	}
	DrawBackdrop(0.8f);
	const bool bSidearm = PC->GunsmithTarget == 1;
	FTOWeaponConfig& Cfg = bSidearm ? GI->Session.Loadout.Sidearm : GI->Session.Loadout.Primary;
	const bool bWarfare = PC->GunsmithReturn == ETOScreen::WarfarePrep;
	DrawTitle(TEXT("Gunsmith"), bWarfare ? TEXT("Warfare equipment is free - build anything.") : TEXT("Every part adds to the deployment cost."));
	const FTOWeaponDef* Def = TODB::FindWeapon(Cfg.WeaponId);
	if (!Def)
	{
		Cfg = TODB::MakeDefaultConfig(bSidearm ? FName(TEXT("G17")) : FName(TEXT("M4A1")), 3);
		return;
	}

	const float Total = FMath::Min(RefW - 80.f, 1840.f);
	const float X0 = (RefW - Total) * 0.5f;
	const float Top = 120.f;
	const float WA = 360.f;
	const float WC = 520.f;
	const float WB = Total - WA - WC - 40.f;
	const float XA = X0;
	const float XB = XA + WA + 20.f;
	const float XC = XB + WB + 20.f;

	// --- weapon list ---------------------------------------------------------------------
	Panel(XA, Top, WA, 860.f);
	Header(bSidearm ? TEXT("Sidearms") : TEXT("Weapons"), XA + 18.f, Top + 14.f, WA - 36.f);
	float Y = Top + 58.f;
	TArray<FName> List;
	if (bSidearm)
	{
		List = TODB::SidearmIds();
	}
	else
	{
		const TArray<ETOWeaponClass>& Classes = PrimaryClasses();
		const int32 Tab = FMath::Clamp(GunsmithClassTab, 0, Classes.Num() - 1);
		static const TCHAR* Short[] = { TEXT("AR"), TEXT("SMG"), TEXT("DMR"), TEXT("SNP"), TEXT("LMG"), TEXT("SG") };
		const float TW = (WA - 36.f - 5.f * 4.f) / 6.f;
		for (int32 i = 0; i < Classes.Num(); ++i)
		{
			Button(XA + 18.f + i * (TW + 4.f), Y, TW, 30.f, Short[i], [this, i]() { GunsmithClassTab = i; }, true, i == Tab, 14.f);
		}
		Y += 42.f;
		List = TODB::WeaponsOfClass(Classes[Tab]);
	}
	for (const FName& Id : List)
	{
		const FTOWeaponDef* W = TODB::FindWeapon(Id);
		if (!W)
		{
			continue;
		}
		const bool bSel = Id == Cfg.WeaponId;
		const bool bHover = Hover(XA + 18.f, Y, WA - 36.f, 48.f);
		Rect(XA + 18.f, Y, WA - 36.f, 48.f, bSel ? FLinearColor(0.12f, 0.15f, 0.08f, 0.95f) : FLinearColor(1.f, 1.f, 1.f, bHover ? 0.07f : 0.03f));
		if (bSel)
		{
			Rect(XA + 18.f, Y, 4.f, 48.f, TOStyle::Accent);
		}
		Text(W->Name, XA + 32.f, Y + 6.f, 18.f, bSel ? TOStyle::Accent : TOStyle::Text);
		Text(FString::Printf(TEXT("%s  -  %s"), TODB::WeaponClassName(W->Class), TODB::CaliberName(W->Caliber)), XA + 32.f, Y + 28.f, 12.f, TOStyle::Dim);
		Text(bWarfare ? FString() : Price(W->Price), XA + WA - 30.f, Y + 14.f, 15.f, TOStyle::Money, ETOAlign::Right);
		const FName WeaponId = Id;
		FTOWeaponConfig* CfgPtr = &Cfg;
		Region(XA + 18.f, Y, WA - 36.f, 48.f, [this, CfgPtr, WeaponId]()
		{
			PlayUISound(false);
			*CfgPtr = TODB::MakeDefaultConfig(WeaponId, CfgPtr->AmmoTier);
		});
		Y += 54.f;
	}

	// --- schematic + attachments ----------------------------------------------------------
	Panel(XB, Top, WB, 860.f);
	Text(Def->Name, XB + 24.f, Top + 18.f, 40.f, TOStyle::Text);
	Text(FString::Printf(TEXT("%s  -  %s"), TODB::WeaponClassName(Def->Class), TODB::CaliberName(Def->Caliber)), XB + 26.f, Top + 64.f, 16.f, TOStyle::Dim);

	// Side-view schematic built from the weapon's procedural dimensions.
	{
		const float CX = XB + WB * 0.5f;
		const float CY = Top + 200.f;
		const float Scale = FMath::Min(5.0f, (WB - 120.f) / FMath::Max(40.f, Def->RecvLen + Def->BarrelLen + 30.f));
		const float RecvL = Def->RecvLen * Scale;
		const float RecvH = Def->RecvH * Scale;
		const float BarrelL = Def->BarrelLen * Scale;
		const float StockL = (Def->Class == ETOWeaponClass::Pistol) ? 0.f : 24.f * Scale;
		const float Left = CX - (RecvL + BarrelL + StockL) * 0.5f;
		const FLinearColor Body = FLinearColor(0.32f, 0.34f, 0.36f, 1.f);
		const FLinearColor Dark = FLinearColor(0.16f, 0.17f, 0.18f, 1.f);
		const FLinearColor Hi = TOStyle::Accent.CopyWithNewOpacity(0.9f);
		auto Has = [&Cfg](ETOAttachSlot Slot) { return !Cfg.GetAttachment(Slot).IsNone(); };
		// stock
		if (StockL > 0.f)
		{
			const bool bStock = Has(ETOAttachSlot::Stock);
			Rect(Left, CY - RecvH * 0.1f, StockL, RecvH * 0.75f, bStock ? Hi : Dark);
		}
		// receiver
		Rect(Left + StockL, CY - RecvH * 0.5f, RecvL, RecvH, Body);
		// barrel / handguard
		const bool bBarrel = Has(ETOAttachSlot::Barrel);
		Rect(Left + StockL + RecvL, CY - RecvH * 0.3f, BarrelL * 0.55f, RecvH * 0.6f, bBarrel ? Hi : Dark);
		Rect(Left + StockL + RecvL + BarrelL * 0.55f, CY - RecvH * 0.12f, BarrelL * 0.45f, RecvH * 0.24f, Dark);
		// muzzle
		if (Has(ETOAttachSlot::Muzzle))
		{
			Rect(Left + StockL + RecvL + BarrelL, CY - RecvH * 0.2f, 14.f * Scale, RecvH * 0.4f, Hi);
		}
		// magazine
		const bool bMag = Has(ETOAttachSlot::Magazine);
		Rect(Left + StockL + RecvL * 0.45f, CY + RecvH * 0.5f, RecvH * 0.6f, RecvH * 1.6f, bMag ? Hi : Dark);
		// grip
		Rect(Left + StockL + RecvL * 0.12f, CY + RecvH * 0.5f, RecvH * 0.45f, RecvH * 1.2f, Dark);
		// optic
		if (Has(ETOAttachSlot::Optic))
		{
			Rect(Left + StockL + RecvL * 0.3f, CY - RecvH * 1.2f, RecvL * 0.42f, RecvH * 0.7f, Hi);
		}
		// underbarrel
		if (Has(ETOAttachSlot::Underbarrel))
		{
			Rect(Left + StockL + RecvL + BarrelL * 0.2f, CY + RecvH * 0.3f, RecvH * 0.4f, RecvH * 1.1f, Hi);
		}
		// tactical
		if (Has(ETOAttachSlot::Tactical))
		{
			Rect(Left + StockL + RecvL + BarrelL * 0.3f, CY - RecvH * 0.55f, RecvH * 0.9f, RecvH * 0.35f, Hi);
		}
	}

	Y = Top + 330.f;
	Header(TEXT("Attachments"), XB + 20.f, Y, WB - 40.f);
	Y += 46.f;
	for (int32 s = 0; s < (int32)ETOAttachSlot::Count; ++s)
	{
		const ETOAttachSlot Slot = (ETOAttachSlot)s;
		const TArray<FName> Options = TODB::GetAttachmentOptions(Cfg.WeaponId, Slot);
		const FName Cur = Cfg.GetAttachment(Slot);
		const FTOAttachmentDef* A = TODB::FindAttachment(Cur);
		const bool bAny = Options.Num() > 1;
		FTOWeaponConfig* CfgPtr = &Cfg;
		auto Cycle = [CfgPtr, Options, Slot](int32 Dir)
		{
			const int32 N = CycleIndex(Options, CfgPtr->GetAttachment(Slot), Dir);
			if (N != INDEX_NONE)
			{
				CfgPtr->SetAttachment(Slot, Options[N]);
			}
		};
		Stepper(XB + 20.f, Y, WB - 40.f, TODB::SlotName(Slot), A ? A->Name : FString(bAny ? TEXT("Standard") : TEXT("- no options -")),
			[Cycle]() { Cycle(-1); }, [Cycle]() { Cycle(1); },
			bWarfare ? FString() : (A ? Price(A->Price) : FString(TEXT("-"))), bAny, bAny);
		Y += 40.f;
	}
	{
		const FTOWeaponStats St = TODB::ComputeStats(Cfg);
		const FTOItemDef* AmmoDef = TODB::FindItem(TODB::AmmoItemId(Def->Caliber, Cfg.AmmoTier));
		FTOWeaponConfig* CfgPtr = &Cfg;
		Stepper(XB + 20.f, Y, WB - 40.f, TEXT("Ammunition"), AmmoDef ? AmmoDef->Name : FString::Printf(TEXT("Tier %d"), Cfg.AmmoTier),
			[CfgPtr]() { CfgPtr->AmmoTier = FMath::Max(1, CfgPtr->AmmoTier - 1); }, [CfgPtr]() { CfgPtr->AmmoTier = FMath::Min(6, CfgPtr->AmmoTier + 1); },
			bWarfare ? FString() : FString::Printf(TEXT("$%s / mag"), *TOUtil::FormatMoney(TODB::AmmoCost(Def->Caliber, Cfg.AmmoTier, St.MagSize))), Cfg.AmmoTier > 1, Cfg.AmmoTier < 6);
		Y += 40.f;
		Text(FString::Printf(TEXT("Penetration tier %d: defeats armor up to level %d reliably."), Cfg.AmmoTier, Cfg.AmmoTier), XB + 24.f, Y + 4.f, 14.f, TOStyle::Dim);
	}

	// --- stats -----------------------------------------------------------------------------
	Panel(XC, Top, WC, 860.f);
	Header(TEXT("Performance"), XC + 18.f, Top + 14.f, WC - 36.f);
	const FTOWeaponConfig Stock = TODB::MakeDefaultConfig(Cfg.WeaponId, Cfg.AmmoTier);
	DrawWeaponStats(XC + 18.f, Top + 64.f, WC - 36.f, Cfg, &Stock);

	float SY = Top + 330.f;
	Header(TEXT("Fire modes"), XC + 18.f, SY, WC - 36.f);
	SY += 44.f;
	FString Modes;
	for (const ETOFireMode M : Def->Modes)
	{
		Modes += FString(TODB::FireModeName(M)) + TEXT("   ");
	}
	Text(Modes, XC + 18.f, SY, 16.f, TOStyle::Text);
	SY += 40.f;
	if (!bWarfare)
	{
		Header(TEXT("Price"), XC + 18.f, SY, WC - 36.f);
		SY += 44.f;
		Text(TEXT("Weapon + parts"), XC + 18.f, SY, 17.f, TOStyle::Dim);
		Text(FString::Printf(TEXT("$%s"), *TOUtil::FormatMoney(TODB::WeaponConfigPrice(Cfg))), XC + WC - 18.f, SY, 17.f, TOStyle::Money, ETOAlign::Right);
		SY += 30.f;
	}
	FTOWeaponConfig* CfgPtr = &Cfg;
	Button(XC + 18.f, Top + 860.f - 140.f, WC - 36.f, 46.f, TEXT("RESET TO STOCK"), [CfgPtr]() { *CfgPtr = TODB::MakeDefaultConfig(CfgPtr->WeaponId, CfgPtr->AmmoTier); }, true, false, 16.f);
	Button(XC + 18.f, Top + 860.f - 82.f, WC - 36.f, 60.f, TEXT("DONE"), [PC, GI]()
	{
		GI->WriteSave();
		PC->SetScreen(PC->GunsmithReturn);
	}, true, true, 22.f);
}

// ---------------------------------------------------------------------------------------------
//  Settings
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawSettings()
{
	ATOPlayerController* PC = GetPC();
	UTOSaveGame* Save = GetSaveGame();
	if (!PC || !Save)
	{
		return;
	}
	DrawBackdrop(0.86f);
	DrawTitle(TEXT("Settings"), TEXT("Changes are saved when you leave this screen."));

	const float W = FMath::Min(RefW - 120.f, 1400.f);
	const float X = (RefW - W) * 0.5f;
	const float CW = W * 0.56f;
	float Y = 140.f;
	Panel(X, Y, CW, 780.f);
	Header(TEXT("Controls"), X + 20.f, Y + 14.f, CW - 40.f);
	Y += 60.f;
	Slider(X + 20.f, Y, CW - 40.f, TEXT("Mouse sensitivity"), Save->MouseSensitivity, 0.1f, 4.f, 0.05f, [Save](float V) { Save->MouseSensitivity = V; }, FString::Printf(TEXT("%.2f"), Save->MouseSensitivity));
	Y += 48.f;
	Slider(X + 20.f, Y, CW - 40.f, TEXT("ADS sensitivity"), Save->ADSSensitivity, 0.2f, 2.f, 0.05f, [Save](float V) { Save->ADSSensitivity = V; }, FString::Printf(TEXT("%.2f"), Save->ADSSensitivity));
	Y += 48.f;
	Toggle(X + 20.f, Y, CW - 40.f, TEXT("Invert vertical look"), Save->bInvertY, [Save]() { Save->bInvertY = !Save->bInvertY; });
	Y += 48.f;
	Toggle(X + 20.f, Y, CW - 40.f, TEXT("Toggle aim (instead of hold)"), Save->bToggleADS, [Save]() { Save->bToggleADS = !Save->bToggleADS; });
	Y += 64.f;
	Header(TEXT("Video & audio"), X + 20.f, Y, CW - 40.f);
	Y += 48.f;
	Slider(X + 20.f, Y, CW - 40.f, TEXT("Field of view"), Save->FieldOfView, 70.f, 110.f, 1.f, [Save](float V) { Save->FieldOfView = V; }, FString::Printf(TEXT("%.0f"), Save->FieldOfView));
	Y += 48.f;
	Choice(X + 20.f, Y, CW - 40.f, TEXT("Graphics quality"), { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic"), TEXT("Cinematic") }, Save->GraphicsQuality, [Save](int32 I) { Save->GraphicsQuality = I; });
	Y += 52.f;
	Slider(X + 20.f, Y, CW - 40.f, TEXT("Master volume"), Save->MasterVolume, 0.f, 1.f, 0.05f, [Save](float V) { Save->MasterVolume = V; }, FString::Printf(TEXT("%.0f%%"), Save->MasterVolume * 100.f));
	Y += 48.f;
	Toggle(X + 20.f, Y, CW - 40.f, TEXT("Show FPS counter"), Save->bShowFPS, [Save]() { Save->bShowFPS = !Save->bShowFPS; });
	Y += 70.f;
	Button(X + 20.f, Y, CW - 40.f, 56.f, TEXT("APPLY & BACK"), [PC]() { PC->CloseSettings(); }, true, true, 22.f);

	// Key reference
	const float KX = X + CW + 20.f;
	const float KW = W - CW - 20.f;
	Panel(KX, 140.f, KW, 780.f);
	Header(TEXT("Key bindings"), KX + 20.f, 154.f, KW - 40.f);
	static const TCHAR* Keys[][2] = {
		{ TEXT("W A S D"), TEXT("Move") }, { TEXT("Mouse"), TEXT("Look") }, { TEXT("LMB / RMB"), TEXT("Fire / Aim down sights") },
		{ TEXT("Shift"), TEXT("Sprint  /  hold breath (scoped)") }, { TEXT("Space"), TEXT("Jump / vault") }, { TEXT("C / Ctrl"), TEXT("Crouch (slide while sprinting)") },
		{ TEXT("Z"), TEXT("Prone") }, { TEXT("Q / E"), TEXT("Lean left / right") }, { TEXT("R"), TEXT("Reload") },
		{ TEXT("F (hold)"), TEXT("Interact / search / revive") }, { TEXT("1 2 3 4"), TEXT("Primary / secondary / pistol / knife") },
		{ TEXT("Wheel"), TEXT("Cycle weapons") }, { TEXT("B"), TEXT("Fire mode") }, { TEXT("G / 5"), TEXT("Throw grenade / grenade type") },
		{ TEXT("X"), TEXT("Operator ability") }, { TEXT("H"), TEXT("Quick heal") }, { TEXT("T / N"), TEXT("Flashlight / night vision") },
		{ TEXT("MMB"), TEXT("Ping / spot enemy / squad move") }, { TEXT("Tab / I"), TEXT("Inventory") }, { TEXT("M"), TEXT("Tactical map") },
		{ TEXT("Esc / P"), TEXT("Pause menu") } };
	float KY = 206.f;
	for (const auto& K : Keys)
	{
		Rect(KX + 20.f, KY, 120.f, 26.f, FLinearColor(1.f, 1.f, 1.f, 0.08f));
		Text(K[0], KX + 80.f, KY + 4.f, 14.f, TOStyle::Accent, ETOAlign::Center);
		Text(K[1], KX + 156.f, KY + 4.f, 15.f, TOStyle::Text);
		KY += 33.f;
	}
}

// ---------------------------------------------------------------------------------------------
//  Pause
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawPause()
{
	ATOPlayerController* PC = GetPC();
	ATOGameMode* GM = GetGM();
	if (!PC || !GM)
	{
		return;
	}
	DrawBackdrop(0.7f);
	const bool bOps = GM->GetMatchMode() == ETOMatchMode::Operations;
	DrawTitle(TEXT("Paused"), bOps ? FString::Printf(TEXT("Operations  -  raid time left %s"), *TOUtil::FormatTime(GM->GetRaidTimeLeft()))
		: FString::Printf(TEXT("Warfare  -  sector %d / %d  -  %s"), GM->GetCurrentSector() + 1, GM->GetNumSectors(), *GM->GetSectorName()));

	const float W = 520.f;
	const float X = 90.f;
	float Y = 220.f;
	Button(X, Y, W, 60.f, TEXT("RESUME"), [PC]() { PC->SetScreen(ETOScreen::None); }, true, true, 24.f);
	Y += 74.f;
	Button(X, Y, W, 52.f, TEXT("SETTINGS"), [PC]() { PC->OpenSettings(); });
	Y += 66.f;
	Button(X, Y, W, 52.f, TEXT("TACTICAL MAP"), [PC]() { PC->SetScreen(ETOScreen::Map); });
	Y += 90.f;
	const bool bConfirm = RealNow() < AbandonConfirmUntil;
	const FString Label = bOps
		? (bConfirm ? FString(TEXT("CONFIRM: ABANDON RAID (gear is lost)")) : FString(TEXT("ABANDON RAID")))
		: (bConfirm ? FString(TEXT("CONFIRM: LEAVE BATTLE")) : FString(TEXT("LEAVE BATTLE")));
	Button(X, Y, W, 52.f, Label, [this, GM]()
	{
		if (RealNow() < AbandonConfirmUntil)
		{
			AbandonConfirmUntil = -1.f;
			GM->PlayerAbandonMatch();
		}
		else
		{
			AbandonConfirmUntil = RealNow() + 4.f;
		}
	}, GM->GetState() == ETOGMState::Playing);
	if (bConfirm)
	{
		Rect(X, Y, 4.f, 52.f, TOStyle::Danger);
	}

	// Objective reminder
	const float PX = RefW - 620.f;
	Panel(PX, 220.f, 540.f, 300.f);
	Header(TEXT("Objective"), PX + 20.f, 234.f, 500.f);
	if (bOps)
	{
		Text(TEXT("Find valuables, survive, and reach an open extraction point."), PX + 20.f, 284.f, 16.f, TOStyle::Text);
		Text(TEXT("Open the map [M] to see which extractions are active."), PX + 20.f, 312.f, 15.f, TOStyle::Dim);
		Text(FString::Printf(TEXT("Kills this raid: %d"), GM->Result.Kills), PX + 20.f, 352.f, 16.f, TOStyle::Text);
		if (const ATOCharacter* C = GetPlayerChar())
		{
			if (const UTOInventoryComponent* Inv = C->GetInventory())
			{
				Text(FString::Printf(TEXT("Carried value: $%s"), *TOUtil::FormatMoney(Inv->GetTotalValue(true, true))), PX + 20.f, 380.f, 16.f, TOStyle::Money);
			}
		}
	}
	else
	{
		Text(FString::Printf(TEXT("You are %s."), GM->GetPlayerTeam() == GM->GetAttackerTeam() ? TEXT("ATTACKING") : TEXT("DEFENDING")), PX + 20.f, 284.f, 18.f, TOStyle::Accent);
		Text(FString::Printf(TEXT("Attacker tickets: %d / %d"), GM->GetTickets(), GM->GetMaxTickets()), PX + 20.f, 316.f, 16.f, TOStyle::Text);
		Text(FString::Printf(TEXT("Your kills: %d    Captures: %d"), GM->Result.Kills, GM->Result.Captures), PX + 20.f, 344.f, 16.f, TOStyle::Text);
	}
}

// ---------------------------------------------------------------------------------------------
//  Death / results
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawDeath()
{
	ATOPlayerController* PC = GetPC();
	UTOGameInstance* GI = GetGI();
	if (!PC || !GI)
	{
		return;
	}
	const float T = FMath::Clamp((WorldNow() - PC->GetDeathShownTime()) / 1.5f, 0.f, 1.f);
	Rect(0.f, 0.f, RefW, RefH, FLinearColor(0.12f, 0.f, 0.f, 0.45f * T));
	Rect(0.f, RefH * 0.32f, RefW, 300.f, FLinearColor(0.f, 0.f, 0.f, 0.7f * T));
	const FTOMatchResult& R = GI->LastResult;
	const FLinearColor Red = TOStyle::Danger.CopyWithNewOpacity(T);
	Text(R.Title.IsEmpty() ? FString(TEXT("KILLED IN ACTION")) : R.Title, RefW * 0.5f, RefH * 0.32f + 36.f, 64.f, Red, ETOAlign::Center);
	if (!R.KilledBy.IsEmpty())
	{
		Text(FString::Printf(TEXT("Killed by  %s"), *R.KilledBy), RefW * 0.5f, RefH * 0.32f + 120.f, 22.f, TOStyle::Text.CopyWithNewOpacity(T), ETOAlign::Center);
	}
	Text(FString::Printf(TEXT("Equipment lost: $%s     Kills: %d     Time: %s"), *TOUtil::FormatMoney(R.LoadoutCost), R.Kills, *TOUtil::FormatTime(R.Duration)),
		RefW * 0.5f, RefH * 0.32f + 160.f, 17.f, TOStyle::Dim.CopyWithNewOpacity(T), ETOAlign::Center);
	if (T >= 1.f)
	{
		Button(RefW * 0.5f - 160.f, RefH * 0.32f + 214.f, 320.f, 54.f, TEXT("CONTINUE"), [PC]() { PC->SetScreen(ETOScreen::Results); }, true, true, 22.f);
	}
}

void ATOHUD::DrawResults()
{
	ATOPlayerController* PC = GetPC();
	UTOGameInstance* GI = GetGI();
	if (!PC || !GI)
	{
		return;
	}
	DrawBackdrop(0.88f);
	const FTOMatchResult& R = GI->LastResult;
	const bool bOps = R.Mode == ETOMatchMode::Operations;
	const bool bGood = bOps ? R.bExtracted : R.bVictory;
	const FLinearColor TitleC = bGood ? TOStyle::Good : TOStyle::Danger;

	const float W = FMath::Min(RefW - 120.f, 1300.f);
	const float X = (RefW - W) * 0.5f;
	Rect(X, 120.f, 8.f, 90.f, TitleC);
	Text(R.Title.IsEmpty() ? FString(TEXT("MATCH OVER")) : R.Title, X + 28.f, 112.f, 70.f, TitleC);
	FString Sub;
	if (bOps)
	{
		Sub = R.bExtracted ? FString::Printf(TEXT("Extracted via %s"), *R.ExtractName) : (R.KilledBy.IsEmpty() ? FString(TEXT("Your equipment was lost.")) : FString::Printf(TEXT("Killed by %s"), *R.KilledBy));
	}
	else
	{
		Sub = R.bVictory ? TEXT("The Dam Valley front is ours.") : TEXT("The operation failed.");
	}
	Text(Sub, X + 32.f, 192.f, 20.f, TOStyle::Dim);

	const float LW = W * 0.45f;
	float Y = 260.f;
	Panel(X, Y, LW, 520.f);
	Header(TEXT("Summary"), X + 20.f, Y + 14.f, LW - 40.f);
	float RY = Y + 64.f;
	auto Row = [&](const FString& L, const FString& V, const FLinearColor& C)
	{
		Text(L, X + 24.f, RY, 18.f, TOStyle::Dim);
		Text(V, X + LW - 24.f, RY, 18.f, C, ETOAlign::Right);
		RY += 36.f;
	};
	Row(TEXT("Duration"), TOUtil::FormatTime(R.Duration), TOStyle::Text);
	Row(TEXT("Kills"), FString::Printf(TEXT("%d"), R.Kills), TOStyle::Text);
	Row(TEXT("Headshots"), FString::Printf(TEXT("%d"), R.Headshots), TOStyle::Text);
	if (bOps)
	{
		Row(TEXT("Value extracted"), FString::Printf(TEXT("$%s"), *TOUtil::FormatMoney(R.ValueExtracted)), TOStyle::Money);
		Row(TEXT("Loadout cost"), FString::Printf(TEXT("-$%s"), *TOUtil::FormatMoney(R.LoadoutCost)), TOStyle::Danger);
		const int64 Net = R.ValueExtracted - R.LoadoutCost;
		RY += 10.f;
		Rect(X + 24.f, RY - 8.f, LW - 48.f, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.2f));
		Text(TEXT("NET PROFIT"), X + 24.f, RY, 22.f, TOStyle::Text);
		Text(FString::Printf(TEXT("%s$%s"), Net >= 0 ? TEXT("+") : TEXT(""), *TOUtil::FormatMoney(Net)), X + LW - 24.f, RY - 4.f, 30.f, Net >= 0 ? TOStyle::Money : TOStyle::Danger, ETOAlign::Right);
	}
	else
	{
		Row(TEXT("Objectives captured"), FString::Printf(TEXT("%d"), R.Captures), TOStyle::Text);
		Row(TEXT("Credits earned"), FString::Printf(TEXT("+$%s"), *TOUtil::FormatMoney(R.ValueExtracted)), TOStyle::Money);
	}

	const float RX = X + LW + 20.f;
	const float RW = W - LW - 20.f;
	Panel(RX, Y, RW, 520.f);
	Header(bOps ? TEXT("Most valuable items") : TEXT("Battle report"), RX + 20.f, Y + 14.f, RW - 40.f);
	RY = Y + 64.f;
	if (bOps && R.TopItems.Num() > 0)
	{
		for (const FString& Entry : R.TopItems)
		{
			TArray<FString> Parts;
			Entry.ParseIntoArray(Parts, TEXT("|"), false);
			const FString Name = Parts.IsValidIndex(0) ? Parts[0] : Entry;
			const FString Value = Parts.IsValidIndex(1) ? Parts[1] : FString();
			const int32 Rarity = Parts.IsValidIndex(2) ? FCString::Atoi(*Parts[2]) : 0;
			const FLinearColor RC = TODB::RarityColor((ETORarity)FMath::Clamp(Rarity, 0, 5));
			Rect(RX + 20.f, RY, RW - 40.f, 36.f, RC.CopyWithNewOpacity(0.12f));
			Rect(RX + 20.f, RY, 4.f, 36.f, RC);
			Text(Fit(Name, 17.f, RW - 220.f), RX + 34.f, RY + 8.f, 17.f, TOStyle::Text);
			Text(FString::Printf(TEXT("$%s"), *Value), RX + RW - 30.f, RY + 8.f, 17.f, TOStyle::Money, ETOAlign::Right);
			RY += 42.f;
		}
	}
	else if (bOps)
	{
		Text(R.bExtracted ? TEXT("Nothing of value was extracted.") : TEXT("Everything you carried is lost - except the secure container."), RX + 20.f, RY, 16.f, TOStyle::Dim);
	}
	else
	{
		Text(R.bVictory ? TEXT("All sectors secured.") : TEXT("The attack was repelled / the line collapsed."), RX + 20.f, RY, 17.f, TOStyle::Text);
		Text(TEXT("Rewards: 1,500 per kill, 5,000 per capture, 40,000 victory / 15,000 participation."), RX + 20.f, RY + 34.f, 14.f, TOStyle::Dim);
	}

	Button(X + W - 420.f, Y + 540.f, 420.f, 62.f, TEXT("RETURN TO LOBBY"), [GI]() { GI->ReturnToLobby(); }, true, true, 22.f);
}

// ---------------------------------------------------------------------------------------------
//  Warfare redeploy
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawRespawn()
{
	ATOPlayerController* PC = GetPC();
	ATOGameMode* GM = GetGM();
	UTOGameInstance* GI = GetGI();
	if (!PC || !GM || !GI)
	{
		return;
	}
	DrawBackdrop(0.8f);
	DrawTitle(TEXT("Redeploy"), FString::Printf(TEXT("Sector %d / %d  -  %s  -  attacker tickets %d"), GM->GetCurrentSector() + 1, GM->GetNumSectors(), *GM->GetSectorName(), GM->GetTickets()));

	const float Total = FMath::Min(RefW - 80.f, 1800.f);
	const float X0 = (RefW - Total) * 0.5f;
	const float LW = Total * 0.55f;
	float Y = 130.f;
	Panel(X0, Y, LW, 840.f);
	Header(TEXT("Operator"), X0 + 20.f, Y + 14.f, LW - 40.f);
	OperatorCards(X0 + 20.f, Y + 56.f, LW - 40.f, PC->RespawnOperator, [PC](ETOOperator Op) { PC->RespawnOperator = Op; });
	Y += 232.f;
	Header(TEXT("Weapon"), X0 + 20.f, Y, LW - 40.f);
	Y += 46.f;
	const TArray<FName>& Ids = WarfareWeapons();
	const int32 Cols = 4;
	const float BW = (LW - 40.f - (Cols - 1) * 8.f) / Cols;
	for (int32 i = 0; i < Ids.Num(); ++i)
	{
		const FString Label = i == 0 ? FString::Printf(TEXT("CUSTOM: %s"), *WeaponName(GI->Session.Loadout.Primary.WeaponId)) : WeaponName(Ids[i]);
		const float BX = X0 + 20.f + (i % Cols) * (BW + 8.f);
		const float BY = Y + (i / Cols) * 48.f;
		Button(BX, BY, BW, 40.f, Label, [PC, i]() { PC->RespawnWeaponIndex = i; }, true, PC->RespawnWeaponIndex == i, 15.f);
	}
	Y += ((Ids.Num() + Cols - 1) / Cols) * 48.f + 10.f;
	const int32 WIdx = FMath::Clamp(PC->RespawnWeaponIndex, 0, Ids.Num() - 1);
	const FTOWeaponConfig Chosen = WIdx == 0 ? GI->Session.Loadout.Primary : TODB::MakeDefaultConfig(Ids[WIdx], 4);
	DrawWeaponStats(X0 + 20.f, Y, LW - 40.f, Chosen, nullptr);

	// Map + deploy
	const float RX = X0 + LW + 20.f;
	const float RW = Total - LW - 20.f;
	Panel(RX, 130.f, RW, 840.f);
	Header(TEXT("Front line"), RX + 20.f, 144.f, RW - 40.f);
	const float MapSize = FMath::Min(RW - 40.f, 560.f);
	DrawMapContents(RX + (RW - MapSize) * 0.5f, 190.f, MapSize, 0.f, 0.f, 1.f, false);

	const bool bCan = GM->CanPlayerRespawn();
	const FString Label = bCan ? FString(TEXT("DEPLOY")) : FString::Printf(TEXT("DEPLOY IN %.0f"), FMath::CeilToFloat(GM->GetRespawnWait()));
	Button(RX + 20.f, 130.f + 840.f - 84.f, RW - 40.f, 64.f, Label, [GM, PC, GI, Chosen]()
	{
		GI->Session.Loadout.Operator = PC->RespawnOperator;
		GM->PlayerRequestRespawn(PC->RespawnOperator, Chosen);
	}, bCan, true, 26.f);
}
