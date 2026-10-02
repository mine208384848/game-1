// TAC-OPS - procedural battlefield generator (core: terrain, water, roads, vegetation, batching, minimap)

#include "World/TOWorldGenerator.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Async/ParallelFor.h"
#include "TacOps.h"

namespace
{
	constexpr int32 GTerrainQuads = 400;    // per side
	constexpr int32 GChunkQuads = 40;       // per chunk side

	float SmoothStep01(float Edge0, float Edge1, float X)
	{
		if (FMath::IsNearlyEqual(Edge0, Edge1))
		{
			return X < Edge0 ? 0.f : 1.f;
		}
		const float T = FMath::Clamp((X - Edge0) / (Edge1 - Edge0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	float Noise2(float X, float Y)
	{
		return FMath::PerlinNoise2D(FVector2D(X, Y));
	}

	float Hill(float Xm, float Ym, float Cx, float Cy, float Radius, float Height)
	{
		const float D = FVector2D::Distance(FVector2D(Xm, Ym), FVector2D(Cx, Cy));
		const float T = FMath::Clamp(1.f - D / Radius, 0.f, 1.f);
		return Height * T * T * (3.f - 2.f * T);
	}

	float DistToSegment2D(const FVector2D& P, const FVector2D& A, const FVector2D& B, float& OutT)
	{
		const FVector2D AB = B - A;
		const float Len2 = AB.SizeSquared();
		OutT = Len2 > KINDA_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / Len2, 0.f, 1.f) : 0.f;
		return FVector2D::Distance(P, A + AB * OutT);
	}

	struct FSectionBuild
	{
		TArray<FVector> Verts;
		TArray<int32> Tris;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;
		TArray<FLinearColor> Colors;
		TArray<FProcMeshTangent> Tangents;
		TMap<int32, int32> Remap;
	};
}

ATOWorldGenerator::ATOWorldGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Root->SetMobility(EComponentMobility::Static);
	RootComponent = Root;
}

// =============================================================================================
//  Height field
// =============================================================================================

float ATOWorldGenerator::RiverDistance(float X, float Y, float* OutT) const
{
	const FVector2D P(X, Y);
	float Best = TNumericLimits<float>::Max();
	float BestT = 0.f;
	for (int32 i = 0; i + 1 < RiverPoints.Num(); ++i)
	{
		float T = 0.f;
		const float D = DistToSegment2D(P, RiverPoints[i], RiverPoints[i + 1], T);
		if (D < Best)
		{
			Best = D;
			const float SegLen = RiverCum[i + 1] - RiverCum[i];
			BestT = (RiverCum[i] + SegLen * T) / FMath::Max(RiverLength, 1.f);
		}
	}
	if (OutT)
	{
		*OutT = BestT;
	}
	return Best;
}

float ATOWorldGenerator::RiverWaterLevel(float T) const
{
	return FMath::Lerp(450.f, 100.f, FMath::Clamp(T, 0.f, 1.f));
}

float ATOWorldGenerator::ReservoirWeight(float X, float Y) const
{
	const float Xm = X / 100.f;
	const float Ym = Y / 100.f;
	const float DamNorth = DamX / 100.f + 12.f;
	if (Xm <= DamNorth)
	{
		return 0.f;
	}
	const float W = 200.f + FMath::Min(380.f, (Xm - DamNorth) * 0.85f);
	const float A = FMath::Abs(Ym) / W;
	const float B = (Xm - DamNorth) / (1080.f - DamNorth);
	const float D = FMath::Max(A, B * B * B);
	return 1.f - SmoothStep01(0.82f, 1.0f, D);
}

float ATOWorldGenerator::NaturalHeight(float X, float Y) const
{
	const float Xm = X / 100.f;
	const float Ym = Y / 100.f;

	float Dr = 9999.f;
	if (RiverPoints.Num() > 1)
	{
		Dr = RiverDistance(X, Y) / 100.f;
	}

	const float N = Noise2(Xm / 420.f, Ym / 420.f) * 14.f + Noise2(Xm / 160.f + 11.3f, Ym / 160.f + 7.1f) * 5.f + Noise2(Xm / 55.f + 3.7f, Ym / 55.f + 5.9f) * 1.2f;
	const float NoiseMask = SmoothStep01(30.f, 220.f, Dr);
	float H = 6.f + FMath::Min(Dr, 900.f) * 0.028f + N * (0.25f + 0.75f * NoiseMask);

	// Map edge mountains
	const float Edge = 1250.f - FMath::Max(FMath::Abs(Xm), FMath::Abs(Ym));
	const float M = SmoothStep01(320.f, 0.f, Edge);
	H += M * M * 210.f + M * FMath::Abs(Noise2(Xm / 90.f, Ym / 90.f)) * 50.f;

	// Hills
	H += Hill(Xm, Ym, -300.f, -900.f, 220.f, 28.f);
	H += Hill(Xm, Ym, -650.f, 760.f, 240.f, 30.f);
	H += Hill(Xm, Ym, -900.f, 300.f, 180.f, 18.f);
	H += Hill(Xm, Ym, 0.f, 760.f, 200.f, 22.f);

	// Gorge shoulders around the dam and the high plateau north of it
	const float DamM = DamX / 100.f;
	const float ShoulderX = SmoothStep01(DamM - 150.f, DamM - 80.f, Xm);
	const float ShoulderY = SmoothStep01(195.f, 265.f, FMath::Abs(Ym));
	const float Plateau = 65.f + N * 0.4f;
	H = FMath::Lerp(H, FMath::Max(H, Plateau), ShoulderX * ShoulderY);
	if (Xm > DamM + 10.f)
	{
		const float North = SmoothStep01(DamM + 10.f, DamM + 40.f, Xm);
		H = FMath::Lerp(H, FMath::Max(H, Plateau), North);
	}

	return H * 100.f;
}

float ATOWorldGenerator::PadWeight(const FTOPad& Pad, float X, float Y) const
{
	const FVector2D Local = FVector2D(FVector(X - Pad.Center.X, Y - Pad.Center.Y, 0.f).RotateAngleAxis(-Pad.Yaw, FVector::UpVector));
	const float Dx = FMath::Max(0.f, FMath::Abs(Local.X) - Pad.HalfSize.X);
	const float Dy = FMath::Max(0.f, FMath::Abs(Local.Y) - Pad.HalfSize.Y);
	const float D = FMath::Sqrt(Dx * Dx + Dy * Dy);
	return 1.f - SmoothStep01(0.f, Pad.Blend, D);
}

float ATOWorldGenerator::RoadInfluence(float X, float Y, float& OutRoadH, float& OutDist) const
{
	const FVector2D P(X, Y);
	float BestW = 0.f;
	OutDist = TNumericLimits<float>::Max();
	OutRoadH = 0.f;
	for (const FTORoad& R : Roads)
	{
		const float Reach = R.Width * 0.5f + 1300.f;
		for (int32 i = 0; i + 1 < R.Points.Num(); ++i)
		{
			const FVector2D& A = R.Points[i];
			const FVector2D& B = R.Points[i + 1];
			if (P.X < FMath::Min(A.X, B.X) - Reach || P.X > FMath::Max(A.X, B.X) + Reach || P.Y < FMath::Min(A.Y, B.Y) - Reach || P.Y > FMath::Max(A.Y, B.Y) + Reach)
			{
				continue;
			}
			float T = 0.f;
			const float D = DistToSegment2D(P, A, B, T);
			if (D < OutDist)
			{
				OutDist = D;
				const float H0 = R.Heights.IsValidIndex(i) ? R.Heights[i] : 0.f;
				const float H1 = R.Heights.IsValidIndex(i + 1) ? R.Heights[i + 1] : H0;
				OutRoadH = FMath::Lerp(H0, H1, T);
				BestW = 1.f - SmoothStep01(R.Width * 0.5f + 150.f, R.Width * 0.5f + 1300.f, D);
			}
		}
	}
	return BestW;
}

float ATOWorldGenerator::HeightStage(float X, float Y, int32 Stage) const
{
	float H = NaturalHeight(X, Y);

	// River valley
	if (RiverPoints.Num() > 1 && X < DamX + 1500.f)
	{
		float T = 0.f;
		const float D = RiverDistance(X, Y, &T);
		const float L = RiverWaterLevel(T);
		if (D < 2600.f)
		{
			H = L - 140.f + 90.f * SmoothStep01(0.f, 2600.f, D);
		}
		else if (D < 9000.f)
		{
			const float Bank = L - 50.f + (D - 2600.f) * 0.08f;
			H = FMath::Lerp(Bank, H, SmoothStep01(2600.f, 9000.f, D));
		}
	}

	// Reservoir basin
	const float RW = ReservoirWeight(X, Y);
	if (RW > 0.f)
	{
		const float Xm = X / 100.f;
		const float Ym = Y / 100.f;
		const float DamNorth = DamX / 100.f + 12.f;
		const float W = 200.f + FMath::Min(380.f, (Xm - DamNorth) * 0.85f);
		const float D = FMath::Max(FMath::Abs(Ym) / W, FMath::Pow((Xm - DamNorth) / (1080.f - DamNorth), 3.f));
		const float Bed = 2600.f + 2400.f * FMath::Clamp(D, 0.f, 1.f);
		H = FMath::Lerp(H, Bed, RW);
	}

	if (Stage >= 1)
	{
		for (const FTOPad& Pad : Pads)
		{
			const float Reach = FMath::Max(Pad.HalfSize.X, Pad.HalfSize.Y) * 1.5f + Pad.Blend;
			if (FMath::Abs(X - Pad.Center.X) > Reach || FMath::Abs(Y - Pad.Center.Y) > Reach)
			{
				continue;
			}
			const float W = PadWeight(Pad, X, Y);
			if (W > 0.f)
			{
				H = FMath::Lerp(H, Pad.Height - 5.f, W);
			}
		}
	}

	if (Stage >= 2 && Roads.Num() > 0)
	{
		float RoadH = 0.f;
		float Dist = 0.f;
		const float W = RoadInfluence(X, Y, RoadH, Dist);
		if (W > 0.f)
		{
			H = FMath::Lerp(H, RoadH - 12.f, W);
		}
	}
	return H;
}

float ATOWorldGenerator::GetTerrainHeight(float X, float Y) const
{
	return HeightStage(X, Y, 2);
}

FVector ATOWorldGenerator::TerrainNormal(float X, float Y) const
{
	const float E = 300.f;
	const float Hx0 = HeightStage(X - E, Y, 2);
	const float Hx1 = HeightStage(X + E, Y, 2);
	const float Hy0 = HeightStage(X, Y - E, 2);
	const float Hy1 = HeightStage(X, Y + E, 2);
	return FVector(Hx0 - Hx1, Hy0 - Hy1, 2.f * E).GetSafeNormal();
}

bool ATOWorldGenerator::GetWaterHeight(const FVector& Location, float& OutZ) const
{
	if (ReservoirWeight(Location.X, Location.Y) > 0.05f && Location.Z < ReservoirLevel + 300.f)
	{
		OutZ = ReservoirLevel;
		return true;
	}
	if (RiverPoints.Num() > 1 && Location.X < DamX)
	{
		float T = 0.f;
		const float D = RiverDistance(Location.X, Location.Y, &T);
		if (D < 2500.f)
		{
			OutZ = RiverWaterLevel(T);
			return Location.Z < OutZ + 400.f;
		}
	}
	return false;
}

FVector ATOWorldGenerator::GroundPoint(float X, float Y, float ZStart) const
{
	UWorld* World = GetWorld();
	if (World)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TOGround), false);
		if (World->LineTraceSingleByChannel(Hit, FVector(X, Y, ZStart), FVector(X, Y, -20000.f), ECC_WorldStatic, Params))
		{
			return Hit.ImpactPoint;
		}
	}
	return FVector(X, Y, GetTerrainHeight(X, Y));
}

