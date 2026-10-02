// TAC-OPS - procedural battlefield generator: building & prop prefabs
//
// All prefabs are built in a local frame (FTOFrame) out of instanced primitives.
// Walls are split around door / window openings so interiors are fully walkable,
// multi-storey buildings get real staircases with stair wells and railings.

#include "World/TOWorldGenerator.h"

namespace
{
	FBox2D MakeBox2D(float X0, float Y0, float X1, float Y1)
	{
		return FBox2D(FVector2D(FMath::Min(X0, X1), FMath::Min(Y0, Y1)), FVector2D(FMath::Max(X0, X1), FMath::Max(Y0, Y1)));
	}

	/** Evenly spaced windows along a wall, skipping a door region. */
	TArray<FTOOpening> WindowRow(float Len, float Spacing, float WinW, float Sill, float WinH, float DoorCenter, float DoorWidth, float DoorHeight)
	{
		TArray<FTOOpening> Out;
		if (DoorWidth > 0.f)
		{
			Out.Add(FTOOpening(DoorCenter, DoorWidth, 0.f, DoorHeight));
		}
		if (Spacing > 0.f && WinW > 0.f)
		{
			const int32 Count = FMath::Max(0, FMath::FloorToInt((Len - 120.f) / Spacing));
			const float Start = (Len - (Count - 1) * Spacing) * 0.5f;
			for (int32 i = 0; i < Count; ++i)
			{
				const float C = Start + i * Spacing;
				if (DoorWidth > 0.f && FMath::Abs(C - DoorCenter) < (DoorWidth + WinW) * 0.5f + 40.f)
				{
					continue;
				}
				if (C - WinW * 0.5f < 60.f || C + WinW * 0.5f > Len - 60.f)
				{
					continue;
				}
				Out.Add(FTOOpening(C, WinW, Sill, Sill + WinH));
			}
		}
		return Out;
	}
}

// =============================================================================================
//  Generic multi-storey building (offices / open floors)
// =============================================================================================

void ATOWorldGenerator::BuildBuilding(const FTOBuildingSpec& S, FRandomStream& Rng)
{
	const FTOFrame F(S.Center, S.Yaw);
	const float HW = S.W * 0.5f;
	const float HD = S.D * 0.5f;
	const float T = S.Wall;
	const float TopZ = S.Floors * S.FloorH;

	// Footprint (world AABB) for minimap / vegetation exclusion
	{
		FBox2D Fp(ForceInit);
		for (int32 c = 0; c < 4; ++c)
		{
			const FVector P = F.ToWorld(FVector((c & 1) ? HW : -HW, (c & 2) ? HD : -HD, 0.f));
			Fp += FVector2D(P.X, P.Y);
		}
		Footprints.Add(Fp);
	}

	// Foundation plinth hides the seam with the terrain
	Box(F, FVector(0.f, 0.f, -160.f), FVector(S.W + 40.f, S.D + 40.f, 320.f), ETOMat::ConcreteDark);

	// Stair geometry (along the +Y wall at the +X end)
	const float Rise = S.FloorH;
	const float Run = Rise / FMath::Tan(FMath::DegreesToRadians(35.f));
	const float StairW = 130.f;
	const float StairX0 = HW - T * 0.5f - 130.f - Run;
	const float StairX1 = StairX0 + Run;
	const float StairY = HD - T * 0.5f - StairW * 0.5f;
	const FBox2D Hole = MakeBox2D(StairX0 + Run * 0.42f, HD - T * 0.5f - StairW - 8.f, StairX1 + 15.f, HD - T * 0.5f);
	const bool bHasStairs = S.Floors > 1 || S.bRoofAccess;

	for (int32 Fl = 0; Fl < S.Floors; ++Fl)
	{
		const float Z0 = Fl * S.FloorH;

		// Slab (ground slab without hole)
		const FBox2D Outer = MakeBox2D(-HW, -HD, HW, HD);
		if (Fl == 0)
		{
			SlabWithHole(F, Outer, Hole, 0.f, 20.f, S.Floor, false);
		}
		else
		{
			SlabWithHole(F, Outer, Hole, Z0, 25.f, S.Floor, true);
			// Ceiling face of the slab below (thin, interior material)
			Railing(F, FVector2D(Hole.Min.X, Hole.Min.Y), FVector2D(Hole.Max.X, Hole.Min.Y), Z0, ETOMat::MetalDark);
		}

		// Exterior walls
		const bool bGround = Fl == 0;
		const float DoorW = 180.f;
		const float DoorH = 240.f;
		const float WinSpacing = S.bWindows ? S.WinSpacing : 0.f;
		Wall(F, FVector2D(-HW, -HD), FVector2D(HW, -HD), Z0, S.FloorH, T, S.Ext,
			WindowRow(S.W, WinSpacing, S.WinW, S.WinSill, S.WinH, HW, (bGround && (S.DoorMask & 1)) ? DoorW : 0.f, DoorH));
		Wall(F, FVector2D(-HW, HD), FVector2D(HW, HD), Z0, S.FloorH, T, S.Ext,
			WindowRow(S.W, WinSpacing, S.WinW, S.WinSill, S.WinH, HW - 300.f, (bGround && (S.DoorMask & 2)) ? DoorW : 0.f, DoorH));
		Wall(F, FVector2D(-HW, -HD), FVector2D(-HW, HD), Z0, S.FloorH, T, S.Ext,
			WindowRow(S.D, WinSpacing, S.WinW, S.WinSill, S.WinH, HD, (bGround && (S.DoorMask & 4)) ? 140.f : 0.f, DoorH));
		Wall(F, FVector2D(HW, -HD), FVector2D(HW, HD), Z0, S.FloorH, T, S.Ext,
			WindowRow(S.D, WinSpacing, S.WinW, S.WinSill, S.WinH, HD - 300.f, (bGround && (S.DoorMask & 8)) ? 140.f : 0.f, DoorH));

		// Facade trim band at each floor line
		Box(F, FVector(0.f, 0.f, Z0 + S.FloorH - 10.f), FVector(S.W + 16.f, S.D + 16.f, 20.f), S.Trim, ETOCol::None);

		// Stairs to the next floor / roof
		if (bHasStairs && (Fl + 1 < S.Floors || S.bRoofAccess))
		{
			Stairs(F, FVector(StairX0, StairY, Z0), Run, Rise, StairW, ETOMat::ConcreteLight);
		}

		// Interior layout
		if (S.Layout == 0)
		{
			BuildOfficeInterior(S, F, Fl, Z0, StairX0, StairX1, Rng);
		}
		else
		{
			// Open floor: a few pillars, furniture and loot
			for (int32 p = -1; p <= 1; p += 2)
			{
				Box(F, FVector(p * HW * 0.4f, 0.f, Z0 + S.FloorH * 0.5f), FVector(40.f, 40.f, S.FloorH), S.Trim);
			}
			const FBox2D Room = MakeBox2D(-HW + T, -HD + T, StairX0 - 40.f, HD - T);
			FurnishRoom(S, F, Room, Z0, (Fl == S.SpecialRoomFloor && !S.SpecialRoomKey.IsNone()) ? 4 : Rng.RandRange(0, 2), Rng, false);
		}

		// Ceiling light
		Box(F, FVector(0.f, 0.f, Z0 + S.FloorH - 28.f), FVector(160.f, 30.f, 5.f), ETOMat::LampCold, ETOCol::None);
		if (Rng.FRand() < 0.35f)
		{
			AddLight(F.ToWorld(FVector(0.f, 0.f, Z0 + S.FloorH - 60.f)), FLinearColor(0.85f, 0.92f, 1.f), 450.f, 1100.f, true);
		}

		// Guards inside
		if (S.bGuards && S.Poi >= 0)
		{
			AddGuard(F.ToWorld(FVector(Rng.FRandRange(-HW * 0.5f, HW * 0.3f), 0.f, Z0 + 100.f)), S.Yaw + Rng.FRandRange(0.f, 360.f), S.Poi, S.Tier);
		}
	}

	// Roof
	const FBox2D RoofOuter = MakeBox2D(-HW, -HD, HW, HD);
	if (S.bGable)
	{
		const float Pitch = 28.f;
		const float PanelW = HD / FMath::Cos(FMath::DegreesToRadians(Pitch)) + 60.f;
		const float RiseR = HD * FMath::Tan(FMath::DegreesToRadians(Pitch));
		Box(F, FVector(0.f, -HD * 0.5f, TopZ + RiseR * 0.5f), FVector(S.W + 60.f, PanelW, 18.f), S.Roof, ETOCol::Solid, FRotator(0.f, 0.f, -Pitch));
		Box(F, FVector(0.f, HD * 0.5f, TopZ + RiseR * 0.5f), FVector(S.W + 60.f, PanelW, 18.f), S.Roof, ETOCol::Solid, FRotator(0.f, 0.f, Pitch));
		SlabWithHole(F, RoofOuter, Hole, TopZ, 20.f, S.Floor, false);
		// Gable ends
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			for (int32 k = 0; k < 4; ++k)
			{
				const float Wk = S.D * (1.f - (k + 0.5f) / 4.f);
				Box(F, FVector(Side * HW, 0.f, TopZ + RiseR * k / 4.f + RiseR / 8.f), FVector(T, Wk, RiseR / 4.f + 2.f), S.Ext);
			}
		}
	}
	else
	{
		SlabWithHole(F, RoofOuter, Hole, TopZ, 30.f, S.Roof, S.bRoofAccess);
		if (S.bParapet)
		{
			Wall(F, FVector2D(-HW, -HD), FVector2D(HW, -HD), TopZ, 105.f, T, S.Ext, {});
			Wall(F, FVector2D(-HW, HD), FVector2D(HW, HD), TopZ, 105.f, T, S.Ext, {});
			Wall(F, FVector2D(-HW, -HD), FVector2D(-HW, HD), TopZ, 105.f, T, S.Ext, {});
			Wall(F, FVector2D(HW, -HD), FVector2D(HW, HD), TopZ, 105.f, T, S.Ext, {});
		}
		if (S.bRoofAccess)
		{
			Railing(F, FVector2D(Hole.Min.X, Hole.Min.Y), FVector2D(Hole.Max.X, Hole.Min.Y), TopZ, ETOMat::MetalDark);
			Railing(F, FVector2D(Hole.Min.X, Hole.Min.Y), FVector2D(Hole.Min.X, Hole.Max.Y), TopZ, ETOMat::MetalDark);
		}
		// Rooftop clutter: AC units, water tank, antenna
		Box(F, FVector(-HW * 0.5f, -HD * 0.4f, TopZ + 70.f), FVector(220.f, 140.f, 140.f), ETOMat::MetalGrey);
		Box(F, FVector(-HW * 0.2f, -HD * 0.4f, TopZ + 70.f), FVector(220.f, 140.f, 140.f), ETOMat::MetalGrey);
		Cyl(F, FVector(-HW * 0.6f, HD * 0.35f, TopZ + 120.f), 200.f, 240.f, ETOMat::MetalWhite);
		Cyl(F, FVector(HW * 0.2f, HD * 0.3f, TopZ + 400.f), 8.f, 800.f, ETOMat::MetalDark, ETOCol::None);
		if (S.bRoofAccess && S.Poi >= 0 && S.bGuards)
		{
			AddGuard(F.ToWorld(FVector(-HW * 0.3f, 0.f, TopZ + 100.f)), S.Yaw, S.Poi, S.Tier, false, true);
		}
	}

	// Entrance doors (actors)
	if (S.DoorMask & 1)
	{
		const ETOMat DoorMat = S.EntranceKey.IsNone() ? ETOMat::Glass : ETOMat::MetalGrey;
		AddDoor(F.ToWorld(FVector(-45.f, -HD, 0.f)), S.Yaw, 90.f, 235.f, S.EntranceKey, DoorMat);
		AddDoor(F.ToWorld(FVector(45.f, -HD, 0.f)), S.Yaw + 180.f, 90.f, 235.f, S.EntranceKey, DoorMat);
	}
	// Entrance canopy & steps
	if (S.DoorMask & 1)
	{
		Box(F, FVector(0.f, -HD - 160.f, 300.f), FVector(500.f, 320.f, 25.f), S.Trim);
		Box(F, FVector(0.f, -HD - 120.f, -10.f), FVector(400.f, 240.f, 20.f), ETOMat::ConcreteLight);
	}
}

