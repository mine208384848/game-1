// TAC-OPS - static game database
//
// Weapon / attachment / item balancing is modelled on the Delta Force (2024) sandbox:
// armor & ammo penetration tiers 1-6, rarity colors white..red, grid inventory sizes,
// gunsmith attachment slots and operator classes (Assault / Support / Engineer / Recon).
// All names of maps, operators, factions and valuables are original.

#include "Core/TODatabase.h"

namespace
{
	// Weapon class bit masks for attachment compatibility.
	constexpr uint32 M_AR = 1u << (uint32)ETOWeaponClass::AssaultRifle;
	constexpr uint32 M_SMG = 1u << (uint32)ETOWeaponClass::SMG;
	constexpr uint32 M_DMR = 1u << (uint32)ETOWeaponClass::DMR;
	constexpr uint32 M_SNP = 1u << (uint32)ETOWeaponClass::Sniper;
	constexpr uint32 M_LMG = 1u << (uint32)ETOWeaponClass::LMG;
	constexpr uint32 M_SG = 1u << (uint32)ETOWeaponClass::Shotgun;
	constexpr uint32 M_PST = 1u << (uint32)ETOWeaponClass::Pistol;
	constexpr uint32 M_RIFLE = M_AR | M_SMG | M_DMR | M_LMG;
	constexpr uint32 M_ALL = M_AR | M_SMG | M_DMR | M_SNP | M_LMG | M_SG | M_PST;

	int32 GNextUid = 1000;

	struct FCaliberInfo
	{
		FName Id;
		const TCHAR* Name;
		int32 Stack;
		float PricePerRound;
	};

	const TArray<FCaliberInfo>& Calibers()
	{
		static TArray<FCaliberInfo> C;
		if (C.Num() == 0)
		{
			C.Add({ FName(TEXT("9x19")), TEXT("9x19mm"), 60, 18.f });
			C.Add({ FName(TEXT("45ACP")), TEXT(".45 ACP"), 50, 24.f });
			C.Add({ FName(TEXT("57x28")), TEXT("5.7x28mm"), 60, 26.f });
			C.Add({ FName(TEXT("556x45")), TEXT("5.56x45mm"), 60, 34.f });
			C.Add({ FName(TEXT("545x39")), TEXT("5.45x39mm"), 60, 30.f });
			C.Add({ FName(TEXT("762x39")), TEXT("7.62x39mm"), 60, 36.f });
			C.Add({ FName(TEXT("762x51")), TEXT("7.62x51mm"), 40, 60.f });
			C.Add({ FName(TEXT("762x54")), TEXT("7.62x54R"), 40, 64.f });
			C.Add({ FName(TEXT("338LM")), TEXT(".338 Lapua"), 30, 180.f });
			C.Add({ FName(TEXT("12ga")), TEXT("12 Gauge"), 30, 40.f });
			C.Add({ FName(TEXT("50AE")), TEXT(".50 AE"), 30, 70.f });
		}
		return C;
	}

	const FCaliberInfo* FindCaliber(FName Id)
	{
		for (const FCaliberInfo& C : Calibers())
		{
			if (C.Id == Id)
			{
				return &C;
			}
		}
		return nullptr;
	}

	float TierPriceMult(int32 Tier)
	{
		static const float Mult[7] = { 0.5f, 0.6f, 1.0f, 1.8f, 3.4f, 6.0f, 9.0f };
		return Mult[FMath::Clamp(Tier, 0, 6)];
	}

	// ---------------------------------------------------------------------------------------
	//  Item table
	// ---------------------------------------------------------------------------------------

	struct FItemTable
	{
		TArray<FTOItemDef> Items;
		TMap<FName, int32> Index;
	};

	FTOItemDef& AddItem(FItemTable& T, const TCHAR* Id, const TCHAR* Name, const TCHAR* Short, ETOItemCategory Cat, ETORarity Rarity, int32 W, int32 H, int32 Value)
	{
		FTOItemDef D;
		D.Id = FName(Id);
		D.Name = Name;
		D.ShortName = Short;
		D.Category = Cat;
		D.Rarity = Rarity;
		D.W = W;
		D.H = H;
		D.Value = Value;
		const int32 Idx = T.Items.Add(D);
		T.Index.Add(D.Id, Idx);
		return T.Items[Idx];
	}

	void Valuable(FItemTable& T, const TCHAR* Id, const TCHAR* Name, const TCHAR* Short, ETORarity R, int32 W, int32 H, int32 Value, bool bTech, const TCHAR* Desc)
	{
		FTOItemDef& D = AddItem(T, Id, Name, Short, ETOItemCategory::Valuable, R, W, H, Value);
		D.bTech = bTech;
		D.Description = Desc;
	}

	const FItemTable& ItemTable();

