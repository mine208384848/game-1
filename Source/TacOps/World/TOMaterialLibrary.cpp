// TAC-OPS - material / mesh library

#include "World/TOMaterialLibrary.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Misc/PackageName.h"
#include "TacOps.h"

namespace
{
	FLinearColor SRGB(uint8 R, uint8 G, uint8 B)
	{
		return FLinearColor::FromSRGBColor(FColor(R, G, B));
	}

	FTOMatInfo MakeInfo(const TCHAR* Name, FLinearColor Color, float Rough, float Metal, const TCHAR* Surface, int32 Kind = 0, float Emissive = 0.f, float VC = 0.f)
	{
		FTOMatInfo I;
		I.Name = Name;
		I.Color = Color;
		I.Roughness = Rough;
		I.Metallic = Metal;
		I.Surface = Surface;
		I.Kind = Kind;
		I.Emissive = Emissive;
		I.VertexColorAmount = VC;
		return I;
	}

	FTOMatInfo BuildInfo(ETOMat M)
	{
		switch (M)
		{
		case ETOMat::Grass: return MakeInfo(TEXT("Grass"), SRGB(88, 104, 52), 0.95f, 0.f, TEXT("Dirt"), 0, 0.f, 1.f);
		case ETOMat::GrassDry: return MakeInfo(TEXT("GrassDry"), SRGB(128, 118, 70), 0.95f, 0.f, TEXT("Dirt"), 0, 0.f, 1.f);
		case ETOMat::Dirt: return MakeInfo(TEXT("Dirt"), SRGB(110, 92, 70), 0.95f, 0.f, TEXT("Dirt"), 0, 0.f, 1.f);
		case ETOMat::Rock: return MakeInfo(TEXT("Rock"), SRGB(118, 114, 106), 0.85f, 0.f, TEXT("Rock"), 0, 0.f, 1.f);
		case ETOMat::RockDark: return MakeInfo(TEXT("RockDark"), SRGB(78, 76, 72), 0.85f, 0.f, TEXT("Rock"), 0, 0.f, 0.6f);
		case ETOMat::Sand: return MakeInfo(TEXT("Sand"), SRGB(168, 152, 118), 0.95f, 0.f, TEXT("Dirt"), 0, 0.f, 1.f);
		case ETOMat::Gravel: return MakeInfo(TEXT("Gravel"), SRGB(128, 124, 116), 0.9f, 0.f, TEXT("Rock"), 0, 0.f, 1.f);
		case ETOMat::Snow: return MakeInfo(TEXT("Snow"), SRGB(230, 232, 236), 0.6f, 0.f, TEXT("Dirt"), 0, 0.f, 0.5f);
		case ETOMat::Asphalt: return MakeInfo(TEXT("Asphalt"), SRGB(52, 53, 55), 0.85f, 0.f, TEXT("Concrete"));
		case ETOMat::RoadLineWhite: return MakeInfo(TEXT("RoadLineWhite"), SRGB(215, 215, 205), 0.7f, 0.f, TEXT("Concrete"));
		case ETOMat::RoadLineYellow: return MakeInfo(TEXT("RoadLineYellow"), SRGB(220, 175, 40), 0.7f, 0.f, TEXT("Concrete"));
		case ETOMat::Sidewalk: return MakeInfo(TEXT("Sidewalk"), SRGB(150, 148, 142), 0.85f, 0.f, TEXT("Concrete"));
		case ETOMat::Concrete: return MakeInfo(TEXT("Concrete"), SRGB(150, 148, 140), 0.85f, 0.f, TEXT("Concrete"));
		case ETOMat::ConcreteDark: return MakeInfo(TEXT("ConcreteDark"), SRGB(100, 100, 98), 0.85f, 0.f, TEXT("Concrete"));
		case ETOMat::ConcreteLight: return MakeInfo(TEXT("ConcreteLight"), SRGB(186, 184, 176), 0.8f, 0.f, TEXT("Concrete"));
		case ETOMat::ConcreteDam: return MakeInfo(TEXT("ConcreteDam"), SRGB(162, 158, 148), 0.88f, 0.f, TEXT("Concrete"));
		case ETOMat::Plaster: return MakeInfo(TEXT("Plaster"), SRGB(196, 190, 178), 0.9f, 0.f, TEXT("Concrete"));
		case ETOMat::PlasterBeige: return MakeInfo(TEXT("PlasterBeige"), SRGB(196, 170, 130), 0.9f, 0.f, TEXT("Concrete"));
		case ETOMat::PlasterBlue: return MakeInfo(TEXT("PlasterBlue"), SRGB(110, 140, 160), 0.9f, 0.f, TEXT("Concrete"));
		case ETOMat::Brick: return MakeInfo(TEXT("Brick"), SRGB(140, 72, 52), 0.9f, 0.f, TEXT("Concrete"));
		case ETOMat::Tile: return MakeInfo(TEXT("Tile"), SRGB(170, 170, 165), 0.4f, 0.f, TEXT("Concrete"));
		case ETOMat::Carpet: return MakeInfo(TEXT("Carpet"), SRGB(70, 74, 86), 0.95f, 0.f, TEXT("Soft"));
		case ETOMat::FloorWood: return MakeInfo(TEXT("FloorWood"), SRGB(120, 84, 52), 0.6f, 0.f, TEXT("Wood"));
		case ETOMat::MetalDark: return MakeInfo(TEXT("MetalDark"), SRGB(40, 42, 44), 0.45f, 0.8f, TEXT("Metal"));
		case ETOMat::MetalGrey: return MakeInfo(TEXT("MetalGrey"), SRGB(120, 122, 124), 0.4f, 0.9f, TEXT("Metal"));
		case ETOMat::MetalRust: return MakeInfo(TEXT("MetalRust"), SRGB(120, 70, 42), 0.8f, 0.5f, TEXT("Metal"));
		case ETOMat::MetalGreen: return MakeInfo(TEXT("MetalGreen"), SRGB(64, 78, 52), 0.6f, 0.4f, TEXT("Metal"));
		case ETOMat::MetalBlue: return MakeInfo(TEXT("MetalBlue"), SRGB(40, 70, 110), 0.5f, 0.4f, TEXT("Metal"));
		case ETOMat::MetalYellow: return MakeInfo(TEXT("MetalYellow"), SRGB(210, 160, 30), 0.5f, 0.3f, TEXT("Metal"));
		case ETOMat::MetalRed: return MakeInfo(TEXT("MetalRed"), SRGB(150, 30, 26), 0.5f, 0.3f, TEXT("Metal"));
		case ETOMat::MetalWhite: return MakeInfo(TEXT("MetalWhite"), SRGB(210, 210, 205), 0.5f, 0.3f, TEXT("Metal"));
		case ETOMat::Aluminium: return MakeInfo(TEXT("Aluminium"), SRGB(170, 172, 176), 0.3f, 1.f, TEXT("Metal"));
		case ETOMat::Wood: return MakeInfo(TEXT("Wood"), SRGB(140, 100, 62), 0.75f, 0.f, TEXT("Wood"));
		case ETOMat::WoodDark: return MakeInfo(TEXT("WoodDark"), SRGB(82, 56, 36), 0.7f, 0.f, TEXT("Wood"));
		case ETOMat::Fabric: return MakeInfo(TEXT("Fabric"), SRGB(90, 90, 80), 0.95f, 0.f, TEXT("Soft"));
		case ETOMat::Canvas: return MakeInfo(TEXT("Canvas"), SRGB(110, 104, 78), 0.95f, 0.f, TEXT("Soft"));
		case ETOMat::Sandbag: return MakeInfo(TEXT("Sandbag"), SRGB(150, 130, 95), 0.95f, 0.f, TEXT("Soft"));
		case ETOMat::Rubber: return MakeInfo(TEXT("Rubber"), SRGB(25, 25, 25), 0.9f, 0.f, TEXT("Soft"));
		case ETOMat::Plastic: return MakeInfo(TEXT("Plastic"), SRGB(60, 62, 64), 0.5f, 0.f, TEXT("Metal"));
		case ETOMat::PlasticTan: return MakeInfo(TEXT("PlasticTan"), SRGB(150, 130, 96), 0.55f, 0.f, TEXT("Metal"));
		case ETOMat::Glass: return MakeInfo(TEXT("Glass"), SRGB(180, 200, 210), 0.05f, 0.f, TEXT("Glass"), 1);
		case ETOMat::Water: return MakeInfo(TEXT("Water"), SRGB(30, 58, 66), 0.04f, 0.f, TEXT("Water"), 2);
		case ETOMat::WaterRiver: return MakeInfo(TEXT("WaterRiver"), SRGB(44, 68, 60), 0.05f, 0.f, TEXT("Water"), 2);
		case ETOMat::FoliageDark: return MakeInfo(TEXT("FoliageDark"), SRGB(40, 62, 30), 0.9f, 0.f, TEXT("Foliage"));
		case ETOMat::FoliageLight: return MakeInfo(TEXT("FoliageLight"), SRGB(82, 104, 40), 0.9f, 0.f, TEXT("Foliage"));
		case ETOMat::FoliagePine: return MakeInfo(TEXT("FoliagePine"), SRGB(32, 54, 34), 0.9f, 0.f, TEXT("Foliage"));
		case ETOMat::Bark: return MakeInfo(TEXT("Bark"), SRGB(72, 56, 42), 0.95f, 0.f, TEXT("Wood"));
		case ETOMat::LampWarm: return MakeInfo(TEXT("LampWarm"), SRGB(255, 220, 160), 0.5f, 0.f, TEXT("Glass"), 4, 8.f);
		case ETOMat::LampCold: return MakeInfo(TEXT("LampCold"), SRGB(200, 225, 255), 0.5f, 0.f, TEXT("Glass"), 4, 8.f);
		case ETOMat::LampRed: return MakeInfo(TEXT("LampRed"), SRGB(255, 30, 20), 0.5f, 0.f, TEXT("Glass"), 4, 12.f);
		case ETOMat::LampGreen: return MakeInfo(TEXT("LampGreen"), SRGB(40, 255, 80), 0.5f, 0.f, TEXT("Glass"), 4, 12.f);
		case ETOMat::Screen: return MakeInfo(TEXT("Screen"), SRGB(80, 160, 220), 0.3f, 0.f, TEXT("Glass"), 4, 3.f);
		case ETOMat::TracerOrange: return MakeInfo(TEXT("TracerOrange"), SRGB(255, 170, 60), 0.5f, 0.f, TEXT("Glass"), 4, 40.f);
		case ETOMat::TracerRed: return MakeInfo(TEXT("TracerRed"), SRGB(255, 60, 40), 0.5f, 0.f, TEXT("Glass"), 4, 40.f);
		case ETOMat::Flash: return MakeInfo(TEXT("Flash"), SRGB(255, 200, 120), 0.5f, 0.f, TEXT("Glass"), 4, 60.f);
		case ETOMat::Smoke: return MakeInfo(TEXT("Smoke"), SRGB(150, 150, 150), 1.f, 0.f, TEXT("Soft"), 3);
		case ETOMat::SmokeGreen: return MakeInfo(TEXT("SmokeGreen"), SRGB(90, 200, 90), 1.f, 0.f, TEXT("Soft"), 3);
		case ETOMat::Blood: return MakeInfo(TEXT("Blood"), SRGB(110, 10, 8), 0.3f, 0.f, TEXT("Flesh"));
		case ETOMat::Black: return MakeInfo(TEXT("Black"), SRGB(12, 12, 12), 0.6f, 0.f, TEXT("Metal"));
		case ETOMat::Gold: return MakeInfo(TEXT("Gold"), SRGB(255, 200, 80), 0.25f, 1.f, TEXT("Metal"));
		case ETOMat::Skin: return MakeInfo(TEXT("Skin"), SRGB(198, 150, 120), 0.6f, 0.f, TEXT("Flesh"));
		case ETOMat::SuitGreen: return MakeInfo(TEXT("SuitGreen"), SRGB(78, 88, 60), 0.9f, 0.f, TEXT("Flesh"));
		case ETOMat::SuitTan: return MakeInfo(TEXT("SuitTan"), SRGB(150, 130, 96), 0.9f, 0.f, TEXT("Flesh"));
		case ETOMat::SuitBlack: return MakeInfo(TEXT("SuitBlack"), SRGB(36, 38, 40), 0.9f, 0.f, TEXT("Flesh"));
		case ETOMat::SuitGrey: return MakeInfo(TEXT("SuitGrey"), SRGB(100, 104, 108), 0.9f, 0.f, TEXT("Flesh"));
		case ETOMat::SuitBlue: return MakeInfo(TEXT("SuitBlue"), SRGB(52, 64, 86), 0.9f, 0.f, TEXT("Flesh"));
		case ETOMat::SuitRed: return MakeInfo(TEXT("SuitRed"), SRGB(110, 40, 36), 0.9f, 0.f, TEXT("Flesh"));
		case ETOMat::SuitUrban: return MakeInfo(TEXT("SuitUrban"), SRGB(90, 92, 98), 0.9f, 0.f, TEXT("Flesh"));
		case ETOMat::Gun: return MakeInfo(TEXT("Gun"), SRGB(30, 31, 33), 0.5f, 0.5f, TEXT("Metal"));
		case ETOMat::GunTan: return MakeInfo(TEXT("GunTan"), SRGB(150, 126, 92), 0.6f, 0.1f, TEXT("Metal"));
		case ETOMat::GunOlive: return MakeInfo(TEXT("GunOlive"), SRGB(78, 84, 62), 0.6f, 0.1f, TEXT("Metal"));
		default: return MakeInfo(TEXT("Default"), FLinearColor::Gray, 0.7f, 0.f, TEXT("Concrete"));
		}
	}