void ATOWorldGenerator::BuildOfficeInterior(const FTOBuildingSpec& S, const FTOFrame& F, int32 Fl, float Z0, float StairX0, float StairX1, FRandomStream& Rng)
{
	const float HW = S.W * 0.5f;
	const float HD = S.D * 0.5f;
	const float T = S.Wall;
	const float CW = 280.f;               // corridor width
	const float IntT = 15.f;
	const float H = S.FloorH;
	const float StairRoomX = StairX0 - 110.f;   // west wall of the stair room

	// Rooms along the corridor
	const float UsableW = S.W - T;
	const int32 RoomsSouth = FMath::Max(2, FMath::RoundToInt(UsableW / 650.f));
	const int32 RoomsNorth = FMath::Max(1, FMath::RoundToInt((StairRoomX + HW) / 650.f));
	const float SouthRoomW = UsableW / RoomsSouth;
	const float NorthRoomW = (StairRoomX - (-HW + T * 0.5f)) / RoomsNorth;

	const bool bSpecialFloor = (Fl == S.SpecialRoomFloor) && !S.SpecialRoomKey.IsNone();
	const int32 SpecialRoom = bSpecialFloor ? Rng.RandRange(0, RoomsSouth - 1) : -1;

	// South side rooms (-Y)
	TArray<FTOOpening> SouthCorr;
	for (int32 r = 0; r < RoomsSouth; ++r)
	{
		const float X0 = -HW + T * 0.5f + r * SouthRoomW;
		const float X1 = X0 + SouthRoomW;
		const float DoorC = (X0 + X1) * 0.5f - (-HW);
		SouthCorr.Add(FTOOpening(DoorC, 110.f, 0.f, 220.f));
		if (r > 0)
		{
			Wall(F, FVector2D(X0, -HD + T * 0.5f), FVector2D(X0, -CW * 0.5f), Z0, H, IntT, S.Int, {});
		}
		const FBox2D Room = MakeBox2D(X0 + 10.f, -HD + T * 0.5f + 10.f, X1 - 10.f, -CW * 0.5f - 10.f);
		const bool bSpecial = (r == SpecialRoom);
		FurnishRoom(S, F, Room, Z0, bSpecial ? 4 : Rng.RandRange(0, 2), Rng, bSpecial);
		const FVector DoorWorld = F.ToWorld(FVector((X0 + X1) * 0.5f, -CW * 0.5f, Z0));
		if (bSpecial)
		{
			AddDoor(DoorWorld, S.Yaw, 105.f, 215.f, S.SpecialRoomKey, ETOMat::MetalGrey);
		}
		else if (Rng.FRand() < 0.45f)
		{
			AddDoor(DoorWorld, S.Yaw, 105.f, 215.f, NAME_None, ETOMat::WoodDark);
		}
	}
	Wall(F, FVector2D(-HW, -CW * 0.5f), FVector2D(HW, -CW * 0.5f), Z0, H, IntT, S.Int, SouthCorr);

	// North side rooms (+Y), stair room at the +X end
	TArray<FTOOpening> NorthCorr;
	for (int32 r = 0; r < RoomsNorth; ++r)
	{
		const float X0 = -HW + T * 0.5f + r * NorthRoomW;
		const float X1 = X0 + NorthRoomW;
		NorthCorr.Add(FTOOpening((X0 + X1) * 0.5f - (-HW), 110.f, 0.f, 220.f));
		if (r > 0)
		{
			Wall(F, FVector2D(X0, CW * 0.5f), FVector2D(X0, HD - T * 0.5f), Z0, H, IntT, S.Int, {});
		}
		const FBox2D Room = MakeBox2D(X0 + 10.f, CW * 0.5f + 10.f, X1 - 10.f, HD - T * 0.5f - 10.f);
		FurnishRoom(S, F, Room, Z0, Rng.RandRange(0, 3), Rng, false);
		if (Rng.FRand() < 0.4f)
		{
			AddDoor(F.ToWorld(FVector((X0 + X1) * 0.5f, CW * 0.5f, Z0)), S.Yaw, 105.f, 215.f, NAME_None, ETOMat::WoodDark);
		}
	}
	// Stair room: wide opening towards the corridor
	NorthCorr.Add(FTOOpening(StairRoomX + (HW - StairRoomX) * 0.5f - (-HW), FMath::Min(320.f, (HW - StairRoomX) - 120.f), 0.f, 240.f));
	Wall(F, FVector2D(StairRoomX, CW * 0.5f), FVector2D(StairRoomX, HD - T * 0.5f), Z0, H, IntT, S.Int, {});
	Wall(F, FVector2D(-HW, CW * 0.5f), FVector2D(HW, CW * 0.5f), Z0, H, IntT, S.Int, NorthCorr);

	// Corridor details: lights, fire extinguisher boxes, a notice board
	for (float X = -HW + 400.f; X < HW - 300.f; X += 700.f)
	{
		Box(F, FVector(X, 0.f, Z0 + H - 28.f), FVector(120.f, 30.f, 5.f), ETOMat::LampCold, ETOCol::None);
	}
	Box(F, FVector(-HW + 200.f, -CW * 0.5f + 12.f, Z0 + 120.f), FVector(20.f, 12.f, 45.f), ETOMat::MetalRed, ETOCol::None);
	if (Rng.FRand() < 0.5f)
	{
		AddLoot(F.ToWorld(FVector(Rng.FRandRange(-HW * 0.6f, HW * 0.4f), CW * 0.5f - 40.f, Z0)), S.Yaw + 180.f, ETOContainerType::Duffel, S.Tier, S.Poi);
	}
}

void ATOWorldGenerator::FurnishRoom(const FTOBuildingSpec& S, const FTOFrame& F, const FBox2D& Room, float Z, int32 RoomType, FRandomStream& Rng, bool bSpecial)
{
	const FVector2D C = Room.GetCenter();
	const FVector2D Ext = Room.GetExtent();
	if (Ext.X < 120.f || Ext.Y < 120.f)
	{
		return;
	}
	const int32 Tier = S.Tier;
	switch (RoomType)
	{
	case 0: // office: desk, chair, computer, cabinet
	{
		const float DX = C.X + Rng.FRandRange(-Ext.X * 0.3f, Ext.X * 0.3f);
		Box(F, FVector(DX, C.Y, Z + 38.f), FVector(160.f, 80.f, 6.f), ETOMat::Wood);
		Box(F, FVector(DX - 70.f, C.Y, Z + 18.f), FVector(6.f, 74.f, 36.f), ETOMat::MetalDark, ETOCol::None);
		Box(F, FVector(DX + 70.f, C.Y, Z + 18.f), FVector(6.f, 74.f, 36.f), ETOMat::MetalDark, ETOCol::None);
		Box(F, FVector(DX, C.Y + 10.f, Z + 66.f), FVector(55.f, 8.f, 40.f), ETOMat::Black, ETOCol::None);
		Box(F, FVector(DX, C.Y + 6.f, Z + 66.f), FVector(50.f, 1.f, 34.f), ETOMat::Screen, ETOCol::None);
		Box(F, FVector(DX, C.Y - 70.f, Z + 25.f), FVector(50.f, 50.f, 50.f), ETOMat::Fabric);
		if (Rng.FRand() < 0.65f)
		{
			AddLoot(F.ToWorld(FVector(DX + 105.f, C.Y, Z)), F.Yaw + 90.f, ETOContainerType::ComputerTower, Tier, S.Poi);
		}
		if (Rng.FRand() < 0.45f)
		{
			AddLoot(F.ToWorld(FVector(Room.Min.X + 35.f, Room.Max.Y - 35.f, Z)), F.Yaw + 180.f, ETOContainerType::FilingCabinet, Tier, S.Poi);
		}
		break;
	}
	case 1: // meeting room
	{
		Box(F, FVector(C.X, C.Y, Z + 40.f), FVector(FMath::Min(Ext.X * 1.3f, 320.f), 110.f, 8.f), ETOMat::WoodDark);
		Box(F, FVector(C.X, C.Y, Z + 18.f), FVector(30.f, 30.f, 36.f), ETOMat::MetalDark, ETOCol::None);
		for (int32 i = -1; i <= 1; ++i)
		{
			Box(F, FVector(C.X + i * 90.f, C.Y - 90.f, Z + 25.f), FVector(45.f, 45.f, 50.f), ETOMat::Fabric);
			Box(F, FVector(C.X + i * 90.f, C.Y + 90.f, Z + 25.f), FVector(45.f, 45.f, 50.f), ETOMat::Fabric);
		}
		Box(F, FVector(Room.Min.X + 5.f, C.Y, Z + 150.f), FVector(4.f, 160.f, 90.f), ETOMat::MetalWhite, ETOCol::None);
		if (Rng.FRand() < 0.3f)
		{
			AddLoot(F.ToWorld(FVector(Room.Max.X - 40.f, Room.Min.Y + 40.f, Z)), F.Yaw, ETOContainerType::Duffel, Tier, S.Poi);
		}
		break;
	}
	case 2: // storage
	{
		for (int32 i = 0; i < 3; ++i)
		{
			const float X = Room.Min.X + 50.f + i * 110.f;
			if (X > Room.Max.X - 50.f) break;
			Box(F, FVector(X, Room.Max.Y - 35.f, Z + 100.f), FVector(100.f, 50.f, 200.f), ETOMat::MetalGrey);
		}
		Box(F, FVector(C.X, C.Y, Z + 40.f), FVector(80.f, 80.f, 80.f), ETOMat::Wood);
		const float R = Rng.FRand();
		AddLoot(F.ToWorld(FVector(Room.Min.X + 50.f, Room.Min.Y + 40.f, Z)), F.Yaw, R < 0.3f ? ETOContainerType::Toolbox : (R < 0.55f ? ETOContainerType::AmmoBox : (R < 0.75f ? ETOContainerType::MedicalCase : ETOContainerType::Locker)), Tier, S.Poi);
		break;
	}
	case 3: // server room
	{
		const int32 Racks = FMath::Clamp(FMath::FloorToInt(Ext.X * 2.f / 90.f) - 1, 1, 4);
		for (int32 i = 0; i < Racks; ++i)
		{
			const FVector P(Room.Min.X + 60.f + i * 90.f, Room.Max.Y - 50.f, Z);
			if (i % 2 == 0 && Rng.FRand() < 0.6f)
			{
				AddLoot(F.ToWorld(P), F.Yaw + 180.f, ETOContainerType::ServerRack, Tier, S.Poi);
			}
			else
			{
				Box(F, P + FVector(0.f, 0.f, 100.f), FVector(62.f, 80.f, 200.f), ETOMat::MetalDark);
				Box(F, P + FVector(0.f, -41.f, 120.f), FVector(40.f, 1.f, 2.f), ETOMat::LampGreen, ETOCol::None);
			}
		}
		Box(F, FVector(C.X, Room.Min.Y + 40.f, Z + 40.f), FVector(150.f, 60.f, 80.f), ETOMat::MetalGrey);
		break;
	}
	default: // 4 special / director's office / vault
	{
		Box(F, FVector(C.X, C.Y + 40.f, Z + 40.f), FVector(220.f, 100.f, 8.f), ETOMat::WoodDark);
		Box(F, FVector(C.X, C.Y + 40.f, Z + 20.f), FVector(200.f, 90.f, 36.f), ETOMat::WoodDark);
		Box(F, FVector(C.X, C.Y + 130.f, Z + 30.f), FVector(60.f, 60.f, 70.f), ETOMat::Fabric);
		Box(F, FVector(Room.Min.X + 20.f, C.Y, Z + 110.f), FVector(30.f, 200.f, 220.f), ETOMat::WoodDark);
		Box(F, FVector(C.X - 60.f, Room.Min.Y + 60.f, Z + 30.f), FVector(200.f, 80.f, 60.f), ETOMat::Fabric);
		AddLoot(F.ToWorld(FVector(Room.Max.X - 45.f, Room.Max.Y - 45.f, Z)), F.Yaw + 180.f, bSpecial ? S.SpecialLoot : ETOContainerType::Safe, Tier + (bSpecial ? 1 : 0), S.Poi);
		if (bSpecial)
		{
			AddLoot(F.ToWorld(FVector(Room.Max.X - 60.f, Room.Min.Y + 50.f, Z)), F.Yaw, ETOContainerType::HighValueCrate, Tier + 1, S.Poi);
			AddLoot(F.ToWorld(FVector(Room.Min.X + 50.f, Room.Max.Y - 40.f, Z)), F.Yaw + 180.f, ETOContainerType::Safe, Tier + 1, S.Poi);
		}
		else if (Rng.FRand() < 0.5f)
		{
			AddLoot(F.ToWorld(FVector(Room.Min.X + 50.f, Room.Max.Y - 40.f, Z)), F.Yaw + 180.f, ETOContainerType::FilingCabinet, Tier, S.Poi);
		}
		break;
	}
	}
}

// =============================================================================================
//  House (1-2 floors, gable roof)
// =============================================================================================

