// TAC-OPS - sky, lighting and post process

#include "World/TOEnvironment.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "TacOps.h"

ATOEnvironment::ATOEnvironment()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(Root);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->bAtmosphereSunLight = true;
	Sun->AtmosphereSunLightIndex = 0;
	Sun->bEnableLightShaftBloom = true;
	Sun->LightSourceAngle = 0.55f;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(Root);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(Root);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(Root);

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(Root);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(Root);
	PostProcess->bUnbound = true;
}

void ATOEnvironment::BeginPlay()
{
	Super::BeginPlay();

	// Engine sample cloud material (ships with every UE5 install).
	const FString CloudPackage = TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst");
	if (FPackageName::DoesPackageExist(CloudPackage))
	{
		if (UMaterialInterface* CloudMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst.m_SimpleVolumetricCloud_Inst"), nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			Clouds->SetMaterial(CloudMat);
		}
	}
	else
	{
		UE_LOG(LogTacOps, Log, TEXT("Volumetric cloud material not found, clouds disabled."));
		Clouds->SetVisibility(false);
	}
	ApplyTimeOfDay(Time);
}

FVector ATOEnvironment::GetSunDirection() const
{
	return Sun ? -Sun->GetForwardVector() : FVector::UpVector;
}

void ATOEnvironment::ApplyTimeOfDay(ETOTimeOfDay InTime)
{
	Time = InTime;

	FRotator SunRot;
	float SunLux = 10.f;
	FLinearColor SunColor;
	float SkyIntensity = 1.f;
	float FogDensity = 0.012f;
	float FogFalloff = 0.08f;
	FLinearColor FogColor;
	float VolExtinction = 1.f;
	float ExposureMin = -10.f;
	float ExposureMax = 20.f;
	float ExposureBias = 0.f;
	FVector4 Saturation(0.92f, 0.92f, 0.95f, 0.92f);
	FVector4 GainShadows(0.96f, 1.f, 1.06f, 1.f);
	FVector4 GainHighlights(1.05f, 1.f, 0.95f, 1.f);

	switch (Time)
	{
	case ETOTimeOfDay::Sunset:
		SunRot = FRotator(-7.f, 255.f, 0.f);
		SunLux = 8.f;
		SunColor = FLinearColor(1.f, 0.62f, 0.38f);
		SkyIntensity = 0.85f;
		FogDensity = 0.022f;
		FogFalloff = 0.12f;
		FogColor = FLinearColor(0.75f, 0.45f, 0.3f);
		VolExtinction = 1.4f;
		ExposureMin = -3.f;
		ExposureBias = 0.3f;
		Saturation = FVector4(1.f, 0.95f, 0.9f, 1.f);
		GainShadows = FVector4(0.92f, 0.96f, 1.1f, 1.f);
		GainHighlights = FVector4(1.1f, 1.f, 0.88f, 1.f);
		break;
	case ETOTimeOfDay::Night:
		SunRot = FRotator(-38.f, 40.f, 0.f);
		SunLux = 0.35f;
		SunColor = FLinearColor(0.5f, 0.62f, 1.f);
		SkyIntensity = 0.3f;
		FogDensity = 0.028f;
		FogFalloff = 0.12f;
		FogColor = FLinearColor(0.02f, 0.03f, 0.06f);
		VolExtinction = 2.f;
		ExposureMin = -0.6f;
		ExposureMax = 1.2f;
		Saturation = FVector4(0.75f, 0.8f, 0.95f, 0.8f);
		GainShadows = FVector4(0.9f, 0.97f, 1.12f, 1.f);
		GainHighlights = FVector4(0.95f, 1.f, 1.08f, 1.f);
		break;
	default:
		SunRot = FRotator(-42.f, 128.f, 0.f);
		SunLux = 10.f;
		SunColor = FLinearColor(1.f, 0.96f, 0.9f);
		SkyIntensity = 1.f;
		FogDensity = 0.012f;
		FogFalloff = 0.08f;
		FogColor = FLinearColor(0.45f, 0.55f, 0.7f);
		VolExtinction = 1.f;
		ExposureMin = -1.f;
		ExposureMax = 14.f;
		break;
	}

	Sun->SetWorldRotation(SunRot);
	Sun->SetIntensity(SunLux);
	Sun->SetLightColor(SunColor);
	Sun->SetVolumetricScatteringIntensity(Time == ETOTimeOfDay::Night ? 2.f : 1.f);

	SkyLight->SetIntensity(SkyIntensity);
	SkyLight->RecaptureSky();

	Fog->SetWorldLocation(FVector(0.f, 0.f, -200.f));
	Fog->SetFogDensity(FogDensity);
	Fog->SetFogHeightFalloff(FogFalloff);
	Fog->SetFogInscatteringColor(FogColor);
	Fog->SetStartDistance(800.f);
	Fog->SetVolumetricFog(true);
	Fog->SetVolumetricFogExtinctionScale(VolExtinction);
	Fog->SetVolumetricFogScatteringDistribution(0.6f);

	// Global grade: Delta Force style - slightly desaturated, contrasty, cool shadows / warm highlights.
	FPostProcessSettings& S = PostProcess->Settings;
	S.bOverride_DynamicGlobalIlluminationMethod = true;
	S.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
	S.bOverride_ReflectionMethod = true;
	S.ReflectionMethod = EReflectionMethod::Lumen;
	S.bOverride_LumenFinalGatherQuality = true;
	S.LumenFinalGatherQuality = 1.5f;
	S.bOverride_LumenSceneLightingQuality = true;
	S.LumenSceneLightingQuality = 1.25f;

	S.bOverride_AutoExposureMinBrightness = true;
	S.AutoExposureMinBrightness = ExposureMin;
	S.bOverride_AutoExposureMaxBrightness = true;
	S.AutoExposureMaxBrightness = ExposureMax;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = ExposureBias;
	S.bOverride_AutoExposureSpeedUp = true;
	S.AutoExposureSpeedUp = 2.5f;
	S.bOverride_AutoExposureSpeedDown = true;
	S.AutoExposureSpeedDown = 1.2f;

	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 0.55f;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.38f;
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = 0.06f;
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = 0.12f;
	S.bOverride_LensFlareIntensity = true;
	S.LensFlareIntensity = 0.f;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.15f;
	S.bOverride_AmbientOcclusionIntensity = true;
	S.AmbientOcclusionIntensity = 0.6f;

	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = Saturation;
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.06f);
	S.bOverride_ColorGainShadows = true;
	S.ColorGainShadows = GainShadows;
	S.bOverride_ColorGainHighlights = true;
	S.ColorGainHighlights = GainHighlights;
}