	void BuildItems(FItemTable& T)
	{
		using R = ETORarity;

		// --- Valuables -------------------------------------------------------------------
		Valuable(T, TEXT("Val_Cigarettes"), TEXT("Cigarette Carton"), TEXT("Cigs"), R::Common, 1, 1, 900, false, TEXT("Universal currency in the valley."));
		Valuable(T, TEXT("Val_Lighter"), TEXT("Brass Lighter"), TEXT("Lighter"), R::Common, 1, 1, 1300, false, TEXT("Still works."));
		Valuable(T, TEXT("Val_Batteries"), TEXT("Batteries"), TEXT("Batt"), R::Common, 1, 1, 1100, true, TEXT("AA cells."));
		Valuable(T, TEXT("Val_DuctTape"), TEXT("Duct Tape"), TEXT("Tape"), R::Common, 1, 1, 900, false, TEXT("Fixes anything."));
		Valuable(T, TEXT("Val_Wires"), TEXT("Copper Wire"), TEXT("Wire"), R::Common, 1, 1, 1600, true, TEXT("Scrap copper."));
		Valuable(T, TEXT("Val_Screws"), TEXT("Box of Screws"), TEXT("Screws"), R::Common, 1, 1, 700, false, TEXT("Assorted hardware."));
		Valuable(T, TEXT("Val_Radio"), TEXT("Old Radio"), TEXT("Radio"), R::Common, 2, 1, 2400, true, TEXT("Vintage receiver."));
		Valuable(T, TEXT("Val_Canned"), TEXT("Canned Food"), TEXT("Cans"), R::Common, 1, 1, 800, false, TEXT("Field rations."));
		Valuable(T, TEXT("Val_Wrench"), TEXT("Pipe Wrench"), TEXT("Wrench"), R::Common, 1, 2, 1800, false, TEXT("Heavy steel wrench."));

		Valuable(T, TEXT("Val_PowerBank"), TEXT("Power Bank"), TEXT("PwrBnk"), R::Uncommon, 1, 1, 4200, true, TEXT("20,000 mAh."));
		Valuable(T, TEXT("Val_Circuit"), TEXT("Circuit Board"), TEXT("PCB"), R::Uncommon, 1, 1, 5600, true, TEXT("Salvaged electronics."));
		Valuable(T, TEXT("Val_HDD"), TEXT("Hard Drive"), TEXT("HDD"), R::Uncommon, 1, 1, 7800, true, TEXT("May contain data."));
		Valuable(T, TEXT("Val_Watch"), TEXT("Wristwatch"), TEXT("Watch"), R::Uncommon, 1, 1, 6400, false, TEXT("Steel diver watch."));
		Valuable(T, TEXT("Val_GasAnalyzer"), TEXT("Gas Analyzer"), TEXT("GasAn"), R::Uncommon, 1, 2, 8800, true, TEXT("Industrial sensor."));
		Valuable(T, TEXT("Val_Fuel"), TEXT("Fuel Canister"), TEXT("Fuel"), R::Uncommon, 2, 2, 7500, false, TEXT("Twenty liters of diesel."));
		Valuable(T, TEXT("Val_Coffee"), TEXT("Premium Coffee"), TEXT("Coffee"), R::Uncommon, 1, 1, 3600, false, TEXT("Officer's private stock."));
		Valuable(T, TEXT("Val_Multimeter"), TEXT("Multimeter"), TEXT("MMeter"), R::Uncommon, 1, 1, 5200, true, TEXT("Digital multimeter."));

		Valuable(T, TEXT("Val_GPU"), TEXT("Graphics Card"), TEXT("GPU"), R::Rare, 2, 1, 19000, true, TEXT("High-end GPU."));
		Valuable(T, TEXT("Val_USB"), TEXT("Encrypted USB Drive"), TEXT("USB"), R::Rare, 1, 1, 14500, true, TEXT("Military encryption."));
		Valuable(T, TEXT("Val_SatPhone"), TEXT("Satellite Phone"), TEXT("SatPh"), R::Rare, 1, 2, 22000, true, TEXT("Works anywhere."));
		Valuable(T, TEXT("Val_MilBattery"), TEXT("Military Battery"), TEXT("MilBat"), R::Rare, 2, 2, 26000, true, TEXT("Vehicle grade cell."));
		Valuable(T, TEXT("Val_NVTube"), TEXT("Night-Vision Tube"), TEXT("NVTube"), R::Rare, 1, 1, 17000, true, TEXT("Gen-3 intensifier."));
		Valuable(T, TEXT("Val_Laptop"), TEXT("Field Laptop"), TEXT("Laptop"), R::Rare, 2, 2, 28000, true, TEXT("Rugged laptop."));
		Valuable(T, TEXT("Val_Radar"), TEXT("Radar Module"), TEXT("RadMod"), R::Rare, 2, 1, 21000, true, TEXT("Phased array module."));

		Valuable(T, TEXT("Val_Tablet"), TEXT("Tactical Tablet"), TEXT("Tablet"), R::Epic, 2, 1, 52000, true, TEXT("Battle management tablet."));
		Valuable(T, TEXT("Val_DroneCtl"), TEXT("Drone Controller"), TEXT("DroneC"), R::Epic, 2, 2, 64000, true, TEXT("UAV ground station."));
		Valuable(T, TEXT("Val_Coins"), TEXT("Rare Coin Set"), TEXT("Coins"), R::Epic, 1, 1, 48000, false, TEXT("Collector's coins."));
		Valuable(T, TEXT("Val_MilLaptop"), TEXT("Military Laptop"), TEXT("MilLap"), R::Epic, 2, 2, 88000, true, TEXT("Contains operation plans."));
		Valuable(T, TEXT("Val_EncSSD"), TEXT("Encrypted SSD"), TEXT("SSD"), R::Epic, 1, 1, 41000, true, TEXT("Classified storage."));
		Valuable(T, TEXT("Val_Thermal"), TEXT("Thermal Imager"), TEXT("Thermal"), R::Epic, 2, 1, 76000, true, TEXT("Handheld thermal camera."));

		Valuable(T, TEXT("Val_GoldBar"), TEXT("Gold Bar"), TEXT("Gold"), R::Legendary, 1, 1, 165000, false, TEXT("999.9 fine gold."));
		Valuable(T, TEXT("Val_ServerBlade"), TEXT("Server Blade"), TEXT("Blade"), R::Legendary, 3, 1, 240000, true, TEXT("Data center blade."));
		Valuable(T, TEXT("Val_QuantumCPU"), TEXT("Quantum Processor"), TEXT("QPU"), R::Legendary, 1, 1, 210000, true, TEXT("Experimental processor."));
		Valuable(T, TEXT("Val_Vase"), TEXT("Antique Vase"), TEXT("Vase"), R::Legendary, 2, 2, 185000, false, TEXT("Priceless antiquity."));
		Valuable(T, TEXT("Val_LuxWatch"), TEXT("Luxury Chronograph"), TEXT("Chrono"), R::Legendary, 1, 1, 150000, false, TEXT("Swiss made."));
		Valuable(T, TEXT("Val_Ledger"), TEXT("Black Ledger"), TEXT("Ledger"), R::Legendary, 1, 2, 130000, false, TEXT("The commander's accounts."));

		Valuable(T, TEXT("Val_AzureHeart"), TEXT("Azure Heart Diamond"), TEXT("AzHeart"), R::Mythic, 1, 1, 1450000, false, TEXT("A legendary blue diamond."));
		Valuable(T, TEXT("Val_FusionCell"), TEXT("Mini Fusion Cell"), TEXT("Fusion"), R::Mythic, 2, 2, 2200000, true, TEXT("Prototype power source."));
		Valuable(T, TEXT("Val_ExoCore"), TEXT("Exoskeleton Core"), TEXT("ExoCore"), R::Mythic, 2, 3, 1800000, true, TEXT("Powered armor heart."));
		Valuable(T, TEXT("Val_Crown"), TEXT("Ancient Crown"), TEXT("Crown"), R::Mythic, 2, 2, 2600000, false, TEXT("Royal relic."));
		Valuable(T, TEXT("Val_ClassServer"), TEXT("Classified Server"), TEXT("CServer"), R::Mythic, 3, 3, 3400000, true, TEXT("The dam's secret archive."));

		// --- Medical ---------------------------------------------------------------------
		{
			FTOItemDef& D = AddItem(T, TEXT("Med_Bandage"), TEXT("Bandage"), TEXT("Bandage"), ETOItemCategory::Medical, R::Common, 1, 1, 1200);
			D.bStopBleed = true; D.Heal = 5.f; D.Uses = 1; D.UseTime = 2.5f; D.Description = TEXT("Stops bleeding.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Med_Splint"), TEXT("Splint"), TEXT("Splint"), ETOItemCategory::Medical, R::Uncommon, 1, 1, 2400);
			D.bFixFracture = true; D.Uses = 1; D.UseTime = 3.f; D.Description = TEXT("Fixes one fracture.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Med_Medkit"), TEXT("Field Medkit"), TEXT("Medkit"), ETOItemCategory::Medical, R::Uncommon, 1, 2, 9000);
			D.Heal = 40.f; D.Uses = 5; D.UseTime = 3.f; D.Description = TEXT("Restores 40 HP per use.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Med_Trauma"), TEXT("Trauma Kit"), TEXT("Trauma"), ETOItemCategory::Medical, R::Rare, 2, 2, 22000);
			D.Heal = 30.f; D.bStopBleed = true; D.bFixFracture = true; D.Uses = 3; D.UseTime = 5.f; D.Description = TEXT("Stops bleeding, fixes fractures, +30 HP.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Med_Painkiller"), TEXT("Painkillers"), TEXT("Pills"), ETOItemCategory::Medical, R::Uncommon, 1, 1, 4500);
			D.Painkiller = 90.f; D.Heal = 3.f; D.Uses = 4; D.UseTime = 1.5f; D.Description = TEXT("Ignore fracture penalties for 90 s.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Med_Adrenaline"), TEXT("Adrenaline Shot"), TEXT("Adren"), ETOItemCategory::Medical, R::Rare, 1, 1, 8000);
			D.Painkiller = 30.f; D.Heal = 10.f; D.Uses = 1; D.UseTime = 1.f; D.Description = TEXT("+10 HP, stamina boost, painkiller 30 s.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Med_Surgical"), TEXT("Surgical Kit"), TEXT("Surgery"), ETOItemCategory::Medical, R::Epic, 3, 2, 38000);
			D.Heal = 60.f; D.bFixFracture = true; D.bStopBleed = true; D.Uses = 2; D.UseTime = 7.f; D.Description = TEXT("Full field surgery: +60 HP and fixes everything.");
		}

		// --- Grenades --------------------------------------------------------------------
		{
			FTOItemDef& D = AddItem(T, TEXT("Gren_Frag"), TEXT("Frag Grenade"), TEXT("Frag"), ETOItemCategory::Grenade, R::Uncommon, 1, 1, 4000);
			D.GrenadeType = ETOGrenadeType::Frag; D.Description = TEXT("Lethal radius 6 m.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Gren_Smoke"), TEXT("Smoke Grenade"), TEXT("Smoke"), ETOItemCategory::Grenade, R::Common, 1, 1, 2500);
			D.GrenadeType = ETOGrenadeType::Smoke; D.Description = TEXT("Blocks line of sight for 25 s.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Gren_Flash"), TEXT("Flashbang"), TEXT("Flash"), ETOItemCategory::Grenade, R::Uncommon, 1, 1, 3000);
			D.GrenadeType = ETOGrenadeType::Flash; D.Description = TEXT("Blinds enemies that look at it.");
		}

		// --- Keys ------------------------------------------------------------------------
		{
			FTOItemDef& D = AddItem(T, TEXT("Key_AdminBlue"), TEXT("Admin Keycard (Blue)"), TEXT("BlueKey"), ETOItemCategory::Key, R::Epic, 1, 1, 85000);
			D.KeyId = FName(TEXT("AdminBlue")); D.Description = TEXT("Opens the Director's office, Admin Building 4F.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Key_ServerRed"), TEXT("Server Keycard (Red)"), TEXT("RedKey"), ETOItemCategory::Key, R::Legendary, 1, 1, 260000);
			D.KeyId = FName(TEXT("ServerRed")); D.Description = TEXT("Opens the server vault, Admin Building 2F.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Key_Armory"), TEXT("Armory Key"), TEXT("ArmKey"), ETOItemCategory::Key, R::Rare, 1, 1, 30000);
			D.KeyId = FName(TEXT("Armory")); D.Description = TEXT("Opens the armory bunker at the Military Base.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Key_DamControl"), TEXT("Dam Control Key"), TEXT("DamKey"), ETOItemCategory::Key, R::Rare, 1, 1, 26000);
			D.KeyId = FName(TEXT("DamControl")); D.Description = TEXT("Opens the control room on top of the dam.");
		}
		{
			FTOItemDef& D = AddItem(T, TEXT("Key_Warehouse"), TEXT("Logistics Office Key"), TEXT("WhKey"), ETOItemCategory::Key, R::Uncommon, 1, 1, 9000);
			D.KeyId = FName(TEXT("Warehouse")); D.Description = TEXT("Opens the logistics yard office.");
		}

		// --- Armor & helmets -------------------------------------------------------------
		static const TCHAR* ArmorNames[6] = { TEXT("Police Vest"), TEXT("Soft Kevlar Vest"), TEXT("Tactical Plate Carrier"), TEXT("Heavy Assault Vest"), TEXT("Ranger Armor"), TEXT("Juggernaut Plate") };
		static const float ArmorDur[6] = { 25.f, 40.f, 60.f, 80.f, 100.f, 125.f };
		static const int32 ArmorVal[6] = { 8000, 18000, 38000, 70000, 130000, 240000 };
		static const TCHAR* HelmetNames[6] = { TEXT("Bump Cap"), TEXT("MICH Lite"), TEXT("Tactical Helmet"), TEXT("FAST Ballistic"), TEXT("Heavy Ballistic Helmet"), TEXT("Assault Visor Helmet") };
		static const float HelmetDur[6] = { 15.f, 25.f, 35.f, 45.f, 55.f, 70.f };
		static const int32 HelmetVal[6] = { 5000, 12000, 25000, 48000, 90000, 170000 };
		for (int32 L = 1; L <= 6; ++L)
		{
			const ETORarity Rar = (ETORarity)FMath::Clamp(L - 1, 0, 5);
			{
				const FString Id = FString::Printf(TEXT("Armor_%d"), L);
				const FString Nm = FString::Printf(TEXT("%s (Lv%d)"), ArmorNames[L - 1], L);
				const FString Sh = FString::Printf(TEXT("Armor%d"), L);
				FTOItemDef& D = AddItem(T, *Id, *Nm, *Sh, ETOItemCategory::Armor, Rar, 3, 3, ArmorVal[L - 1]);
				D.Level = L; D.Durability = ArmorDur[L - 1];
				D.Description = FString::Printf(TEXT("Body armor, protection level %d. Covers thorax and stomach."), L);
			}
			{
				const FString Id = FString::Printf(TEXT("Helmet_%d"), L);
				const FString Nm = FString::Printf(TEXT("%s (Lv%d)"), HelmetNames[L - 1], L);
				const FString Sh = FString::Printf(TEXT("Helm%d"), L);
				FTOItemDef& D = AddItem(T, *Id, *Nm, *Sh, ETOItemCategory::Helmet, Rar, 2, 2, HelmetVal[L - 1]);
				D.Level = L; D.Durability = HelmetDur[L - 1];
				D.Description = FString::Printf(TEXT("Ballistic helmet, protection level %d."), L);
			}
		}

		// --- Rigs & backpacks ----------------------------------------------------------
		static const TCHAR* RigNames[4] = { TEXT("Chest Pouch"), TEXT("Light Chest Rig"), TEXT("Assault Rig"), TEXT("Heavy Battle Rig") };
		static const int32 RigW[4] = { 3, 4, 4, 5 };
		static const int32 RigH[4] = { 2, 2, 3, 3 };
		static const int32 RigVal[4] = { 4000, 10000, 22000, 45000 };
		static const TCHAR* PackNames[4] = { TEXT("Sling Bag"), TEXT("Assault Pack"), TEXT("Raid Backpack"), TEXT("Expedition Pack") };
		static const int32 PackW[4] = { 4, 5, 6, 7 };
		static const int32 PackH[4] = { 3, 4, 5, 6 };
		static const int32 PackVal[4] = { 6000, 15000, 32000, 60000 };
		for (int32 Tr = 1; Tr <= 4; ++Tr)
		{
			const ETORarity Rar = (ETORarity)FMath::Clamp(Tr - 1, 0, 5);
			{
				const FString Id = FString::Printf(TEXT("Rig_%d"), Tr);
				const FString Sh = FString::Printf(TEXT("Rig%d"), Tr);
				FTOItemDef& D = AddItem(T, *Id, RigNames[Tr - 1], *Sh, ETOItemCategory::Rig, Rar, 2, (Tr >= 3) ? 3 : 2, RigVal[Tr - 1]);
				D.Level = Tr; D.GridW = RigW[Tr - 1]; D.GridH = RigH[Tr - 1];
				D.Description = FString::Printf(TEXT("Chest rig with %dx%d storage."), D.GridW, D.GridH);
			}
			{
				const FString Id = FString::Printf(TEXT("Pack_%d"), Tr);
				const FString Sh = FString::Printf(TEXT("Pack%d"), Tr);
				FTOItemDef& D = AddItem(T, *Id, PackNames[Tr - 1], *Sh, ETOItemCategory::Backpack, Rar, (Tr >= 3) ? 4 : 3, Tr + 2, PackVal[Tr - 1]);
				D.Level = Tr; D.GridW = PackW[Tr - 1]; D.GridH = PackH[Tr - 1];
				D.Description = FString::Printf(TEXT("Backpack with %dx%d storage."), D.GridW, D.GridH);
			}
		}

		// --- Ammo (every caliber in tiers 1..6) -----------------------------------------
		for (const FCaliberInfo& C : Calibers())
		{
			for (int32 Tier = 1; Tier <= 6; ++Tier)
			{
				const FString Id = TODB::AmmoItemId(C.Id, Tier).ToString();
				const FString Nm = FString::Printf(TEXT("%s Lv%d"), C.Name, Tier);
				const FString Sh = FString::Printf(TEXT("%s L%d"), C.Name, Tier);
				const ETORarity Rar = (ETORarity)FMath::Clamp(Tier - 1, 0, 5);
				FTOItemDef& D = AddItem(T, *Id, *Nm, *Sh, ETOItemCategory::Ammo, Rar, 1, 1, FMath::Max(1, FMath::RoundToInt(C.PricePerRound * TierPriceMult(Tier))));
				D.Caliber = C.Id;
				D.Level = Tier;
				D.StackMax = C.Stack;
				D.Description = FString::Printf(TEXT("Penetration level %d. Value is per round."), Tier);
			}
		}

		// --- Weapons as items -----------------------------------------------------------
		for (const FTOWeaponDef& W : TODB::Weapons())
		{
			if (W.Class == ETOWeaponClass::Melee)
			{
				continue;
			}
			const FString Id = TODB::WeaponItemId(W.Id).ToString();
			ETORarity Rar = ETORarity::Uncommon;
			if (W.Price >= 120000) Rar = ETORarity::Legendary;
			else if (W.Price >= 60000) Rar = ETORarity::Epic;
			else if (W.Price >= 35000) Rar = ETORarity::Rare;
			else if (W.Price < 10000) Rar = ETORarity::Common;
			FTOItemDef& D = AddItem(T, *Id, *W.Name, *W.Name, ETOItemCategory::Weapon, Rar, W.InvW, W.InvH, W.Price);
			D.WeaponId = W.Id;
			D.Caliber = W.Caliber;
			D.Description = FString::Printf(TEXT("%s chambered in %s."), TODB::WeaponClassName(W.Class), TODB::CaliberName(W.Caliber));
		}
	}

	const FItemTable& ItemTable()
	{
		static FItemTable Table;
		static bool bBuilt = false;
		if (!bBuilt)
		{
			bBuilt = true;
			BuildItems(Table);
		}
		return Table;
	}

	// ---------------------------------------------------------------------------------------
	//  Weapon table
	// ---------------------------------------------------------------------------------------

	struct FWeaponTable
	{
		TArray<FTOWeaponDef> Weapons;
		TMap<FName, int32> Index;
	};

	FTOWeaponDef& AddWeapon(FWeaponTable& T, const TCHAR* Id, const TCHAR* Name, ETOWeaponClass Class, const TCHAR* Caliber,
		float Damage, float RPM, int32 Mag, float Reload, float ReloadEmpty, float Velocity,
		float RecoilV, float RecoilH, float Hip, float Ads, float AdsTime, float Range, float ArmorDmg, int32 Price)
	{
		FTOWeaponDef D;
		D.Id = FName(Id);
		D.Name = Name;
		D.Class = Class;
		D.Caliber = FName(Caliber);
		D.Damage = Damage;
		D.RPM = RPM;
		D.MagSize = Mag;
		D.ReloadTime = Reload;
		D.ReloadEmptyTime = ReloadEmpty;
		D.Velocity = Velocity;
		D.RecoilV = RecoilV;
		D.RecoilH = RecoilH;
		D.HipSpread = Hip;
		D.AdsSpread = Ads;
		D.AdsTime = AdsTime;
		D.Range = Range;
		D.ArmorDamage = ArmorDmg;
		D.Price = Price;
		const int32 Idx = T.Weapons.Add(D);
		T.Index.Add(D.Id, Idx);
		return T.Weapons[Idx];
	}

	void Visual(FTOWeaponDef& D, uint8 Style, float RecvLen, float RecvH, float BarrelLen, ETOMat Body, ETOMat Furniture, int32 InvW, int32 InvH, float MoveMult)
	{
		D.Style = Style;
		D.RecvLen = RecvLen;
		D.RecvH = RecvH;
		D.BarrelLen = BarrelLen;
		D.BodyMat = Body;
		D.FurnitureMat = Furniture;
		D.InvW = InvW;
		D.InvH = InvH;
		D.MoveMult = MoveMult;
	}

	void BuildWeapons(FWeaponTable& T)
	{
		using C = ETOWeaponClass;
		using F = ETOFireMode;

		// Assault rifles ----------------------------------------------------------------------
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("M4A1"), TEXT("M4A1"), C::AssaultRifle, TEXT("556x45"), 34, 800, 30, 2.1f, 2.7f, 880, 0.42f, 0.22f, 2.4f, 0.12f, 0.22f, 65, 1.0f, 42000);
			D.Modes = { F::Auto, F::Semi }; Visual(D, 0, 30, 7, 36, ETOMat::Gun, ETOMat::Gun, 4, 2, 1.0f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("AKM"), TEXT("AKM"), C::AssaultRifle, TEXT("762x39"), 41, 600, 30, 2.4f, 3.0f, 715, 0.62f, 0.30f, 2.6f, 0.14f, 0.26f, 55, 1.15f, 36000);
			D.Modes = { F::Auto, F::Semi }; Visual(D, 1, 32, 7.5f, 40, ETOMat::Gun, ETOMat::WoodDark, 4, 2, 0.98f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("K416"), TEXT("K416"), C::AssaultRifle, TEXT("556x45"), 35, 760, 30, 2.2f, 2.8f, 890, 0.40f, 0.20f, 2.3f, 0.11f, 0.23f, 68, 1.0f, 52000);
			D.Modes = { F::Auto, F::Semi }; Visual(D, 0, 31, 7, 34, ETOMat::Gun, ETOMat::GunTan, 4, 2, 1.0f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("AUG"), TEXT("AUG A3"), C::AssaultRifle, TEXT("556x45"), 33, 700, 30, 2.5f, 3.1f, 940, 0.36f, 0.18f, 2.2f, 0.10f, 0.25f, 70, 1.0f, 48000);
			D.Modes = { F::Auto, F::Semi }; Visual(D, 2, 26, 8, 44, ETOMat::GunOlive, ETOMat::GunOlive, 4, 2, 1.0f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("AK12"), TEXT("AK-12"), C::AssaultRifle, TEXT("545x39"), 34, 700, 30, 2.3f, 2.9f, 880, 0.40f, 0.24f, 2.5f, 0.12f, 0.24f, 62, 1.05f, 46000);
			D.Modes = { F::Auto, F::Burst, F::Semi }; Visual(D, 1, 32, 7.5f, 40, ETOMat::Gun, ETOMat::Gun, 4, 2, 0.99f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("SCARH"), TEXT("SCAR-H"), C::AssaultRifle, TEXT("762x51"), 49, 580, 20, 2.4f, 3.0f, 800, 0.78f, 0.30f, 2.6f, 0.12f, 0.28f, 75, 1.3f, 68000);
			D.Modes = { F::Auto, F::Semi }; Visual(D, 0, 33, 8, 38, ETOMat::GunTan, ETOMat::GunTan, 4, 2, 0.96f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("G3"), TEXT("G3A3"), C::AssaultRifle, TEXT("762x51"), 50, 550, 20, 2.5f, 3.2f, 790, 0.85f, 0.32f, 2.7f, 0.13f, 0.30f, 72, 1.3f, 54000);
			D.Modes = { F::Auto, F::Semi }; Visual(D, 0, 34, 8, 42, ETOMat::GunOlive, ETOMat::GunOlive, 5, 2, 0.95f);
		}

		// SMGs ---------------------------------------------------------------------------------
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("MP5"), TEXT("MP5"), C::SMG, TEXT("9x19"), 27, 800, 30, 2.0f, 2.4f, 400, 0.24f, 0.16f, 1.8f, 0.14f, 0.17f, 32, 0.75f, 22000);
			D.Modes = { F::Auto, F::Burst, F::Semi }; Visual(D, 3, 24, 7, 20, ETOMat::Gun, ETOMat::Gun, 3, 2, 1.06f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("Vector"), TEXT("Vector .45"), C::SMG, TEXT("45ACP"), 26, 1150, 25, 1.9f, 2.3f, 300, 0.20f, 0.18f, 1.9f, 0.16f, 0.16f, 26, 0.75f, 38000);
			D.Modes = { F::Auto, F::Burst, F::Semi }; Visual(D, 10, 26, 10, 18, ETOMat::Gun, ETOMat::Gun, 3, 2, 1.07f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("P90"), TEXT("P90"), C::SMG, TEXT("57x28"), 25, 900, 50, 2.6f, 3.1f, 715, 0.20f, 0.16f, 1.7f, 0.13f, 0.18f, 38, 0.95f, 34000);
			D.Modes = { F::Auto, F::Semi }; Visual(D, 10, 34, 9, 16, ETOMat::Gun, ETOMat::Gun, 3, 2, 1.06f);
		}

		// DMRs ---------------------------------------------------------------------------------
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("SR25"), TEXT("SR-25"), C::DMR, TEXT("762x51"), 62, 300, 20, 2.5f, 3.1f, 820, 1.1f, 0.30f, 3.2f, 0.06f, 0.32f, 95, 1.3f, 72000);
			D.Modes = { F::Semi }; Visual(D, 4, 34, 7.5f, 52, ETOMat::Gun, ETOMat::Gun, 5, 2, 0.95f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("M14"), TEXT("M14 EBR"), C::DMR, TEXT("762x51"), 60, 320, 20, 2.5f, 3.2f, 850, 1.2f, 0.35f, 3.3f, 0.07f, 0.30f, 90, 1.3f, 58000);
			D.Modes = { F::Semi }; Visual(D, 4, 36, 7, 50, ETOMat::Gun, ETOMat::Wood, 5, 2, 0.95f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("SVD"), TEXT("SVD"), C::DMR, TEXT("762x54"), 72, 260, 10, 2.6f, 3.3f, 830, 1.4f, 0.35f, 3.5f, 0.06f, 0.34f, 105, 1.4f, 66000);
			D.Modes = { F::Semi }; Visual(D, 4, 38, 7.5f, 58, ETOMat::Gun, ETOMat::WoodDark, 5, 2, 0.94f);
		}

		// Snipers ------------------------------------------------------------------------------
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("M700"), TEXT("M700"), C::Sniper, TEXT("762x51"), 98, 50, 5, 3.2f, 3.8f, 860, 2.2f, 0.4f, 4.5f, 0.03f, 0.38f, 140, 1.5f, 64000);
			D.Modes = { F::Bolt }; Visual(D, 5, 30, 6, 62, ETOMat::Gun, ETOMat::PlasticTan, 5, 2, 0.93f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("AWM"), TEXT("AWM"), C::Sniper, TEXT("338LM"), 128, 45, 5, 3.6f, 4.2f, 915, 2.8f, 0.5f, 4.8f, 0.02f, 0.42f, 170, 1.8f, 145000);
			D.Modes = { F::Bolt }; Visual(D, 5, 34, 7, 68, ETOMat::GunOlive, ETOMat::GunOlive, 5, 2, 0.9f);
		}

		// LMGs ---------------------------------------------------------------------------------
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("M249"), TEXT("M249"), C::LMG, TEXT("556x45"), 33, 750, 100, 5.2f, 6.0f, 915, 0.42f, 0.32f, 3.0f, 0.18f, 0.40f, 70, 1.0f, 78000);
			D.Modes = { F::Auto }; Visual(D, 6, 36, 9, 46, ETOMat::Gun, ETOMat::Gun, 5, 2, 0.88f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("PKM"), TEXT("PKM"), C::LMG, TEXT("762x54"), 45, 650, 100, 5.8f, 6.5f, 825, 0.60f, 0.38f, 3.2f, 0.20f, 0.44f, 82, 1.3f, 92000);
			D.Modes = { F::Auto }; Visual(D, 6, 38, 9, 52, ETOMat::Gun, ETOMat::WoodDark, 5, 2, 0.86f);
		}

		// Shotguns -----------------------------------------------------------------------------
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("M870"), TEXT("M870"), C::Shotgun, TEXT("12ga"), 19, 75, 6, 3.2f, 3.8f, 400, 2.5f, 0.6f, 4.0f, 2.0f, 0.28f, 18, 0.6f, 18000);
			D.Pellets = 9; D.Modes = { F::Bolt }; Visual(D, 7, 30, 7, 46, ETOMat::Gun, ETOMat::WoodDark, 4, 2, 0.98f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("M1014"), TEXT("M1014"), C::Shotgun, TEXT("12ga"), 17, 300, 7, 3.4f, 3.9f, 400, 1.8f, 0.5f, 4.0f, 2.0f, 0.28f, 18, 0.6f, 32000);
			D.Pellets = 9; D.Modes = { F::Semi }; Visual(D, 7, 32, 7, 44, ETOMat::Gun, ETOMat::Gun, 4, 2, 0.97f);
		}

		// Pistols ------------------------------------------------------------------------------
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("G17"), TEXT("G17"), C::Pistol, TEXT("9x19"), 28, 400, 17, 1.5f, 1.9f, 360, 0.55f, 0.25f, 1.6f, 0.25f, 0.14f, 22, 0.75f, 6000);
			D.Modes = { F::Semi }; Visual(D, 8, 18, 3.5f, 10, ETOMat::Gun, ETOMat::Gun, 2, 1, 1.0f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("G18"), TEXT("G18C"), C::Pistol, TEXT("9x19"), 26, 1100, 33, 1.7f, 2.1f, 360, 0.42f, 0.32f, 2.0f, 0.30f, 0.15f, 20, 0.75f, 14000);
			D.Modes = { F::Auto, F::Semi }; Visual(D, 8, 18, 3.5f, 11, ETOMat::Gun, ETOMat::Gun, 2, 1, 1.0f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("M1911"), TEXT("M1911"), C::Pistol, TEXT("45ACP"), 34, 380, 8, 1.6f, 2.0f, 255, 0.70f, 0.30f, 1.7f, 0.25f, 0.15f, 20, 0.8f, 8000);
			D.Modes = { F::Semi }; Visual(D, 8, 20, 3.5f, 11, ETOMat::MetalGrey, ETOMat::WoodDark, 2, 1, 1.0f);
		}
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("DEagle"), TEXT("Desert .50"), C::Pistol, TEXT("50AE"), 62, 240, 7, 1.8f, 2.2f, 470, 1.6f, 0.5f, 2.4f, 0.35f, 0.17f, 30, 1.1f, 24000);
			D.Modes = { F::Semi }; Visual(D, 8, 24, 4.2f, 14, ETOMat::MetalGrey, ETOMat::Gun, 2, 1, 1.0f);
		}

		// Melee --------------------------------------------------------------------------------
		{
			FTOWeaponDef& D = AddWeapon(T, TEXT("Knife"), TEXT("Combat Knife"), C::Melee, TEXT("None"), 65, 90, 0, 0.f, 0.f, 0, 0.f, 0.f, 0.f, 0.f, 0.1f, 2, 0.3f, 0);
			D.Modes = { F::Semi }; Visual(D, 9, 12, 3, 18, ETOMat::MetalGrey, ETOMat::Rubber, 1, 2, 1.08f);
		}
	}

	const FWeaponTable& WeaponTable()
	{
		static FWeaponTable Table;
		static bool bBuilt = false;
		if (!bBuilt)
		{
			bBuilt = true;
			BuildWeapons(Table);
		}
		return Table;
	}

	// ---------------------------------------------------------------------------------------
	//  Attachment table
	// ---------------------------------------------------------------------------------------

	struct FAttachmentTable
	{
		TArray<FTOAttachmentDef> Attachments;
		TMap<FName, int32> Index;
	};

	FTOAttachmentDef& AddAtt(FAttachmentTable& T, const TCHAR* Id, const TCHAR* Name, ETOAttachSlot Slot, uint32 Mask, int32 Price)
	{
		FTOAttachmentDef D;
		D.Id = FName(Id);
		D.Name = Name;
		D.Slot = Slot;
		D.ClassMask = Mask;
		D.Price = Price;
		const int32 Idx = T.Attachments.Add(D);
		T.Index.Add(D.Id, Idx);
		return T.Attachments[Idx];
	}

	void BuildAttachments(FAttachmentTable& T)
	{
		using S = ETOAttachSlot;
		using O = ETOOpticType;

		// Optics
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Opt_RedDot"), TEXT("Mini Red Dot"), S::Optic, M_RIFLE | M_SG | M_PST | M_SNP, 3500); D.Optic = O::RedDot; D.Zoom = 1.25f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Opt_Holo"), TEXT("Holographic Sight"), S::Optic, M_RIFLE | M_SG, 6000); D.Optic = O::Holo; D.Zoom = 1.35f; D.AdsTimeMult = 1.03f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Opt_Prism"), TEXT("3x Prism Scope"), S::Optic, M_AR | M_SMG | M_DMR | M_LMG, 9000); D.Optic = O::Prism; D.Zoom = 3.0f; D.AdsTimeMult = 1.08f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Opt_ACOG"), TEXT("4x Combat Scope"), S::Optic, M_AR | M_DMR | M_LMG, 12000); D.Optic = O::ACOG; D.Zoom = 4.0f; D.AdsTimeMult = 1.12f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Opt_Sniper"), TEXT("8x Sniper Scope"), S::Optic, M_AR | M_DMR | M_SNP, 22000); D.Optic = O::Sniper; D.Zoom = 8.0f; D.AdsTimeMult = 1.2f; }

		// Muzzle
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Muz_Comp"), TEXT("Compensator"), S::Muzzle, M_RIFLE | M_PST, 4000); D.RecoilVMult = 0.92f; D.RecoilHMult = 0.82f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Muz_Brake"), TEXT("Muzzle Brake"), S::Muzzle, M_AR | M_DMR | M_SNP | M_LMG, 5500); D.RecoilVMult = 0.84f; D.RecoilHMult = 0.95f; D.NoiseMult = 1.25f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Muz_Flash"), TEXT("Flash Hider"), S::Muzzle, M_RIFLE | M_SNP, 2500); D.RecoilVMult = 0.95f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Muz_Supp"), TEXT("Suppressor"), S::Muzzle, M_AR | M_SMG | M_DMR | M_SNP | M_LMG | M_PST, 14000); D.bSuppressor = true; D.NoiseMult = 0.3f; D.RecoilVMult = 0.94f; D.AdsTimeMult = 1.05f; D.VelocityMult = 1.04f; D.RangeMult = 1.05f; }

		// Barrel
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Brl_Short"), TEXT("Short Barrel"), S::Barrel, M_AR | M_SMG | M_DMR | M_LMG | M_SG, 3000); D.AdsTimeMult = 0.9f; D.RangeMult = 0.85f; D.VelocityMult = 0.92f; D.RecoilVMult = 1.06f; D.MoveMult = 1.02f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Brl_Long"), TEXT("Extended Barrel"), S::Barrel, M_AR | M_SMG | M_DMR | M_SNP | M_LMG | M_SG, 7000); D.AdsTimeMult = 1.08f; D.RangeMult = 1.2f; D.VelocityMult = 1.07f; D.RecoilVMult = 0.95f; D.MoveMult = 0.98f; }

		// Underbarrel
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Ub_Vert"), TEXT("Vertical Grip"), S::Underbarrel, M_AR | M_SMG | M_DMR | M_LMG | M_SG, 3000); D.RecoilVMult = 0.88f; D.AdsTimeMult = 1.02f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Ub_Angled"), TEXT("Angled Grip"), S::Underbarrel, M_AR | M_SMG | M_DMR | M_LMG | M_SG, 3500); D.AdsTimeMult = 0.9f; D.RecoilHMult = 0.92f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Ub_Bipod"), TEXT("Bipod"), S::Underbarrel, M_LMG | M_DMR | M_SNP, 4000); D.bBipod = true; D.AdsTimeMult = 1.05f; }

		// Stock
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Stk_Light"), TEXT("Lightweight Stock"), S::Stock, M_AR | M_SMG | M_DMR | M_LMG | M_SG, 2500); D.AdsTimeMult = 0.92f; D.RecoilVMult = 1.06f; D.MoveMult = 1.02f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Stk_Heavy"), TEXT("Heavy Stock"), S::Stock, M_AR | M_SMG | M_DMR | M_SNP | M_LMG | M_SG, 4500); D.RecoilVMult = 0.88f; D.RecoilHMult = 0.9f; D.AdsTimeMult = 1.06f; D.MoveMult = 0.98f; }

		// Magazine
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Mag_Ext"), TEXT("Extended Magazine"), S::Magazine, M_AR | M_SMG | M_DMR | M_SG | M_PST | M_SNP, 4000); D.MagMult = 1.5f; D.ReloadMult = 1.1f; D.AdsTimeMult = 1.03f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Mag_Drum"), TEXT("Drum Magazine"), S::Magazine, M_AR | M_SMG, 9000); D.MagMult = 2.0f; D.ReloadMult = 1.3f; D.AdsTimeMult = 1.1f; D.MoveMult = 0.97f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Mag_Fast"), TEXT("Speed Magazine"), S::Magazine, M_AR | M_SMG | M_DMR | M_SNP | M_LMG | M_PST, 5000); D.ReloadMult = 0.78f; }

		// Tactical
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Tac_Light"), TEXT("Tactical Flashlight"), S::Tactical, M_ALL, 2000); D.bLight = true; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Tac_Laser"), TEXT("Laser Sight"), S::Tactical, M_ALL, 4000); D.bLaser = true; D.HipSpreadMult = 0.75f; }
		{ FTOAttachmentDef& D = AddAtt(T, TEXT("Tac_Combo"), TEXT("Light / Laser Combo"), S::Tactical, M_ALL, 6500); D.bLight = true; D.bLaser = true; D.HipSpreadMult = 0.8f; }
	}

	const FAttachmentTable& AttachmentTable()
	{
		static FAttachmentTable Table;
		static bool bBuilt = false;
		if (!bBuilt)
		{
			bBuilt = true;
			BuildAttachments(Table);
		}
		return Table;
	}

	// ---------------------------------------------------------------------------------------
	//  Operators
	// ---------------------------------------------------------------------------------------

	const TArray<FTOOperatorDef>& OperatorTable()
	{
		static TArray<FTOOperatorDef> Ops;
		if (Ops.Num() == 0)
		{
			FTOOperatorDef A;
			A.Id = ETOOperator::Viper; A.Name = TEXT("VIPER"); A.Role = TEXT("Assault");
			A.AbilityName = TEXT("Adrenal Surge"); A.AbilityDesc = TEXT("+30% speed, faster ADS, no fracture penalty for 8 s");
			A.Cooldown = 40.f; A.SuitMat = ETOMat::SuitTan; A.UIColor = FLinearColor(1.f, 0.55f, 0.15f);
			Ops.Add(A);

			FTOOperatorDef B;
			B.Id = ETOOperator::Halo; B.Name = TEXT("HALO"); B.Role = TEXT("Support");
			B.AbilityName = TEXT("Heal Pulse"); B.AbilityDesc = TEXT("Heals you and nearby allies, stops bleeding, fast revives");
			B.Cooldown = 45.f; B.SuitMat = ETOMat::SuitGreen; B.UIColor = FLinearColor(0.35f, 0.9f, 0.45f);
			Ops.Add(B);

			FTOOperatorDef C;
			C.Id = ETOOperator::Bastion; C.Name = TEXT("BASTION"); C.Role = TEXT("Engineer");
			C.AbilityName = TEXT("Deploy Shield"); C.AbilityDesc = TEXT("Deploys a bulletproof cover barrier (800 HP)");
			C.Cooldown = 45.f; C.SuitMat = ETOMat::SuitGrey; C.UIColor = FLinearColor(0.35f, 0.6f, 1.f);
			Ops.Add(C);

			FTOOperatorDef D;
			D.Id = ETOOperator::Echo; D.Name = TEXT("ECHO"); D.Role = TEXT("Recon");
			D.AbilityName = TEXT("Recon Pulse"); D.AbilityDesc = TEXT("Reveals all enemies within 70 m for 8 s");
			D.Cooldown = 40.f; D.SuitMat = ETOMat::SuitBlack; D.UIColor = FLinearColor(0.75f, 0.45f, 1.f);
			Ops.Add(D);
		}
		return Ops;
	}

	int32 RandomIndexWeighted(const TArray<float>& Weights, FRandomStream& Rng)
	{
		float Total = 0.f;
		for (float W : Weights)
		{
			Total += FMath::Max(0.f, W);
		}
		if (Total <= 0.f)
		{
			return 0;
		}
		float Pick = Rng.FRand() * Total;
		for (int32 i = 0; i < Weights.Num(); ++i)
		{
			Pick -= FMath::Max(0.f, Weights[i]);
			if (Pick <= 0.f)
			{
				return i;
			}
		}
		return Weights.Num() - 1;
	}
}