FVector ATOWorldGenerator::OnGround(float X, float Y, float Offset) const
{
	return FVector(X, Y, GetTerrainHeight(X, Y) + Offset);
}

FVector2D ATOWorldGenerator::WorldToMapUV(const FVector& World) const
{
	return FVector2D((World.Y + HalfSize) / (2.f * HalfSize), (HalfSize - World.X) / (2.f * HalfSize));
}

bool ATOWorldGenerator::IsTerrainReady() const
{
	UWorld* World = GetWorld();
	if (!World || !bGenerated)
	{
		return false;
	}
	// Async cooked collision is ready once a ray hits the terrain near the center and the insertion points.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TOReady), true);
	TArray<FVector> Probes;
	Probes.Add(FVector(0.f, 0.f, 0.f));
	for (const FTOSpawnDef& S : Insertions)
	{
		Probes.Add(S.Location);
	}
	for (const FVector& P : Probes)
	{
		if (!World->LineTraceSingleByChannel(Hit, FVector(P.X, P.Y, 80000.f), FVector(P.X, P.Y, -20000.f), ECC_WorldStatic, Params))
		{
			return false;
		}
	}
	return true;
}

ETOMat ATOWorldGenerator::SurfaceAt(float X, float Y, const FVector& Normal, float H) const
{
	const float Slope = Normal.Z;
	const float N = Noise2(X / 9000.f + 4.1f, Y / 9000.f + 1.7f);
	const float N2 = Noise2(X / 2500.f + 9.3f, Y / 2500.f + 2.2f);

	for (const FTOPad& Pad : Pads)
	{
		const float Reach = FMath::Max(Pad.HalfSize.X, Pad.HalfSize.Y) * 1.5f;
		if (FMath::Abs(X - Pad.Center.X) > Reach || FMath::Abs(Y - Pad.Center.Y) > Reach)
		{
			continue;
		}
		if (PadWeight(Pad, X, Y) > 0.9f)
		{
			return Pad.Surface;
		}
	}
	if (Slope < 0.72f)
	{
		return N2 > 0.f ? ETOMat::Rock : ETOMat::RockDark;
	}
	if (RiverPoints.Num() > 1 && X < DamX + 1000.f)
	{
		const float D = RiverDistance(X, Y);
		if (D < 3600.f)
		{
			return D < 2900.f ? ETOMat::Gravel : ETOMat::Sand;
		}
	}
	if (ReservoirWeight(X, Y) > 0.02f && H < ReservoirLevel + 400.f)
	{
		return ETOMat::Gravel;
	}
	if (H > 16000.f || Slope < 0.82f)
	{
		return N > 0.1f ? ETOMat::Rock : ETOMat::GrassDry;
	}
	if (N2 > 0.35f)
	{
		return ETOMat::Dirt;
	}
	return N > 0.f ? ETOMat::Grass : ETOMat::GrassDry;
}