	const TCHAR* GBasicShapeMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	UMaterialInterface* TryLoadMaterial(const FString& PackagePath, const FString& AssetName)
	{
		if (!FPackageName::DoesPackageExist(PackagePath))
		{
			return nullptr;
		}
		const FString ObjectPath = PackagePath + TEXT(".") + AssetName;
		return LoadObject<UMaterialInterface>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}
}

void UTOMaterialLibrary::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	MasterMaterial = TryLoadMaterial(TEXT("/Game/TacOps/Materials/M_TO_Master"), TEXT("M_TO_Master"));
	GlassMaterial = TryLoadMaterial(TEXT("/Game/TacOps/Materials/M_TO_Glass"), TEXT("M_TO_Glass"));
	WaterMaterial = TryLoadMaterial(TEXT("/Game/TacOps/Materials/M_TO_Water"), TEXT("M_TO_Water"));
	SmokeMaterial = TryLoadMaterial(TEXT("/Game/TacOps/Materials/M_TO_Smoke"), TEXT("M_TO_Smoke"));
	UnlitMaterial = TryLoadMaterial(TEXT("/Game/TacOps/Materials/M_TO_Unlit"), TEXT("M_TO_Unlit"));
	FallbackMaterial = LoadObject<UMaterialInterface>(nullptr, GBasicShapeMaterialPath);

	if (!MasterMaterial)
	{
		UE_LOG(LogTacOps, Warning, TEXT("TacOps: generated materials not found (run Tools/setup_assets.py in the editor for better visuals). Using BasicShapeMaterial fallback."));
	}

	static const TCHAR* MeshPaths[] =
	{
		TEXT("/Engine/BasicShapes/Cube.Cube"),
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		TEXT("/Engine/BasicShapes/Sphere.Sphere"),
		TEXT("/Engine/BasicShapes/Cone.Cone"),
		TEXT("/Engine/BasicShapes/Plane.Plane")
	};
	Meshes.Reset();
	for (const TCHAR* Path : MeshPaths)
	{
		Meshes.Add(LoadObject<UStaticMesh>(nullptr, Path));
	}
}

