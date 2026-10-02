// TAC-OPS - "Dam Valley" map layout
//
// An original 2.5 x 2.5 km valley inspired by the structure of Delta Force's hydro-dam
// operation map: a reservoir held back by a concrete gravity dam, the hydro power station at
// its foot, a heavily guarded administration district (keycard rooms + commander boss),
// a walled military base, a radar station on the plateau, and lower risk outskirts
// (village, gas station, logistics yard, pumping station, farm, lumber camp) along the river.
//
// Coordinates in this file are in METERS (X = north, Y = east) and converted with M().

#include "World/TOWorldGenerator.h"

namespace
{
	constexpr float CM = 100.f;
	FVector2D M2(float X, float Y) { return FVector2D(X * CM, Y * CM); }
}

void ATOWorldGenerator::SetupLayout()
{
	MapName = TEXT("Dam Valley");
	HalfSize = 125000.f;
	DamX = 43000.f;
	ReservoirLevel = 5800.f;

	// River (downstream of the dam to the south edge) ------------------------------------
	static const float RiverM[][2] =
	{
		{ 430.f, 100.f }, { 380.f, 105.f }, { 300.f, 85.f }, { 200.f, 40.f }, { 90.f, -30.f }, { -40.f, -70.f },
		{ -170.f, -40.f }, { -300.f, 40.f }, { -440.f, 90.f }, { -580.f, 50.f }, { -730.f, -30.f }, { -890.f, -50.f },
		{ -1060.f, 0.f }, { -1330.f, 40.f }
	};
	RiverPoints.Reset();
	RiverCum.Reset();
	for (const auto& P : RiverM)
	{
		RiverPoints.Add(M2(P[0], P[1]));
	}
	float Cum = 0.f;
	RiverCum.Add(0.f);
	for (int32 i = 1; i < RiverPoints.Num(); ++i)
	{
		Cum += FVector2D::Distance(RiverPoints[i - 1], RiverPoints[i]);
		RiverCum.Add(Cum);
	}
	RiverLength = FMath::Max(Cum, 1.f);

	// Points of interest -----------------------------------------------------------------
	POIs.Reset();
	AddPOI(TEXT("Administration"), FVector(12000.f, 33000.f, 0.f), 9000.f, 3, true);
	AddPOI(TEXT("Hydro Power Station"), FVector(37500.f, -9500.f, 0.f), 7000.f, 2, true);
	AddPOI(TEXT("Hydro Dam"), FVector(43000.f, 0.f, 6200.f), 14000.f, 2, true);
	AddPOI(TEXT("Military Base"), FVector(13000.f, -38000.f, 0.f), 11000.f, 2, true);
	AddPOI(TEXT("Radar Station"), FVector(38000.f, 65000.f, 0.f), 4500.f, 2, true);
	AddPOI(TEXT("Logistics Yard"), FVector(-38000.f, 33000.f, 0.f), 10000.f, 1, true);
	AddPOI(TEXT("Pumping Station"), FVector(-14000.f, 12000.f, 0.f), 5000.f, 1, false);
	AddPOI(TEXT("Ridge Outpost"), FVector(48000.f, -62000.f, 0.f), 3500.f, 1, false);
	AddPOI(TEXT("Old Village"), FVector(-47000.f, -26000.f, 0.f), 11000.f, 0, true);
	AddPOI(TEXT("Gas Station"), FVector(-30000.f, -14000.f, 0.f), 3500.f, 0, false);
	AddPOI(TEXT("Lumber Camp"), FVector(-85000.f, -65000.f, 0.f), 4000.f, 0, false);
	AddPOI(TEXT("Farmstead"), FVector(-80000.f, 65000.f, 0.f), 5500.f, 0, false);

	// Flattened building pads -----------------------------------------------------------
	Pads.Reset();
	AddPad(TEXT("Admin"), M2(120.f, 330.f), M2(75.f, 65.f), 0.f, 3000.f, ETOMat::Concrete);
	AddPad(TEXT("Base"), M2(130.f, -380.f), M2(95.f, 80.f), 0.f, 3500.f, ETOMat::Dirt);
	AddPad(TEXT("Power"), M2(368.f, -95.f), M2(32.f, 62.f), 0.f, 2000.f, ETOMat::Concrete);
	AddPad(TEXT("Radar"), M2(380.f, 650.f), M2(35.f, 35.f), 0.f, 2500.f, ETOMat::Gravel);
	AddPad(TEXT("Yard"), M2(-380.f, 330.f), M2(90.f, 70.f), 0.f, 3000.f, ETOMat::Concrete);
	AddPad(TEXT("Pump"), M2(-140.f, 120.f), M2(40.f, 40.f), 0.f, 2000.f, ETOMat::Gravel);
	AddPad(TEXT("Outpost"), M2(480.f, -620.f), M2(25.f, 25.f), 0.f, 2000.f, ETOMat::Dirt);
	AddPad(TEXT("Village"), M2(-470.f, -260.f), M2(95.f, 95.f), 0.f, 4000.f, ETOMat::GrassDry);
	AddPad(TEXT("Gas"), M2(-300.f, -142.f), M2(30.f, 32.f), 0.f, 1500.f, ETOMat::Concrete);
	AddPad(TEXT("Camp"), M2(-850.f, -650.f), M2(25.f, 25.f), 0.f, 2000.f, ETOMat::Dirt);
	AddPad(TEXT("Farm"), M2(-800.f, 650.f), M2(45.f, 35.f), 0.f, 2500.f, ETOMat::Dirt);
	AddPad(TEXT("Checkpoint"), M2(-1150.f, -128.f), M2(20.f, 20.f), 0.f, 1500.f, ETOMat::Concrete);
	AddPad(TEXT("Tunnel"), M2(-210.f, 1055.f), M2(18.f, 18.f), 0.f, 1500.f, ETOMat::Gravel);
	AddPad(TEXT("Pass"), M2(520.f, -1030.f), M2(22.f, 22.f), 0.f, 2000.f, ETOMat::Gravel);

	// Road network (asphalt + dirt tracks) ------------------------------------------------
	Roads.Reset();
	auto AddRoad = [this](std::initializer_list<FVector2D> Pts, float WidthM, bool bDirt)
	{
		FTORoad R;
		for (const FVector2D& P : Pts)
		{
			R.Points.Add(P * CM);
		}
		R.Width = WidthM * CM;
		R.bDirt = bDirt;
		R.bMarkings = !bDirt;
		Roads.Add(R);
	};
	// West bank highway from the south checkpoint to the power station
	AddRoad({ { -1240.f, -120.f }, { -1100.f, -132.f }, { -960.f, -150.f }, { -820.f, -175.f }, { -680.f, -195.f }, { -560.f, -212.f }, { -470.f, -205.f },
		{ -380.f, -180.f }, { -300.f, -168.f }, { -200.f, -170.f }, { -80.f, -185.f }, { 40.f, -230.f }, { 95.f, -285.f } }, 8.f, false);
	AddRoad({ { 95.f, -285.f }, { 160.f, -255.f }, { 240.f, -195.f }, { 300.f, -158.f }, { 335.f, -150.f } }, 8.f, false);
	// Switchback up to the west end of the dam crest
	AddRoad({ { 335.f, -165.f }, { 300.f, -232.f }, { 330.f, -292.f }, { 390.f, -305.f }, { 425.f, -290.f } }, 7.f, false);
	// East end of the dam to the radar station
	AddRoad({ { 425.f, 290.f }, { 415.f, 400.f }, { 395.f, 520.f }, { 382.f, 612.f } }, 7.f, false);
	// Ridge track to the outpost and the mountain pass
	AddRoad({ { 425.f, -300.f }, { 455.f, -420.f }, { 475.f, -560.f }, { 480.f, -600.f }, { 495.f, -800.f }, { 515.f, -1005.f } }, 5.f, true);
	// East road: highway junction -> river bridge (south abutment)
	AddRoad({ { -200.f, -170.f }, { -125.f, -152.f }, { -85.f, -132.f }, { -73.f, -122.f } }, 7.f, false);
	// Bridge (north abutment) -> administration
	AddRoad({ { -46.f, -6.f }, { -20.f, 40.f }, { 40.f, 150.f }, { 80.f, 240.f }, { 95.f, 268.f } }, 7.f, false);
	// South-east road to the logistics yard and the farm
	AddRoad({ { -20.f, 40.f }, { -90.f, 140.f }, { -200.f, 230.f }, { -290.f, 300.f } }, 7.f, false);
	AddRoad({ { -420.f, 400.f }, { -520.f, 520.f }, { -690.f, 640.f }, { -760.f, 650.f } }, 6.f, false);
	// Rail tunnel access
	AddRoad({ { -330.f, 400.f }, { -300.f, 650.f }, { -240.f, 880.f }, { -212.f, 1040.f } }, 6.f, true);
	// South bridge: highway -> farm side
	AddRoad({ { -760.f, -182.f }, { -756.f, -135.f }, { -753.f, -96.f } }, 6.f, false);
	AddRoad({ { -768.f, 28.f }, { -785.f, 130.f }, { -815.f, 320.f }, { -805.f, 610.f } }, 6.f, false);
	// Boat dock and lumber camp tracks
	AddRoad({ { -905.f, -152.f }, { -905.f, -95.f } }, 4.f, true);
	AddRoad({ { -680.f, -198.f }, { -760.f, -400.f }, { -845.f, -620.f } }, 4.f, true);
	// Village back street
	AddRoad({ { -470.f, -206.f }, { -470.f, -340.f } }, 5.f, true);

	// Extraction points --------------------------------------------------------------
	Extracts.Reset();
	auto AddExtract = [this](const TCHAR* Name, FVector2D LocM, ETOExtractRule Rule, float RadiusM, int32 Cost, FVector2D SwitchM, float Chance)
	{
		FTOExtractDef E;
		E.Name = Name;
		E.Location = FVector(LocM.X * CM, LocM.Y * CM, 0.f);
		E.Rule = Rule;
		E.Radius = RadiusM * CM;
		E.Cost = Cost;
		E.SwitchLocation = FVector(SwitchM.X * CM, SwitchM.Y * CM, 0.f);
		E.Chance = Chance;
		Extracts.Add(E);
	};
	AddExtract(TEXT("South Checkpoint"), FVector2D(-1150.f, -128.f), ETOExtractRule::Always, 9.f, 0, FVector2D::ZeroVector, 1.f);
	AddExtract(TEXT("River Boat"), FVector2D(-905.f, -82.f), ETOExtractRule::Always, 7.f, 0, FVector2D::ZeroVector, 0.7f);
	AddExtract(TEXT("Base Helipad"), FVector2D(190.f, -425.f), ETOExtractRule::Radio, 10.f, 0, FVector2D(170.f, -404.f), 1.f);
	AddExtract(TEXT("Mountain Pass"), FVector2D(520.f, -1030.f), ETOExtractRule::Always, 9.f, 0, FVector2D::ZeroVector, 0.6f);
	AddExtract(TEXT("Rail Tunnel"), FVector2D(-210.f, 1055.f), ETOExtractRule::NoBackpack, 7.f, 0, FVector2D::ZeroVector, 1.f);
	AddExtract(TEXT("Dam Cable Car"), FVector2D(438.f, 305.f), ETOExtractRule::Paid, 6.f, 60000, FVector2D(430.f, 296.f), 0.8f);

	// Insertion points ---------------------------------------------------------------
	Insertions.Reset();
	auto AddIns = [this](const TCHAR* Name, FVector2D LocM, float Yaw)
	{
		FTOSpawnDef S;
		S.Name = Name;
		S.Location = FVector(LocM.X * CM, LocM.Y * CM, 0.f);
		S.Yaw = Yaw;
		Insertions.Add(S);
	};
	AddIns(TEXT("SW Forest"), FVector2D(-980.f, -600.f), 30.f);
	AddIns(TEXT("SE Farmland"), FVector2D(-950.f, 820.f), -20.f);
	AddIns(TEXT("West Ridge"), FVector2D(200.f, -1000.f), 70.f);
	AddIns(TEXT("East Hills"), FVector2D(100.f, 1000.f), -100.f);

	// Warfare sectors (attackers push from the south towards the dam) -----------------
	Sectors.Reset();
	auto AddSector = [this](const TCHAR* Name, std::initializer_list<FVector2D> PointsM, std::initializer_list<const TCHAR*> Labels, FVector2D AttM, FVector2D DefM)
	{
		FTOSectorDef S;
		S.Name = Name;
		for (const FVector2D& P : PointsM) S.Points.Add(FVector(P.X * CM, P.Y * CM, 0.f));
		for (const TCHAR* L : Labels) S.Labels.Add(L);
		S.AttackerSpawn = FVector(AttM.X * CM, AttM.Y * CM, 0.f);
		S.DefenderSpawn = FVector(DefM.X * CM, DefM.Y * CM, 0.f);
		Sectors.Add(S);
	};
	AddSector(TEXT("Old Village"), { { -470.f, -255.f }, { -300.f, -140.f } }, { TEXT("A1"), TEXT("B1") }, FVector2D(-1050.f, -150.f), FVector2D(-140.f, -120.f));
	AddSector(TEXT("River Crossing"), { { -140.f, 120.f }, { -380.f, 330.f } }, { TEXT("A2"), TEXT("B2") }, FVector2D(-640.f, -200.f), FVector2D(60.f, 170.f));
	AddSector(TEXT("Admin & Base"), { { 130.f, -370.f }, { 125.f, 330.f } }, { TEXT("A3"), TEXT("B3") }, FVector2D(-220.f, 20.f), FVector2D(300.f, 40.f));
	AddSector(TEXT("Hydro Dam"), { { 360.f, -95.f }, { 433.f, 0.f } }, { TEXT("A4"), TEXT("B4") }, FVector2D(150.f, 30.f), FVector2D(470.f, 420.f));
}