// =============================================================================================
//  Batching
// =============================================================================================

void ATOWorldGenerator::AddInstance(ETOShape Shape, ETOMat Mat, ETOCol Col, const FTransform& T)
{
	const uint32 Key = ((uint32)Shape << 16) | ((uint32)Mat << 8) | (uint32)Col;
	PendingInstances.FindOrAdd(Key).Add(T);
}

void ATOWorldGenerator::Box(const FTOFrame& F, const FVector& Center, const FVector& Size, ETOMat Mat, ETOCol Col, const FRotator& Rot)
{
	if (Size.X <= 0.5f || Size.Y <= 0.5f || Size.Z <= 0.5f)
	{
		return;
	}
	AddInstance(ETOShape::Cube, Mat, Col, FTransform(F.ToWorldRot(Rot), F.ToWorld(Center), Size / 100.f));
}

void ATOWorldGenerator::Cyl(const FTOFrame& F, const FVector& Center, float Diameter, float Height, ETOMat Mat, ETOCol Col, const FRotator& Rot)
{
	AddInstance(ETOShape::Cylinder, Mat, Col, FTransform(F.ToWorldRot(Rot), F.ToWorld(Center), FVector(Diameter, Diameter, Height) / 100.f));
}

void ATOWorldGenerator::Sphere(const FTOFrame& F, const FVector& Center, const FVector& Size, ETOMat Mat, ETOCol Col)
{
	AddInstance(ETOShape::Sphere, Mat, Col, FTransform(F.ToWorldRot(FRotator::ZeroRotator), F.ToWorld(Center), Size / 100.f));
}

void ATOWorldGenerator::Cone(const FTOFrame& F, const FVector& Center, float Diameter, float Height, ETOMat Mat, ETOCol Col)
{
	AddInstance(ETOShape::Cone, Mat, Col, FTransform(F.ToWorldRot(FRotator::ZeroRotator), F.ToWorld(Center), FVector(Diameter, Diameter, Height) / 100.f));
}

void ATOWorldGenerator::FinalizeBatches()
{
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	if (!Lib)
	{
		return;
	}
	for (TPair<uint32, TArray<FTransform>>& Pair : PendingInstances)
	{
		if (Pair.Value.Num() == 0)
		{
			continue;
		}
		const ETOShape Shape = (ETOShape)((Pair.Key >> 16) & 0xFF);
		const ETOMat Mat = (ETOMat)((Pair.Key >> 8) & 0xFF);
		const ETOCol Col = (ETOCol)(Pair.Key & 0xFF);

		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
		H->SetMobility(EComponentMobility::Static);
		H->SetStaticMesh(Lib->GetMesh(Shape));
		H->SetMaterial(0, Lib->GetMaterial(Mat));
		H->SetupAttachment(Root);
		H->ComponentTags.Add(UTOMaterialLibrary::GetSurfaceTag(Mat));
		switch (Col)
		{
		case ETOCol::Solid:
			H->SetCollisionProfileName(TEXT("BlockAll"));
			H->SetCollisionResponseToChannel(TO_TRACE_BULLET, ECR_Block);
			H->SetCanEverAffectNavigation(true);
			break;
		case ETOCol::Foliage:
			H->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			H->SetCollisionResponseToAllChannels(ECR_Ignore);
			H->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
			H->SetCanEverAffectNavigation(false);
			break;
		case ETOCol::PawnOnly:
			H->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			H->SetCollisionResponseToAllChannels(ECR_Ignore);
			H->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
			H->SetCanEverAffectNavigation(false);
			H->SetHiddenInGame(true);
			break;
		default:
			H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			H->SetCanEverAffectNavigation(false);
			break;
		}
		const FTOMatInfo& Info = UTOMaterialLibrary::GetInfo(Mat);
		const bool bVegetation = Info.Surface == FName(TEXT("Foliage")) || Mat == ETOMat::Bark;
		if (bVegetation)
		{
			H->SetCullDistances(110000, 140000);
		}
		else if (Mat == ETOMat::Wood || Mat == ETOMat::Fabric || Mat == ETOMat::Carpet || Mat == ETOMat::Plastic || Mat == ETOMat::Screen)
		{
			H->SetCullDistances(14000, 20000);
		}
		H->SetCastShadow(Col != ETOCol::PawnOnly && Info.Kind != 2 && Info.Kind != 4);
		H->RegisterComponent();
		H->AddInstances(Pair.Value, false, true);
		Batches.Add(Pair.Key, H);
	}
	PendingInstances.Reset();
}

// =============================================================================================
//  Construction helpers
// =============================================================================================

void ATOWorldGenerator::Wall(const FTOFrame& F, const FVector2D& A, const FVector2D& B, float Z0, float Height, float Thick, ETOMat Mat, TArray<FTOOpening> Openings)
{
	const FVector2D AB = B - A;
	const float Len = AB.Size();
	if (Len < 1.f)
	{
		return;
	}
	const FVector2D Dir = AB / Len;
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Dir.Y, Dir.X));
	Openings.Sort([](const FTOOpening& L, const FTOOpening& R) { return L.Center < R.Center; });

	auto Piece = [&](float S0, float S1, float Zb, float Zt)
	{
		if (S1 - S0 < 1.f || Zt - Zb < 1.f)
		{
			return;
		}
		const float Mid = (S0 + S1) * 0.5f;
		const FVector2D P = A + Dir * Mid;
		Box(F, FVector(P.X, P.Y, Z0 + (Zb + Zt) * 0.5f), FVector(S1 - S0, Thick, Zt - Zb), Mat, ETOCol::Solid, FRotator(0.f, Yaw, 0.f));
	};

	float Cursor = 0.f;
	for (const FTOOpening& O : Openings)
	{
		const float S0 = FMath::Clamp(O.Center - O.Width * 0.5f, 0.f, Len);
		const float S1 = FMath::Clamp(O.Center + O.Width * 0.5f, 0.f, Len);
		Piece(Cursor, S0, 0.f, Height);
		if (O.Bottom > 0.f)
		{
			Piece(S0, S1, 0.f, O.Bottom);
		}
		if (O.Top < Height)
		{
			Piece(S0, S1, O.Top, Height);
		}
		Cursor = FMath::Max(Cursor, S1);
	}
	Piece(Cursor, Len, 0.f, Height);
}

