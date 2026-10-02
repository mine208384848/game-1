// TAC-OPS - procedural battlefield generator
//
// Builds the whole map at runtime from engine primitives: height-field terrain with biome
// sections, river & reservoir, road network, bridges, forests, rocks and every building
// (multi-storey offices with stairs, barracks, warehouses, houses, the hydro dam, the power
// station, radar station...).  It also records gameplay metadata: points of interest with
// danger tiers, loot spots, guard posts, doors / keycard rooms, extraction points,
// insertion points and Warfare sectors, plus a top-down minimap texture.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TOTypes.h"
#include "World/TOMaterialLibrary.h"
#include "TOWorldGenerator.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UProceduralMeshComponent;
class UTexture2D;
class UPointLightComponent;

UENUM()
enum class ETOCol : uint8
{
	Solid,        // blocks everything (walls, rocks, terrain props)
	Foliage,      // only blocks visibility (AI sight) - bullets & players pass
	None,         // decoration
	PawnOnly      // invisible map boundary
};

struct FTOFrame
{
	FVector Origin = FVector::ZeroVector;
	float Yaw = 0.f;

	FTOFrame() {}
	FTOFrame(const FVector& InOrigin, float InYaw) : Origin(InOrigin), Yaw(InYaw) {}

	FVector ToWorld(const FVector& Local) const { return Origin + FRotator(0.f, Yaw, 0.f).RotateVector(Local); }
	FRotator ToWorldRot(const FRotator& Local) const { return (FRotator(0.f, Yaw, 0.f).Quaternion() * Local.Quaternion()).Rotator(); }
	FTOFrame Sub(const FVector& LocalOffset, float LocalYaw) const { return FTOFrame(ToWorld(LocalOffset), Yaw + LocalYaw); }
};

struct FTOOpening
{
	float Center = 0.f;
	float Width = 100.f;
	float Bottom = 0.f;
	float Top = 210.f;
	FTOOpening() {}
	FTOOpening(float InCenter, float InWidth, float InBottom, float InTop) : Center(InCenter), Width(InWidth), Bottom(InBottom), Top(InTop) {}
};

struct FTORoad
{
	TArray<FVector2D> Points;      // cm
	TArray<float> Heights;         // smoothed road surface height per point (cm)
	float Width = 800.f;
	bool bDirt = false;
	bool bMarkings = true;
};

struct FTOPad
{
	FString Name;
	FVector2D Center = FVector2D::ZeroVector;
	FVector2D HalfSize = FVector2D(1000.f, 1000.f);
	float Yaw = 0.f;
	float Height = 0.f;
	float Blend = 1500.f;
	ETOMat Surface = ETOMat::Gravel;
};

struct FTOPOI
{
	FString Name;
	FVector Location = FVector::ZeroVector;
	float Radius = 5000.f;
	int32 Tier = 0;
	bool bMajor = false;
};

struct FTOLootSpot
{
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
	ETOContainerType Type = ETOContainerType::Duffel;
	int32 Tier = 0;
	int32 Poi = -1;
};

struct FTOGuardPost
{
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
	int32 Poi = -1;
	int32 Tier = 0;
	bool bBoss = false;
	bool bOverwatch = false;
};

struct FTODoorDef
{
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
	float Width = 110.f;
	float Height = 220.f;
	FName KeyId;
	ETOMat Mat = ETOMat::WoodDark;
};

struct FTOExtractDef
{
	FString Name;
	FVector Location = FVector::ZeroVector;
	ETOExtractRule Rule = ETOExtractRule::Always;
	float Radius = 700.f;
	int32 Cost = 0;
	FVector SwitchLocation = FVector::ZeroVector;
	float SwitchYaw = 0.f;
	float Chance = 1.f;
};

struct FTOSpawnDef
{
	FString Name;
	FVector Location = FVector::ZeroVector;
	float Yaw = 0.f;
};

struct FTOSectorDef
{
	FString Name;
	TArray<FVector> Points;
	TArray<FString> Labels;
	FVector AttackerSpawn = FVector::ZeroVector;
	FVector DefenderSpawn = FVector::ZeroVector;
};