void ATOWorldGenerator::BuildHouse(const FVector& Center, float Yaw, int32 Floors, ETOMat WallMat, int32 Poi, int32 Tier, FRandomStream& Rng)
{
	const FTOFrame F(Center, Yaw);
	const float W = Rng.FRandRange(900.f, 1150.f);
	const float D = Rng.FRandRange(750.f, 900.f);
	const float HW = W * 0.5f;
	const float HD = D * 0.5f;
	const float T = 22.f;
	const float FH = 320.f;
	const float TopZ = Floors * FH;

	FBox2D Fp(ForceInit);
	for (int32 c = 0; c < 4; ++c)
	{
		const FVector P = F.ToWorld(FVector((c & 1) ? HW : -HW, (c & 2) ? HD : -HD, 0.f));
		Fp += FVector2D(P.X, P.Y);
	}
	Footprints.Add(Fp);

	Box(F, FVector(0.f, 0.f, -150.f), FVector(W + 30.f, D + 30.f, 300.f), ETOMat::ConcreteDark);

	const float Run = 460.f;
	const float StairW = 100.f;
	const float SX0 = -HW + T * 0.5f + 40.f;
	const float SY = HD - T * 0.5f - StairW * 0.5f;
	const FBox2D Hole = MakeBox2D(SX0 + Run * 0.42f, HD - T * 0.5f - StairW - 6.f, SX0 + Run + 15.f, HD - T * 0.5f);
	const float DividerX = SX0 + Run + 100.f;

	for (int32 Fl = 0; Fl < Floors; ++Fl)
	{
		const float Z0 = Fl * FH;
		SlabWithHole(F, MakeBox2D(-HW, -HD, HW, HD), Hole, Z0, 20.f, Fl == 0 ? ETOMat::Tile : ETOMat::FloorWood, Fl > 0);
		const bool bG = Fl == 0;
		Wall(F, FVector2D(-HW, -HD), FVector2D(HW, -HD), Z0, FH, T, WallMat, WindowRow(W, 300.f, 110.f, 95.f, 120.f, HW * 0.5f, bG ? 110.f : 0.f, 215.f));
		Wall(F, FVector2D(-HW, HD), FVector2D(HW, HD), Z0, FH, T, WallMat, WindowRow(W, 340.f, 110.f, 95.f, 120.f, W * 0.8f, (bG && Rng.FRand() < 0.5f) ? 100.f : 0.f, 210.f));
		Wall(F, FVector2D(-HW, -HD), FVector2D(-HW, HD), Z0, FH, T, WallMat, WindowRow(D, 320.f, 110.f, 95.f, 120.f, 0.f, 0.f, 0.f));
		Wall(F, FVector2D(HW, -HD), FVector2D(HW, HD), Z0, FH, T, WallMat, WindowRow(D, 320.f, 110.f, 95.f, 120.f, 0.f, 0.f, 0.f));
		// Divider between the two rooms
		Wall(F, FVector2D(DividerX, -HD), FVector2D(DividerX, HD), Z0, FH, 14.f, ETOMat::Plaster, { FTOOpening(HD - 160.f, 100.f, 0.f, 210.f) });

		if (Floors > 1 && Fl + 1 < Floors)
		{
			Stairs(F, FVector(SX0, SY, Z0), Run, FH, StairW, ETOMat::WoodDark);
		}
		if (Fl > 0)
		{
			Railing(F, FVector2D(Hole.Min.X, Hole.Min.Y), FVector2D(Hole.Max.X, Hole.Min.Y), Z0, ETOMat::WoodDark);
		}

		// Furniture
		const FBox2D Left = MakeBox2D(-HW + T, -HD + T, DividerX - 10.f, HD - T - 110.f);
		const FBox2D Right = MakeBox2D(DividerX + 10.f, -HD + T, HW - T, HD - T);
		if (Fl == 0)
		{
			// living room / kitchen
			Box(F, FVector(Left.GetCenter().X, Left.GetCenter().Y, Z0 + 38.f), FVector(140.f, 80.f, 6.f), ETOMat::Wood);
			Box(F, FVector(Left.GetCenter().X, Left.GetCenter().Y, Z0 + 18.f), FVector(10.f, 10.f, 36.f), ETOMat::WoodDark, ETOCol::None);
			Box(F, FVector(Right.Max.X - 35.f, Right.GetCenter().Y, Z0 + 45.f), FVector(60.f, 260.f, 90.f), ETOMat::MetalWhite);
			Box(F, FVector(Right.GetCenter().X, Right.Min.Y + 45.f, Z0 + 45.f), FVector(200.f, 80.f, 90.f), ETOMat::Fabric);
		}
		else
		{
			// bedroom
			Box(F, FVector(Right.GetCenter().X, Right.GetCenter().Y, Z0 + 30.f), FVector(200.f, 150.f, 50.f), ETOMat::Fabric);
			Box(F, FVector(Right.Max.X - 30.f, Right.GetCenter().Y, Z0 + 60.f), FVector(40.f, 150.f, 100.f), ETOMat::WoodDark);
		}
		const float R = Rng.FRand();
		const ETOContainerType LootType = R < 0.35f ? ETOContainerType::Duffel : (R < 0.55f ? ETOContainerType::Locker : (R < 0.75f ? ETOContainerType::Toolbox : (R < 0.88f ? ETOContainerType::MedicalCase : ETOContainerType::Safe)));
		AddLoot(F.ToWorld(FVector(Right.Max.X - 40.f, Right.Min.Y + 40.f, Z0)), Yaw, LootType, Tier, Poi);
		if (Rng.FRand() < 0.35f)
		{
			AddLoot(F.ToWorld(FVector(Left.Min.X + 40.f, Left.Min.Y + 40.f, Z0)), Yaw, ETOContainerType::Duffel, Tier, Poi);
		}
		Box(F, FVector(0.f, 0.f, Z0 + FH - 22.f), FVector(50.f, 50.f, 5.f), ETOMat::LampWarm, ETOCol::None);
	}

	// Gable roof
	const float Pitch = 32.f;
	const float RiseR = HD * FMath::Tan(FMath::DegreesToRadians(Pitch));
	const float PanelW = HD / FMath::Cos(FMath::DegreesToRadians(Pitch)) + 50.f;
	const ETOMat RoofMat = Rng.FRand() < 0.5f ? ETOMat::Brick : ETOMat::MetalRust;
	SlabWithHole(F, MakeBox2D(-HW, -HD, HW, HD), Hole, TopZ, 20.f, ETOMat::WoodDark, false);
	Box(F, FVector(0.f, -HD * 0.5f, TopZ + RiseR * 0.5f + 10.f), FVector(W + 70.f, PanelW, 16.f), RoofMat, ETOCol::Solid, FRotator(0.f, 0.f, -Pitch));
	Box(F, FVector(0.f, HD * 0.5f, TopZ + RiseR * 0.5f + 10.f), FVector(W + 70.f, PanelW, 16.f), RoofMat, ETOCol::Solid, FRotator(0.f, 0.f, Pitch));
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		for (int32 k = 0; k < 4; ++k)
		{
			const float Wk = D * (1.f - (k + 0.5f) / 4.f);
			Box(F, FVector(Side * HW, 0.f, TopZ + RiseR * (k + 0.5f) / 4.f), FVector(T, Wk, RiseR / 4.f + 2.f), WallMat);
		}
	}
	// Chimney
	Box(F, FVector(HW * 0.5f, HD * 0.3f, TopZ + RiseR + 40.f), FVector(60.f, 60.f, 220.f), ETOMat::Brick);
	// Front door
	AddDoor(F.ToWorld(FVector(-HW * 0.5f, -HD, 0.f)), Yaw, 105.f, 210.f, NAME_None, ETOMat::WoodDark);
	if (Poi >= 0 && Rng.FRand() < 0.6f)
	{
		AddGuard(F.ToWorld(FVector(0.f, -HD - 250.f, 50.f)), Yaw + 180.f, Poi, Tier);
	}
}

// =============================================================================================
//  Military barracks
// =============================================================================================

void ATOWorldGenerator::BuildBarracks(const FVector& Center, float Yaw, int32 Poi, int32 Tier, FRandomStream& Rng)
{
	const FTOFrame F(Center, Yaw);
	const float W = 2800.f;
	const float D = 1000.f;
	const float HW = W * 0.5f;
	const float HD = D * 0.5f;
	const float T = 25.f;
	const float H = 340.f;

	FBox2D Fp(ForceInit);
	for (int32 c = 0; c < 4; ++c)
	{
		const FVector P = F.ToWorld(FVector((c & 1) ? HW : -HW, (c & 2) ? HD : -HD, 0.f));
		Fp += FVector2D(P.X, P.Y);
	}
	Footprints.Add(Fp);

	Box(F, FVector(0.f, 0.f, -140.f), FVector(W + 30.f, D + 30.f, 300.f), ETOMat::ConcreteDark);
	Box(F, FVector(0.f, 0.f, -5.f), FVector(W, D, 10.f), ETOMat::Concrete);
	const ETOMat Ext = ETOMat::PlasterBeige;
	Wall(F, FVector2D(-HW, -HD), FVector2D(HW, -HD), 0.f, H, T, Ext, WindowRow(W, 450.f, 120.f, 120.f, 100.f, HW, 160.f, 230.f));
	Wall(F, FVector2D(-HW, HD), FVector2D(HW, HD), 0.f, H, T, Ext, WindowRow(W, 450.f, 120.f, 120.f, 100.f, 0.f, 0.f, 0.f));
	Wall(F, FVector2D(-HW, -HD), FVector2D(-HW, HD), 0.f, H, T, Ext, { FTOOpening(HD, 140.f, 0.f, 230.f) });
	Wall(F, FVector2D(HW, -HD), FVector2D(HW, HD), 0.f, H, T, Ext, { FTOOpening(HD, 140.f, 0.f, 230.f) });
	// Office at +X end
	const float OfficeX = HW - 420.f;
	Wall(F, FVector2D(OfficeX, -HD), FVector2D(OfficeX, HD), 0.f, H, 15.f, ETOMat::Plaster, { FTOOpening(HD, 110.f, 0.f, 215.f) });
	AddDoor(F.ToWorld(FVector(OfficeX, 0.f, 0.f)), Yaw + 90.f, 105.f, 210.f, NAME_None, ETOMat::WoodDark);
	Box(F, FVector(HW - 200.f, -HD + 120.f, 38.f), FVector(160.f, 80.f, 6.f), ETOMat::WoodDark);
	AddLoot(F.ToWorld(FVector(HW - 60.f, HD - 60.f, 0.f)), Yaw + 180.f, ETOContainerType::FilingCabinet, Tier, Poi);
	AddLoot(F.ToWorld(FVector(HW - 200.f, -HD + 60.f, 0.f)), Yaw, ETOContainerType::ComputerTower, Tier, Poi);

	// Bunk beds and lockers
	for (float X = -HW + 220.f; X < OfficeX - 150.f; X += 300.f)
	{
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const float Y = Side * (HD - 70.f);
			Box(F, FVector(X, Y, 35.f), FVector(200.f, 90.f, 20.f), ETOMat::Fabric);
			Box(F, FVector(X, Y, 150.f), FVector(200.f, 90.f, 20.f), ETOMat::Fabric);
			for (int32 c = 0; c < 4; ++c)
			{
				Box(F, FVector(X + ((c & 1) ? 95.f : -95.f), Y + ((c & 2) ? 42.f : -42.f), 90.f), FVector(6.f, 6.f, 180.f), ETOMat::MetalGreen, ETOCol::None);
			}
			if (Rng.FRand() < 0.4f)
			{
				AddLoot(F.ToWorld(FVector(X + 140.f, Side * (HD - 40.f), 0.f)), Yaw + (Side > 0 ? 180.f : 0.f), ETOContainerType::Locker, Tier, Poi);
			}
		}
	}
	Box(F, FVector(-HW * 0.3f, 0.f, 40.f), FVector(240.f, 90.f, 8.f), ETOMat::Wood);
	if (Rng.FRand() < 0.6f)
	{
		AddLoot(F.ToWorld(FVector(-HW * 0.3f + 160.f, 0.f, 0.f)), Yaw, ETOContainerType::AmmoBox, Tier, Poi);
	}

	// Shallow pitched metal roof
	const float Pitch = 10.f;
	const float RiseR = HD * FMath::Tan(FMath::DegreesToRadians(Pitch));
	const float PanelW = HD / FMath::Cos(FMath::DegreesToRadians(Pitch)) + 60.f;
	Box(F, FVector(0.f, -HD * 0.5f, H + RiseR * 0.5f), FVector(W + 80.f, PanelW, 12.f), ETOMat::MetalGreen, ETOCol::Solid, FRotator(0.f, 0.f, -Pitch));
	Box(F, FVector(0.f, HD * 0.5f, H + RiseR * 0.5f), FVector(W + 80.f, PanelW, 12.f), ETOMat::MetalGreen, ETOCol::Solid, FRotator(0.f, 0.f, Pitch));
	Box(F, FVector(0.f, 0.f, H - 6.f), FVector(W, D, 12.f), ETOMat::Plaster, ETOCol::None);
	for (float X = -HW + 400.f; X < HW; X += 800.f)
	{
		Box(F, FVector(X, 0.f, H - 15.f), FVector(120.f, 30.f, 5.f), ETOMat::LampCold, ETOCol::None);
	}
	AddLight(F.ToWorld(FVector(0.f, 0.f, H - 50.f)), FLinearColor(0.9f, 0.95f, 1.f), 500.f, 1600.f, true);
	AddDoor(F.ToWorld(FVector(0.f, -HD, 0.f)), Yaw, 150.f, 225.f, NAME_None, ETOMat::MetalGreen);
	AddGuard(F.ToWorld(FVector(-HW * 0.5f, 0.f, 100.f)), Yaw + 90.f, Poi, Tier);
}

