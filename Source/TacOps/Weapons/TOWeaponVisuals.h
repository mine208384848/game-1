// TAC-OPS - procedural weapon models
//
// Every weapon (and every gunsmith attachment) is assembled from engine primitive meshes,
// so the project runs without any imported art.  The same builder is used for the
// first-person view model, third-person weapons and gunsmith previews.

#pragma once

#include "CoreMinimal.h"
#include "Core/TOTypes.h"
#include "World/TOMaterialLibrary.h"

class AActor;
class USceneComponent;
class UStaticMeshComponent;

struct FTOGunVisual
{
	TArray<UStaticMeshComponent*> Parts;
	/** Parts that make up the optic housing (hidden while looking through magnified scopes). */
	TArray<UStaticMeshComponent*> OpticParts;
	UStaticMeshComponent* Magazine = nullptr;
	FVector MagazineRest = FVector::ZeroVector;
	FVector MuzzleLocal = FVector(60.f, 0.f, 0.f);
	FVector LightLocal = FVector(30.f, 3.f, 0.f);
	FVector LeftHandLocal = FVector(25.f, 0.f, -4.f);
	FVector RightHandLocal = FVector(-2.f, 0.f, -5.f);
	float SightHeight = 6.f;
	float Length = 60.f;
	float RearX = -30.f;
};

namespace TOWeaponVisuals
{
	/**
	 * Builds a weapon model attached to Parent. Gun local frame: +X forward along the bore,
	 * bore axis at Z = 0, pistol grip just behind/below the origin.
	 */
	void BuildGun(AActor* Owner, USceneComponent* Parent, const FTOWeaponConfig& Config, bool bFirstPerson, bool bHideFromOwner, FTOGunVisual& Out);

	UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, ETOShape Shape, ETOMat Mat,
		const FVector& Location, const FVector& SizeCm, const FRotator& Rotation, bool bFirstPerson, bool bHideFromOwner);

	void DestroyParts(TArray<UStaticMeshComponent*>& Parts);
}