struct FTOBuildingSpec
{
	FVector Center = FVector::ZeroVector;   // ground floor level, center of footprint
	float Yaw = 0.f;
	float W = 2000.f;                       // along local X
	float D = 1200.f;                       // along local Y
	int32 Floors = 2;
	float FloorH = 360.f;
	float Wall = 25.f;
	ETOMat Ext = ETOMat::Plaster;
	ETOMat Int = ETOMat::Plaster;
	ETOMat Floor = ETOMat::Tile;
	ETOMat Roof = ETOMat::ConcreteDark;
	ETOMat Trim = ETOMat::ConcreteLight;
	int32 Layout = 0;                       // 0 offices, 1 open hall, 2 house, 3 barracks, 4 warehouse
	bool bWindows = true;
	float WinW = 140.f;
	float WinH = 140.f;
	float WinSill = 95.f;
	float WinSpacing = 420.f;
	int32 DoorMask = 1;                     // 1 front(-Y) 2 back(+Y) 4 left(-X) 8 right(+X)
	bool bRoofAccess = true;
	bool bParapet = true;
	bool bGable = false;
	int32 Tier = 0;
	int32 Poi = -1;
	FName EntranceKey;                      // locked main entrance
	FName SpecialRoomKey;                   // keycard room
	int32 SpecialRoomFloor = -1;
	ETOContainerType SpecialLoot = ETOContainerType::Safe;
	bool bGuards = true;
};

UCLASS()
class TACOPS_API ATOWorldGenerator : public AActor
{
	GENERATED_BODY()

public:
	ATOWorldGenerator();

	/** Builds the complete Dam Valley battlefield. */
	void Generate(int32 Seed, ETOTimeOfDay Time);

	// Queries ------------------------------------------------------------------------------
	float GetTerrainHeight(float X, float Y) const;
	bool GetWaterHeight(const FVector& Location, float& OutZ) const;
	/** Trace down from the sky against everything (buildings included). */
	FVector GroundPoint(float X, float Y, float ZStart = 60000.f) const;
	float GetHalfSize() const { return HalfSize; }
	UTexture2D* GetMapTexture() const { return MapTexture; }
	FVector2D WorldToMapUV(const FVector& World) const;
	bool IsTerrainReady() const;

	// Gameplay metadata --------------------------------------------------------------------
	TArray<FTOPOI> POIs;
	TArray<FTOLootSpot> LootSpots;
	TArray<FTOGuardPost> GuardPosts;
	TArray<FTODoorDef> Doors;
	TArray<FTOExtractDef> Extracts;
	TArray<FTOSpawnDef> Insertions;
	TArray<FTOSectorDef> Sectors;
	TArray<FVector> BarrelSpots;
	FString MapName = TEXT("Dam Valley");

protected:
	// Height field -----------------------------------------------------------------------
	float HeightStage(float X, float Y, int32 Stage) const;
	float NaturalHeight(float X, float Y) const;
	float RiverDistance(float X, float Y, float* OutT = nullptr) const;
	float RiverWaterLevel(float T) const;
	float ReservoirWeight(float X, float Y) const;
	float PadWeight(const FTOPad& Pad, float X, float Y) const;
	float RoadInfluence(float X, float Y, float& OutRoadH, float& OutDist) const;
	FVector TerrainNormal(float X, float Y) const;
	ETOMat SurfaceAt(float X, float Y, const FVector& Normal, float H) const;

	// Build steps --------------------------------------------------------------------------
	void SetupLayout();
	void PrepareRoads();
	void BuildTerrain();
	void BuildWater();
	void BuildRoads();
	void BuildVegetation(FRandomStream& Rng);
	void BuildBoundary();
	void BuildMapTexture();
	void BuildDamValley(FRandomStream& Rng);
	void FinalizeBatches();

	// Primitive batching -------------------------------------------------------------------
	void AddInstance(ETOShape Shape, ETOMat Mat, ETOCol Col, const FTransform& T);
	void Box(const FTOFrame& F, const FVector& Center, const FVector& Size, ETOMat Mat, ETOCol Col = ETOCol::Solid, const FRotator& Rot = FRotator::ZeroRotator);
	void Cyl(const FTOFrame& F, const FVector& Center, float Diameter, float Height, ETOMat Mat, ETOCol Col = ETOCol::Solid, const FRotator& Rot = FRotator::ZeroRotator);
	void Sphere(const FTOFrame& F, const FVector& Center, const FVector& Size, ETOMat Mat, ETOCol Col = ETOCol::Solid);
	void Cone(const FTOFrame& F, const FVector& Center, float Diameter, float Height, ETOMat Mat, ETOCol Col = ETOCol::Foliage);
	void Wall(const FTOFrame& F, const FVector2D& A, const FVector2D& B, float Z0, float Height, float Thick, ETOMat Mat, TArray<FTOOpening> Openings);
	void SlabWithHole(const FTOFrame& F, const FBox2D& Outer, const FBox2D& Hole, float Z, float Thick, ETOMat Mat, bool bHasHole);
	void Stairs(const FTOFrame& F, const FVector& Start, float Run, float Rise, float Width, ETOMat Mat);
	void Railing(const FTOFrame& F, const FVector2D& A, const FVector2D& B, float Z, ETOMat Mat);
	void AddLight(const FVector& World, const FLinearColor& Color, float Intensity, float Radius, bool bNightOnly);