// =============================================================================================
//  Warehouse with racks and a locked mezzanine office
// =============================================================================================

void ATOWorldGenerator::BuildWarehouse(const FVector& Center, float Yaw, float W, float D, int32 Poi, int32 Tier, FName OfficeKey, FRandomStream& Rng)
{
	const FTOFrame F(Center, Yaw);
	const float HW = W * 0.5f;
	const float HD = D * 0.5f;
	const float T = 30.f;
	const float H = 950.f;

	FBox2D Fp(ForceInit);
	for (int32 c = 0; c < 4; ++c)
	{
		const FVector P = F.ToWorld(FVector((c & 1) ? HW : -HW, (c & 2) ? HD : -HD, 0.f));
		Fp += FVector2D(P.X, P.Y);
	}
	Footprints.Add(Fp);

	Box(F, FVector(0.f, 0.f, -140.f), FVector(W + 30.f, D + 30.f, 300.f), ETOMat::ConcreteDark);
	Box(F, FVector(0.f, 0.f, -5.f), FVector(W, D, 10.f), ETOMat::Concrete);

	const ETOMat Ext = Rng.FRand() < 0.5f ? ETOMat::MetalBlue : ETOMat::Aluminium;
	// Lower concrete band + corrugated upper walls with clerestory windows
	auto WallSet = [&](const FVector2D& A, const FVector2D& B, float Len, TArray<FTOOpening> Doors)
	{
		TArray<FTOOpening> Lower = Doors;
		Wall(F, A, B, 0.f, 250.f, T, ETOMat::Concrete, Lower);
		TArray<FTOOpening> Upper;
		for (const FTOOpening& O : Doors)
		{
			if (O.Top > 250.f)
			{
				Upper.Add(FTOOpening(O.Center, O.Width, 0.f, O.Top - 250.f));
			}
		}
		for (float C = 300.f; C < Len - 300.f; C += 600.f)
		{
			bool bClash = false;
			for (const FTOOpening& O : Doors)
			{
				bClash |= FMath::Abs(O.Center - C) < O.Width * 0.5f + 200.f;
			}
			if (!bClash)
			{
				Upper.Add(FTOOpening(C, 260.f, 480.f, 600.f));
			}
		}
		Wall(F, A, B, 250.f, H - 250.f, T * 0.6f, Ext, Upper);
	};
	WallSet(FVector2D(-HW, -HD), FVector2D(HW, -HD), W, { FTOOpening(W * 0.3f, 520.f, 0.f, 480.f), FTOOpening(W * 0.75f, 120.f, 0.f, 220.f) });
	WallSet(FVector2D(-HW, HD), FVector2D(HW, HD), W, { FTOOpening(W * 0.6f, 520.f, 0.f, 480.f) });
	WallSet(FVector2D(-HW, -HD), FVector2D(-HW, HD), D, {});
	WallSet(FVector2D(HW, -HD), FVector2D(HW, HD), D, { FTOOpening(D * 0.5f, 120.f, 0.f, 220.f) });

	// Roof with skylights
	Box(F, FVector(0.f, 0.f, H + 15.f), FVector(W + 60.f, D + 60.f, 30.f), ETOMat::MetalGrey);
	for (float X = -HW + 600.f; X < HW - 400.f; X += 900.f)
	{
		Box(F, FVector(X, 0.f, H + 32.f), FVector(300.f, D * 0.6f, 6.f), ETOMat::Glass, ETOCol::None);
		Box(F, FVector(X, 0.f, H - 20.f), FVector(150.f, 40.f, 6.f), ETOMat::LampCold, ETOCol::None);
	}
	// Columns
	for (float X = -HW + 600.f; X < HW - 300.f; X += 900.f)
	{
		Box(F, FVector(X, -HD * 0.33f, H * 0.5f), FVector(30.f, 30.f, H), ETOMat::MetalYellow);
		Box(F, FVector(X, HD * 0.33f, H * 0.5f), FVector(30.f, 30.f, H), ETOMat::MetalYellow);
	}

	// Pallet racks
	const float OfficeLen = 700.f;
	for (int32 Row = -1; Row <= 1; ++Row)
	{
		const float Y = Row * HD * 0.45f;
		for (float X = -HW + OfficeLen + 200.f; X < HW - 300.f; X += 280.f)
		{
			for (int32 c = 0; c < 4; ++c)
			{
				Box(F, FVector(X + ((c & 1) ? 120.f : -120.f), Y + ((c & 2) ? 45.f : -45.f), 300.f), FVector(10.f, 10.f, 600.f), ETOMat::MetalBlue, ETOCol::None);
			}
			for (int32 Lvl = 0; Lvl < 3; ++Lvl)
			{
				const float Z = 10.f + Lvl * 200.f;
				Box(F, FVector(X, Y, Z), FVector(250.f, 100.f, 8.f), ETOMat::MetalYellow);
				if (Rng.FRand() < 0.7f)
				{
					Box(F, FVector(X + Rng.FRandRange(-50.f, 50.f), Y, Z + 55.f), FVector(110.f, 90.f, 100.f), Rng.FRand() < 0.5f ? ETOMat::Wood : ETOMat::Canvas);
				}
			}
			if (Rng.FRand() < 0.25f)
			{
				const float R = Rng.FRand();
				AddLoot(F.ToWorld(FVector(X, Y + (Row >= 0 ? 110.f : -110.f), 0.f)), Yaw, R < 0.3f ? ETOContainerType::WeaponCrate : (R < 0.6f ? ETOContainerType::Duffel : ETOContainerType::Toolbox), Tier, Poi);
			}
		}
	}

	// Mezzanine office at the -X end (walkway in front, locked office behind)
	const float MZ = 340.f;
	const float OX0 = -HW + T * 0.5f;
	const float OX1 = -HW + OfficeLen;
	const float OfficeFront = OX1 - 250.f;
	Box(F, FVector((OX0 + OX1) * 0.5f, 0.f, MZ - 12.f), FVector(OX1 - OX0, D - T, 25.f), ETOMat::MetalGrey);
	for (int32 c = 0; c < 3; ++c)
	{
		Box(F, FVector(OX1 - 20.f, -HD * 0.6f + c * HD * 0.6f, MZ * 0.5f), FVector(25.f, 25.f, MZ), ETOMat::MetalYellow);
	}
	const float MRun = MZ / FMath::Tan(FMath::DegreesToRadians(35.f));
	Stairs(F.Sub(FVector(OX1 + MRun, -HD + T + 80.f, 0.f), 180.f), FVector::ZeroVector, MRun, MZ, 120.f, ETOMat::MetalGrey);
	Railing(F, FVector2D(OX1, -HD + 160.f), FVector2D(OX1, HD - T), MZ, ETOMat::MetalYellow);
	Wall(F, FVector2D(OfficeFront, -HD + T * 0.5f), FVector2D(OfficeFront, HD - T * 0.5f), MZ, 280.f, 12.f, ETOMat::Plaster,
		{ FTOOpening(D * 0.5f, 110.f, 0.f, 215.f), FTOOpening(D * 0.78f, 260.f, 100.f, 200.f) });
	AddDoor(F.ToWorld(FVector(OfficeFront, -HD + T * 0.5f + D * 0.5f, MZ)), Yaw + 90.f, 105.f, 210.f, OfficeKey, ETOMat::WoodDark);
	{
		FTOBuildingSpec Fake;
		Fake.Tier = Tier + (OfficeKey.IsNone() ? 0 : 1);
		Fake.Poi = Poi;
		Fake.SpecialLoot = ETOContainerType::Safe;
		FurnishRoom(Fake, F, MakeBox2D(OX0 + 20.f, -HD + 40.f, OfficeFront - 20.f, HD - 40.f), MZ, OfficeKey.IsNone() ? 0 : 4, Rng, !OfficeKey.IsNone());
	}

	// Forklift and pallets
	BuildVehicle(F.ToWorld(FVector(HW * 0.3f, -HD * 0.75f, 0.f)), Yaw + 90.f, 4, Rng);
	BuildCrateCluster(F.ToWorld(FVector(HW - 300.f, HD - 250.f, 0.f)), Yaw, Rng);
	AddLight(F.ToWorld(FVector(0.f, 0.f, H - 100.f)), FLinearColor(1.f, 0.93f, 0.8f), 800.f, 2600.f, true);
	AddGuard(F.ToWorld(FVector(0.f, 0.f, 100.f)), Yaw, Poi, Tier);
	AddGuard(F.ToWorld(FVector((OfficeFront + OX1) * 0.5f, 0.f, MZ + 100.f)), Yaw, Poi, Tier, false, true);
}

// =============================================================================================
//  Small structures & props
// =============================================================================================

void ATOWorldGenerator::BuildWatchtower(const FVector& Base, float Yaw, int32 Poi, int32 Tier)
{
	const FTOFrame F(Base, Yaw);
	const float PZ = 420.f;
	for (int32 c = 0; c < 4; ++c)
	{
		Box(F, FVector((c & 1) ? 140.f : -140.f, (c & 2) ? 140.f : -140.f, PZ * 0.5f), FVector(22.f, 22.f, PZ + 200.f), ETOMat::WoodDark);
	}
	Box(F, FVector(0.f, 0.f, PZ - 10.f), FVector(320.f, 320.f, 20.f), ETOMat::Wood);
	// Parapet (sandbags / planks) leaving the stair opening on -Y
	Box(F, FVector(0.f, 155.f, PZ + 55.f), FVector(320.f, 10.f, 110.f), ETOMat::Sandbag);
	Box(F, FVector(155.f, 0.f, PZ + 55.f), FVector(10.f, 320.f, 110.f), ETOMat::Sandbag);
	Box(F, FVector(-155.f, 0.f, PZ + 55.f), FVector(10.f, 320.f, 110.f), ETOMat::Sandbag);
	Box(F, FVector(60.f, -155.f, PZ + 55.f), FVector(200.f, 10.f, 110.f), ETOMat::Sandbag);
	// Roof
	Box(F, FVector(0.f, 0.f, PZ + 260.f), FVector(380.f, 380.f, 15.f), ETOMat::MetalRust);
	// Stairs up to the platform along -Y side
	const float Run = PZ / FMath::Tan(FMath::DegreesToRadians(38.f));
	Stairs(F.Sub(FVector(-100.f, -160.f - Run, 0.f), 90.f), FVector::ZeroVector, Run, PZ, 100.f, ETOMat::Wood);
	// Search light
	Cyl(F, FVector(100.f, 100.f, PZ + 130.f), 50.f, 40.f, ETOMat::LampWarm, ETOCol::None, FRotator(80.f, 0.f, 0.f));
	AddLight(F.ToWorld(FVector(0.f, 0.f, PZ + 200.f)), FLinearColor(1.f, 0.85f, 0.6f), 900.f, 2200.f, true);
	if (Poi >= 0)
	{
		AddGuard(F.ToWorld(FVector(0.f, 0.f, PZ + 100.f)), Yaw, Poi, Tier, false, true);
	}
	FBox2D Fp(ForceInit);
	Fp += FVector2D(Base.X - 200.f, Base.Y - 200.f);
	Fp += FVector2D(Base.X + 200.f, Base.Y + 200.f);
	Footprints.Add(Fp);
}

void ATOWorldGenerator::BuildBridge(const FVector& A, const FVector& B, float Width, ETOMat Mat)
{
	const FVector Dir = (B - A);
	const float Len = Dir.Size2D();
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Dir.Z, Len));
	const FTOFrame F((A + B) * 0.5f, Yaw);
	// Deck
	Box(F, FVector(0.f, 0.f, -35.f), FVector(Len + 200.f, Width, 70.f), Mat, ETOCol::Solid, FRotator(Pitch, 0.f, 0.f));
	Box(F, FVector(0.f, 0.f, 2.f), FVector(Len + 200.f, Width - 200.f, 6.f), ETOMat::Asphalt, ETOCol::None, FRotator(Pitch, 0.f, 0.f));
	// Railings / parapets
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		Box(F, FVector(0.f, Side * (Width * 0.5f - 15.f), 50.f), FVector(Len + 200.f, 30.f, 100.f), ETOMat::ConcreteLight, ETOCol::Solid, FRotator(Pitch, 0.f, 0.f));
	}
	// Piers
	const int32 Piers = FMath::Max(1, FMath::FloorToInt(Len / 2500.f));
	for (int32 i = 1; i <= Piers; ++i)
	{
		const float X = -Len * 0.5f + Len * i / (Piers + 1);
		Box(F, FVector(X, 0.f, -800.f), FVector(200.f, Width * 0.6f, 1500.f), ETOMat::ConcreteDark);
	}
	// Steel truss arches
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		for (int32 k = 0; k < 8; ++k)
		{
			const float X = -Len * 0.5f + Len * (k + 0.5f) / 8.f;
			const float Hh = 120.f + 380.f * FMath::Sin(PI * (k + 0.5f) / 8.f);
			Box(F, FVector(X, Side * (Width * 0.5f + 10.f), Hh * 0.5f + 60.f), FVector(20.f, 20.f, Hh), ETOMat::MetalGreen, ETOCol::None);
		}
		Box(F, FVector(0.f, Side * (Width * 0.5f + 10.f), 520.f), FVector(Len * 0.75f, 25.f, 25.f), ETOMat::MetalGreen, ETOCol::None);
	}
	BridgeEnds.Add(A);
	BridgeEnds.Add(B);
}