// =============================================================================================
//  Public API
// =============================================================================================

namespace TODB
{
	const TArray<FTOItemDef>& Items()
	{
		return ItemTable().Items;
	}

	const FTOItemDef* FindItem(FName Id)
	{
		const FItemTable& T = ItemTable();
		if (const int32* Idx = T.Index.Find(Id))
		{
			return &T.Items[*Idx];
		}
		return nullptr;
	}

	FName AmmoItemId(FName Caliber, int32 Tier)
	{
		return FName(*FString::Printf(TEXT("Ammo_%s_%d"), *Caliber.ToString(), FMath::Clamp(Tier, 1, 6)));
	}

	FName ArmorItemId(int32 Level)
	{
		return Level <= 0 ? NAME_None : FName(*FString::Printf(TEXT("Armor_%d"), FMath::Clamp(Level, 1, 6)));
	}

	FName HelmetItemId(int32 Level)
	{
		return Level <= 0 ? NAME_None : FName(*FString::Printf(TEXT("Helmet_%d"), FMath::Clamp(Level, 1, 6)));
	}

	FName RigItemId(int32 Tier)
	{
		return Tier <= 0 ? NAME_None : FName(*FString::Printf(TEXT("Rig_%d"), FMath::Clamp(Tier, 1, 4)));
	}