UTOMaterialLibrary* UTOMaterialLibrary::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	const UWorld* World = WorldContext->GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UTOMaterialLibrary>() : nullptr;
}

const FTOMatInfo& UTOMaterialLibrary::GetInfo(ETOMat Mat)
{
	static TArray<FTOMatInfo> Infos;
	if (Infos.Num() == 0)
	{
		for (int32 i = 0; i < (int32)ETOMat::Count; ++i)
		{
			Infos.Add(BuildInfo((ETOMat)i));
		}
	}
	const int32 Idx = FMath::Clamp((int32)Mat, 0, Infos.Num() - 1);
	return Infos[Idx];
}

FName UTOMaterialLibrary::GetSurfaceTag(ETOMat Mat)
{
	return FName(GetInfo(Mat).Surface);
}

UStaticMesh* UTOMaterialLibrary::GetMesh(ETOShape Shape)
{
	const int32 Idx = (int32)Shape;
	return Meshes.IsValidIndex(Idx) ? Meshes[Idx].Get() : nullptr;
}

UMaterialInterface* UTOMaterialLibrary::ParentFor(ETOMat Mat, bool& bOutIsMaster) const
{
	const FTOMatInfo& Info = GetInfo(Mat);
	bOutIsMaster = true;
	switch (Info.Kind)
	{
	case 1: if (GlassMaterial) return GlassMaterial; break;
	case 2: if (WaterMaterial) return WaterMaterial; break;
	case 3: if (SmokeMaterial) return SmokeMaterial; break;
	case 4: if (UnlitMaterial) return UnlitMaterial; break;
	default: break;
	}
	if (MasterMaterial)
	{
		return MasterMaterial;
	}
	bOutIsMaster = false;
	return FallbackMaterial;
}