void ATOWorldGenerator::BuildContainerStack(const FVector& Base, float Yaw, int32 Count, FRandomStream& Rng, bool bLootable, int32 Poi, int32 Tier)
{
	const FTOFrame F(Base, Yaw);
	static const ETOMat Colors[] = { ETOMat::MetalRed, ETOMat::MetalBlue, ETOMat::MetalGreen, ETOMat::MetalRust, ETOMat::MetalYellow, ETOMat::MetalWhite };
	for (int32 i = 0; i < Count; ++i)
	{
		const int32 Col = i % 3;
		const int32 Lvl = i / 3;
		const FVector C(0.f, Col * 260.f, 130.f + Lvl * 262.f);
		const ETOMat M = Colors[Rng.RandRange(0, 5)];
		if (bLootable && Lvl == 0 && i == 0)
		{
			// Open container with loot inside
			Box(F, C + FVector(0.f, 0.f, 125.f), FVector(610.f, 244.f, 10.f), M);
			Box(F, C - FVector(0.f, 0.f, 125.f), FVector(610.f, 244.f, 10.f), M);
			Box(F, C + FVector(0.f, 117.f, 0.f), FVector(610.f, 10.f, 259.f), M);
			Box(F, C - FVector(0.f, 117.f, 0.f), FVector(610.f, 10.f, 259.f), M);
			Box(F, C + FVector(300.f, 0.f, 0.f), FVector(10.f, 244.f, 259.f), M);
			Box(F, C + FVector(-330.f, 90.f, 0.f), FVector(10.f, 240.f, 259.f), M, ETOCol::Solid, FRotator(0.f, 60.f, 0.f));
			AddLoot(F.ToWorld(C + FVector(150.f, 0.f, -125.f)), Yaw + 180.f, Rng.FRand() < 0.5f ? ETOContainerType::WeaponCrate : ETOContainerType::HighValueCrate, Tier, Poi);
		}
		else
		{
			Box(F, C, FVector(610.f, 244.f, 259.f), M);
			Box(F, C + FVector(-306.f, 0.f, 0.f), FVector(4.f, 230.f, 240.f), ETOMat::MetalDark, ETOCol::None);
		}
	}
	FBox2D Fp(ForceInit);
	for (int32 c = 0; c < 4; ++c)
	{
		const FVector P = F.ToWorld(FVector((c & 1) ? 310.f : -310.f, (c & 2) ? 650.f : -130.f, 0.f));
		Fp += FVector2D(P.X, P.Y);
	}
	Footprints.Add(Fp);
}

void ATOWorldGenerator::BuildVehicle(const FVector& Base, float Yaw, int32 Kind, FRandomStream& Rng)
{
	const FTOFrame F(Base, Yaw);
	auto Wheel = [&](float X, float Y, float Dia)
	{
		Cyl(F, FVector(X, Y, Dia * 0.5f), Dia, 30.f, ETOMat::Rubber, ETOCol::Solid, FRotator(0.f, 0.f, 90.f));
	};
	switch (Kind)
	{
	case 0: // civilian car
	{
		static const ETOMat Paint[] = { ETOMat::MetalWhite, ETOMat::MetalRed, ETOMat::MetalBlue, ETOMat::MetalGrey, ETOMat::MetalDark };
		const ETOMat P = Paint[Rng.RandRange(0, 4)];
		Box(F, FVector(0.f, 0.f, 75.f), FVector(440.f, 180.f, 70.f), P);
		Box(F, FVector(-20.f, 0.f, 135.f), FVector(240.f, 168.f, 55.f), P);
		Box(F, FVector(85.f, 0.f, 135.f), FVector(10.f, 160.f, 45.f), ETOMat::Glass, ETOCol::None, FRotator(-35.f, 0.f, 0.f));
		for (int32 c = 0; c < 4; ++c) Wheel((c & 1) ? 140.f : -140.f, (c & 2) ? 85.f : -85.f, 66.f);
		break;
	}
	case 1: // military truck
	{
		Box(F, FVector(260.f, 0.f, 150.f), FVector(200.f, 240.f, 200.f), ETOMat::MetalGreen);
		Box(F, FVector(320.f, 0.f, 190.f), FVector(10.f, 220.f, 70.f), ETOMat::Glass, ETOCol::None);
		Box(F, FVector(-120.f, 0.f, 120.f), FVector(520.f, 250.f, 30.f), ETOMat::MetalGreen);
		Box(F, FVector(-120.f, 0.f, 230.f), FVector(520.f, 250.f, 200.f), ETOMat::Canvas);
		for (int32 c = 0; c < 6; ++c) Wheel(c < 2 ? 260.f : (c < 4 ? -60.f : -260.f), (c % 2) ? 110.f : -110.f, 105.f);
		break;
	}
	case 2: // APC
	{
		Box(F, FVector(0.f, 0.f, 150.f), FVector(680.f, 290.f, 160.f), ETOMat::MetalGreen);
		Box(F, FVector(260.f, 0.f, 150.f), FVector(160.f, 280.f, 120.f), ETOMat::MetalGreen, ETOCol::Solid, FRotator(-25.f, 0.f, 0.f));
		Cyl(F, FVector(-40.f, 0.f, 260.f), 160.f, 70.f, ETOMat::MetalGreen);
		Cyl(F, FVector(120.f, 0.f, 270.f), 14.f, 250.f, ETOMat::MetalDark, ETOCol::Solid, FRotator(90.f, 0.f, 0.f));
		for (int32 c = 0; c < 8; ++c) Wheel(-240.f + (c / 2) * 160.f, (c % 2) ? 135.f : -135.f, 110.f);
		break;
	}
	case 3: // pickup
	{
		Box(F, FVector(100.f, 0.f, 120.f), FVector(220.f, 190.f, 120.f), ETOMat::MetalWhite);
		Box(F, FVector(-160.f, 0.f, 95.f), FVector(260.f, 190.f, 70.f), ETOMat::MetalWhite);
		for (int32 c = 0; c < 4; ++c) Wheel((c & 1) ? 140.f : -170.f, (c & 2) ? 90.f : -90.f, 75.f);
		break;
	}
	default: // forklift
	{
		Box(F, FVector(0.f, 0.f, 80.f), FVector(220.f, 120.f, 100.f), ETOMat::MetalYellow);
		Box(F, FVector(-40.f, 0.f, 200.f), FVector(120.f, 110.f, 10.f), ETOMat::MetalDark, ETOCol::None);
		Box(F, FVector(130.f, 0.f, 150.f), FVector(10.f, 90.f, 300.f), ETOMat::MetalDark);
		Box(F, FVector(180.f, 0.f, 15.f), FVector(110.f, 80.f, 6.f), ETOMat::MetalDark, ETOCol::None);
		for (int32 c = 0; c < 4; ++c) Wheel((c & 1) ? 70.f : -70.f, (c & 2) ? 55.f : -55.f, 50.f);
		break;
	}
	}
}

void ATOWorldGenerator::BuildHelicopterProp(const FVector& Base, float Yaw)
{
	const FTOFrame F(Base, Yaw);
	Box(F, FVector(0.f, 0.f, 170.f), FVector(800.f, 250.f, 240.f), ETOMat::MetalGreen);
	Box(F, FVector(380.f, 0.f, 190.f), FVector(80.f, 210.f, 150.f), ETOMat::Glass, ETOCol::None);
	Box(F, FVector(-750.f, 0.f, 240.f), FVector(760.f, 70.f, 80.f), ETOMat::MetalGreen);
	Box(F, FVector(-1110.f, 0.f, 340.f), FVector(60.f, 20.f, 200.f), ETOMat::MetalGreen);
	Box(F, FVector(0.f, 0.f, 330.f), FVector(1500.f, 40.f, 8.f), ETOMat::MetalDark, ETOCol::None, FRotator(0.f, 20.f, 0.f));
	Box(F, FVector(0.f, 0.f, 330.f), FVector(1500.f, 40.f, 8.f), ETOMat::MetalDark, ETOCol::None, FRotator(0.f, 110.f, 0.f));
	Box(F, FVector(0.f, -150.f, 20.f), FVector(600.f, 12.f, 12.f), ETOMat::MetalDark);
	Box(F, FVector(0.f, 150.f, 20.f), FVector(600.f, 12.f, 12.f), ETOMat::MetalDark);
}

void ATOWorldGenerator::BuildSandbagWall(const FVector& Center, float Yaw, float Length)
{
	const FTOFrame F(Center, Yaw);
	for (int32 Row = 0; Row < 3; ++Row)
	{
		const float Off = (Row % 2) * 30.f;
		for (float X = -Length * 0.5f + Off; X < Length * 0.5f - 30.f; X += 62.f)
		{
			Box(F, FVector(X + 30.f, 0.f, 18.f + Row * 30.f), FVector(60.f, 45.f, 30.f), ETOMat::Sandbag);
		}
	}
}

void ATOWorldGenerator::BuildFence(const FVector2D& A, const FVector2D& B, float Z, bool bConcrete)
{
	const FVector2D AB = B - A;
	const float Len = AB.Size();
	if (Len < 10.f)
	{
		return;
	}
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(AB.Y, AB.X));
	const FVector2D Mid = (A + B) * 0.5f;
	const FTOFrame F(FVector(Mid.X, Mid.Y, Z), Yaw);
	if (bConcrete)
	{
		// HESCO / concrete barrier wall
		const int32 Segs = FMath::Max(1, FMath::RoundToInt(Len / 300.f));
		const float SegLen = Len / Segs;
		for (int32 i = 0; i < Segs; ++i)
		{
			const float X = -Len * 0.5f + SegLen * (i + 0.5f);
			const FVector Ground = OnGround(F.ToWorld(FVector(X, 0.f, 0.f)).X, F.ToWorld(FVector(X, 0.f, 0.f)).Y);
			const float LocalZ = Ground.Z - Z;
			Box(F, FVector(X, 0.f, LocalZ + 120.f), FVector(SegLen - 6.f, 60.f, 300.f), ETOMat::Concrete);
			Box(F, FVector(X, 0.f, LocalZ + 275.f), FVector(SegLen, 20.f, 10.f), ETOMat::MetalDark, ETOCol::None);
		}
	}
	else
	{
		const int32 Posts = FMath::Max(2, FMath::CeilToInt(Len / 300.f) + 1);
		for (int32 i = 0; i < Posts; ++i)
		{
			const float X = -Len * 0.5f + Len * i / (Posts - 1);
			const FVector W = F.ToWorld(FVector(X, 0.f, 0.f));
			const float LocalZ = GetTerrainHeight(W.X, W.Y) - Z;
			Box(F, FVector(X, 0.f, LocalZ + 110.f), FVector(8.f, 8.f, 240.f), ETOMat::MetalGrey);
		}
		Box(F, FVector(0.f, 0.f, 225.f), FVector(Len, 4.f, 4.f), ETOMat::MetalGrey, ETOCol::None);
		Box(F, FVector(0.f, 0.f, 150.f), FVector(Len, 3.f, 3.f), ETOMat::MetalGrey, ETOCol::None);
		Box(F, FVector(0.f, 0.f, 75.f), FVector(Len, 3.f, 3.f), ETOMat::MetalGrey, ETOCol::None);
		Box(F, FVector(0.f, 0.f, 120.f), FVector(Len, 10.f, 260.f), ETOMat::Concrete, ETOCol::PawnOnly);
	}
}

