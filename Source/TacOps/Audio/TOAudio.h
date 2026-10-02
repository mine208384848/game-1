// TAC-OPS - procedural audio
//
// All sounds (gunshots per weapon class, suppressed shots, distant shots, explosions,
// footsteps, bullet whiz, hit markers, reloads, UI) are synthesized at startup into PCM
// buffers and played through USoundWaveProcedural.  No sound assets are required.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TOAudio.generated.h"

class UAudioComponent;
class USoundAttenuation;
class USoundWaveProcedural;

UENUM()
enum class ETOSound : uint8
{
	RifleShot,
	HeavyShot,
	PistolShot,
	SniperShot,
	ShotgunShot,
	SMGShot,
	SuppressedShot,
	DistantShot,
	Explosion,
	Footstep,
	FootstepWater,
	Land,
	ReloadStart,
	ReloadEnd,
	DryFire,
	HitMarker,
	KillMarker,
	Headshot,
	Whiz,
	Impact,
	UIClick,
	Beep,
	LootReveal,
	LootRare,
	Door,
	Pickup,
	Heal,
	Ability,
	GrenadeBounce,
	Tinnitus,
	Heartbeat,
	Capture,
	Count UMETA(Hidden)
};

UCLASS()
class TACOPS_API UTOAudio : public UObject
{
	GENERATED_BODY()

public:
	void Init(UWorld* InWorld);
	void Tick(float DeltaTime);

	/** 3D positional sound. */
	void Play(ETOSound Sound, const FVector& Location, float Volume = 1.f, float Pitch = 1.f);
	/** Non spatialized (UI, own body). */
	void Play2D(ETOSound Sound, float Volume = 1.f, float Pitch = 1.f);

	float MasterVolume = 1.f;

private:
	void Generate();
	void SpawnClip(ETOSound Sound, int32 Variant, const FVector* Location, float Volume, float Pitch);
	int32 NumVariants(ETOSound Sound) const;
	int32 AttenuationIndex(ETOSound Sound) const;
	float MaxAudibleDistance(ETOSound Sound) const;

	TWeakObjectPtr<UWorld> World;
	TArray<TArray<int16>> Clips;     // index = Sound * MaxVariants + Variant
	TArray<float> ClipSeconds;
	static constexpr int32 MaxVariants = 4;
	int32 SampleRate = 24000;
	bool bGenerated = false;

	UPROPERTY() TArray<TObjectPtr<USoundAttenuation>> Attenuations;
	UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Active;
	TArray<float> ActiveEnd;
	UPROPERTY() TArray<TObjectPtr<USoundWaveProcedural>> RecentWaves;
	float Clock = 0.f;
};