	FName BackpackItemId(int32 Tier)
	{
		return Tier <= 0 ? NAME_None : FName(*FString::Printf(TEXT("Pack_%d"), FMath::Clamp(Tier, 1, 4)));
	}

	FName WeaponItemId(FName WeaponId)
	{
		return FName(*FString::Printf(TEXT("W_%s"), *WeaponId.ToString()));
	}

	FName GrenadeItemId(ETOGrenadeType Type)
	{
		switch (Type)
		{
		case ETOGrenadeType::Smoke: return FName(TEXT("Gren_Smoke"));
		case ETOGrenadeType::Flash: return FName(TEXT("Gren_Flash"));
		default: return FName(TEXT("Gren_Frag"));
		}
	}

	int32 NewUid()
	{
		return ++GNextUid;
	}

	FTOItemInstance MakeItem(FName Id, int32 Count)
	{
		FTOItemInstance Item;
		const FTOItemDef* Def = FindItem(Id);
		if (!Def)
		{
			return Item;
		}
		Item.ItemId = Id;
		Item.Uid = NewUid();
		Item.bRevealed = true;
		switch (Def->Category)
		{
		case ETOItemCategory::Ammo:
			Item.Count = Count > 0 ? Count : Def->StackMax;
			break;
		case ETOItemCategory::Medical:
			Item.Count = Count > 0 ? Count : Def->Uses;
			break;
		case ETOItemCategory::Armor:
		case ETOItemCategory::Helmet:
			Item.Count = 1;
			Item.Durability = Def->Durability;
			break;
		case ETOItemCategory::Weapon:
			Item.Count = 1;
			Item.Weapon = MakeDefaultConfig(Def->WeaponId, 3);
			break;
		default:
			Item.Count = 1;
			break;
		}
		return Item;
	}