void ATOWorldGenerator::BuildDamValley(FRandomStream& Rng)
{
	auto PoiIndex = [this](const TCHAR* Name) -> int32
	{
		return POIs.IndexOfByPredicate([Name](const FTOPOI& P) { return P.Name == Name; });
	};
	auto G = [this](float Xm, float Ym, float OffsetCm = 0.f) -> FVector
	{
		return OnGround(Xm * CM, Ym * CM, OffsetCm);
	};

	const int32 PAdmin = PoiIndex(TEXT("Administration"));
	const int32 PPower = PoiIndex(TEXT("Hydro Power Station"));
	const int32 PDam = PoiIndex(TEXT("Hydro Dam"));
	const int32 PBase = PoiIndex(TEXT("Military Base"));
	const int32 PRadar = PoiIndex(TEXT("Radar Station"));
	const int32 PYard = PoiIndex(TEXT("Logistics Yard"));
	const int32 PPump = PoiIndex(TEXT("Pumping Station"));
	const int32 POutpost = PoiIndex(TEXT("Ridge Outpost"));
	const int32 PVillage = PoiIndex(TEXT("Old Village"));
	const int32 PGas = PoiIndex(TEXT("Gas Station"));
	const int32 PCamp = PoiIndex(TEXT("Lumber Camp"));
	const int32 PFarm = PoiIndex(TEXT("Farmstead"));

	// Fix POI heights to the ground
	for (FTOPOI& P : POIs)
	{
		if (P.Name != TEXT("Hydro Dam"))
		{
			P.Location.Z = GetTerrainHeight(P.Location.X, P.Location.Y);
		}
	}
	for (FTOExtractDef& E : Extracts)
	{
		E.Location.Z = GetTerrainHeight(E.Location.X, E.Location.Y);
		if (!E.SwitchLocation.IsZero())
		{
			E.SwitchLocation.Z = GetTerrainHeight(E.SwitchLocation.X, E.SwitchLocation.Y);
		}
	}
	for (FTOSpawnDef& S : Insertions)
	{
		S.Location.Z = GetTerrainHeight(S.Location.X, S.Location.Y) + 120.f;
	}
	for (FTOSectorDef& S : Sectors)
	{
		for (FVector& P : S.Points) P.Z = GetTerrainHeight(P.X, P.Y);
		S.AttackerSpawn.Z = GetTerrainHeight(S.AttackerSpawn.X, S.AttackerSpawn.Y) + 120.f;
		S.DefenderSpawn.Z = GetTerrainHeight(S.DefenderSpawn.X, S.DefenderSpawn.Y) + 120.f;
	}

	// =========================================================================== Hydro dam
	{
		const float TopZ = 6200.f;
		BuildDam(FVector(DamX + 1200.f, 0.f, 0.f), 50000.f, 0.f, TopZ, Rng);
		// Fix the B4 capture point & cable car on the crest
		for (FTOSectorDef& S : Sectors)
		{
			for (FVector& P : S.Points)
			{
				if (FMath::Abs(P.X - 43300.f) < 2000.f && FMath::Abs(P.Y) < 25000.f)
				{
					P.Z = TopZ + 20.f;
				}
			}
		}
		for (FTOExtractDef& E : Extracts)
		{
			if (E.Rule == ETOExtractRule::Paid)
			{
				E.Location.Z = FMath::Max(E.Location.Z, TopZ + 20.f);
				E.SwitchLocation.Z = FMath::Max(E.SwitchLocation.Z, TopZ + 20.f);
			}
		}
		// Cable car station & cables running up the eastern mountain
		const FTOFrame CF(FVector(43800.f, 30500.f, TopZ + 20.f), 0.f);
		Box(CF, FVector(0.f, 0.f, 200.f), FVector(800.f, 600.f, 20.f), ETOMat::MetalGrey);
		for (int32 c = 0; c < 4; ++c)
		{
			Box(CF, FVector((c & 1) ? 380.f : -380.f, (c & 2) ? 280.f : -280.f, 100.f), FVector(20.f, 20.f, 200.f), ETOMat::MetalGrey);
		}
		Box(CF, FVector(0.f, 0.f, 230.f), FVector(500.f, 300.f, 300.f), ETOMat::MetalRed);
		Box(CF, FVector(0.f, -151.f, 260.f), FVector(400.f, 2.f, 120.f), ETOMat::Glass, ETOCol::None);
		const FVector Top = G(700.f, 520.f, 2500.f);
		const FVector Start = CF.ToWorld(FVector(0.f, 0.f, 600.f));
		const FVector D = Top - Start;
		AddInstance(ETOShape::Cylinder, ETOMat::MetalDark, ETOCol::None, FTransform(FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator(), (Start + Top) * 0.5f, FVector(0.04f, 0.04f, D.Size() / 100.f)));
		BuildPylon(G(560.f, 400.f), 30.f);
		// Guards on the crest + overwatch
		AddGuard(FVector(42000.f, -24000.f, TopZ + 120.f), 0.f, PDam, 2);
		AddGuard(FVector(42000.f, 24000.f, TopZ + 120.f), 180.f, PDam, 2);
		AddLoot(FVector(42500.f, 3000.f, TopZ + 20.f), 0.f, ETOContainerType::Toolbox, 2, PDam);
		AddLoot(FVector(42500.f, -6000.f, TopZ + 20.f), 0.f, ETOContainerType::AmmoBox, 2, PDam);
	}

	// ================================================================= Hydro power station
	{
		const float Z = PadHeight(TEXT("Power"));
		BuildPowerStation(FVector(37500.f, -9500.f, Z), -90.f, PPower, Rng);
		BuildVehicle(FVector(34500.f, -2000.f, Z), 0.f, 1, Rng);
		BuildVehicle(FVector(34200.f, -15000.f, Z), 90.f, 3, Rng);
		BuildCrateCluster(FVector(35000.f, -4000.f, Z), 15.f, Rng);
		AddLoot(FVector(35200.f, -3200.f, Z), 0.f, ETOContainerType::Duffel, 2, PPower);
		AddGuard(FVector(34500.f, -9500.f, Z + 100.f), 180.f, PPower, 2);
		AddGuard(FVector(34000.f, -14000.f, Z + 100.f), 200.f, PPower, 2);
		for (int32 i = 0; i < 4; ++i)
		{
			BuildLampPost(FVector(34000.f, -13500.f + i * 3000.f, Z), 0.f);
		}
		// Power lines heading south along the highway
		for (int32 i = 0; i < 9; ++i)
		{
			const float X = 30000.f - i * 15000.f;
			BuildPylon(OnGround(X, -24000.f - i * 600.f), 0.f);
		}
	}

	// ====================================================================== Administration
	{
		const float Z = PadHeight(TEXT("Admin"));
		FTOBuildingSpec A;
		A.Center = FVector(13500.f, 34000.f, Z);
		A.Yaw = 0.f;
		A.W = 5600.f;
		A.D = 2200.f;
		A.Floors = 4;
		A.FloorH = 370.f;
		A.Ext = ETOMat::PlasterBeige;
		A.Int = ETOMat::Plaster;
		A.Floor = ETOMat::Tile;
		A.Trim = ETOMat::ConcreteLight;
		A.Layout = 0;
		A.DoorMask = 1 | 4 | 8;
		A.Tier = 3;
		A.Poi = PAdmin;
		A.SpecialRoomKey = FName(TEXT("AdminBlue"));
		A.SpecialRoomFloor = 3;
		A.SpecialLoot = ETOContainerType::HighValueCrate;
		A.WinW = 180.f;
		A.WinH = 170.f;
		A.WinSpacing = 380.f;
		BuildBuilding(A, Rng);

		// Commander (boss) + bodyguards on the director's floor
		const float BossZ = Z + 3.f * A.FloorH + 100.f;
		AddGuard(FVector(13500.f + 900.f, 34000.f, BossZ), 180.f, PAdmin, 3, true);
		AddGuard(FVector(13500.f - 600.f, 34000.f, BossZ), 0.f, PAdmin, 3);
		AddGuard(FVector(13500.f + 1800.f, 34000.f, BossZ), 180.f, PAdmin, 3);
		AddGuard(FVector(13500.f, 34000.f, Z + 2.f * A.FloorH + 100.f), 90.f, PAdmin, 3);

		// Annex with the server vault (red keycard)
		FTOBuildingSpec B;
		B.Center = FVector(6500.f, 28500.f, Z);
		B.Yaw = 90.f;
		B.W = 2800.f;
		B.D = 1500.f;
		B.Floors = 2;
		B.FloorH = 360.f;
		B.Ext = ETOMat::Plaster;
		B.Trim = ETOMat::MetalBlue;
		B.Layout = 0;
		B.DoorMask = 1 | 2;
		B.Tier = 3;
		B.Poi = PAdmin;
		B.SpecialRoomKey = FName(TEXT("ServerRed"));
		B.SpecialRoomFloor = 1;
		B.SpecialLoot = ETOContainerType::ServerRack;
		BuildBuilding(B, Rng);

		// Security booth at the gate
		FTOBuildingSpec Booth;
		Booth.Center = FVector(7900.f, 27300.f, Z);
		Booth.W = 450.f;
		Booth.D = 380.f;
		Booth.Floors = 1;
		Booth.FloorH = 300.f;
		Booth.Layout = 1;
		Booth.Ext = ETOMat::MetalWhite;
		Booth.Trim = ETOMat::MetalRed;
		Booth.bRoofAccess = false;
		Booth.DoorMask = 2;
		Booth.Tier = 2;
		Booth.Poi = PAdmin;
		Booth.bGuards = false;
		Booth.WinW = 160.f;
		Booth.WinSpacing = 220.f;
		BuildBuilding(Booth, Rng);
		const FTOFrame GF(FVector(8500.f, 26700.f, Z), 0.f);
		Box(GF, FVector(0.f, 0.f, 100.f), FVector(20.f, 20.f, 200.f), ETOMat::MetalYellow);
		Box(GF, FVector(1250.f, 0.f, 110.f), FVector(2400.f, 10.f, 10.f), ETOMat::MetalRed, ETOCol::None);

		// Parking lot with cars
		const FTOFrame PF(FVector(17500.f, 27800.f, Z), 0.f);
		for (int32 i = 0; i < 9; ++i)
		{
			Box(PF, FVector(i * 320.f - 1300.f, 0.f, 5.f), FVector(10.f, 500.f, 2.f), ETOMat::RoadLineWhite, ETOCol::None);
			if (Rng.FRand() < 0.6f && i < 8)
			{
				BuildVehicle(PF.ToWorld(FVector(i * 320.f - 1140.f, 0.f, 0.f)), 90.f + Rng.FRandRange(-4.f, 4.f), Rng.FRand() < 0.8f ? 0 : 3, Rng);
			}
		}
		// Flag poles & lamps
		for (int32 i = -1; i <= 1; ++i)
		{
			const FVector FP(10200.f, 34000.f + i * 400.f, Z);
			AddInstance(ETOShape::Cylinder, ETOMat::Aluminium, ETOCol::Solid, FTransform(FRotator::ZeroRotator, FP + FVector(0.f, 0.f, 600.f), FVector(0.12f, 0.12f, 12.f)));
			AddInstance(ETOShape::Cube, i == 0 ? ETOMat::MetalBlue : ETOMat::MetalWhite, ETOCol::None, FTransform(FRotator::ZeroRotator, FP + FVector(0.f, 75.f, 1120.f), FVector(0.02f, 1.4f, 0.9f)));
		}
		for (int32 i = 0; i < 6; ++i)
		{
			BuildLampPost(FVector(5500.f + i * 2400.f, 31000.f, Z), 90.f);
		}
		// Perimeter wall with gates
		const float X0 = 4600.f, X1 = 19400.f, Y0 = 26600.f, Y1 = 39400.f;
		BuildFence(FVector2D(X0, Y0), FVector2D(X0, Y1), Z, true);
		BuildFence(FVector2D(X1, Y0), FVector2D(X1, Y1), Z, true);
		BuildFence(FVector2D(X0, Y1), FVector2D(X1, Y1), Z, true);
		BuildFence(FVector2D(X0, Y0), FVector2D(8500.f, Y0), Z, true);
		BuildFence(FVector2D(11000.f, Y0), FVector2D(X1, Y0), Z, true);
		// Outdoor guards & loot
		AddGuard(FVector(9500.f, 27500.f, Z + 100.f), 180.f, PAdmin, 3);
		AddGuard(FVector(17500.f, 31000.f, Z + 100.f), 270.f, PAdmin, 3);
		AddGuard(FVector(13500.f, 38500.f, Z + 100.f), 0.f, PAdmin, 3);
		AddLoot(FVector(16000.f, 31200.f, Z), 0.f, ETOContainerType::Duffel, 2, PAdmin);
		AddLoot(FVector(10500.f, 31200.f, Z), 0.f, ETOContainerType::MedicalCase, 2, PAdmin);
	}

	// ======================================================================= Military base
	{
		const float Z = PadHeight(TEXT("Base"));
		const float X0 = 3600.f, X1 = 22400.f, Y0 = -46200.f, Y1 = -29800.f;
		// HESCO perimeter with north (road) and east gates
		BuildFence(FVector2D(X0, Y0), FVector2D(X1, Y0), Z, true);
		BuildFence(FVector2D(X0, Y0), FVector2D(X0, Y1), Z, true);
		BuildFence(FVector2D(X1, Y0), FVector2D(X1, -38500.f), Z, true);
		BuildFence(FVector2D(X1, -36500.f), FVector2D(X1, Y1), Z, true);
		BuildFence(FVector2D(X0, Y1), FVector2D(8500.f, Y1), Z, true);
		BuildFence(FVector2D(10500.f, Y1), FVector2D(X1, Y1), Z, true);
		// Gate barrier
		const FTOFrame GF(FVector(9500.f, Y1, Z), 0.f);
		Box(GF, FVector(-1000.f, 0.f, 150.f), FVector(60.f, 60.f, 300.f), ETOMat::MetalYellow);
		Box(GF, FVector(0.f, 0.f, 110.f), FVector(1900.f, 12.f, 12.f), ETOMat::MetalRed, ETOCol::None);

		for (int32 i = 0; i < 3; ++i)
		{
			BuildBarracks(FVector(7800.f, -33500.f - i * 4500.f, Z), 0.f, PBase, 2, Rng);
		}
		FTOBuildingSpec HQ;
		HQ.Center = FVector(17500.f, -33200.f, Z);
		HQ.Yaw = 0.f;
		HQ.W = 2600.f;
		HQ.D = 1400.f;
		HQ.Floors = 2;
		HQ.FloorH = 360.f;
		HQ.Ext = ETOMat::PlasterBeige;
		HQ.Trim = ETOMat::MetalGreen;
		HQ.Roof = ETOMat::MetalGreen;
		HQ.Layout = 0;
		HQ.DoorMask = 1 | 2;
		HQ.Tier = 2;
		HQ.Poi = PBase;
		BuildBuilding(HQ, Rng);

		// Armory bunker (needs the Armory Key)
		FTOBuildingSpec Arm;
		Arm.Center = FVector(13500.f, -43500.f, Z);
		Arm.Yaw = 180.f;
		Arm.W = 1100.f;
		Arm.D = 800.f;
		Arm.Floors = 1;
		Arm.FloorH = 320.f;
		Arm.Ext = ETOMat::ConcreteDark;
		Arm.Trim = ETOMat::ConcreteDark;
		Arm.Layout = 1;
		Arm.DoorMask = 1;
		Arm.bWindows = false;
		Arm.bRoofAccess = false;
		Arm.Tier = 3;
		Arm.Poi = PBase;
		Arm.EntranceKey = FName(TEXT("Armory"));
		Arm.SpecialRoomKey = FName(TEXT("Armory"));
		Arm.SpecialRoomFloor = 0;
		Arm.SpecialLoot = ETOContainerType::WeaponCrate;
		Arm.bGuards = false;
		BuildBuilding(Arm, Rng);
		AddLoot(FVector(13900.f, -43300.f, Z), 0.f, ETOContainerType::WeaponCrate, 3, PBase);
		AddLoot(FVector(13100.f, -43300.f, Z), 0.f, ETOContainerType::AmmoBox, 3, PBase);
		// Sandbag dome on top
		BuildSandbagWall(FVector(13500.f, -43900.f, Z + 330.f), 0.f, 900.f);

		// Helipad (radio extraction lands here)
		BuildHelipad(FVector(19000.f, -42500.f, Z));
		// Watchtowers
		BuildWatchtower(FVector(X0 + 400.f, Y1 - 400.f, Z), 45.f, PBase, 2);
		BuildWatchtower(FVector(X1 - 400.f, Y1 - 400.f, Z), 135.f, PBase, 2);
		BuildWatchtower(FVector(X0 + 400.f, Y0 + 400.f, Z), -45.f, PBase, 2);
		BuildWatchtower(FVector(X1 - 400.f, Y0 + 400.f, Z), -135.f, PBase, 2);
		// Vehicle depot
		const FTOFrame DF(FVector(13000.f, -33000.f, Z), 0.f);
		for (int32 c = 0; c < 6; ++c)
		{
			Box(DF, FVector((c % 3) * 900.f - 900.f, (c / 3) ? 600.f : -600.f, 250.f), FVector(30.f, 30.f, 500.f), ETOMat::MetalGreen);
		}
		Box(DF, FVector(0.f, 0.f, 510.f), FVector(2000.f, 1400.f, 20.f), ETOMat::MetalGreen);
		BuildVehicle(DF.ToWorld(FVector(-500.f, 0.f, 0.f)), 0.f, 2, Rng);
		BuildVehicle(DF.ToWorld(FVector(500.f, 0.f, 0.f)), 0.f, 1, Rng);
		Footprints.Add(FBox2D(FVector2D(12000.f, -33700.f), FVector2D(14000.f, -32300.f)));
		// Containers, sandbags, tents, fuel
		BuildContainerStack(FVector(17500.f, -38500.f, Z), 0.f, 5, Rng, true, PBase, 2);
		BuildContainerStack(FVector(9500.f, -45000.f, Z), 90.f, 3, Rng, false, PBase, 2);
		BuildTankFarm(FVector(20200.f, -36500.f, Z), 90.f, PBase);
		BuildTent(FVector(12500.f, -38000.f, Z), 0.f);
		BuildTent(FVector(12500.f, -39200.f, Z), 0.f);
		BuildSandbagWall(FVector(9500.f, -31200.f, Z), 0.f, 700.f);
		BuildSandbagWall(FVector(21200.f, -37500.f, Z), 90.f, 600.f);
		BuildCrateCluster(FVector(15500.f, -41000.f, Z), 30.f, Rng);
		AddLoot(FVector(12500.f, -38600.f, Z), 0.f, ETOContainerType::WeaponCrate, 2, PBase);
		AddLoot(FVector(15800.f, -40500.f, Z), 0.f, ETOContainerType::AmmoBox, 2, PBase);
		AddLoot(FVector(10800.f, -40800.f, Z), 90.f, ETOContainerType::MedicalCase, 2, PBase);
		AddGuard(FVector(9500.f, -31500.f, Z + 100.f), 0.f, PBase, 2);
		AddGuard(FVector(16000.f, -42000.f, Z + 100.f), 270.f, PBase, 2);
		AddGuard(FVector(11000.f, -36000.f, Z + 100.f), 90.f, PBase, 2);
		for (int32 i = 0; i < 4; ++i)
		{
			BuildLampPost(FVector(11000.f + i * 2500.f, -36200.f, Z), 0.f);
		}
	}

	// ======================================================================= Radar station
	{
		const float Z = PadHeight(TEXT("Radar"));
		BuildRadarStation(FVector(38000.f, 65000.f, Z), PRadar, Rng);
	}

	// ====================================================================== Logistics yard
	{
		const float Z = PadHeight(TEXT("Yard"));
		BuildWarehouse(FVector(-41000.f, 30500.f, Z), 0.f, 4200.f, 2400.f, PYard, 1, FName(TEXT("Warehouse")), Rng);
		BuildWarehouse(FVector(-33000.f, 37000.f, Z), 90.f, 3000.f, 2000.f, PYard, 1, NAME_None, Rng);
		BuildContainerStack(FVector(-33500.f, 27500.f, Z), 0.f, 6, Rng, true, PYard, 1);
		BuildContainerStack(FVector(-33500.f, 30500.f, Z), 0.f, 4, Rng, false, PYard, 1);
		BuildContainerStack(FVector(-45500.f, 36500.f, Z), 90.f, 5, Rng, true, PYard, 1);
		BuildCraneGantry(FVector(-33500.f, 29200.f, Z), 0.f, 4200.f);
		BuildVehicle(FVector(-37500.f, 26800.f, Z), 0.f, 1, Rng);
		BuildVehicle(FVector(-44000.f, 27500.f, Z), 180.f, 3, Rng);
		FTOBuildingSpec Office;
		Office.Center = FVector(-46500.f, 30000.f, Z);
		Office.Yaw = 90.f;
		Office.W = 1400.f;
		Office.D = 700.f;
		Office.Floors = 1;
		Office.FloorH = 300.f;
		Office.Layout = 1;
		Office.Ext = ETOMat::MetalWhite;
		Office.Trim = ETOMat::MetalBlue;
		Office.bRoofAccess = false;
		Office.Tier = 1;
		Office.Poi = PYard;
		BuildBuilding(Office, Rng);
		BarrelSpots.Add(FVector(-36000.f, 26700.f, Z));
		BarrelSpots.Add(FVector(-36300.f, 26600.f, Z));
		const float X0 = -47000.f, X1 = -29000.f, Y0 = 26000.f, Y1 = 40000.f;
		BuildFence(FVector2D(X0, Y0), FVector2D(X1, Y0), Z, false);
		BuildFence(FVector2D(X0, Y1), FVector2D(X1, Y1), Z, false);
		BuildFence(FVector2D(X0, Y0), FVector2D(X0, Y1), Z, false);
		BuildFence(FVector2D(X1, Y0), FVector2D(X1, 29000.f), Z, false);
		BuildFence(FVector2D(X1, 31500.f), FVector2D(X1, Y1), Z, false);
		AddGuard(FVector(-30500.f, 30000.f, Z + 100.f), 0.f, PYard, 1);
		AddGuard(FVector(-38000.f, 34500.f, Z + 100.f), 90.f, PYard, 1);
	}

	// ===================================================================== Pumping station
	{
		const float Z = PadHeight(TEXT("Pump"));
		BuildPumpStation(FVector(-14000.f, 11000.f, Z), 0.f, PPump, Rng);
		AddLoot(FVector(-12000.f, 9000.f, Z), 0.f, ETOContainerType::Duffel, 1, PPump);
		AddGuard(FVector(-16000.f, 9500.f, Z + 100.f), 180.f, PPump, 1);
	}

	// ======================================================================= Ridge outpost
	{
		const float Z = PadHeight(TEXT("Outpost"));
		const FVector C(48000.f, -62000.f, Z);
		BuildWatchtower(C + FVector(800.f, 800.f, 0.f), 0.f, POutpost, 1);
		FTOBuildingSpec Bunker;
		Bunker.Center = C + FVector(-600.f, 0.f, 0.f);
		Bunker.W = 900.f;
		Bunker.D = 700.f;
		Bunker.Floors = 1;
		Bunker.FloorH = 300.f;
		Bunker.Layout = 1;
		Bunker.Ext = ETOMat::ConcreteDark;
		Bunker.Trim = ETOMat::ConcreteDark;
		Bunker.WinW = 120.f;
		Bunker.WinH = 40.f;
		Bunker.WinSill = 150.f;
		Bunker.Tier = 1;
		Bunker.Poi = POutpost;
		BuildBuilding(Bunker, Rng);
		BuildTent(C + FVector(600.f, -900.f, 0.f), 30.f);
		BuildSandbagWall(C + FVector(-1500.f, 600.f, 0.f), 60.f, 600.f);
		BuildSandbagWall(C + FVector(1500.f, -600.f, 0.f), -30.f, 600.f);
		BuildCrateCluster(C + FVector(200.f, 900.f, 0.f), 0.f, Rng);
		AddLoot(C + FVector(400.f, -400.f, 0.f), 0.f, ETOContainerType::WeaponCrate, 1, POutpost);
		AddLoot(C + FVector(-200.f, 900.f, 0.f), 90.f, ETOContainerType::MedicalCase, 1, POutpost);
		AddGuard(C + FVector(0.f, 0.f, 100.f), 270.f, POutpost, 1);
	}

	// ========================================================================= Old village
	{
		const float Z = PadHeight(TEXT("Village"));
		struct FHouse { float X; float Y; float Yaw; int32 Floors; };
		const FHouse Houses[] =
		{
			{ -540.f, -245.f, 0.f, 2 }, { -505.f, -248.f, 0.f, 1 }, { -440.f, -245.f, 0.f, 2 }, { -405.f, -248.f, 0.f, 1 },
			{ -540.f, -300.f, 180.f, 1 }, { -505.f, -302.f, 180.f, 2 }, { -430.f, -300.f, 180.f, 2 },
			{ -545.f, -340.f, 0.f, 1 }, { -400.f, -342.f, 0.f, 1 },
			{ -525.f, -180.f, 180.f, 1 }, { -420.f, -182.f, 180.f, 2 }
		};
		static const ETOMat WallMats[] = { ETOMat::Plaster, ETOMat::PlasterBeige, ETOMat::Brick, ETOMat::PlasterBlue };
		for (const FHouse& H : Houses)
		{
			BuildHouse(FVector(H.X * CM, H.Y * CM, Z), H.Yaw + Rng.FRandRange(-3.f, 3.f), H.Floors, WallMats[Rng.RandRange(0, 3)], PVillage, 0, Rng);
		}
		BuildChurch(FVector(-47000.f, -33600.f, Z), 90.f, PVillage, Rng);
		// Village square: well, benches, a parked pickup, fences
		const FTOFrame SF(FVector(-47000.f, -27200.f, Z), 0.f);
		Cyl(SF, FVector(0.f, 0.f, 50.f), 220.f, 100.f, ETOMat::Brick);
		Box(SF, FVector(0.f, 0.f, 220.f), FVector(260.f, 20.f, 20.f), ETOMat::WoodDark);
		Box(SF, FVector(-110.f, 0.f, 150.f), FVector(15.f, 15.f, 200.f), ETOMat::WoodDark);
		Box(SF, FVector(110.f, 0.f, 150.f), FVector(15.f, 15.f, 200.f), ETOMat::WoodDark);
		for (int32 i = 0; i < 4; ++i)
		{
			Box(SF, FVector(FMath::Cos(i * PI * 0.5f) * 500.f, FMath::Sin(i * PI * 0.5f) * 500.f, 25.f), FVector(180.f, 50.f, 50.f), ETOMat::Wood, ETOCol::Solid, FRotator(0.f, i * 90.f + 90.f, 0.f));
		}
		BuildVehicle(FVector(-45500.f, -27000.f, Z), 70.f, 3, Rng);
		BuildVehicle(FVector(-49500.f, -22500.f, Z), 10.f, 0, Rng);
		for (int32 i = 0; i < 5; ++i)
		{
			BuildLampPost(FVector(-55000.f + i * 3500.f, -21800.f, Z), 0.f);
		}
		AddGuard(FVector(-47000.f, -26000.f, Z + 100.f), 0.f, PVillage, 0);
		AddGuard(FVector(-44000.f, -31000.f, Z + 100.f), 90.f, PVillage, 0);
		for (int32 i = 0; i < 6; ++i)
		{
			AddLoot(FVector(-55000.f + Rng.FRandRange(0.f, 16000.f), -34000.f + Rng.FRandRange(0.f, 14000.f), Z), Rng.FRandRange(0.f, 360.f), ETOContainerType::LooseItem, 0, PVillage);
		}
	}

	// ========================================================================= Gas station
	{
		const float Z = PadHeight(TEXT("Gas"));
		BuildGasStation(FVector(-30000.f, -14200.f, Z), 0.f, PGas, Rng);
		AddGuard(FVector(-28500.f, -13000.f, Z + 100.f), 0.f, PGas, 0);
	}

	// ========================================================================= Lumber camp
	{
		const float Z = PadHeight(TEXT("Camp"));
		const FVector C(-85000.f, -65000.f, Z);
		BuildTent(C + FVector(500.f, 400.f, 0.f), 20.f);
		BuildTent(C + FVector(-600.f, 500.f, 0.f), -15.f);
		BuildTent(C + FVector(0.f, -700.f, 0.f), 80.f);
		const FTOFrame SF(C + FVector(800.f, -800.f, 0.f), 30.f);
		for (int32 c = 0; c < 4; ++c)
		{
			Box(SF, FVector((c & 1) ? 500.f : -500.f, (c & 2) ? 300.f : -300.f, 200.f), FVector(25.f, 25.f, 400.f), ETOMat::WoodDark);
		}
		Box(SF, FVector(0.f, 0.f, 410.f), FVector(1100.f, 700.f, 20.f), ETOMat::MetalRust);
		for (int32 i = 0; i < 6; ++i)
		{
			AddInstance(ETOShape::Cylinder, ETOMat::Bark, ETOCol::Solid, FTransform(FRotator(90.f, 0.f, 0.f), SF.ToWorld(FVector(0.f, -200.f + (i % 3) * 70.f, 35.f + (i / 3) * 65.f)), FVector(0.65f, 0.65f, 8.f)));
		}
		AddInstance(ETOShape::Cylinder, ETOMat::RockDark, ETOCol::Solid, FTransform(FRotator::ZeroRotator, C + FVector(0.f, 0.f, 10.f), FVector(1.2f, 1.2f, 0.2f)));
		AddLight(C + FVector(0.f, 0.f, 60.f), FLinearColor(1.f, 0.55f, 0.2f), 600.f, 1200.f, true);
		AddLoot(C + FVector(300.f, 0.f, 0.f), 0.f, ETOContainerType::Duffel, 0, PCamp);
		AddLoot(SF.ToWorld(FVector(300.f, 200.f, 0.f)), 0.f, ETOContainerType::Toolbox, 0, PCamp);
		AddGuard(C + FVector(0.f, 300.f, 100.f), 0.f, PCamp, 0);
	}

	// =========================================================================== Farmstead
	{
		const float Z = PadHeight(TEXT("Farm"));
		const FVector C(-80000.f, 65000.f, Z);
		BuildHouse(C + FVector(1500.f, -1500.f, 0.f), 0.f, 2, ETOMat::PlasterBeige, PFarm, 0, Rng);
		FTOBuildingSpec Barn;
		Barn.Center = C + FVector(-1000.f, 1200.f, 0.f);
		Barn.Yaw = 90.f;
		Barn.W = 2200.f;
		Barn.D = 1300.f;
		Barn.Floors = 1;
		Barn.FloorH = 600.f;
		Barn.Layout = 1;
		Barn.Ext = ETOMat::WoodDark;
		Barn.Trim = ETOMat::Wood;
		Barn.Roof = ETOMat::MetalRust;
		Barn.Floor = ETOMat::Dirt;
		Barn.bGable = true;
		Barn.bRoofAccess = false;
		Barn.bParapet = false;
		Barn.DoorMask = 1 | 2;
		Barn.WinW = 80.f;
		Barn.WinH = 80.f;
		Barn.WinSill = 380.f;
		Barn.Tier = 0;
		Barn.Poi = PFarm;
		BuildBuilding(Barn, Rng);
		const FTOFrame SF(C + FVector(1500.f, 1800.f, 0.f), 0.f);
		Cyl(SF, FVector(0.f, 0.f, 650.f), 500.f, 1300.f, ETOMat::MetalGrey);
		Sphere(SF, FVector(0.f, 0.f, 1300.f), FVector(500.f, 500.f, 260.f), ETOMat::MetalGrey);
		Footprints.Add(FBox2D(FVector2D(C.X + 1200.f, C.Y + 1500.f), FVector2D(C.X + 1800.f, C.Y + 2100.f)));
		BuildVehicle(C + FVector(0.f, 0.f, 0.f), 40.f, 3, Rng);
		BuildCrateCluster(C + FVector(-2000.f, -1500.f, 0.f), 0.f, Rng);
		AddLoot(C + FVector(-300.f, -700.f, 0.f), 0.f, ETOContainerType::Toolbox, 0, PFarm);
		AddGuard(C + FVector(0.f, -2000.f, 100.f), 0.f, PFarm, 0);
	}

	// ====================================================================== River bridges
	{
		auto Bridge = [this](FVector2D AM, FVector2D BM, float WidthM)
		{
			const FVector A(AM.X * CM, AM.Y * CM, HeightStage(AM.X * CM, AM.Y * CM, 1) + 30.f);
			const FVector B(BM.X * CM, BM.Y * CM, HeightStage(BM.X * CM, BM.Y * CM, 1) + 30.f);
			BuildBridge(A, B, WidthM * CM, ETOMat::ConcreteLight);
		};
		Bridge(FVector2D(-73.f, -122.f), FVector2D(-46.f, -6.f), 9.f);
		Bridge(FVector2D(-753.f, -96.f), FVector2D(-768.f, 28.f), 8.f);
		// Footbridge near the power station
		Bridge(FVector2D(300.f, 30.f), FVector2D(325.f, 150.f), 3.f);
	}

	// ==================================================================== Extraction props
	for (const FTOExtractDef& E : Extracts)
	{
		const FTOFrame F(E.Location, 0.f);
		switch (E.Rule)
		{
		case ETOExtractRule::Always:
			if (E.Name == TEXT("South Checkpoint"))
			{
				FTOBuildingSpec Booth;
				Booth.Center = E.Location + FVector(0.f, 900.f, 0.f);
				Booth.W = 500.f;
				Booth.D = 400.f;
				Booth.Floors = 1;
				Booth.FloorH = 300.f;
				Booth.Layout = 1;
				Booth.Ext = ETOMat::MetalWhite;
				Booth.Trim = ETOMat::MetalRed;
				Booth.bRoofAccess = false;
				Booth.bGuards = false;
				BuildBuilding(Booth, Rng);
				Box(F, FVector(0.f, 0.f, 110.f), FVector(20.f, 1000.f, 15.f), ETOMat::MetalRed, ETOCol::None);
				BuildSandbagWall(E.Location + FVector(-800.f, -600.f, 0.f), 0.f, 500.f);
				BuildSandbagWall(E.Location + FVector(800.f, 600.f, 0.f), 0.f, 500.f);
			}
			else if (E.Name == TEXT("River Boat"))
			{
				const float WZ = 260.f;
				Box(F, FVector(0.f, 600.f, WZ + 30.f), FVector(400.f, 1400.f, 20.f), ETOMat::Wood);
				for (int32 c = 0; c < 6; ++c)
				{
					Box(F, FVector((c & 1) ? 180.f : -180.f, 100.f + (c / 2) * 500.f, WZ - 100.f), FVector(20.f, 20.f, 300.f), ETOMat::WoodDark);
				}
				Box(F, FVector(400.f, 1100.f, WZ + 30.f), FVector(700.f, 220.f, 80.f), ETOMat::MetalWhite);
				Box(F, FVector(300.f, 1100.f, WZ + 110.f), FVector(160.f, 160.f, 90.f), ETOMat::Glass, ETOCol::None);
			}
			else
			{
				BuildTent(E.Location + FVector(500.f, 500.f, 0.f), 15.f);
				BuildCrateCluster(E.Location + FVector(-600.f, 300.f, 0.f), 0.f, Rng);
			}
			break;
		case ETOExtractRule::NoBackpack:
		{
			// Rail tunnel portal cut into the hillside
			const FTOFrame TF(E.Location + FVector(0.f, 900.f, 0.f), 0.f);
			Box(TF, FVector(-450.f, 0.f, 350.f), FVector(150.f, 400.f, 700.f), ETOMat::ConcreteDark);
			Box(TF, FVector(450.f, 0.f, 350.f), FVector(150.f, 400.f, 700.f), ETOMat::ConcreteDark);
			Box(TF, FVector(0.f, 0.f, 760.f), FVector(1100.f, 400.f, 160.f), ETOMat::ConcreteDark);
			Box(TF, FVector(0.f, 300.f, 350.f), FVector(800.f, 300.f, 700.f), ETOMat::Black);
			for (int32 i = -1; i <= 1; i += 2)
			{
				Box(TF, FVector(i * 75.f, -1500.f, 8.f), FVector(10.f, 3000.f, 15.f), ETOMat::MetalRust, ETOCol::None);
			}
			for (int32 k = 0; k < 10; ++k)
			{
				Box(TF, FVector(0.f, -2800.f + k * 300.f, 3.f), FVector(240.f, 25.f, 10.f), ETOMat::WoodDark, ETOCol::None);
			}
			break;
		}
		default:
			break;
		}
	}

	// ===================================================================== Street furniture
	// Lamps along the highway through the valley floor
	for (const FTORoad& R : Roads)
	{
		if (R.bDirt)
		{
			continue;
		}
		for (int32 i = 3; i < R.Points.Num(); i += 9)
		{
			const FVector2D P = R.Points[i];
			const FVector2D N = (R.Points[FMath::Min(i + 1, R.Points.Num() - 1)] - R.Points[i - 1]).GetSafeNormal();
			const FVector2D Side(-N.Y, N.X);
			const FVector2D L = P + Side * (R.Width * 0.5f + 150.f);
			if (Rng.FRand() < 0.5f)
			{
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Side.Y, -Side.X));
				BuildLampPost(OnGround(L.X, L.Y), Yaw);
			}
		}
	}

	// Scattered roadside wrecks & cover along the valley
	for (int32 i = 0; i < 18; ++i)
	{
		const FTORoad& R = Roads[Rng.RandRange(0, Roads.Num() - 1)];
		if (R.Points.Num() < 3)
		{
			continue;
		}
		const int32 Idx = Rng.RandRange(1, R.Points.Num() - 2);
		const FVector2D Dir = (R.Points[Idx + 1] - R.Points[Idx - 1]).GetSafeNormal();
		const FVector2D Side(-Dir.Y, Dir.X);
		const FVector2D P = R.Points[Idx] + Side * (R.Width * 0.5f + Rng.FRandRange(150.f, 500.f)) * (Rng.FRand() < 0.5f ? 1.f : -1.f);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X)) + Rng.FRandRange(-25.f, 25.f);
		BuildVehicle(OnGround(P.X, P.Y), Yaw, Rng.RandRange(0, 3), Rng);
		if (Rng.FRand() < 0.4f)
		{
			AddLoot(OnGround(P.X + 300.f, P.Y + 300.f), Yaw, ETOContainerType::LooseItem, 0, -1);
		}
	}

	// Bridge ends stay clear of trees
	for (const FVector& E : BridgeEnds)
	{
		Footprints.Add(FBox2D(FVector2D(E.X - 2000.f, E.Y - 2000.f), FVector2D(E.X + 2000.f, E.Y + 2000.f)));
	}
	// Extraction & insertion zones stay clear of trees
	for (const FTOExtractDef& E : Extracts)
	{
		Footprints.Add(FBox2D(FVector2D(E.Location.X - 1200.f, E.Location.Y - 1200.f), FVector2D(E.Location.X + 1200.f, E.Location.Y + 1200.f)));
	}
	for (const FTOSpawnDef& S : Insertions)
	{
		Footprints.Add(FBox2D(FVector2D(S.Location.X - 1500.f, S.Location.Y - 1500.f), FVector2D(S.Location.X + 1500.f, S.Location.Y + 1500.f)));
	}
}