void UTOMaterialLibrary::ApplyParams(UMaterialInstanceDynamic* MID, const FTOMatInfo& Info, bool bIsMaster) const
{
	if (!MID)
	{
		return;
	}
	if (bIsMaster)
	{
		MID->SetVectorParameterValue(TEXT("BaseColor"), Info.Color);
		MID->SetScalarParameterValue(TEXT("Roughness"), Info.Roughness);
		MID->SetScalarParameterValue(TEXT("Metallic"), Info.Metallic);
		MID->SetScalarParameterValue(TEXT("EmissiveStrength"), Info.Emissive);
		MID->SetScalarParameterValue(TEXT("VertexColorAmount"), Info.VertexColorAmount);
		MID->SetScalarParameterValue(TEXT("Variation"), Info.Kind == 0 ? 0.18f : 0.f);
		MID->SetScalarParameterValue(TEXT("Opacity"), Info.Kind == 1 ? 0.22f : (Info.Kind == 2 ? 0.82f : 0.65f));
	}
	else
	{
		// BasicShapeMaterial: only a "Color" parameter. Brighten emissive materials so they read as lights.
		FLinearColor C = Info.Color;
		if (Info.Kind == 4)
		{
			C = C * 1.6f;
		}
		MID->SetVectorParameterValue(TEXT("Color"), C);
	}
}