	FTOItemInstance MakeWeaponItem(const FTOWeaponConfig& Config)
	{
		FTOItemInstance Item = MakeItem(WeaponItemId(Config.WeaponId));
		Item.Weapon = Config;
		return Item;
	}

	int64 ItemValue(const FTOItemInstance& Item)
	{
		const FTOItemDef* Def = FindItem(Item.ItemId);
		if (!Def)
		{
			return 0;
		}
		switch (Def->Category)
		{
		case ETOItemCategory::Ammo:
			return (int64)Def->Value * FMath::Max(0, Item.Count);
		case ETOItemCategory::Medical:
			return (int64)FMath::RoundToInt(Def->Value * (float)FMath::Max(0, Item.Count) / (float)FMath::Max(1, Def->Uses));
		case ETOItemCategory::Armor:
		case ETOItemCategory::Helmet:
		{
			const float Frac = Def->Durability > 0.f ? FMath::Clamp(Item.Durability / Def->Durability, 0.f, 1.f) : 1.f;
			return (int64)FMath::RoundToInt(Def->Value * (0.25f + 0.75f * Frac));
		}
		case ETOItemCategory::Weapon:
			return (int64)WeaponConfigPrice(Item.Weapon);
		default:
			return (int64)Def->Value;
		}
	}