void ATOWorldGenerator::SlabWithHole(const FTOFrame& F, const FBox2D& Outer, const FBox2D& Hole, float Z, float Thick, ETOMat Mat, bool bHasHole)
{
	auto Rect = [&](float X0, float X1, float Y0, float Y1)
	{
		if (X1 - X0 < 1.f || Y1 - Y0 < 1.f)
		{
			return;
		}
		Box(F, FVector((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, Z - Thick * 0.5f), FVector(X1 - X0, Y1 - Y0, Thick), Mat);
	};
	if (!bHasHole)
	{
		Rect(Outer.Min.X, Outer.Max.X, Outer.Min.Y, Outer.Max.Y);
		return;
	}
	Rect(Outer.Min.X, Hole.Min.X, Outer.Min.Y, Outer.Max.Y);
	Rect(Hole.Max.X, Outer.Max.X, Outer.Min.Y, Outer.Max.Y);
	Rect(Hole.Min.X, Hole.Max.X, Outer.Min.Y, Hole.Min.Y);
	Rect(Hole.Min.X, Hole.Max.X, Hole.Max.Y, Outer.Max.Y);
}

void ATOWorldGenerator::Stairs(const FTOFrame& F, const FVector& Start, float Run, float Rise, float Width, ETOMat Mat)
{
	// Straight flight along local +X starting at Start (bottom), thin floating treads + stringer.
	const int32 Steps = FMath::Max(2, FMath::RoundToInt(Rise / 20.f));
	const float StepH = Rise / Steps;
	const float StepD = Run / Steps;
	for (int32 i = 0; i < Steps; ++i)
	{
		const float X = Start.X + StepD * (i + 0.5f);
		const float Z = Start.Z + StepH * (i + 1) - 10.f;
		Box(F, FVector(X, Start.Y, Z), FVector(StepD + 2.f, Width, 20.f), Mat);
	}
	// Underside stringer (visual)
	const float Len = FMath::Sqrt(Run * Run + Rise * Rise);
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Rise, Run));
	Box(F, FVector(Start.X + Run * 0.5f, Start.Y, Start.Z + Rise * 0.5f - 22.f), FVector(Len, Width, 8.f), Mat, ETOCol::None, FRotator(Pitch, 0.f, 0.f));
	// Hand rail
	Box(F, FVector(Start.X + Run * 0.5f, Start.Y + Width * 0.5f, Start.Z + Rise * 0.5f + 95.f), FVector(Len, 5.f, 5.f), ETOMat::MetalDark, ETOCol::None, FRotator(Pitch, 0.f, 0.f));
}

void ATOWorldGenerator::Railing(const FTOFrame& F, const FVector2D& A, const FVector2D& B, float Z, ETOMat Mat)
{
	const FVector2D AB = B - A;
	const float Len = AB.Size();
	if (Len < 1.f)
	{
		return;
	}
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(AB.Y, AB.X));
	const FVector2D Mid = (A + B) * 0.5f;
	Box(F, FVector(Mid.X, Mid.Y, Z + 100.f), FVector(Len, 6.f, 6.f), Mat, ETOCol::Solid, FRotator(0.f, Yaw, 0.f));
	Box(F, FVector(Mid.X, Mid.Y, Z + 55.f), FVector(Len, 4.f, 4.f), Mat, ETOCol::None, FRotator(0.f, Yaw, 0.f));
	const int32 Posts = FMath::Max(2, FMath::CeilToInt(Len / 150.f) + 1);
	for (int32 i = 0; i < Posts; ++i)
	{
		const FVector2D P = A + AB * ((float)i / (Posts - 1));
		Box(F, FVector(P.X, P.Y, Z + 50.f), FVector(6.f, 6.f, 100.f), Mat, ETOCol::None);
	}
}

void ATOWorldGenerator::AddLight(const FVector& World, const FLinearColor& Color, float Intensity, float Radius, bool bNightOnly)
{
	if (bNightOnly && TimeOfDay == ETOTimeOfDay::Day)
	{
		return;
	}
	if (Lights.Num() >= LightBudget)
	{
		return;
	}
	UPointLightComponent* L = NewObject<UPointLightComponent>(this);
	L->SetMobility(EComponentMobility::Movable);
	L->SetupAttachment(Root);
	L->SetWorldLocation(World);
	L->SetIntensityUnits(ELightUnits::Candelas);
	L->SetIntensity(Intensity * (TimeOfDay == ETOTimeOfDay::Night ? 1.f : 0.6f));
	L->SetAttenuationRadius(Radius);
	L->SetLightColor(Color);
	L->SetCastShadows(false);
	L->SetVolumetricScatteringIntensity(TimeOfDay == ETOTimeOfDay::Night ? 1.5f : 0.3f);
	L->RegisterComponent();
	Lights.Add(L);
}

int32 ATOWorldGenerator::AddPOI(const FString& Name, const FVector& Location, float Radius, int32 Tier, bool bMajor)
{
	FTOPOI P;
	P.Name = Name;
	P.Location = Location;
	P.Radius = Radius;
	P.Tier = Tier;
	P.bMajor = bMajor;
	return POIs.Add(P);
}

void ATOWorldGenerator::AddLoot(const FVector& World, float Yaw, ETOContainerType Type, int32 Tier, int32 Poi)
{
	FTOLootSpot L;
	L.Location = World;
	L.Yaw = Yaw;
	L.Type = Type;
	L.Tier = Tier;
	L.Poi = Poi;
	LootSpots.Add(L);
}

void ATOWorldGenerator::AddGuard(const FVector& World, float Yaw, int32 Poi, int32 Tier, bool bBoss, bool bOverwatch)
{
	FTOGuardPost G;
	G.Location = World;
	G.Yaw = Yaw;
	G.Poi = Poi;
	G.Tier = Tier;
	G.bBoss = bBoss;
	G.bOverwatch = bOverwatch;
	GuardPosts.Add(G);
}

void ATOWorldGenerator::AddDoor(const FVector& World, float Yaw, float Width, float Height, FName Key, ETOMat Mat)
{
	FTODoorDef D;
	D.Location = World;
	D.Yaw = Yaw;
	D.Width = Width;
	D.Height = Height;
	D.KeyId = Key;
	D.Mat = Mat;
	Doors.Add(D);
}

float ATOWorldGenerator::PadHeight(const FString& Name) const
{
	for (const FTOPad& P : Pads)
	{
		if (P.Name == Name)
		{
			return P.Height - 5.f;
		}
	}
	return 0.f;
}

FTOPad& ATOWorldGenerator::AddPad(const FString& Name, const FVector2D& Center, const FVector2D& InHalfSize, float Yaw, float Blend, ETOMat Surface, float FixedHeight)
{
	FTOPad P;
	P.Name = Name;
	P.Center = Center;
	P.HalfSize = InHalfSize;
	P.Yaw = Yaw;
	P.Blend = Blend;
	P.Surface = Surface;
	P.Height = FixedHeight >= 0.f ? FixedHeight : HeightStage(Center.X, Center.Y, 0);
	const int32 Idx = Pads.Add(P);
	return Pads[Idx];
}

// =============================================================================================
//  Roads
// =============================================================================================

void ATOWorldGenerator::PrepareRoads()
{
	for (FTORoad& R : Roads)
	{
		// Densify to ~10 m spacing
		TArray<FVector2D> Dense;
		for (int32 i = 0; i + 1 < R.Points.Num(); ++i)
		{
			const FVector2D A = R.Points[i];
			const FVector2D B = R.Points[i + 1];
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / 1000.f));
			for (int32 s = 0; s < Steps; ++s)
			{
				Dense.Add(FMath::Lerp(A, B, (float)s / Steps));
			}
		}
		if (R.Points.Num() > 0)
		{
			Dense.Add(R.Points.Last());
		}
		R.Points = Dense;

		TArray<float> Raw;
		for (const FVector2D& P : R.Points)
		{
			Raw.Add(HeightStage(P.X, P.Y, 1));
		}
		// Smooth so roads have gentle grades
		R.Heights.SetNum(Raw.Num());
		for (int32 i = 0; i < Raw.Num(); ++i)
		{
			float Sum = 0.f;
			float Wsum = 0.f;
			for (int32 k = -5; k <= 5; ++k)
			{
				const int32 j = FMath::Clamp(i + k, 0, Raw.Num() - 1);
				const float W = 1.f - FMath::Abs(k) / 6.f;
				Sum += Raw[j] * W;
				Wsum += W;
			}
			R.Heights[i] = Sum / Wsum;
		}
		// Keep the end points exactly on their pads / bridge abutments
		if (Raw.Num() > 1)
		{
			R.Heights[0] = Raw[0];
			R.Heights.Last() = Raw.Last();
		}
	}
}