void ATOWorldGenerator::BuildPylon(const FVector& Base, float Yaw)
{
	const FTOFrame F(Base, Yaw);
	const float H = 2600.f;
	for (int32 c = 0; c < 4; ++c)
	{
		const float SX = (c & 1) ? 1.f : -1.f;
		const float SY = (c & 2) ? 1.f : -1.f;
		Box(F, FVector(SX * 120.f, SY * 120.f, H * 0.5f), FVector(18.f, 18.f, H), ETOMat::Aluminium, ETOCol::Solid, FRotator(SX * 2.5f, 0.f, -SY * 2.5f));
	}
	for (int32 k = 1; k < 6; ++k)
	{
		const float Z = k * H / 6.f;
		const float S = 240.f - k * 25.f;
		Box(F, FVector(0.f, 0.f, Z), FVector(S, 12.f, 12.f), ETOMat::Aluminium, ETOCol::None, FRotator(0.f, 45.f, 0.f));
		Box(F, FVector(0.f, 0.f, Z), FVector(S, 12.f, 12.f), ETOMat::Aluminium, ETOCol::None, FRotator(0.f, -45.f, 0.f));
	}
	Box(F, FVector(0.f, 0.f, H - 200.f), FVector(20.f, 900.f, 25.f), ETOMat::Aluminium, ETOCol::None);
	Box(F, FVector(0.f, 0.f, H - 600.f), FVector(20.f, 700.f, 25.f), ETOMat::Aluminium, ETOCol::None);
	for (int32 s = -1; s <= 1; s += 2)
	{
		Cyl(F, FVector(0.f, s * 420.f, H - 260.f), 16.f, 90.f, ETOMat::Glass, ETOCol::None);
		Cyl(F, FVector(0.f, s * 320.f, H - 660.f), 16.f, 90.f, ETOMat::Glass, ETOCol::None);
	}
}

void ATOWorldGenerator::BuildLampPost(const FVector& Base, float Yaw)
{
	const FTOFrame F(Base, Yaw);
	Cyl(F, FVector(0.f, 0.f, 400.f), 16.f, 800.f, ETOMat::MetalGrey);
	Box(F, FVector(80.f, 0.f, 790.f), FVector(170.f, 10.f, 10.f), ETOMat::MetalGrey, ETOCol::None);
	Box(F, FVector(150.f, 0.f, 780.f), FVector(60.f, 30.f, 12.f), ETOMat::LampWarm, ETOCol::None);
	AddLight(F.ToWorld(FVector(150.f, 0.f, 740.f)), FLinearColor(1.f, 0.8f, 0.55f), 1200.f, 1800.f, true);
}

void ATOWorldGenerator::BuildTankFarm(const FVector& Center, float Yaw, int32 Poi)
{
	const FTOFrame F(Center, Yaw);
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector C(i * 1000.f - 1000.f, 0.f, 0.f);
		Cyl(F, C + FVector(0.f, 0.f, 350.f), 800.f, 700.f, ETOMat::MetalWhite);
		Cyl(F, C + FVector(0.f, 0.f, 710.f), 780.f, 20.f, ETOMat::MetalGrey);
		Box(F, C + FVector(0.f, -410.f, 350.f), FVector(60.f, 10.f, 700.f), ETOMat::MetalDark, ETOCol::None);
		BarrelSpots.Add(F.ToWorld(C + FVector(500.f, 400.f, 0.f)));
	}
	Cyl(F, FVector(0.f, 500.f, 120.f), 40.f, 3000.f, ETOMat::MetalGrey, ETOCol::Solid, FRotator(90.f, 0.f, 0.f));
	FBox2D Fp(ForceInit);
	Fp += FVector2D(Center.X - 1600.f, Center.Y - 600.f);
	Fp += FVector2D(Center.X + 1600.f, Center.Y + 600.f);
	Footprints.Add(Fp);
}

void ATOWorldGenerator::BuildCrateCluster(const FVector& Base, float Yaw, FRandomStream& Rng)
{
	const FTOFrame F(Base, Yaw);
	const int32 N = Rng.RandRange(3, 6);
	for (int32 i = 0; i < N; ++i)
	{
		const float S = Rng.FRandRange(80.f, 125.f);
		const FVector P((i % 3) * 130.f, (i / 3) * 130.f, S * 0.5f);
		Box(F, P, FVector(S, S, S), Rng.FRand() < 0.7f ? ETOMat::Wood : ETOMat::MetalGreen, ETOCol::Solid, FRotator(0.f, Rng.FRandRange(-10.f, 10.f), 0.f));
		if (Rng.FRand() < 0.3f)
		{
			Box(F, P + FVector(0.f, 0.f, S * 0.5f + 40.f), FVector(80.f, 80.f, 80.f), ETOMat::Wood);
		}
	}
}

void ATOWorldGenerator::BuildTent(const FVector& Base, float Yaw)
{
	const FTOFrame F(Base, Yaw);
	const float W = 600.f;
	const float D = 450.f;
	const float Pitch = 40.f;
	const float Rise = D * 0.5f * FMath::Tan(FMath::DegreesToRadians(Pitch));
	const float Panel = D * 0.5f / FMath::Cos(FMath::DegreesToRadians(Pitch)) + 20.f;
	Box(F, FVector(0.f, -D * 0.25f, Rise * 0.5f + 60.f), FVector(W, Panel, 8.f), ETOMat::Canvas, ETOCol::Solid, FRotator(0.f, 0.f, -Pitch));
	Box(F, FVector(0.f, D * 0.25f, Rise * 0.5f + 60.f), FVector(W, Panel, 8.f), ETOMat::Canvas, ETOCol::Solid, FRotator(0.f, 0.f, Pitch));
	Box(F, FVector(0.f, -D * 0.5f, 30.f), FVector(W, 6.f, 60.f), ETOMat::Canvas);
	Box(F, FVector(0.f, D * 0.5f, 30.f), FVector(W, 6.f, 60.f), ETOMat::Canvas);
	Box(F, FVector(-W * 0.5f, 0.f, 80.f), FVector(6.f, D, 160.f), ETOMat::Canvas);
}

void ATOWorldGenerator::BuildHelipad(const FVector& Center)
{
	const FTOFrame F(Center, 0.f);
	Cyl(F, FVector(0.f, 0.f, -40.f), 2400.f, 100.f, ETOMat::Concrete);
	Cyl(F, FVector(0.f, 0.f, 12.f), 1800.f, 4.f, ETOMat::MetalYellow, ETOCol::None);
	Cyl(F, FVector(0.f, 0.f, 14.f), 1700.f, 4.f, ETOMat::Concrete, ETOCol::None);
	Box(F, FVector(0.f, -250.f, 16.f), FVector(800.f, 80.f, 4.f), ETOMat::RoadLineWhite, ETOCol::None);
	Box(F, FVector(0.f, 250.f, 16.f), FVector(800.f, 80.f, 4.f), ETOMat::RoadLineWhite, ETOCol::None);
	Box(F, FVector(0.f, 0.f, 16.f), FVector(80.f, 500.f, 4.f), ETOMat::RoadLineWhite, ETOCol::None);
	for (int32 i = 0; i < 8; ++i)
	{
		const float A = i * PI / 4.f;
		Box(F, FVector(FMath::Cos(A) * 1150.f, FMath::Sin(A) * 1150.f, 15.f), FVector(20.f, 20.f, 20.f), ETOMat::LampGreen, ETOCol::None);
	}
}

void ATOWorldGenerator::BuildGasStation(const FVector& Center, float Yaw, int32 Poi, FRandomStream& Rng)
{
	const FTOFrame F(Center, Yaw);
	// Canopy
	for (int32 c = 0; c < 4; ++c)
	{
		Box(F, FVector((c & 1) ? 600.f : -600.f, (c & 2) ? 300.f : -300.f, 250.f), FVector(40.f, 40.f, 500.f), ETOMat::MetalWhite);
	}
	Box(F, FVector(0.f, 0.f, 520.f), FVector(1700.f, 1000.f, 60.f), ETOMat::MetalRed);
	Box(F, FVector(0.f, 0.f, 488.f), FVector(1500.f, 800.f, 4.f), ETOMat::LampCold, ETOCol::None);
	for (int32 i = -1; i <= 1; i += 2)
	{
		Box(F, FVector(i * 300.f, 0.f, 15.f), FVector(400.f, 120.f, 30.f), ETOMat::ConcreteLight);
		Box(F, FVector(i * 300.f, 0.f, 110.f), FVector(80.f, 60.f, 160.f), ETOMat::MetalWhite);
	}
	BarrelSpots.Add(F.ToWorld(FVector(900.f, 600.f, 0.f)));
	AddLight(F.ToWorld(FVector(0.f, 0.f, 440.f)), FLinearColor(0.9f, 0.95f, 1.f), 1400.f, 2200.f, true);
	// Shop
	FTOBuildingSpec S;
	S.Center = F.ToWorld(FVector(0.f, 1300.f, 0.f));
	S.Yaw = Yaw + 180.f;
	S.W = 1400.f;
	S.D = 900.f;
	S.Floors = 1;
	S.FloorH = 380.f;
	S.Layout = 1;
	S.Ext = ETOMat::PlasterBlue;
	S.Trim = ETOMat::MetalRed;
	S.DoorMask = 1;
	S.bRoofAccess = false;
	S.Tier = 0;
	S.Poi = Poi;
	S.WinW = 220.f;
	S.WinH = 160.f;
	S.WinSill = 70.f;
	BuildBuilding(S, Rng);
	BuildVehicle(F.ToWorld(FVector(-300.f, -150.f, 0.f)), Yaw + 5.f, 0, Rng);
}

void ATOWorldGenerator::BuildChurch(const FVector& Center, float Yaw, int32 Poi, FRandomStream& Rng)
{
	const FTOFrame F(Center, Yaw);
	const float W = 2000.f;
	const float D = 1000.f;
	const float H = 800.f;
	FTOBuildingSpec S;
	S.Center = Center;
	S.Yaw = Yaw;
	S.W = W;
	S.D = D;
	S.Floors = 1;
	S.FloorH = H;
	S.Layout = 1;
	S.Ext = ETOMat::PlasterBeige;
	S.Trim = ETOMat::ConcreteLight;
	S.Roof = ETOMat::Brick;
	S.bGable = true;
	S.bRoofAccess = false;
	S.bParapet = false;
	S.DoorMask = 8;
	S.WinW = 100.f;
	S.WinH = 300.f;
	S.WinSill = 250.f;
	S.WinSpacing = 400.f;
	S.Poi = Poi;
	S.Tier = 1;
	BuildBuilding(S, Rng);
	// Pews
	for (float X = -W * 0.3f; X < W * 0.35f; X += 160.f)
	{
		Box(F, FVector(X, -220.f, 45.f), FVector(50.f, 300.f, 90.f), ETOMat::WoodDark);
		Box(F, FVector(X, 220.f, 45.f), FVector(50.f, 300.f, 90.f), ETOMat::WoodDark);
	}
	// Bell tower at the -X end
	const FVector TC(-W * 0.5f - 250.f, 0.f, 0.f);
	Box(F, TC + FVector(0.f, 0.f, 800.f), FVector(500.f, 500.f, 1600.f), ETOMat::PlasterBeige);
	Box(F, TC + FVector(0.f, 0.f, 1400.f), FVector(520.f, 520.f, 30.f), ETOMat::ConcreteLight);
	Cone(F, TC + FVector(0.f, 0.f, 1850.f), 600.f, 700.f, ETOMat::Brick, ETOCol::Solid);
	AddLoot(F.ToWorld(FVector(W * 0.4f, 0.f, 0.f)), Yaw + 180.f, ETOContainerType::Safe, 1, Poi);
	FBox2D Fp(ForceInit);
	const FVector P = F.ToWorld(TC);
	Fp += FVector2D(P.X - 260.f, P.Y - 260.f);
	Fp += FVector2D(P.X + 260.f, P.Y + 260.f);
	Footprints.Add(Fp);
}

void ATOWorldGenerator::BuildPumpStation(const FVector& Center, float Yaw, int32 Poi, FRandomStream& Rng)
{
	const FTOFrame F(Center, Yaw);
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector C(-1600.f + i * 1600.f, 1400.f, 0.f);
		Cyl(F, C + FVector(0.f, 0.f, 400.f), 1300.f, 800.f, ETOMat::ConcreteLight);
		Cyl(F, C + FVector(0.f, 0.f, 810.f), 1320.f, 30.f, ETOMat::MetalGrey);
		Box(F, C + FVector(0.f, -660.f, 400.f), FVector(80.f, 20.f, 800.f), ETOMat::MetalYellow, ETOCol::None);
	}
	Cyl(F, FVector(0.f, 600.f, 150.f), 60.f, 4000.f, ETOMat::MetalBlue, ETOCol::Solid, FRotator(90.f, 0.f, 0.f));
	FTOBuildingSpec S;
	S.Center = Center;
	S.Yaw = Yaw;
	S.W = 1800.f;
	S.D = 1000.f;
	S.Floors = 1;
	S.FloorH = 450.f;
	S.Layout = 1;
	S.Ext = ETOMat::Brick;
	S.Trim = ETOMat::ConcreteLight;
	S.DoorMask = 1 | 8;
	S.bRoofAccess = true;
	S.Poi = Poi;
	S.Tier = 1;
	BuildBuilding(S, Rng);
	// Pumps inside
	for (int32 i = -1; i <= 1; ++i)
	{
		Cyl(F, FVector(i * 450.f, 0.f, 70.f), 120.f, 140.f, ETOMat::MetalBlue);
		Cyl(F, FVector(i * 450.f, 0.f, 160.f), 60.f, 120.f, ETOMat::MetalGrey);
	}
	AddLoot(F.ToWorld(FVector(700.f, -350.f, 0.f)), Yaw, ETOContainerType::Toolbox, 1, Poi);
	FBox2D Fp(ForceInit);
	Fp += FVector2D(Center.X - 2400.f, Center.Y - 2400.f);
	Fp += FVector2D(Center.X + 2400.f, Center.Y + 2400.f);
	Footprints.Add(Fp);
}