	FString ItemName(const FTOItemInstance& Item, bool bShort)
	{
		const FTOItemDef* Def = FindItem(Item.ItemId);
		if (!Def)
		{
			return TEXT("?");
		}
		return bShort ? Def->ShortName : Def->Name;
	}

	FLinearColor RarityColor(ETORarity Rarity)
	{
		switch (Rarity)
		{
		case ETORarity::Common: return FLinearColor(0.78f, 0.80f, 0.82f);
		case ETORarity::Uncommon: return FLinearColor(0.35f, 0.85f, 0.40f);
		case ETORarity::Rare: return FLinearColor(0.30f, 0.62f, 1.00f);
		case ETORarity::Epic: return FLinearColor(0.72f, 0.42f, 1.00f);
		case ETORarity::Legendary: return FLinearColor(1.00f, 0.72f, 0.18f);
		case ETORarity::Mythic: return FLinearColor(1.00f, 0.22f, 0.20f);
		default: return FLinearColor::White;
		}
	}

	const TCHAR* RarityName(ETORarity Rarity)
	{
		switch (Rarity)
		{
		case ETORarity::Common: return TEXT("Common");
		case ETORarity::Uncommon: return TEXT("Uncommon");
		case ETORarity::Rare: return TEXT("Rare");
		case ETORarity::Epic: return TEXT("Epic");
		case ETORarity::Legendary: return TEXT("Legendary");
		case ETORarity::Mythic: return TEXT("Mythic");
		default: return TEXT("");
		}
	}

	const TCHAR* CategoryName(ETOItemCategory Category)
	{
		switch (Category)
		{
		case ETOItemCategory::Valuable: return TEXT("Valuable");
		case ETOItemCategory::Medical: return TEXT("Medical");
		case ETOItemCategory::Ammo: return TEXT("Ammo");
		case ETOItemCategory::Grenade: return TEXT("Throwable");
		case ETOItemCategory::Key: return TEXT("Key");
		case ETOItemCategory::Armor: return TEXT("Body Armor");
		case ETOItemCategory::Helmet: return TEXT("Helmet");
		case ETOItemCategory::Rig: return TEXT("Chest Rig");
		case ETOItemCategory::Backpack: return TEXT("Backpack");
		case ETOItemCategory::Weapon: return TEXT("Weapon");
		default: return TEXT("");
		}
	}

	const TArray<FTOWeaponDef>& Weapons()
	{
		return WeaponTable().Weapons;
	}

	const FTOWeaponDef* FindWeapon(FName Id)
	{
		const FWeaponTable& T = WeaponTable();
		if (const int32* Idx = T.Index.Find(Id))
		{
			return &T.Weapons[*Idx];
		}
		return nullptr;
	}

	const TCHAR* WeaponClassName(ETOWeaponClass Class)
	{
		switch (Class)
		{
		case ETOWeaponClass::AssaultRifle: return TEXT("Assault Rifle");
		case ETOWeaponClass::SMG: return TEXT("SMG");
		case ETOWeaponClass::DMR: return TEXT("Marksman Rifle");
		case ETOWeaponClass::Sniper: return TEXT("Sniper Rifle");
		case ETOWeaponClass::LMG: return TEXT("Light Machine Gun");
		case ETOWeaponClass::Shotgun: return TEXT("Shotgun");
		case ETOWeaponClass::Pistol: return TEXT("Pistol");
		case ETOWeaponClass::Melee: return TEXT("Melee");
		default: return TEXT("");
		}
	}

	const TCHAR* FireModeName(ETOFireMode Mode)
	{
		switch (Mode)
		{
		case ETOFireMode::Semi: return TEXT("SEMI");
		case ETOFireMode::Burst: return TEXT("BURST");
		case ETOFireMode::Auto: return TEXT("AUTO");
		case ETOFireMode::Bolt: return TEXT("BOLT");
		default: return TEXT("");
		}
	}

	const TCHAR* SlotName(ETOAttachSlot Slot)
	{
		switch (Slot)
		{
		case ETOAttachSlot::Optic: return TEXT("Optic");
		case ETOAttachSlot::Muzzle: return TEXT("Muzzle");
		case ETOAttachSlot::Barrel: return TEXT("Barrel");
		case ETOAttachSlot::Underbarrel: return TEXT("Underbarrel");
		case ETOAttachSlot::Stock: return TEXT("Stock");
		case ETOAttachSlot::Magazine: return TEXT("Magazine");
		case ETOAttachSlot::Tactical: return TEXT("Tactical");
		default: return TEXT("");
		}
	}

	const TCHAR* CaliberName(FName Caliber)
	{
		if (const FCaliberInfo* C = FindCaliber(Caliber))
		{
			return C->Name;
		}
		return TEXT("-");
	}

	const TArray<FTOAttachmentDef>& Attachments()
	{
		return AttachmentTable().Attachments;
	}

	const FTOAttachmentDef* FindAttachment(FName Id)
	{
		if (Id.IsNone())
		{
			return nullptr;
		}
		const FAttachmentTable& T = AttachmentTable();
		if (const int32* Idx = T.Index.Find(Id))
		{
			return &T.Attachments[*Idx];
		}
		return nullptr;
	}

	bool IsAttachmentCompatible(const FTOAttachmentDef& Att, const FTOWeaponDef& Weapon)
	{
		return (Att.ClassMask & (1u << (uint32)Weapon.Class)) != 0;
	}

	TArray<FName> GetAttachmentOptions(FName WeaponId, ETOAttachSlot Slot)
	{
		TArray<FName> Out;
		Out.Add(NAME_None);
		const FTOWeaponDef* W = FindWeapon(WeaponId);
		if (!W)
		{
			return Out;
		}
		for (const FTOAttachmentDef& A : Attachments())
		{
			if (A.Slot == Slot && IsAttachmentCompatible(A, *W))
			{
				Out.Add(A.Id);
			}
		}
		return Out;
	}

	FTOWeaponStats ComputeStats(const FTOWeaponConfig& Config)
	{
		FTOWeaponStats S;
		const FTOWeaponDef* D = FindWeapon(Config.WeaponId);
		if (!D)
		{
			return S;
		}
		S.Def = D;
		S.Damage = D->Damage;
		S.RPM = D->RPM;
		S.ReloadTime = D->ReloadTime;
		S.ReloadEmptyTime = D->ReloadEmptyTime;
		S.Velocity = D->Velocity;
		S.RecoilV = D->RecoilV;
		S.RecoilH = D->RecoilH;
		S.HipSpread = D->HipSpread;
		S.AdsSpread = D->AdsSpread;
		S.AdsTime = D->AdsTime;
		S.Range = D->Range;
		S.Noise = D->Noise;
		S.MoveMult = D->MoveMult;
		S.Optic = ETOOpticType::Iron;
		S.Zoom = (D->Class == ETOWeaponClass::Pistol) ? 1.1f : 1.2f;

		float MagMult = 1.f;
		for (const FName& AttId : Config.Attachments)
		{
			const FTOAttachmentDef* A = FindAttachment(AttId);
			if (!A || !IsAttachmentCompatible(*A, *D))
			{
				continue;
			}
			S.RecoilV *= A->RecoilVMult;
			S.RecoilH *= A->RecoilHMult;
			S.AdsTime *= A->AdsTimeMult;
			S.HipSpread *= A->HipSpreadMult;
			S.Range *= A->RangeMult;
			S.Velocity *= A->VelocityMult;
			S.ReloadTime *= A->ReloadMult;
			S.ReloadEmptyTime *= A->ReloadMult;
			S.Noise *= A->NoiseMult;
			S.MoveMult *= A->MoveMult;
			MagMult *= A->MagMult;
			if (A->Slot == ETOAttachSlot::Optic)
			{
				S.Optic = A->Optic;
				S.Zoom = A->Zoom;
			}
			S.bSuppressed |= A->bSuppressor;
			S.bLight |= A->bLight;
			S.bLaser |= A->bLaser;
			S.bBipod |= A->bBipod;
		}
		S.MagSize = FMath::Max(D->MagSize > 0 ? 1 : 0, FMath::RoundToInt(D->MagSize * MagMult));

		switch (S.Optic)
		{
		case ETOOpticType::RedDot: S.SightHeight = 6.5f; break;
		case ETOOpticType::Holo: S.SightHeight = 6.8f; break;
		case ETOOpticType::Prism: S.SightHeight = 7.0f; break;
		case ETOOpticType::ACOG: S.SightHeight = 7.2f; break;
		case ETOOpticType::Sniper: S.SightHeight = 7.8f; break;
		default: S.SightHeight = (D->Class == ETOWeaponClass::Pistol) ? 4.2f : 5.6f; break;
		}
		return S;
	}

