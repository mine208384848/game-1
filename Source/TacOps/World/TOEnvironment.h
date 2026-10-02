// TAC-OPS - sky, sun/moon, atmosphere, volumetric clouds & fog, global post process
//
// Graphics target: Delta Force's slightly desaturated, high contrast, hazy military look.
// Everything is physically based (Sky Atmosphere + Lumen + Volumetric Fog) and switches
// between Day, Sunset and Night ("night ops" with flashlights and NVG).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TOTypes.h"
#include "TOEnvironment.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class UPostProcessComponent;

UCLASS()
class TACOPS_API ATOEnvironment : public AActor
{
	GENERATED_BODY()

public:
	ATOEnvironment();

	virtual void BeginPlay() override;

	void ApplyTimeOfDay(ETOTimeOfDay InTime);
	ETOTimeOfDay GetTimeOfDay() const { return Time; }
	bool IsNight() const { return Time == ETOTimeOfDay::Night; }
	FVector GetSunDirection() const;

protected:
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UDirectionalLightComponent> Sun;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USkyAtmosphereComponent> Atmosphere;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USkyLightComponent> SkyLight;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UExponentialHeightFogComponent> Fog;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UVolumetricCloudComponent> Clouds;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UPostProcessComponent> PostProcess;

	ETOTimeOfDay Time = ETOTimeOfDay::Day;
};