void ATOWorldGenerator::BuildRoads()
{
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	if (!Lib)
	{
		return;
	}
	for (const FTORoad& R : Roads)
	{
		if (R.Points.Num() < 2)
		{
			continue;
		}
		TArray<FVector> V;
		TArray<int32> T;
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> C;
		TArray<FProcMeshTangent> Tan;
		TArray<FVector> MV;
		TArray<int32> MT;
		TArray<FVector> MN;
		TArray<FVector2D> MUV;
		TArray<FLinearColor> MC;
		TArray<FProcMeshTangent> MTan;

		float Dist = 0.f;
		for (int32 i = 0; i < R.Points.Num(); ++i)
		{
			const FVector2D Prev = R.Points[FMath::Max(0, i - 1)];
			const FVector2D Next = R.Points[FMath::Min(R.Points.Num() - 1, i + 1)];
			FVector2D Dir = (Next - Prev).GetSafeNormal();
			const FVector2D Side(-Dir.Y, Dir.X);
			const float Z = R.Heights[i] + 8.f;
			if (i > 0)
			{
				Dist += FVector2D::Distance(R.Points[i - 1], R.Points[i]);
			}
			const FVector2D L = R.Points[i] - Side * R.Width * 0.5f;
			const FVector2D Rt = R.Points[i] + Side * R.Width * 0.5f;
			V.Add(FVector(L.X, L.Y, Z));
			V.Add(FVector(Rt.X, Rt.Y, Z));
			N.Add(FVector::UpVector);
			N.Add(FVector::UpVector);
			UV.Add(FVector2D(0.f, Dist / 400.f));
			UV.Add(FVector2D(R.Width / 400.f, Dist / 400.f));
			C.Add(FLinearColor::White);
			C.Add(FLinearColor::White);
			Tan.Add(FProcMeshTangent(FVector(Dir.X, Dir.Y, 0.f), false));
			Tan.Add(FProcMeshTangent(FVector(Dir.X, Dir.Y, 0.f), false));
			if (i > 0)
			{
				const int32 B0 = (i - 1) * 2;
				const int32 B1 = i * 2;
				T.Append({ B0, B0 + 1, B1, B0 + 1, B1 + 1, B1 });
			}

			// Dashed center line (3 m on / 6 m off)
			if (R.bMarkings && !R.bDirt && i + 1 < R.Points.Num() && FMath::Fmod(Dist, 900.f) < 300.f)
			{
				const FVector2D P0 = R.Points[i];
				const FVector2D P1 = FMath::Lerp(R.Points[i], R.Points[i + 1], 0.3f);
				const float Z1 = FMath::Lerp(R.Heights[i], R.Heights[i + 1], 0.3f) + 10.f;
				const int32 Base = MV.Num();
				const FVector2D S = Side * 8.f;
				MV.Add(FVector(P0.X - S.X, P0.Y - S.Y, Z + 2.f));
				MV.Add(FVector(P0.X + S.X, P0.Y + S.Y, Z + 2.f));
				MV.Add(FVector(P1.X - S.X, P1.Y - S.Y, Z1));
				MV.Add(FVector(P1.X + S.X, P1.Y + S.Y, Z1));
				for (int32 k = 0; k < 4; ++k)
				{
					MN.Add(FVector::UpVector);
					MUV.Add(FVector2D(k % 2, k / 2));
					MC.Add(FLinearColor::White);
					MTan.Add(FProcMeshTangent(FVector(Dir.X, Dir.Y, 0.f), false));
				}
				MT.Append({ Base, Base + 1, Base + 2, Base + 1, Base + 3, Base + 2 });
			}
		}

		UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this);
		PMC->SetupAttachment(Root);
		PMC->bUseAsyncCooking = true;
		PMC->CreateMeshSection_LinearColor(0, V, T, N, UV, C, Tan, true);
		PMC->SetMaterial(0, Lib->GetMaterial(R.bDirt ? ETOMat::Dirt : ETOMat::Asphalt));
		if (MV.Num() > 0)
		{
			PMC->CreateMeshSection_LinearColor(1, MV, MT, MN, MUV, MC, MTan, false);
			PMC->SetMaterial(1, Lib->GetMaterial(ETOMat::RoadLineYellow));
		}
		PMC->SetCollisionProfileName(TEXT("BlockAll"));
		PMC->ComponentTags.Add(FName(TEXT("Concrete")));
		PMC->SetCastShadow(false);
		PMC->RegisterComponent();
		ExtraMeshes.Add(PMC);
	}
}

// =============================================================================================
//  Terrain
// =============================================================================================