	int32 WeaponConfigPrice(const FTOWeaponConfig& Config)
	{
		const FTOWeaponDef* D = FindWeapon(Config.WeaponId);
		if (!D)
		{
			return 0;
		}
		int32 Price = D->Price;
		for (const FName& AttId : Config.Attachments)
		{
			if (const FTOAttachmentDef* A = FindAttachment(AttId))
			{
				Price += A->Price;
			}
		}
		return Price;
	}

	FTOWeaponConfig MakeDefaultConfig(FName WeaponId, int32 AmmoTier)
	{
		FTOWeaponConfig C;
		C.WeaponId = WeaponId;
		C.AmmoTier = FMath::Clamp(AmmoTier, 1, 6);
		C.Attachments.SetNum((int32)ETOAttachSlot::Count);
		const FTOWeaponDef* D = FindWeapon(WeaponId);
		if (!D)
		{
			return C;
		}
		switch (D->Class)
		{
		case ETOWeaponClass::AssaultRifle:
			C.SetAttachment(ETOAttachSlot::Optic, FName(TEXT("Opt_Holo")));
			C.SetAttachment(ETOAttachSlot::Muzzle, FName(TEXT("Muz_Comp")));
			C.SetAttachment(ETOAttachSlot::Underbarrel, FName(TEXT("Ub_Vert")));
			C.SetAttachment(ETOAttachSlot::Tactical, FName(TEXT("Tac_Light")));
			break;
		case ETOWeaponClass::SMG:
			C.SetAttachment(ETOAttachSlot::Optic, FName(TEXT("Opt_RedDot")));
			C.SetAttachment(ETOAttachSlot::Underbarrel, FName(TEXT("Ub_Angled")));
			C.SetAttachment(ETOAttachSlot::Tactical, FName(TEXT("Tac_Light")));
			break;
		case ETOWeaponClass::DMR:
			C.SetAttachment(ETOAttachSlot::Optic, FName(TEXT("Opt_ACOG")));
			C.SetAttachment(ETOAttachSlot::Muzzle, FName(TEXT("Muz_Brake")));
			break;
		case ETOWeaponClass::Sniper:
			C.SetAttachment(ETOAttachSlot::Optic, FName(TEXT("Opt_Sniper")));
			break;
		case ETOWeaponClass::LMG:
			C.SetAttachment(ETOAttachSlot::Optic, FName(TEXT("Opt_Holo")));
			C.SetAttachment(ETOAttachSlot::Underbarrel, FName(TEXT("Ub_Bipod")));
			break;
		case ETOWeaponClass::Shotgun:
			C.SetAttachment(ETOAttachSlot::Optic, FName(TEXT("Opt_RedDot")));
			C.SetAttachment(ETOAttachSlot::Tactical, FName(TEXT("Tac_Light")));
			break;
		case ETOWeaponClass::Pistol:
			C.SetAttachment(ETOAttachSlot::Tactical, FName(TEXT("Tac_Light")));
			break;
		default:
			break;
		}
		return C;
	}

	FName RandomWeaponForTier(int32 Tier, FRandomStream& Rng, bool bAllowSniper)
	{
		TArray<FName> Pool;
		TArray<float> Weights;
		for (const FTOWeaponDef& W : Weapons())
		{
			if (W.Class == ETOWeaponClass::Melee || W.Class == ETOWeaponClass::Pistol)
			{
				continue;
			}
			if (!bAllowSniper && W.Class == ETOWeaponClass::Sniper)
			{
				continue;
			}
			// Cheap guns are more common on low tier soldiers, expensive guns on elites.
			const float PriceK = W.Price / 1000.f;
			float Weight = 1.f;
			if (Tier <= 0) Weight = FMath::Clamp(60.f / PriceK, 0.15f, 3.f);
			else if (Tier == 1) Weight = 1.f;
			else Weight = FMath::Clamp(PriceK / 50.f, 0.3f, 3.f);
			if (W.Class == ETOWeaponClass::Sniper || W.Class == ETOWeaponClass::LMG)
			{
				Weight *= 0.35f;
			}
			Pool.Add(W.Id);
			Weights.Add(Weight);
		}
		if (Pool.Num() == 0)
		{
			return FName(TEXT("M4A1"));
		}
		return Pool[RandomIndexWeighted(Weights, Rng)];
	}

	FTOWeaponConfig MakeRandomConfig(int32 Tier, FRandomStream& Rng, bool bAllowSniper)
	{
		const FName WeaponId = RandomWeaponForTier(Tier, Rng, bAllowSniper);
		FTOWeaponConfig C;
		C.WeaponId = WeaponId;
		C.AmmoTier = FMath::Clamp(2 + Tier + (Rng.FRand() < 0.3f ? 1 : 0), 1, 6);
		C.Attachments.SetNum((int32)ETOAttachSlot::Count);
		for (int32 SlotIdx = 0; SlotIdx < (int32)ETOAttachSlot::Count; ++SlotIdx)
		{
			const float Chance = 0.2f + 0.2f * Tier;
			if (Rng.FRand() > Chance && SlotIdx != (int32)ETOAttachSlot::Optic)
			{
				continue;
			}
			TArray<FName> Options = GetAttachmentOptions(WeaponId, (ETOAttachSlot)SlotIdx);
			if (Options.Num() > 1)
			{
				C.Attachments[SlotIdx] = Options[Rng.RandRange(0, Options.Num() - 1)];
			}
		}
		return C;
	}

	TArray<FName> WeaponsOfClass(ETOWeaponClass Class)
	{
		TArray<FName> Out;
		for (const FTOWeaponDef& W : Weapons())
		{
			if (W.Class == Class)
			{
				Out.Add(W.Id);
			}
		}
		return Out;
	}

	TArray<FName> PrimaryWeaponIds()
	{
		TArray<FName> Out;
		for (const FTOWeaponDef& W : Weapons())
		{
			if (W.Class != ETOWeaponClass::Pistol && W.Class != ETOWeaponClass::Melee)
			{
				Out.Add(W.Id);
			}
		}
		return Out;
	}

	TArray<FName> SidearmIds()
	{
		return WeaponsOfClass(ETOWeaponClass::Pistol);
	}

	const FTOOperatorDef& GetOperator(ETOOperator Op)
	{
		const TArray<FTOOperatorDef>& Ops = OperatorTable();
		const int32 Idx = FMath::Clamp((int32)Op, 0, Ops.Num() - 1);
		return Ops[Idx];
	}

	int32 NumOperators()
	{
		return OperatorTable().Num();
	}

	int64 AmmoCost(FName Caliber, int32 Tier, int32 Rounds)
	{
		const FTOItemDef* Def = FindItem(AmmoItemId(Caliber, Tier));
		return Def ? (int64)Def->Value * FMath::Max(0, Rounds) : 0;
	}

	void GetMedKit(int32 MedTier, TArray<FName>& OutItems)
	{
		OutItems.Reset();
		OutItems.Add(FName(TEXT("Med_Bandage")));
		OutItems.Add(FName(TEXT("Med_Bandage")));
		if (MedTier >= 1)
		{
			OutItems.Add(FName(TEXT("Med_Splint")));
			OutItems.Add(FName(TEXT("Med_Medkit")));
		}
		if (MedTier >= 2)
		{
			OutItems.Add(FName(TEXT("Med_Trauma")));
			OutItems.Add(FName(TEXT("Med_Painkiller")));
		}
		if (MedTier >= 3)
		{
			OutItems.Add(FName(TEXT("Med_Surgical")));
			OutItems.Add(FName(TEXT("Med_Adrenaline")));
		}
	}

	const TCHAR* MedKitName(int32 MedTier)
	{
		switch (MedTier)
		{
		case 0: return TEXT("Basic (2 Bandages)");
		case 1: return TEXT("Standard (+Splint, Medkit)");
		case 2: return TEXT("Advanced (+Trauma, Pills)");
		default: return TEXT("Elite (+Surgical, Adrenaline)");
		}
	}

	int64 LoadoutCost(const FTOLoadout& L, bool bIncludePrimary)
	{
		int64 Cost = 0;
		if (bIncludePrimary && L.Primary.IsValid())
		{
			Cost += WeaponConfigPrice(L.Primary);
		}
		if (L.Sidearm.IsValid())
		{
			Cost += WeaponConfigPrice(L.Sidearm);
		}
		auto AddItemCost = [&Cost](FName Id, int32 Count)
		{
			if (const FTOItemDef* D = FindItem(Id))
			{
				Cost += (int64)D->Value * Count;
			}
		};
		AddItemCost(ArmorItemId(L.ArmorLevel), 1);
		AddItemCost(HelmetItemId(L.HelmetLevel), 1);
		AddItemCost(RigItemId(L.RigTier), 1);
		AddItemCost(BackpackItemId(L.BackpackTier), 1);
		AddItemCost(FName(TEXT("Gren_Frag")), L.Frags);
		AddItemCost(FName(TEXT("Gren_Smoke")), L.Smokes);
		AddItemCost(FName(TEXT("Gren_Flash")), L.Flashes);
		TArray<FName> Meds;
		GetMedKit(L.MedTier, Meds);
		for (const FName& M : Meds)
		{
			AddItemCost(M, 1);
		}
		if (L.Primary.IsValid())
		{
			const FTOWeaponStats S = ComputeStats(L.Primary);
			if (S.Def)
			{
				Cost += AmmoCost(S.Def->Caliber, L.Primary.AmmoTier, S.MagSize * (L.SpareMags + 1));
			}
		}
		if (L.Sidearm.IsValid())
		{
			const FTOWeaponStats S = ComputeStats(L.Sidearm);
			if (S.Def)
			{
				Cost += AmmoCost(S.Def->Caliber, L.Sidearm.AmmoTier, S.MagSize * 3);
			}
		}
		return Cost;
	}