void ATOWorldGenerator::BuildCraneGantry(const FVector& Center, float Yaw, float Span)
{
	const FTOFrame F(Center, Yaw);
	const float H = 1500.f;
	for (int32 Side = -1; Side <= 1; Side += 2)
	{
		Box(F, FVector(-300.f, Side * Span * 0.5f, H * 0.5f), FVector(60.f, 60.f, H), ETOMat::MetalYellow);
		Box(F, FVector(300.f, Side * Span * 0.5f, H * 0.5f), FVector(60.f, 60.f, H), ETOMat::MetalYellow);
		Box(F, FVector(0.f, Side * Span * 0.5f, 60.f), FVector(800.f, 80.f, 60.f), ETOMat::MetalYellow);
	}
	Box(F, FVector(-300.f, 0.f, H), FVector(80.f, Span + 200.f, 120.f), ETOMat::MetalYellow);
	Box(F, FVector(300.f, 0.f, H), FVector(80.f, Span + 200.f, 120.f), ETOMat::MetalYellow);
	Box(F, FVector(0.f, Span * 0.2f, H - 100.f), FVector(500.f, 300.f, 200.f), ETOMat::MetalDark);
	Box(F, FVector(0.f, Span * 0.2f, H - 600.f), FVector(10.f, 10.f, 800.f), ETOMat::MetalDark, ETOCol::None);
}

// =============================================================================================
//  Hydro dam & power station & radar
// =============================================================================================

void ATOWorldGenerator::BuildDam(const FVector& Center, float Length, float BaseZ, float TopZ, FRandomStream& Rng)
{
	// Center.X = upstream face, dam runs along world Y.
	const FTOFrame F(FVector(Center.X, Center.Y, 0.f), 0.f);
	const float HL = Length * 0.5f;
	struct FTier { float Z0; float Z1; float Thick; };
	const FTier Tiers[] = { { BaseZ - 600.f, 1500.f, 4000.f }, { 1500.f, 3000.f, 3200.f }, { 3000.f, 4500.f, 2400.f }, { 4500.f, TopZ, 1800.f } };
	for (const FTier& T : Tiers)
	{
		Box(F, FVector(-T.Thick * 0.5f, 0.f, (T.Z0 + T.Z1) * 0.5f), FVector(T.Thick, Length, T.Z1 - T.Z0), ETOMat::ConcreteDam);
		// Expansion joints for scale
		for (float Y = -HL + 1500.f; Y < HL; Y += 1500.f)
		{
			Box(F, FVector(-T.Thick - 2.f, Y, (T.Z0 + T.Z1) * 0.5f), FVector(6.f, 20.f, T.Z1 - T.Z0), ETOMat::ConcreteDark, ETOCol::None);
		}
	}
	Footprints.Add(FBox2D(FVector2D(Center.X - 4000.f, -HL), FVector2D(Center.X, HL)));

	// Crest road (west to east, extends into the gorge shoulders)
	const float CrestW = 1800.f;
	Box(F, FVector(-CrestW * 0.5f, 0.f, TopZ + 10.f), FVector(CrestW, Length + 6000.f, 20.f), ETOMat::Asphalt);
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float X = Side == 0 ? -CrestW + 20.f : -20.f;
		Box(F, FVector(X, 0.f, TopZ + 70.f), FVector(40.f, Length + 6000.f, 120.f), ETOMat::ConcreteLight);
	}
	for (float Y = -HL - 2000.f; Y <= HL + 2000.f; Y += 4000.f)
	{
		BuildLampPost(F.ToWorld(FVector(-CrestW + 60.f, Y, TopZ + 20.f)), 0.f);
	}

	// Spillway gates on the east part (Y 6000..14000)
	const float SpillY0 = 6000.f;
	const float SpillY1 = 14000.f;
	for (int32 i = 0; i <= 4; ++i)
	{
		const float Y = FMath::Lerp(SpillY0, SpillY1, i / 4.f);
		Box(F, FVector(-450.f, Y, TopZ + 500.f), FVector(900.f, 300.f, 1000.f), ETOMat::ConcreteDam);
		if (i < 4)
		{
			const float GY = FMath::Lerp(SpillY0, SpillY1, (i + 0.5f) / 4.f);
			Box(F, FVector(-200.f, GY, TopZ + 300.f), FVector(80.f, 1700.f, 900.f), ETOMat::MetalBlue);
		}
	}
	// Gantry crane on rails over the gates
	Box(F, FVector(-850.f, (SpillY0 + SpillY1) * 0.5f, TopZ + 1030.f), FVector(80.f, SpillY1 - SpillY0 + 300.f, 60.f), ETOMat::MetalDark);
	Box(F, FVector(-50.f, (SpillY0 + SpillY1) * 0.5f, TopZ + 1030.f), FVector(80.f, SpillY1 - SpillY0 + 300.f, 60.f), ETOMat::MetalDark);
	const float CraneY = FMath::Lerp(SpillY0, SpillY1, Rng.FRandRange(0.2f, 0.8f));
	Box(F, FVector(-450.f, CraneY, TopZ + 1500.f), FVector(1000.f, 300.f, 150.f), ETOMat::MetalYellow);
	Box(F, FVector(-850.f, CraneY, TopZ + 1250.f), FVector(110.f, 280.f, 380.f), ETOMat::MetalYellow);
	Box(F, FVector(-50.f, CraneY, TopZ + 1250.f), FVector(110.f, 280.f, 380.f), ETOMat::MetalYellow);
	Box(F, FVector(-450.f, CraneY, TopZ + 1700.f), FVector(500.f, 280.f, 250.f), ETOMat::MetalYellow);

	// Spillway chute: water racing down the downstream face into the river
	{
		const float X0 = -1800.f;
		const float X1 = -6200.f;
		const float Z0 = TopZ - 200.f;
		const float Z1 = 300.f;
		const float Len = FMath::Sqrt(FMath::Square(X1 - X0) + FMath::Square(Z0 - Z1));
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Z0 - Z1, X0 - X1));
		const FVector Mid((X0 + X1) * 0.5f, (SpillY0 + SpillY1) * 0.5f, (Z0 + Z1) * 0.5f);
		Box(F, Mid, FVector(Len, SpillY1 - SpillY0, 150.f), ETOMat::ConcreteDam, ETOCol::Solid, FRotator(Pitch, 0.f, 0.f));
		Box(F, Mid + FVector(0.f, 0.f, 90.f), FVector(Len, SpillY1 - SpillY0 - 200.f, 20.f), ETOMat::WaterRiver, ETOCol::None, FRotator(Pitch, 0.f, 0.f));
		for (int32 s = -1; s <= 1; s += 2)
		{
			Box(F, Mid + FVector(0.f, s * (SpillY1 - SpillY0) * 0.5f, 150.f), FVector(Len, 120.f, 300.f), ETOMat::ConcreteDam, ETOCol::Solid, FRotator(Pitch, 0.f, 0.f));
		}
		for (int32 k = 0; k < 10; ++k)
		{
			Sphere(F, FVector(X1 - 600.f + Rng.FRandRange(-500.f, 500.f), Rng.FRandRange(SpillY0, SpillY1), 350.f), FVector(Rng.FRandRange(500.f, 900.f), Rng.FRandRange(500.f, 900.f), 260.f), ETOMat::Smoke, ETOCol::None);
		}
	}

	// Penstocks feeding the power station (west side)
	for (int32 i = 0; i < 3; ++i)
	{
		const float Y = -12000.f + i * 2500.f;
		const FVector A(-3200.f, Y, 2300.f);
		const FVector B(-5100.f, Y, 900.f);
		const FVector D = B - A;
		const float Len = D.Size();
		const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(D.Z, D.Size2D()));
		Cyl(F, (A + B) * 0.5f, 450.f, Len, ETOMat::MetalRust, ETOCol::Solid, FRotator(90.f + Pitch, 180.f, 0.f));
	}

	// Two control towers on the crest (upstream side)
	FTOBuildingSpec T1;
	T1.Center = FVector(Center.X - 450.f, -12000.f, TopZ + 20.f);
	T1.Yaw = -90.f;
	T1.W = 900.f;
	T1.D = 800.f;
	T1.Floors = 2;
	T1.FloorH = 360.f;
	T1.Layout = 1;
	T1.Ext = ETOMat::ConcreteLight;
	T1.Trim = ETOMat::MetalBlue;
	T1.DoorMask = 1;
	T1.Tier = 2;
	T1.Poi = POIs.IndexOfByPredicate([](const FTOPOI& P) { return P.Name == TEXT("Hydro Dam"); });
	T1.SpecialRoomKey = FName(TEXT("DamControl"));
	T1.SpecialRoomFloor = 1;
	T1.SpecialLoot = ETOContainerType::HighValueCrate;
	BuildBuilding(T1, Rng);

	FTOBuildingSpec T2 = T1;
	T2.Center = FVector(Center.X - 450.f, 17500.f, TopZ + 20.f);
	T2.SpecialRoomKey = NAME_None;
	T2.SpecialRoomFloor = -1;
	BuildBuilding(T2, Rng);

	for (int32 i = 0; i < 4; ++i)
	{
		AddGuard(FVector(Center.X - 900.f, -15000.f + i * 9000.f, TopZ + 120.f), 180.f, T1.Poi, 2);
	}
}