UMaterialInterface* UTOMaterialLibrary::CreateFor(ETOMat Mat)
{
	const FTOMatInfo& Info = GetInfo(Mat);

	// 1) Artist override
	const FString OverridePackage = FString::Printf(TEXT("/Game/TacOps/Materials/Overrides/MO_%s"), Info.Name);
	if (UMaterialInterface* Override = TryLoadMaterial(OverridePackage, FString::Printf(TEXT("MO_%s"), Info.Name)))
	{
		return Override;
	}

	// 2/3) Dynamic instance of the generated master or of the engine fallback
	bool bIsMaster = false;
	UMaterialInterface* Parent = ParentFor(Mat, bIsMaster);
	if (!Parent)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
	ApplyParams(MID, Info, bIsMaster);
	return MID;
}

UMaterialInterface* UTOMaterialLibrary::GetMaterial(ETOMat Mat)
{
	const uint8 Key = (uint8)Mat;
	if (TObjectPtr<UMaterialInterface>* Found = Cache.Find(Key))
	{
		if (*Found)
		{
			return *Found;
		}
	}
	UMaterialInterface* M = CreateFor(Mat);
	Cache.Add(Key, M);
	return M;
}

UMaterialInstanceDynamic* UTOMaterialLibrary::MakeDynamic(ETOMat Mat)
{
	const FTOMatInfo& Info = GetInfo(Mat);
	bool bIsMaster = false;
	UMaterialInterface* Parent = ParentFor(Mat, bIsMaster);
	if (!Parent)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(Parent, this);
	ApplyParams(MID, Info, bIsMaster);
	ExtraDynamics.Add(MID);
	if (ExtraDynamics.Num() > 512)
	{
		ExtraDynamics.RemoveAt(0, 128);
	}
	return MID;
}

void UTOMaterialLibrary::SetDynamicColor(UMaterialInstanceDynamic* MID, const FLinearColor& Color, float Emissive)
{
	if (!MID)
	{
		return;
	}
	MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
	MID->SetVectorParameterValue(TEXT("Color"), Color);
	if (Emissive >= 0.f)
	{
		MID->SetScalarParameterValue(TEXT("EmissiveStrength"), Emissive);
	}
}