void ATOWorldGenerator::BuildTerrain()
{
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	if (!Lib)
	{
		return;
	}
	const int32 NV = GTerrainQuads + 1;
	const float Cell = 2.f * HalfSize / GTerrainQuads;

	TArray<float> Heights;
	Heights.SetNumUninitialized(NV * NV);
	ParallelFor(NV, [&](int32 j)
	{
		for (int32 i = 0; i < NV; ++i)
		{
			const float X = -HalfSize + i * Cell;
			const float Y = -HalfSize + j * Cell;
			Heights[j * NV + i] = HeightStage(X, Y, 2);
		}
	});

	auto HAt = [&](int32 i, int32 j) -> float
	{
		i = FMath::Clamp(i, 0, NV - 1);
		j = FMath::Clamp(j, 0, NV - 1);
		return Heights[j * NV + i];
	};
	auto NormalAt = [&](int32 i, int32 j) -> FVector
	{
		const float Dx = HAt(i - 1, j) - HAt(i + 1, j);
		const float Dy = HAt(i, j - 1) - HAt(i, j + 1);
		return FVector(Dx, Dy, 2.f * Cell).GetSafeNormal();
	};

	// Per-quad surface classification
	TArray<uint8> QuadMat;
	QuadMat.SetNumUninitialized(GTerrainQuads * GTerrainQuads);
	ParallelFor(GTerrainQuads, [&](int32 j)
	{
		for (int32 i = 0; i < GTerrainQuads; ++i)
		{
			const float X = -HalfSize + (i + 0.5f) * Cell;
			const float Y = -HalfSize + (j + 0.5f) * Cell;
			const float H = (HAt(i, j) + HAt(i + 1, j) + HAt(i, j + 1) + HAt(i + 1, j + 1)) * 0.25f;
			const FVector Nrm = (NormalAt(i, j) + NormalAt(i + 1, j + 1)).GetSafeNormal();
			QuadMat[j * GTerrainQuads + i] = (uint8)SurfaceAt(X, Y, Nrm, H);
		}
	});

	const int32 Chunks = GTerrainQuads / GChunkQuads;
	for (int32 cy = 0; cy < Chunks; ++cy)
	{
		for (int32 cx = 0; cx < Chunks; ++cx)
		{
			TMap<uint8, FSectionBuild> Sections;
			for (int32 qj = cy * GChunkQuads; qj < (cy + 1) * GChunkQuads; ++qj)
			{
				for (int32 qi = cx * GChunkQuads; qi < (cx + 1) * GChunkQuads; ++qi)
				{
					const uint8 M = QuadMat[qj * GTerrainQuads + qi];
					FSectionBuild& S = Sections.FindOrAdd(M);
					int32 Idx[4];
					const int32 Corners[4][2] = { { qi, qj }, { qi + 1, qj }, { qi, qj + 1 }, { qi + 1, qj + 1 } };
					for (int32 k = 0; k < 4; ++k)
					{
						const int32 gi = Corners[k][0];
						const int32 gj = Corners[k][1];
						const int32 Key = gj * NV + gi;
						if (int32* Found = S.Remap.Find(Key))
						{
							Idx[k] = *Found;
							continue;
						}
						const float X = -HalfSize + gi * Cell;
						const float Y = -HalfSize + gj * Cell;
						const float Z = Heights[Key];
						const FVector Nrm = NormalAt(gi, gj);
						const float Tint = 0.9f + 0.2f * (0.5f + 0.5f * Noise2(X / 3000.f + 1.3f, Y / 3000.f + 7.7f));
						const float Shade = FMath::Lerp(0.85f, 1.f, Nrm.Z);
						const int32 NewIdx = S.Verts.Add(FVector(X, Y, Z));
						S.Normals.Add(Nrm);
						S.UVs.Add(FVector2D(X / 400.f, Y / 400.f));
						S.Colors.Add(FLinearColor(Tint * Shade, Tint * Shade, Tint * Shade * 0.97f, 1.f));
						const FVector TanX = FVector::CrossProduct(FVector(0.f, 1.f, 0.f), Nrm).GetSafeNormal();
						S.Tangents.Add(FProcMeshTangent(TanX, false));
						S.Remap.Add(Key, NewIdx);
						Idx[k] = NewIdx;
					}
					// Triangle winding: counter-clockwise when viewed from above in UE (left handed) -> (0,2,1),(1,2,3)
					S.Tris.Append({ Idx[0], Idx[2], Idx[1], Idx[1], Idx[2], Idx[3] });
				}
			}

			UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this);
			PMC->SetMobility(EComponentMobility::Static);
			PMC->SetupAttachment(Root);
			PMC->bUseAsyncCooking = true;
			int32 SectionIndex = 0;
			for (TPair<uint8, FSectionBuild>& Pair : Sections)
			{
				FSectionBuild& S = Pair.Value;
				PMC->CreateMeshSection_LinearColor(SectionIndex, S.Verts, S.Tris, S.Normals, S.UVs, S.Colors, S.Tangents, true);
				PMC->SetMaterial(SectionIndex, Lib->GetMaterial((ETOMat)Pair.Key));
				++SectionIndex;
			}
			PMC->SetCollisionProfileName(TEXT("BlockAll"));
			PMC->SetCollisionResponseToChannel(TO_TRACE_BULLET, ECR_Block);
			PMC->ComponentTags.Add(FName(TEXT("Dirt")));
			PMC->SetCastShadow(true);
			PMC->RegisterComponent();
			TerrainChunks.Add(PMC);
		}
	}
}

// =============================================================================================
//  Water
// =============================================================================================

void ATOWorldGenerator::BuildWater()
{
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	if (!Lib)
	{
		return;
	}
	auto MakeWaterPMC = [&](const TArray<FVector>& V, const TArray<int32>& T, ETOMat Mat)
	{
		TArray<FVector> N;
		TArray<FVector2D> UV;
		TArray<FLinearColor> C;
		TArray<FProcMeshTangent> Tan;
		for (const FVector& P : V)
		{
			N.Add(FVector::UpVector);
			UV.Add(FVector2D(P.X / 1000.f, P.Y / 1000.f));
			C.Add(FLinearColor::White);
			Tan.Add(FProcMeshTangent(FVector(1.f, 0.f, 0.f), false));
		}
		UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this);
		PMC->SetupAttachment(Root);
		PMC->CreateMeshSection_LinearColor(0, V, T, N, UV, C, Tan, true);
		PMC->SetMaterial(0, Lib->GetMaterial(Mat));
		PMC->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PMC->SetCollisionResponseToAllChannels(ECR_Ignore);
		PMC->SetCollisionResponseToChannel(TO_TRACE_BULLET, ECR_Block);
		PMC->SetCanEverAffectNavigation(false);
		PMC->ComponentTags.Add(FName(TEXT("Water")));
		PMC->SetCastShadow(false);
		PMC->RegisterComponent();
		ExtraMeshes.Add(PMC);
	};

	// River ribbon following the channel (slopes gently downstream)
	if (RiverPoints.Num() > 1)
	{
		TArray<FVector> V;
		TArray<int32> T;
		int32 Row = 0;
		for (int32 i = 0; i + 1 < RiverPoints.Num(); ++i)
		{
			const FVector2D A = RiverPoints[i];
			const FVector2D B = RiverPoints[i + 1];
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(A, B) / 1500.f));
			for (int32 s = 0; s < Steps || (i + 2 == RiverPoints.Num() && s == Steps); ++s)
			{
				const float F = (float)s / Steps;
				const FVector2D P = FMath::Lerp(A, B, F);
				const FVector2D Dir = (B - A).GetSafeNormal();
				const FVector2D Side(-Dir.Y, Dir.X);
				const float TT = (RiverCum[i] + (RiverCum[i + 1] - RiverCum[i]) * F) / RiverLength;
				const float Z = RiverWaterLevel(TT);
				V.Add(FVector(P.X - Side.X * 2700.f, P.Y - Side.Y * 2700.f, Z));
				V.Add(FVector(P.X + Side.X * 2700.f, P.Y + Side.Y * 2700.f, Z));
				if (Row > 0)
				{
					const int32 B0 = (Row - 1) * 2;
					const int32 B1 = Row * 2;
					T.Append({ B0, B0 + 1, B1, B0 + 1, B1 + 1, B1 });
				}
				++Row;
			}
		}
		MakeWaterPMC(V, T, ETOMat::WaterRiver);
	}

	// Reservoir lake behind the dam
	{
		const float X0 = DamX + 1100.f;
		const float X1 = 118000.f;
		const float Y0 = -72000.f;
		const float Y1 = 72000.f;
		const float Z = ReservoirLevel;
		TArray<FVector> V = { FVector(X0, Y0, Z), FVector(X1, Y0, Z), FVector(X0, Y1, Z), FVector(X1, Y1, Z) };
		TArray<int32> T = { 0, 2, 1, 1, 2, 3 };
		MakeWaterPMC(V, T, ETOMat::Water);
	}
}

// =============================================================================================
//  Vegetation, rocks
// =============================================================================================