	// Prefabs ------------------------------------------------------------------------------
	void BuildBuilding(const FTOBuildingSpec& S, FRandomStream& Rng);
	void BuildOfficeInterior(const FTOBuildingSpec& S, const FTOFrame& F, int32 Floor, float Z, float StairX0, float StairX1, FRandomStream& Rng);
	void FurnishRoom(const FTOBuildingSpec& S, const FTOFrame& F, const FBox2D& Room, float Z, int32 RoomType, FRandomStream& Rng, bool bSpecial);
	void BuildHouse(const FVector& Center, float Yaw, int32 Floors, ETOMat WallMat, int32 Poi, int32 Tier, FRandomStream& Rng);
	void BuildBarracks(const FVector& Center, float Yaw, int32 Poi, int32 Tier, FRandomStream& Rng);
	void BuildWarehouse(const FVector& Center, float Yaw, float W, float D, int32 Poi, int32 Tier, FName OfficeKey, FRandomStream& Rng);
	void BuildWatchtower(const FVector& Base, float Yaw, int32 Poi, int32 Tier);
	void BuildDam(const FVector& Center, float Length, float BaseZ, float TopZ, FRandomStream& Rng);
	void BuildPowerStation(const FVector& Center, float Yaw, int32 Poi, FRandomStream& Rng);
	void BuildRadarStation(const FVector& Center, int32 Poi, FRandomStream& Rng);
	void BuildBridge(const FVector& A, const FVector& B, float Width, ETOMat Mat);
	void BuildContainerStack(const FVector& Base, float Yaw, int32 Count, FRandomStream& Rng, bool bLootable, int32 Poi, int32 Tier);
	void BuildVehicle(const FVector& Base, float Yaw, int32 Kind, FRandomStream& Rng);
	void BuildHelicopterProp(const FVector& Base, float Yaw);
	void BuildSandbagWall(const FVector& Center, float Yaw, float Length);
	void BuildFence(const FVector2D& A, const FVector2D& B, float Z, bool bConcrete);
	void BuildPylon(const FVector& Base, float Yaw);
	void BuildLampPost(const FVector& Base, float Yaw);
	void BuildTankFarm(const FVector& Center, float Yaw, int32 Poi);
	void BuildTree(const FVector& Base, int32 Kind, float Scale, FRandomStream& Rng);
	void BuildRock(const FVector& Base, float Scale, FRandomStream& Rng);
	void BuildCrateCluster(const FVector& Base, float Yaw, FRandomStream& Rng);
	void BuildTent(const FVector& Base, float Yaw);
	void BuildHelipad(const FVector& Center);
	void BuildGasStation(const FVector& Center, float Yaw, int32 Poi, FRandomStream& Rng);
	void BuildChurch(const FVector& Center, float Yaw, int32 Poi, FRandomStream& Rng);
	void BuildPumpStation(const FVector& Center, float Yaw, int32 Poi, FRandomStream& Rng);
	void BuildCraneGantry(const FVector& Center, float Yaw, float Span);

	// Helpers ------------------------------------------------------------------------------
	int32 AddPOI(const FString& Name, const FVector& Location, float Radius, int32 Tier, bool bMajor);
	void AddLoot(const FVector& World, float Yaw, ETOContainerType Type, int32 Tier, int32 Poi);
	void AddGuard(const FVector& World, float Yaw, int32 Poi, int32 Tier, bool bBoss = false, bool bOverwatch = false);
	void AddDoor(const FVector& World, float Yaw, float Width, float Height, FName Key, ETOMat Mat);
	FVector OnGround(float X, float Y, float Offset = 0.f) const;
	FTOPad& AddPad(const FString& Name, const FVector2D& Center, const FVector2D& HalfSize, float Yaw, float Blend, ETOMat Surface, float FixedHeight = -1.f);
	float PadHeight(const FString& Name) const;

	UPROPERTY() TObjectPtr<USceneComponent> Root;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> TerrainChunks;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> ExtraMeshes;
	UPROPERTY() TMap<uint32, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> Batches;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY() TObjectPtr<UTexture2D> MapTexture;

	TMap<uint32, TArray<FTransform>> PendingInstances;

	// Layout data
	TArray<FVector2D> RiverPoints;   // cm
	TArray<float> RiverCum;          // cumulative length
	float RiverLength = 1.f;
	TArray<FTORoad> Roads;
	TArray<FTOPad> Pads;
	TArray<FBox2D> Footprints;       // building footprints for the minimap / tree exclusion
	TArray<FVector> BridgeEnds;

	float HalfSize = 125000.f;
	float ReservoirLevel = 5800.f;
	float DamX = 43000.f;
	ETOTimeOfDay TimeOfDay = ETOTimeOfDay::Day;
	int32 GenSeed = 1;
	int32 LightBudget = 90;
	bool bGenerated = false;
};