void ATOWorldGenerator::BuildPowerStation(const FVector& Center, float Yaw, int32 Poi, FRandomStream& Rng)
{
	const FTOFrame F(Center, Yaw);
	const float W = 9600.f;
	const float D = 3000.f;
	const float H = 2000.f;
	const float HW = W * 0.5f;
	const float HD = D * 0.5f;
	const float T = 40.f;

	FBox2D Fp(ForceInit);
	for (int32 c = 0; c < 4; ++c)
	{
		const FVector P = F.ToWorld(FVector((c & 1) ? HW : -HW, (c & 2) ? HD : -HD, 0.f));
		Fp += FVector2D(P.X, P.Y);
	}
	Footprints.Add(Fp);

	Box(F, FVector(0.f, 0.f, -150.f), FVector(W + 60.f, D + 60.f, 300.f), ETOMat::ConcreteDark);
	Box(F, FVector(0.f, 0.f, -5.f), FVector(W, D, 10.f), ETOMat::Tile);

	// Walls: concrete base, glazed band, upper concrete
	auto HallWall = [&](const FVector2D& A, const FVector2D& B, float Len, const TArray<FTOOpening>& Doors)
	{
		Wall(F, A, B, 0.f, 700.f, T, ETOMat::ConcreteLight, Doors);
		TArray<FTOOpening> Band;
		for (const FTOOpening& O : Doors)
		{
			if (O.Top > 700.f) Band.Add(FTOOpening(O.Center, O.Width, 0.f, O.Top - 700.f));
		}
		for (float C = 500.f; C < Len - 400.f; C += 900.f)
		{
			bool bClash = false;
			for (const FTOOpening& O : Doors) bClash |= FMath::Abs(O.Center - C) < O.Width * 0.5f + 350.f;
			if (!bClash) Band.Add(FTOOpening(C, 600.f, 400.f, 1000.f));
		}
		Wall(F, A, B, 700.f, H - 700.f, T, ETOMat::ConcreteDam, Band);
	};
	HallWall(FVector2D(-HW, -HD), FVector2D(HW, -HD), W, { FTOOpening(W * 0.25f, 600.f, 0.f, 650.f), FTOOpening(W * 0.7f, 600.f, 0.f, 650.f), FTOOpening(W * 0.5f, 120.f, 0.f, 230.f) });
	HallWall(FVector2D(-HW, HD), FVector2D(HW, HD), W, {});
	HallWall(FVector2D(-HW, -HD), FVector2D(-HW, HD), D, { FTOOpening(D * 0.5f, 140.f, 0.f, 230.f) });
	HallWall(FVector2D(HW, -HD), FVector2D(HW, HD), D, { FTOOpening(D * 0.3f, 140.f, 0.f, 230.f) });
	Box(F, FVector(0.f, 0.f, H + 20.f), FVector(W + 100.f, D + 100.f, 40.f), ETOMat::ConcreteDark);
	for (float X = -HW + 800.f; X < HW; X += 1600.f)
	{
		Box(F, FVector(X, 0.f, H + 45.f), FVector(500.f, D * 0.5f, 10.f), ETOMat::Glass, ETOCol::None);
		Box(F, FVector(X, 0.f, H - 40.f), FVector(200.f, 60.f, 8.f), ETOMat::LampCold, ETOCol::None);
	}
	// Columns
	for (float X = -HW + 1200.f; X < HW; X += 1200.f)
	{
		Box(F, FVector(X, -HD + 60.f, H * 0.5f), FVector(60.f, 60.f, H), ETOMat::ConcreteLight);
		Box(F, FVector(X, HD - 60.f, H * 0.5f), FVector(60.f, 60.f, H), ETOMat::ConcreteLight);
	}

	// Generator units
	for (int32 i = 0; i < 4; ++i)
	{
		const float X = -HW + 1500.f + i * 2000.f;
		Cyl(F, FVector(X, 300.f, 125.f), 760.f, 250.f, ETOMat::ConcreteLight);
		Cyl(F, FVector(X, 300.f, 420.f), 560.f, 340.f, i % 2 ? ETOMat::MetalYellow : ETOMat::MetalBlue);
		Sphere(F, FVector(X, 300.f, 590.f), FVector(560.f, 560.f, 240.f), ETOMat::MetalGrey);
		Box(F, FVector(X, -350.f, 110.f), FVector(150.f, 80.f, 220.f), ETOMat::MetalGrey);
		Box(F, FVector(X, -391.f, 160.f), FVector(4.f, 60.f, 50.f), ETOMat::Screen, ETOCol::None);
	}

	// Overhead crane
	Box(F, FVector(0.f, -HD + 150.f, 1600.f), FVector(W - 200.f, 60.f, 80.f), ETOMat::MetalDark);
	Box(F, FVector(0.f, HD - 150.f, 1600.f), FVector(W - 200.f, 60.f, 80.f), ETOMat::MetalDark);
	const float CX = Rng.FRandRange(-HW * 0.5f, HW * 0.5f);
	Box(F, FVector(CX, 0.f, 1680.f), FVector(150.f, D - 200.f, 120.f), ETOMat::MetalYellow);
	Box(F, FVector(CX, 200.f, 1540.f), FVector(160.f, 160.f, 160.f), ETOMat::MetalYellow);
	Box(F, FVector(CX, 200.f, 1100.f), FVector(10.f, 10.f, 800.f), ETOMat::MetalDark, ETOCol::None);

	// Catwalk along the back wall + stairs
	const float CZ = 600.f;
	Box(F, FVector(0.f, HD - 140.f, CZ - 10.f), FVector(W - 400.f, 200.f, 20.f), ETOMat::MetalGrey);
	Railing(F, FVector2D(-HW + 200.f, HD - 245.f), FVector2D(HW - 200.f, HD - 245.f), CZ, ETOMat::MetalYellow);
	const float Run = CZ / FMath::Tan(FMath::DegreesToRadians(36.f));
	Stairs(F, FVector(-HW + 300.f, HD - 330.f, 0.f), Run, CZ, 120.f, ETOMat::MetalGrey);
	AddGuard(F.ToWorld(FVector(0.f, HD - 140.f, CZ + 100.f)), Yaw + 180.f, Poi, 2, false, true);

	// Control room mezzanine at +X end
	const float RX0 = HW - 1500.f;
	Box(F, FVector(HW - 750.f, 0.f, CZ - 10.f), FVector(1500.f - T, D - T, 20.f), ETOMat::Carpet);
	for (int32 c = 0; c < 2; ++c)
	{
		Box(F, FVector(RX0 + 30.f, c ? 600.f : -600.f, CZ * 0.5f), FVector(40.f, 40.f, CZ), ETOMat::MetalGrey);
	}
	Wall(F, FVector2D(RX0, -HD), FVector2D(RX0, HD), CZ, 380.f, 15.f, ETOMat::Plaster, { FTOOpening(D * 0.2f, 110.f, 0.f, 215.f), FTOOpening(D * 0.6f, 900.f, 110.f, 260.f) });
	Railing(F, FVector2D(RX0 - 300.f, -HD + T), FVector2D(RX0 - 300.f, -HD + 600.f), CZ, ETOMat::MetalYellow);
	Stairs(F, FVector(RX0 - 300.f - Run, -HD + 200.f, 0.f), Run, CZ, 120.f, ETOMat::MetalGrey);
	Box(F, FVector(RX0 - 150.f, -HD + 200.f, CZ - 10.f), FVector(300.f, 300.f, 20.f), ETOMat::MetalGrey);
	{
		FTOBuildingSpec Fake;
		Fake.Tier = 2;
		Fake.Poi = Poi;
		FurnishRoom(Fake, F, MakeBox2D(RX0 + 30.f, -HD + 40.f, HW - 40.f, -100.f), CZ, 3, Rng, false);
		FurnishRoom(Fake, F, MakeBox2D(RX0 + 30.f, 100.f, HW - 40.f, HD - 40.f), CZ, 0, Rng, false);
	}
	AddDoor(F.ToWorld(FVector(RX0, -HD + D * 0.2f, CZ)), Yaw + 90.f, 105.f, 210.f, NAME_None, ETOMat::MetalGrey);
	AddGuard(F.ToWorld(FVector(HW - 700.f, 0.f, CZ + 100.f)), Yaw + 180.f, Poi, 2);

	// Ground floor loot & guards
	AddLoot(F.ToWorld(FVector(-HW + 400.f, -HD + 200.f, 0.f)), Yaw, ETOContainerType::Toolbox, 2, Poi);
	AddLoot(F.ToWorld(FVector(-HW + 700.f, -HD + 200.f, 0.f)), Yaw, ETOContainerType::WeaponCrate, 2, Poi);
	AddLoot(F.ToWorld(FVector(0.f, -HD + 220.f, 0.f)), Yaw, ETOContainerType::AmmoBox, 2, Poi);
	AddLoot(F.ToWorld(FVector(HW - 2200.f, HD - 300.f, 0.f)), Yaw + 180.f, ETOContainerType::Locker, 2, Poi);
	for (int32 i = 0; i < 3; ++i)
	{
		AddGuard(F.ToWorld(FVector(-HW + 1500.f + i * 3000.f, -500.f, 100.f)), Yaw + Rng.FRandRange(0.f, 360.f), Poi, 2);
	}
	AddLight(F.ToWorld(FVector(-HW * 0.5f, 0.f, H - 200.f)), FLinearColor(0.9f, 0.95f, 1.f), 1200.f, 3500.f, false);
	AddLight(F.ToWorld(FVector(HW * 0.5f, 0.f, H - 200.f)), FLinearColor(0.9f, 0.95f, 1.f), 1200.f, 3500.f, false);

	// Transformer yard in front
	for (int32 i = 0; i < 3; ++i)
	{
		const FVector P(-HW + 2000.f + i * 2500.f, -HD - 1300.f, 0.f);
		Box(F, P + FVector(0.f, 0.f, 160.f), FVector(380.f, 260.f, 320.f), ETOMat::MetalGreen);
		for (int32 k = -1; k <= 1; ++k)
		{
			Cyl(F, P + FVector(k * 100.f, 0.f, 400.f), 30.f, 160.f, ETOMat::Glass);
		}
		for (int32 k = 0; k < 5; ++k)
		{
			Box(F, P + FVector(-150.f + k * 75.f, -140.f, 160.f), FVector(20.f, 30.f, 280.f), ETOMat::MetalDark, ETOCol::None);
		}
	}
	BuildFence(FVector2D(F.ToWorld(FVector(-HW + 1200.f, -HD - 2200.f, 0.f))), FVector2D(F.ToWorld(FVector(HW - 1500.f, -HD - 2200.f, 0.f))), Center.Z, false);
}

void ATOWorldGenerator::BuildRadarStation(const FVector& Center, int32 Poi, FRandomStream& Rng)
{
	const FTOFrame F(Center, 0.f);
	// Radome
	Cyl(F, FVector(0.f, 0.f, 300.f), 900.f, 600.f, ETOMat::ConcreteLight);
	Sphere(F, FVector(0.f, 0.f, 1050.f), FVector(1350.f, 1350.f, 1300.f), ETOMat::MetalWhite);
	Footprints.Add(FBox2D(FVector2D(Center.X - 700.f, Center.Y - 700.f), FVector2D(Center.X + 700.f, Center.Y + 700.f)));

	// Lattice comms tower
	const FVector TC(1800.f, 1800.f, 0.f);
	const float TH = 3600.f;
	for (int32 c = 0; c < 4; ++c)
	{
		const float SX = (c & 1) ? 1.f : -1.f;
		const float SY = (c & 2) ? 1.f : -1.f;
		Box(F, TC + FVector(SX * 150.f, SY * 150.f, TH * 0.5f), FVector(25.f, 25.f, TH), ETOMat::MetalRed, ETOCol::Solid, FRotator(SX * 1.5f, 0.f, -SY * 1.5f));
	}
	for (int32 k = 1; k < 9; ++k)
	{
		const float Z = k * TH / 9.f;
		Box(F, TC + FVector(0.f, 0.f, Z), FVector(320.f, 14.f, 14.f), (k % 2) ? ETOMat::MetalWhite : ETOMat::MetalRed, ETOCol::None, FRotator(0.f, 45.f, 0.f));
		Box(F, TC + FVector(0.f, 0.f, Z), FVector(320.f, 14.f, 14.f), (k % 2) ? ETOMat::MetalWhite : ETOMat::MetalRed, ETOCol::None, FRotator(0.f, -45.f, 0.f));
	}
	Box(F, TC + FVector(0.f, 0.f, TH + 30.f), FVector(400.f, 400.f, 20.f), ETOMat::MetalGrey);
	Cyl(F, TC + FVector(0.f, 0.f, TH + 500.f), 10.f, 900.f, ETOMat::MetalDark, ETOCol::None);
	Box(F, TC + FVector(0.f, 0.f, TH + 950.f), FVector(20.f, 20.f, 20.f), ETOMat::LampRed, ETOCol::None);
	AddLight(F.ToWorld(TC + FVector(0.f, 0.f, TH + 950.f)), FLinearColor(1.f, 0.1f, 0.05f), 800.f, 1500.f, true);

	// Operations building
	FTOBuildingSpec S;
	S.Center = F.ToWorld(FVector(-1800.f, 1200.f, 0.f));
	S.Yaw = 0.f;
	S.W = 1800.f;
	S.D = 1100.f;
	S.Floors = 1;
	S.FloorH = 380.f;
	S.Layout = 1;
	S.Ext = ETOMat::PlasterBlue;
	S.Trim = ETOMat::ConcreteLight;
	S.DoorMask = 1 | 4;
	S.Tier = 2;
	S.Poi = Poi;
	BuildBuilding(S, Rng);
	{
		FTOBuildingSpec Fake;
		Fake.Tier = 2;
		Fake.Poi = Poi;
		FurnishRoom(Fake, FTOFrame(S.Center, 0.f), MakeBox2D(-800.f, -450.f, 100.f, 450.f), 0.f, 3, Rng, false);
	}

	// Generator, fuel, sandbags
	Box(F, FVector(1500.f, -1500.f, 120.f), FVector(400.f, 200.f, 240.f), ETOMat::MetalGreen);
	BarrelSpots.Add(F.ToWorld(FVector(1900.f, -1500.f, 0.f)));
	BuildSandbagWall(F.ToWorld(FVector(-2600.f, -1600.f, 0.f)), 30.f, 500.f);
	BuildSandbagWall(F.ToWorld(FVector(2600.f, -800.f, 0.f)), -60.f, 500.f);
	AddLoot(F.ToWorld(FVector(1200.f, -1200.f, 0.f)), 0.f, ETOContainerType::WeaponCrate, 2, Poi);
	AddLoot(F.ToWorld(FVector(-500.f, -900.f, 0.f)), 90.f, ETOContainerType::AmmoBox, 2, Poi);

	// Perimeter fence
	const float R = 3000.f;
	BuildFence(FVector2D(Center.X - R, Center.Y - R), FVector2D(Center.X - R, Center.Y + R), Center.Z, false);
	BuildFence(FVector2D(Center.X + R, Center.Y - R), FVector2D(Center.X + R, Center.Y + R), Center.Z, false);
	BuildFence(FVector2D(Center.X - R, Center.Y + R), FVector2D(Center.X + R, Center.Y + R), Center.Z, false);
	BuildFence(FVector2D(Center.X - R, Center.Y - R), FVector2D(Center.X - 400.f, Center.Y - R), Center.Z, false);
	BuildFence(FVector2D(Center.X + 400.f, Center.Y - R), FVector2D(Center.X + R, Center.Y - R), Center.Z, false);

	AddGuard(F.ToWorld(FVector(0.f, -1200.f, 100.f)), 180.f, Poi, 2);
	AddGuard(F.ToWorld(FVector(1500.f, 500.f, 100.f)), 90.f, Poi, 2);
	AddGuard(F.ToWorld(FVector(-1500.f, -500.f, 100.f)), 270.f, Poi, 2);
	AddGuard(F.ToWorld(TC + FVector(0.f, 0.f, TH + 140.f)), 0.f, Poi, 2, false, true);
}