	ETORarity RollRarity(int32 Tier, FRandomStream& Rng)
	{
		static const float Base[6] = { 50.f, 28.f, 14.f, 6.f, 1.6f, 0.35f };
		TArray<float> W;
		const float Boost = 1.f + 0.55f * FMath::Clamp(Tier, 0, 4);
		for (int32 i = 0; i < 6; ++i)
		{
			W.Add(Base[i] * FMath::Pow(Boost, (float)i));
		}
		return (ETORarity)RandomIndexWeighted(W, Rng);
	}

	FTOItemInstance RollValuable(ETORarity Rarity, bool bTechOnly, FRandomStream& Rng)
	{
		TArray<const FTOItemDef*> Pool;
		for (int32 Pass = 0; Pass < 2 && Pool.Num() == 0; ++Pass)
		{
			for (const FTOItemDef& D : Items())
			{
				if (D.Category != ETOItemCategory::Valuable || D.Rarity != Rarity)
				{
					continue;
				}
				if (bTechOnly && Pass == 0 && !D.bTech)
				{
					continue;
				}
				Pool.Add(&D);
			}
		}
		if (Pool.Num() == 0)
		{
			return MakeItem(FName(TEXT("Val_Screws")));
		}
		return MakeItem(Pool[Rng.RandRange(0, Pool.Num() - 1)]->Id);
	}

	static FTOItemInstance RollAmmo(int32 Tier, FRandomStream& Rng)
	{
		const TArray<FCaliberInfo>& Cals = Calibers();
		const FCaliberInfo& C = Cals[Rng.RandRange(0, Cals.Num() - 1)];
		const int32 AmmoTier = FMath::Clamp(1 + (int32)RollRarity(Tier, Rng), 1, 6);
		const int32 Count = FMath::Max(5, FMath::RoundToInt(C.Stack * Rng.FRandRange(0.3f, 1.f)));
		return MakeItem(AmmoItemId(C.Id, AmmoTier), Count);
	}

	static FTOItemInstance RollMedical(int32 Tier, FRandomStream& Rng)
	{
		static const TCHAR* Meds[] = { TEXT("Med_Bandage"), TEXT("Med_Bandage"), TEXT("Med_Splint"), TEXT("Med_Medkit"), TEXT("Med_Painkiller"), TEXT("Med_Trauma"), TEXT("Med_Adrenaline"), TEXT("Med_Surgical") };
		const int32 MaxIdx = FMath::Clamp(4 + Tier * 2, 4, 7);
		return MakeItem(FName(Meds[Rng.RandRange(0, MaxIdx)]));
	}

	static FTOItemInstance RollGear(int32 Tier, FRandomStream& Rng)
	{
		const int32 Kind = Rng.RandRange(0, 3);
		const int32 Level = FMath::Clamp(1 + (int32)RollRarity(Tier, Rng), 1, 6);
		switch (Kind)
		{
		case 0: return MakeItem(ArmorItemId(Level));
		case 1: return MakeItem(HelmetItemId(Level));
		case 2: return MakeItem(RigItemId(FMath::Clamp(Level, 1, 4)));
		default: return MakeItem(BackpackItemId(FMath::Clamp(Level, 1, 4)));
		}
	}

	static FTOItemInstance RollGrenade(FRandomStream& Rng)
	{
		const float R = Rng.FRand();
		return MakeItem(GrenadeItemId(R < 0.5f ? ETOGrenadeType::Frag : (R < 0.8f ? ETOGrenadeType::Smoke : ETOGrenadeType::Flash)));
	}

	static FTOItemInstance RollKey(FRandomStream& Rng)
	{
		static const TCHAR* Keys[] = { TEXT("Key_Warehouse"), TEXT("Key_DamControl"), TEXT("Key_Armory"), TEXT("Key_AdminBlue"), TEXT("Key_ServerRed") };
		static const float KeyWeights[] = { 30.f, 25.f, 22.f, 15.f, 4.f };
		TArray<float> W;
		for (float F : KeyWeights)
		{
			W.Add(F);
		}
		return MakeItem(FName(Keys[RandomIndexWeighted(W, Rng)]));
	}

	void RollLoot(ETOContainerType Type, int32 Tier, FRandomStream& Rng, TArray<FTOItemInstance>& Out)
	{
		int32 MinCount = 1, MaxCount = 3;
		int32 EffTier = Tier;
		switch (Type)
		{
		case ETOContainerType::WeaponCrate: MinCount = 1; MaxCount = 3; break;
		case ETOContainerType::AmmoBox: MinCount = 2; MaxCount = 4; break;
		case ETOContainerType::MedicalCase: MinCount = 2; MaxCount = 4; break;
		case ETOContainerType::Toolbox: MinCount = 2; MaxCount = 5; break;
		case ETOContainerType::Safe: MinCount = 1; MaxCount = 3; EffTier += 1; break;
		case ETOContainerType::ComputerTower: MinCount = 1; MaxCount = 3; break;
		case ETOContainerType::Duffel: MinCount = 2; MaxCount = 5; break;
		case ETOContainerType::FilingCabinet: MinCount = 1; MaxCount = 4; break;
		case ETOContainerType::ServerRack: MinCount = 1; MaxCount = 3; EffTier += 1; break;
		case ETOContainerType::Locker: MinCount = 1; MaxCount = 3; break;
		case ETOContainerType::HighValueCrate: MinCount = 2; MaxCount = 4; EffTier += 2; break;
		case ETOContainerType::LooseItem: MinCount = 1; MaxCount = 1; break;
		default: break;
		}

		const int32 Count = Rng.RandRange(MinCount, MaxCount);
		for (int32 i = 0; i < Count; ++i)
		{
			FTOItemInstance Item;
			const float Roll = Rng.FRand();
			switch (Type)
			{
			case ETOContainerType::WeaponCrate:
				if (Roll < 0.45f) Item = MakeWeaponItem(MakeRandomConfig(EffTier, Rng));
				else if (Roll < 0.8f) Item = RollAmmo(EffTier, Rng);
				else Item = RollGrenade(Rng);
				break;
			case ETOContainerType::AmmoBox:
				Item = (Roll < 0.85f) ? RollAmmo(EffTier, Rng) : RollGrenade(Rng);
				break;
			case ETOContainerType::MedicalCase:
				Item = (Roll < 0.85f) ? RollMedical(EffTier, Rng) : RollValuable(RollRarity(EffTier, Rng), false, Rng);
				break;
			case ETOContainerType::ComputerTower:
			case ETOContainerType::ServerRack:
				Item = RollValuable(RollRarity(EffTier, Rng), true, Rng);
				break;
			case ETOContainerType::Safe:
			case ETOContainerType::FilingCabinet:
				Item = (Roll < 0.08f) ? RollKey(Rng) : RollValuable(RollRarity(EffTier, Rng), false, Rng);
				break;
			case ETOContainerType::Locker:
				if (Roll < 0.4f) Item = RollGear(EffTier, Rng);
				else if (Roll < 0.65f) Item = RollMedical(EffTier, Rng);
				else if (Roll < 0.85f) Item = RollAmmo(EffTier, Rng);
				else Item = RollValuable(RollRarity(EffTier, Rng), false, Rng);
				break;
			case ETOContainerType::HighValueCrate:
				if (Roll < 0.25f) Item = RollGear(EffTier, Rng);
				else Item = RollValuable((ETORarity)FMath::Max((int32)ETORarity::Epic, (int32)RollRarity(EffTier, Rng)), false, Rng);
				break;
			case ETOContainerType::Duffel:
				if (Roll < 0.15f) Item = RollAmmo(EffTier, Rng);
				else if (Roll < 0.3f) Item = RollMedical(EffTier, Rng);
				else if (Roll < 0.38f) Item = RollGrenade(Rng);
				else if (Roll < 0.45f) Item = RollGear(EffTier, Rng);
				else Item = RollValuable(RollRarity(EffTier, Rng), false, Rng);
				break;
			default:
				Item = RollValuable(RollRarity(EffTier, Rng), false, Rng);
				break;
			}
			if (Item.IsValid())
			{
				Item.bRevealed = false;
				Out.Add(Item);
			}
		}
	}

	const TCHAR* ContainerName(ETOContainerType Type)
	{
		switch (Type)
		{
		case ETOContainerType::WeaponCrate: return TEXT("Weapon Crate");
		case ETOContainerType::AmmoBox: return TEXT("Ammo Box");
		case ETOContainerType::MedicalCase: return TEXT("Medical Case");
		case ETOContainerType::Toolbox: return TEXT("Toolbox");
		case ETOContainerType::Safe: return TEXT("Safe");
		case ETOContainerType::ComputerTower: return TEXT("Computer");
		case ETOContainerType::Duffel: return TEXT("Duffel Bag");
		case ETOContainerType::FilingCabinet: return TEXT("Filing Cabinet");
		case ETOContainerType::ServerRack: return TEXT("Server Rack");
		case ETOContainerType::Locker: return TEXT("Locker");
		case ETOContainerType::HighValueCrate: return TEXT("Secure Crate");
		case ETOContainerType::Corpse: return TEXT("Body");
		case ETOContainerType::LooseItem: return TEXT("Item");
		default: return TEXT("Container");
		}
	}

	float ContainerSearchTime(ETOContainerType Type)
	{
		switch (Type)
		{
		case ETOContainerType::Safe: return 1.2f;
		case ETOContainerType::ServerRack: return 1.1f;
		case ETOContainerType::HighValueCrate: return 1.3f;
		case ETOContainerType::Corpse: return 0.55f;
		case ETOContainerType::LooseItem: return 0.f;
		default: return 0.75f;
		}
	}

	void ContainerGridSize(ETOContainerType Type, int32& OutW, int32& OutH)
	{
		switch (Type)
		{
		case ETOContainerType::Corpse: OutW = 8; OutH = 9; break;
		case ETOContainerType::WeaponCrate: OutW = 6; OutH = 4; break;
		case ETOContainerType::HighValueCrate: OutW = 6; OutH = 4; break;
		case ETOContainerType::Locker: OutW = 5; OutH = 6; break;
		case ETOContainerType::Duffel: OutW = 6; OutH = 4; break;
		case ETOContainerType::LooseItem: OutW = 3; OutH = 3; break;
		default: OutW = 5; OutH = 4; break;
		}
	}
}