void ATOWorldGenerator::BuildTree(const FVector& Base, int32 Kind, float Scale, FRandomStream& Rng)
{
	const FTOFrame F(Base, Rng.FRandRange(0.f, 360.f));
	const float S = Scale;
	if (Kind == 0)
	{
		// Pine
		Cyl(F, FVector(0.f, 0.f, 350.f * S), 38.f * S, 700.f * S, ETOMat::Bark, ETOCol::Solid);
		Cone(F, FVector(0.f, 0.f, 420.f * S), 430.f * S, 480.f * S, ETOMat::FoliagePine);
		Cone(F, FVector(0.f, 0.f, 640.f * S), 340.f * S, 420.f * S, ETOMat::FoliagePine);
		Cone(F, FVector(0.f, 0.f, 840.f * S), 230.f * S, 360.f * S, ETOMat::FoliagePine);
	}
	else
	{
		// Broadleaf
		const ETOMat Leaf = Rng.FRand() < 0.5f ? ETOMat::FoliageDark : ETOMat::FoliageLight;
		Cyl(F, FVector(0.f, 0.f, 230.f * S), 45.f * S, 460.f * S, ETOMat::Bark, ETOCol::Solid);
		Sphere(F, FVector(0.f, 0.f, 560.f * S), FVector(520.f, 500.f, 420.f) * S, Leaf, ETOCol::Foliage);
		Sphere(F, FVector(140.f * S, 60.f * S, 470.f * S), FVector(360.f, 340.f, 300.f) * S, Leaf, ETOCol::Foliage);
		Sphere(F, FVector(-120.f * S, -90.f * S, 500.f * S), FVector(330.f, 320.f, 280.f) * S, Leaf, ETOCol::Foliage);
	}
}

void ATOWorldGenerator::BuildRock(const FVector& Base, float Scale, FRandomStream& Rng)
{
	const FVector Size(FMath::FRandRange(220.f, 480.f), FMath::FRandRange(180.f, 380.f), FMath::FRandRange(120.f, 280.f));
	const FRotator Rot(Rng.FRandRange(-20.f, 20.f), Rng.FRandRange(0.f, 360.f), Rng.FRandRange(-20.f, 20.f));
	const ETOMat M = Rng.FRand() < 0.5f ? ETOMat::Rock : ETOMat::RockDark;
	AddInstance(Rng.FRand() < 0.6f ? ETOShape::Sphere : ETOShape::Cube, M, ETOCol::Solid,
		FTransform(Rot, Base + FVector(0.f, 0.f, Size.Z * Scale * 0.15f), Size * Scale / 100.f));
}

void ATOWorldGenerator::BuildVegetation(FRandomStream& Rng)
{
	auto Blocked = [&](float X, float Y, float Margin) -> bool
	{
		for (const FTOPad& Pad : Pads)
		{
			if (FMath::Abs(X - Pad.Center.X) > 40000.f || FMath::Abs(Y - Pad.Center.Y) > 40000.f)
			{
				continue;
			}
			if (PadWeight(Pad, X, Y) > 0.05f)
			{
				return true;
			}
		}
		for (const FBox2D& Fp : Footprints)
		{
			if (X > Fp.Min.X - Margin && X < Fp.Max.X + Margin && Y > Fp.Min.Y - Margin && Y < Fp.Max.Y + Margin)
			{
				return true;
			}
		}
		float RoadH = 0.f;
		float Dist = 0.f;
		RoadInfluence(X, Y, RoadH, Dist);
		if (Dist < 1100.f)
		{
			return true;
		}
		if (RiverPoints.Num() > 1 && X < DamX + 2000.f && RiverDistance(X, Y) < 3400.f)
		{
			return true;
		}
		if (ReservoirWeight(X, Y) > 0.05f && GetTerrainHeight(X, Y) < ReservoirLevel + 200.f)
		{
			return true;
		}
		// Keep the dam & its gorge clear
		if (FMath::Abs(X - DamX) < 9000.f && FMath::Abs(Y) < 26000.f)
		{
			return true;
		}
		return false;
	};

	int32 Trees = 0;
	for (int32 i = 0; i < 16000 && Trees < 4200; ++i)
	{
		const float X = Rng.FRandRange(-HalfSize * 0.985f, HalfSize * 0.985f);
		const float Y = Rng.FRandRange(-HalfSize * 0.985f, HalfSize * 0.985f);
		const float Forest = Noise2(X / 26000.f + 3.3f, Y / 26000.f + 8.8f) + 0.25f * Noise2(X / 7000.f, Y / 7000.f);
		if (Forest < -0.05f || Rng.FRand() > FMath::Clamp(Forest + 0.35f, 0.f, 1.f))
		{
			continue;
		}
		if (Blocked(X, Y, 900.f))
		{
			continue;
		}
		const FVector Nrm = TerrainNormal(X, Y);
		if (Nrm.Z < 0.8f)
		{
			continue;
		}
		const float H = GetTerrainHeight(X, Y);
		const int32 Kind = (H > 5000.f || Rng.FRand() < 0.55f) ? 0 : 1;
		BuildTree(FVector(X, Y, H - 20.f), Kind, Rng.FRandRange(0.8f, 1.35f), Rng);
		++Trees;
		// Undergrowth
		if (Rng.FRand() < 0.5f)
		{
			const FVector B(X + Rng.FRandRange(-500.f, 500.f), Y + Rng.FRandRange(-500.f, 500.f), 0.f);
			const float BH = GetTerrainHeight(B.X, B.Y);
			const float S = Rng.FRandRange(0.6f, 1.3f);
			AddInstance(ETOShape::Sphere, Rng.FRand() < 0.5f ? ETOMat::FoliageDark : ETOMat::FoliageLight, ETOCol::Foliage,
				FTransform(FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f), FVector(B.X, B.Y, BH + 40.f * S), FVector(2.2f, 1.9f, 1.3f) * S));
		}
	}

	// Rocks: cliffs, riverbanks, scattered boulders
	for (int32 i = 0; i < 3500; ++i)
	{
		const float X = Rng.FRandRange(-HalfSize * 0.98f, HalfSize * 0.98f);
		const float Y = Rng.FRandRange(-HalfSize * 0.98f, HalfSize * 0.98f);
		const FVector Nrm = TerrainNormal(X, Y);
		const bool bSteep = Nrm.Z < 0.85f;
		if (!bSteep && Rng.FRand() > 0.12f)
		{
			continue;
		}
		if (Blocked(X, Y, 600.f))
		{
			continue;
		}
		BuildRock(FVector(X, Y, GetTerrainHeight(X, Y)), bSteep ? Rng.FRandRange(1.f, 3.2f) : Rng.FRandRange(0.4f, 1.2f), Rng);
	}
}

void ATOWorldGenerator::BuildBoundary()
{
	const float Edge = HalfSize * 0.985f;
	const FTOFrame F(FVector::ZeroVector, 0.f);
	Box(F, FVector(Edge, 0.f, 20000.f), FVector(200.f, HalfSize * 2.f, 60000.f), ETOMat::Concrete, ETOCol::PawnOnly);
	Box(F, FVector(-Edge, 0.f, 20000.f), FVector(200.f, HalfSize * 2.f, 60000.f), ETOMat::Concrete, ETOCol::PawnOnly);
	Box(F, FVector(0.f, Edge, 20000.f), FVector(HalfSize * 2.f, 200.f, 60000.f), ETOMat::Concrete, ETOCol::PawnOnly);
	Box(F, FVector(0.f, -Edge, 20000.f), FVector(HalfSize * 2.f, 200.f, 60000.f), ETOMat::Concrete, ETOCol::PawnOnly);
}

// =============================================================================================
//  Minimap
// =============================================================================================

void ATOWorldGenerator::BuildMapTexture()
{
	const int32 Size = 512;
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Size * Size);
	const float Step = 2.f * HalfSize / Size;
	const FVector LightDir = FVector(-1.f, -1.f, 2.f).GetSafeNormal();

	TArray<float> H;
	H.SetNumUninitialized(Size * Size);
	ParallelFor(Size, [&](int32 py)
	{
		for (int32 px = 0; px < Size; ++px)
		{
			// pixel -> world: u (px) = east (+Y), v (py) = south (-X)
			const float Y = -HalfSize + (px + 0.5f) * Step;
			const float X = HalfSize - (py + 0.5f) * Step;
			H[py * Size + px] = HeightStage(X, Y, 2);
		}
	});

	ParallelFor(Size, [&](int32 py)
	{
		for (int32 px = 0; px < Size; ++px)
		{
			const float Y = -HalfSize + (px + 0.5f) * Step;
			const float X = HalfSize - (py + 0.5f) * Step;
			const float Hc = H[py * Size + px];
			const float Hl = H[py * Size + FMath::Max(px - 1, 0)];
			const float Hr = H[py * Size + FMath::Min(px + 1, Size - 1)];
			const float Hu = H[FMath::Max(py - 1, 0) * Size + px];
			const float Hd = H[FMath::Min(py + 1, Size - 1) * Size + px];
			// world normal: dX corresponds to -py, dY to +px
			const FVector Nrm = FVector(Hd - Hu, Hl - Hr, 2.f * Step).GetSafeNormal();
			const float Shade = FMath::Clamp(0.55f + 0.6f * FVector::DotProduct(Nrm, LightDir), 0.35f, 1.2f);

			FLinearColor C;
			const ETOMat Surf = SurfaceAt(X, Y, Nrm, Hc);
			switch (Surf)
			{
			case ETOMat::Rock:
			case ETOMat::RockDark: C = FLinearColor(0.42f, 0.41f, 0.38f); break;
			case ETOMat::Gravel:
			case ETOMat::Sand: C = FLinearColor(0.55f, 0.52f, 0.44f); break;
			case ETOMat::Dirt: C = FLinearColor(0.45f, 0.4f, 0.3f); break;
			case ETOMat::Concrete:
			case ETOMat::Asphalt: C = FLinearColor(0.5f, 0.5f, 0.5f); break;
			case ETOMat::GrassDry: C = FLinearColor(0.47f, 0.47f, 0.3f); break;
			default: C = FLinearColor(0.32f, 0.42f, 0.24f); break;
			}
			C *= Shade;

			// Contour lines every 10 m (topographic look)
			const int32 Band = FMath::FloorToInt(Hc / 1000.f);
			if (FMath::FloorToInt(Hr / 1000.f) != Band || FMath::FloorToInt(Hd / 1000.f) != Band)
			{
				C *= 0.82f;
			}

			float WaterZ = 0.f;
			if (GetWaterHeight(FVector(X, Y, Hc), WaterZ) && Hc < WaterZ)
			{
				C = FLinearColor(0.16f, 0.3f, 0.42f);
			}
			Pixels[py * Size + px] = C.ToFColor(true);
		}
	});

	auto PlotDisc = [&](float X, float Y, float RadiusCm, const FColor& Col)
	{
		const int32 cx = FMath::FloorToInt((Y + HalfSize) / Step);
		const int32 cy = FMath::FloorToInt((HalfSize - X) / Step);
		const int32 R = FMath::Max(0, FMath::CeilToInt(RadiusCm / Step));
		for (int32 dy = -R; dy <= R; ++dy)
		{
			for (int32 dx = -R; dx <= R; ++dx)
			{
				if (dx * dx + dy * dy > R * R + 1)
				{
					continue;
				}
				const int32 x = cx + dx;
				const int32 y = cy + dy;
				if (x >= 0 && y >= 0 && x < Size && y < Size)
				{
					Pixels[y * Size + x] = Col;
				}
			}
		}
	};

	// Roads
	for (const FTORoad& R : Roads)
	{
		const FColor Col = R.bDirt ? FColor(150, 128, 96) : FColor(196, 192, 182);
		for (int32 i = 0; i + 1 < R.Points.Num(); ++i)
		{
			const float Len = FVector2D::Distance(R.Points[i], R.Points[i + 1]);
			const int32 Steps = FMath::Max(1, FMath::CeilToInt(Len / (Step * 0.5f)));
			for (int32 s = 0; s <= Steps; ++s)
			{
				const FVector2D P = FMath::Lerp(R.Points[i], R.Points[i + 1], (float)s / Steps);
				PlotDisc(P.X, P.Y, R.Width * 0.45f, Col);
			}
		}
	}
	// Building footprints
	for (const FBox2D& Fp : Footprints)
	{
		const int32 x0 = FMath::Clamp(FMath::FloorToInt((Fp.Min.Y + HalfSize) / Step), 0, Size - 1);
		const int32 x1 = FMath::Clamp(FMath::FloorToInt((Fp.Max.Y + HalfSize) / Step), 0, Size - 1);
		const int32 y0 = FMath::Clamp(FMath::FloorToInt((HalfSize - Fp.Max.X) / Step), 0, Size - 1);
		const int32 y1 = FMath::Clamp(FMath::FloorToInt((HalfSize - Fp.Min.X) / Step), 0, Size - 1);
		for (int32 y = y0; y <= y1; ++y)
		{
			for (int32 x = x0; x <= x1; ++x)
			{
				const bool bEdge = (x == x0 || x == x1 || y == y0 || y == y1);
				Pixels[y * Size + x] = bEdge ? FColor(40, 40, 42) : FColor(88, 88, 92);
			}
		}
	}

	MapTexture = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!MapTexture)
	{
		return;
	}
	MapTexture->SRGB = true;
	MapTexture->Filter = TF_Bilinear;
	FTexture2DMipMap& Mip = MapTexture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	MapTexture->UpdateResource();
}

// =============================================================================================
//  Entry point
// =============================================================================================

void ATOWorldGenerator::Generate(int32 Seed, ETOTimeOfDay Time)
{
	if (bGenerated)
	{
		return;
	}
	GenSeed = Seed;
	TimeOfDay = Time;
	FRandomStream Rng(Seed);
	const double T0 = FPlatformTime::Seconds();

	SetupLayout();
	PrepareRoads();
	BuildTerrain();
	BuildWater();
	BuildRoads();
	BuildDamValley(Rng);
	BuildVegetation(Rng);
	BuildBoundary();
	FinalizeBatches();
	BuildMapTexture();
	bGenerated = true;

	UE_LOG(LogTacOps, Log, TEXT("TacOps: generated %s in %.2fs (%d POIs, %d loot spots, %d guard posts, %d doors, %d lights)"),
		*MapName, FPlatformTime::Seconds() - T0, POIs.Num(), LootSpots.Num(), GuardPosts.Num(), Doors.Num(), Lights.Num());
}
