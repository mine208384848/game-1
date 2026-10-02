// TAC-OPS - procedural audio

#include "Audio/TOAudio.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"

namespace
{
	/** Tiny synthesizer helpers operating on float buffers. */
	struct FSynth
	{
		int32 SR = 24000;
		FRandomStream Rng;

		explicit FSynth(int32 InSR, int32 Seed) : SR(InSR), Rng(Seed) {}

		TArray<float> Make(float Seconds) const
		{
			TArray<float> B;
			B.SetNumZeroed(FMath::Max(1, FMath::RoundToInt(Seconds * SR)));
			return B;
		}

		static float Env(float T, float Tau) { return T < 0.f ? 0.f : FMath::Exp(-T / FMath::Max(Tau, 1e-4f)); }

		/** Adds filtered noise: lowpass cutoff (0 = none), highpass cutoff (0 = none), envelope tau, start time. */
		void Noise(TArray<float>& B, float Gain, float LowpassHz, float HighpassHz, float Tau, float Start = 0.f, float Attack = 0.f)
		{
			const float Alpha = LowpassHz > 0.f ? 1.f - FMath::Exp(-2.f * PI * LowpassHz / SR) : 1.f;
			const float AlphaH = HighpassHz > 0.f ? 1.f - FMath::Exp(-2.f * PI * HighpassHz / SR) : 0.f;
			float Lp = 0.f;
			float Hp = 0.f;
			const int32 S0 = FMath::Clamp(FMath::RoundToInt(Start * SR), 0, B.Num());
			for (int32 i = S0; i < B.Num(); ++i)
			{
				const float T = (i - S0) / (float)SR;
				const float X = Rng.FRandRange(-1.f, 1.f);
				Lp += Alpha * (X - Lp);
				Hp += AlphaH * (Lp - Hp);
				const float Sig = HighpassHz > 0.f ? (Lp - Hp) : Lp;
				const float A = Attack > 0.f ? FMath::Min(1.f, T / Attack) : 1.f;
				B[i] += Sig * Gain * A * Env(T, Tau);
			}
		}

		/** Adds a sine with an exponential pitch drop (FreqStart -> FreqEnd with time constant PitchTau). */
		void Tone(TArray<float>& B, float Gain, float FreqEnd, float FreqStart, float PitchTau, float Tau, float Start = 0.f, float Attack = 0.f)
		{
			const int32 S0 = FMath::Clamp(FMath::RoundToInt(Start * SR), 0, B.Num());
			float Phase = 0.f;
			for (int32 i = S0; i < B.Num(); ++i)
			{
				const float T = (i - S0) / (float)SR;
				const float F = FreqEnd + (FreqStart - FreqEnd) * Env(T, PitchTau);
				Phase += 2.f * PI * F / SR;
				const float A = Attack > 0.f ? FMath::Min(1.f, T / Attack) : 1.f;
				B[i] += FMath::Sin(Phase) * Gain * A * Env(T, Tau);
			}
		}

		static void Normalize(TArray<float>& B, float Peak)
		{
			float Max = 0.f;
			for (float V : B)
			{
				Max = FMath::Max(Max, FMath::Abs(V));
			}
			if (Max <= 1e-5f)
			{
				return;
			}
			const float K = Peak / Max;
			for (float& V : B)
			{
				V = FMath::Clamp(V * K, -1.f, 1.f);
			}
		}

		static void SoftClip(TArray<float>& B, float Drive)
		{
			for (float& V : B)
			{
				const float E = FMath::Exp(2.f * FMath::Clamp(V * Drive, -10.f, 10.f));
				V = (E - 1.f) / (E + 1.f);
			}
		}

		static void FadeOut(TArray<float>& B, float Seconds, int32 SR)
		{
			const int32 N = FMath::Min(B.Num(), FMath::RoundToInt(Seconds * SR));
			for (int32 i = 0; i < N; ++i)
			{
				B[B.Num() - 1 - i] *= (float)i / (float)FMath::Max(1, N);
			}
		}
	};

	TArray<int16> ToPCM(const TArray<float>& B)
	{
		TArray<int16> Out;
		Out.SetNumUninitialized(B.Num());
		for (int32 i = 0; i < B.Num(); ++i)
		{
			Out[i] = (int16)FMath::Clamp(FMath::RoundToInt(B[i] * 32000.f), -32767, 32767);
		}
		return Out;
	}
}

void UTOAudio::Init(UWorld* InWorld)
{
	World = InWorld;
	if (!bGenerated)
	{
		Generate();
	}

	// Attenuation presets: 0 gun, 1 explosion, 2 footstep, 3 small, 4 medium
	Attenuations.Reset();
	struct FPreset { float Inner; float Falloff; bool bLPF; };
	const FPreset Presets[] = { { 1500.f, 40000.f, true }, { 3000.f, 60000.f, true }, { 150.f, 2600.f, false }, { 200.f, 1800.f, false }, { 500.f, 6000.f, true } };
	for (const FPreset& P : Presets)
	{
		USoundAttenuation* A = NewObject<USoundAttenuation>(this);
		A->Attenuation.bAttenuate = true;
		A->Attenuation.bSpatialize = true;
		A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
		A->Attenuation.AttenuationShapeExtents = FVector(P.Inner, 0.f, 0.f);
		A->Attenuation.FalloffDistance = P.Falloff;
		A->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
		A->Attenuation.bAttenuateWithLPF = P.bLPF;
		A->Attenuation.LPFRadiusMin = P.Inner * 2.f;
		A->Attenuation.LPFRadiusMax = P.Falloff * 0.6f;
		A->Attenuation.LPFFrequencyAtMax = 1800.f;
		Attenuations.Add(A);
	}
}

int32 UTOAudio::NumVariants(ETOSound Sound) const
{
	switch (Sound)
	{
	case ETOSound::RifleShot:
	case ETOSound::HeavyShot:
	case ETOSound::PistolShot:
	case ETOSound::SMGShot:
	case ETOSound::SuppressedShot:
	case ETOSound::Footstep:
	case ETOSound::Impact:
		return 3;
	default:
		return 1;
	}
}

int32 UTOAudio::AttenuationIndex(ETOSound Sound) const
{
	switch (Sound)
	{
	case ETOSound::RifleShot:
	case ETOSound::HeavyShot:
	case ETOSound::PistolShot:
	case ETOSound::SniperShot:
	case ETOSound::ShotgunShot:
	case ETOSound::SMGShot:
	case ETOSound::DistantShot:
		return 0;
	case ETOSound::Explosion:
		return 1;
	case ETOSound::Footstep:
	case ETOSound::FootstepWater:
	case ETOSound::Land:
		return 2;
	case ETOSound::SuppressedShot:
	case ETOSound::Door:
	case ETOSound::GrenadeBounce:
	case ETOSound::Capture:
		return 4;
	default:
		return 3;
	}
}

float UTOAudio::MaxAudibleDistance(ETOSound Sound) const
{
	const int32 Idx = AttenuationIndex(Sound);
	static const float Max[] = { 42000.f, 62000.f, 2800.f, 2000.f, 6500.f };
	return Max[FMath::Clamp(Idx, 0, 4)];
}

void UTOAudio::Generate()
{
	bGenerated = true;
	const int32 N = (int32)ETOSound::Count * MaxVariants;
	Clips.SetNum(N);
	ClipSeconds.SetNumZeroed(N);

	for (int32 S = 0; S < (int32)ETOSound::Count; ++S)
	{
		const ETOSound Sound = (ETOSound)S;
		for (int32 V = 0; V < NumVariants(Sound); ++V)
		{
			FSynth Syn(SampleRate, 1000 + S * 31 + V * 7);
			const float Var = 1.f + 0.06f * (V - 1);
			TArray<float> B;
			switch (Sound)
			{
			case ETOSound::RifleShot:
				B = Syn.Make(0.95f);
				Syn.Noise(B, 1.0f, 0.f, 1800.f, 0.006f);
				Syn.Noise(B, 0.9f, 2200.f * Var, 0.f, 0.05f);
				Syn.Tone(B, 0.9f, 65.f * Var, 160.f, 0.02f, 0.07f);
				Syn.Noise(B, 0.22f, 700.f, 0.f, 0.3f, 0.02f, 0.02f);
				FSynth::SoftClip(B, 1.6f);
				break;
			case ETOSound::HeavyShot:
				B = Syn.Make(1.1f);
				Syn.Noise(B, 1.0f, 0.f, 1500.f, 0.007f);
				Syn.Noise(B, 1.0f, 1700.f * Var, 0.f, 0.07f);
				Syn.Tone(B, 1.1f, 52.f * Var, 140.f, 0.025f, 0.09f);
				Syn.Noise(B, 0.28f, 600.f, 0.f, 0.38f, 0.02f, 0.02f);
				FSynth::SoftClip(B, 1.8f);
				break;
			case ETOSound::PistolShot:
				B = Syn.Make(0.6f);
				Syn.Noise(B, 1.0f, 0.f, 2000.f, 0.005f);
				Syn.Noise(B, 0.8f, 2600.f * Var, 0.f, 0.035f);
				Syn.Tone(B, 0.6f, 90.f * Var, 200.f, 0.015f, 0.05f);
				Syn.Noise(B, 0.15f, 900.f, 0.f, 0.18f, 0.015f, 0.015f);
				FSynth::SoftClip(B, 1.5f);
				break;
			case ETOSound::SMGShot:
				B = Syn.Make(0.55f);
				Syn.Noise(B, 0.9f, 0.f, 2200.f, 0.004f);
				Syn.Noise(B, 0.75f, 2800.f * Var, 0.f, 0.03f);
				Syn.Tone(B, 0.5f, 100.f * Var, 220.f, 0.012f, 0.04f);
				Syn.Noise(B, 0.14f, 900.f, 0.f, 0.16f, 0.015f, 0.015f);
				FSynth::SoftClip(B, 1.4f);
				break;
			case ETOSound::SniperShot:
				B = Syn.Make(1.8f);
				Syn.Noise(B, 1.2f, 0.f, 1400.f, 0.009f);
				Syn.Noise(B, 1.1f, 1500.f, 0.f, 0.09f);
				Syn.Tone(B, 1.2f, 45.f, 130.f, 0.03f, 0.12f);
				Syn.Noise(B, 0.35f, 500.f, 0.f, 0.65f, 0.03f, 0.03f);
				FSynth::SoftClip(B, 2.0f);
				break;
			case ETOSound::ShotgunShot:
				B = Syn.Make(1.2f);
				Syn.Noise(B, 1.0f, 0.f, 1200.f, 0.008f);
				Syn.Noise(B, 1.1f, 1300.f, 0.f, 0.09f);
				Syn.Tone(B, 1.2f, 48.f, 120.f, 0.03f, 0.13f);
				Syn.Noise(B, 0.3f, 500.f, 0.f, 0.42f, 0.02f, 0.02f);
				FSynth::SoftClip(B, 2.0f);
				break;
			case ETOSound::SuppressedShot:
				B = Syn.Make(0.35f);
				Syn.Noise(B, 0.6f, 900.f * Var, 0.f, 0.03f);
				Syn.Tone(B, 0.5f, 110.f, 260.f, 0.01f, 0.03f);
				Syn.Tone(B, 0.25f, 2800.f, 3200.f, 0.01f, 0.008f, 0.04f);
				Syn.Noise(B, 0.2f, 0.f, 3000.f, 0.01f, 0.04f);
				break;
			case ETOSound::DistantShot:
				B = Syn.Make(1.6f);
				Syn.Noise(B, 0.9f, 420.f, 0.f, 0.12f);
				Syn.Tone(B, 0.5f, 50.f, 90.f, 0.05f, 0.12f);
				Syn.Noise(B, 0.4f, 300.f, 0.f, 0.55f, 0.05f, 0.05f);
				break;
			case ETOSound::Explosion:
				B = Syn.Make(2.8f);
				Syn.Noise(B, 1.2f, 0.f, 900.f, 0.02f);
				Syn.Noise(B, 1.4f, 260.f, 0.f, 0.55f);
				Syn.Tone(B, 1.3f, 35.f, 90.f, 0.08f, 0.35f);
				Syn.Noise(B, 0.4f, 1600.f, 0.f, 0.25f, 0.05f);
				FSynth::SoftClip(B, 2.2f);
				break;
			case ETOSound::Footstep:
				B = Syn.Make(0.16f);
				Syn.Noise(B, 0.8f, 1100.f * Var, 180.f, 0.03f);
				Syn.Tone(B, 0.35f, 80.f, 120.f, 0.01f, 0.025f);
				break;
			case ETOSound::FootstepWater:
				B = Syn.Make(0.3f);
				Syn.Noise(B, 0.9f, 2600.f, 250.f, 0.08f, 0.f, 0.01f);
				break;
			case ETOSound::Land:
				B = Syn.Make(0.3f);
				Syn.Noise(B, 0.9f, 900.f, 0.f, 0.05f);
				Syn.Tone(B, 0.8f, 55.f, 100.f, 0.02f, 0.07f);
				break;
			case ETOSound::ReloadStart:
				B = Syn.Make(0.45f);
				Syn.Tone(B, 0.5f, 2300.f, 2600.f, 0.01f, 0.01f);
				Syn.Noise(B, 0.5f, 0.f, 2500.f, 0.008f);
				Syn.Noise(B, 0.3f, 3000.f, 600.f, 0.06f, 0.12f, 0.03f);
				Syn.Tone(B, 0.3f, 1700.f, 1900.f, 0.01f, 0.02f, 0.28f);
				break;
			case ETOSound::ReloadEnd:
				B = Syn.Make(0.5f);
				Syn.Tone(B, 0.6f, 1800.f, 2400.f, 0.01f, 0.03f);
				Syn.Noise(B, 0.6f, 0.f, 2200.f, 0.01f);
				Syn.Tone(B, 0.7f, 1300.f, 2000.f, 0.01f, 0.04f, 0.2f);
				Syn.Noise(B, 0.7f, 0.f, 1800.f, 0.012f, 0.2f);
				break;
			case ETOSound::DryFire:
				B = Syn.Make(0.1f);
				Syn.Tone(B, 0.6f, 2600.f, 3000.f, 0.005f, 0.006f);
				Syn.Noise(B, 0.4f, 0.f, 3000.f, 0.005f);
				break;
			case ETOSound::HitMarker:
				B = Syn.Make(0.07f);
				Syn.Tone(B, 0.8f, 2600.f, 2600.f, 0.01f, 0.012f);
				Syn.Noise(B, 0.3f, 0.f, 4000.f, 0.004f);
				break;
			case ETOSound::KillMarker:
				B = Syn.Make(0.35f);
				Syn.Tone(B, 0.7f, 1400.f, 1400.f, 0.01f, 0.05f);
				Syn.Tone(B, 0.7f, 2100.f, 2100.f, 0.01f, 0.08f, 0.09f);
				break;
			case ETOSound::Headshot:
				B = Syn.Make(0.4f);
				Syn.Tone(B, 0.7f, 3200.f, 3400.f, 0.01f, 0.08f);
				Syn.Tone(B, 0.4f, 4700.f, 4700.f, 0.01f, 0.05f);
				Syn.Noise(B, 0.3f, 0.f, 3500.f, 0.006f);
				break;
			case ETOSound::Whiz:
				B = Syn.Make(0.28f);
				Syn.Noise(B, 0.8f, 3800.f, 900.f, 0.12f, 0.f, 0.08f);
				Syn.Tone(B, 0.25f, 380.f, 1200.f, 0.06f, 0.1f, 0.f, 0.06f);
				break;
			case ETOSound::Impact:
				B = Syn.Make(0.1f);
				Syn.Noise(B, 0.8f, 3000.f * Var, 300.f, 0.012f);
				break;
			case ETOSound::UIClick:
				B = Syn.Make(0.05f);
				Syn.Tone(B, 0.5f, 1800.f, 1900.f, 0.005f, 0.008f);
				break;
			case ETOSound::Beep:
				B = Syn.Make(0.16f);
				Syn.Tone(B, 0.5f, 880.f, 880.f, 0.01f, 0.08f, 0.f, 0.005f);
				break;
			case ETOSound::LootReveal:
				B = Syn.Make(0.14f);
				Syn.Tone(B, 0.45f, 1500.f, 1200.f, 0.03f, 0.04f, 0.f, 0.004f);
				break;
			case ETOSound::LootRare:
				B = Syn.Make(1.0f);
				Syn.Tone(B, 0.4f, 1046.f, 1046.f, 0.01f, 0.25f, 0.f, 0.005f);
				Syn.Tone(B, 0.4f, 1318.f, 1318.f, 0.01f, 0.25f, 0.09f, 0.005f);
				Syn.Tone(B, 0.4f, 1568.f, 1568.f, 0.01f, 0.35f, 0.18f, 0.005f);
				Syn.Tone(B, 0.3f, 2093.f, 2093.f, 0.01f, 0.4f, 0.27f, 0.005f);
				break;
			case ETOSound::Door:
				B = Syn.Make(0.6f);
				Syn.Tone(B, 0.4f, 160.f, 240.f, 0.2f, 0.25f, 0.f, 0.05f);
				Syn.Noise(B, 0.3f, 800.f, 100.f, 0.2f, 0.f, 0.05f);
				Syn.Noise(B, 0.6f, 600.f, 0.f, 0.04f, 0.45f);
				break;
			case ETOSound::Pickup:
				B = Syn.Make(0.15f);
				Syn.Noise(B, 0.5f, 2500.f, 400.f, 0.04f);
				Syn.Tone(B, 0.3f, 900.f, 700.f, 0.02f, 0.03f);
				break;
			case ETOSound::Heal:
				B = Syn.Make(0.5f);
				Syn.Noise(B, 0.5f, 900.f, 150.f, 0.18f, 0.f, 0.15f);
				break;
			case ETOSound::Ability:
				B = Syn.Make(0.7f);
				Syn.Tone(B, 0.5f, 900.f, 300.f, 0.25f, 0.3f, 0.f, 0.05f);
				Syn.Tone(B, 0.3f, 1350.f, 450.f, 0.25f, 0.3f, 0.f, 0.05f);
				break;
			case ETOSound::GrenadeBounce:
				B = Syn.Make(0.12f);
				Syn.Tone(B, 0.6f, 2400.f, 2600.f, 0.01f, 0.02f);
				Syn.Noise(B, 0.4f, 0.f, 1500.f, 0.01f);
				break;
			case ETOSound::Tinnitus:
				B = Syn.Make(3.5f);
				Syn.Tone(B, 0.35f, 3800.f, 3800.f, 1.f, 1.4f, 0.f, 0.02f);
				break;
			case ETOSound::Heartbeat:
				B = Syn.Make(0.8f);
				Syn.Tone(B, 0.9f, 55.f, 80.f, 0.02f, 0.06f);
				Syn.Tone(B, 0.7f, 50.f, 75.f, 0.02f, 0.06f, 0.24f);
				break;
			case ETOSound::Capture:
				B = Syn.Make(0.4f);
				Syn.Tone(B, 0.45f, 660.f, 660.f, 0.01f, 0.06f, 0.f, 0.005f);
				Syn.Tone(B, 0.45f, 990.f, 990.f, 0.01f, 0.08f, 0.15f, 0.005f);
				break;
			default:
				B = Syn.Make(0.05f);
				break;
			}
			FSynth::Normalize(B, 0.95f);
			FSynth::FadeOut(B, 0.02f, SampleRate);
			const int32 Index = S * MaxVariants + V;
			Clips[Index] = ToPCM(B);
			ClipSeconds[Index] = B.Num() / (float)SampleRate;
		}
	}
}

void UTOAudio::SpawnClip(ETOSound Sound, int32 Variant, const FVector* Location, float Volume, float Pitch)
{
	UWorld* W = World.Get();
	if (!W || W->bIsTearingDown)
	{
		return;
	}
	const int32 Index = (int32)Sound * MaxVariants + Variant;
	if (!Clips.IsValidIndex(Index) || Clips[Index].Num() == 0)
	{
		return;
	}

	// Voice budget: drop low priority sounds when busy.
	if (Active.Num() > 40 && (Sound == ETOSound::Footstep || Sound == ETOSound::Impact || Sound == ETOSound::FootstepWater))
	{
		return;
	}
	if (Active.Num() > 64)
	{
		return;
	}

	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(this);
	Wave->SetSampleRate(SampleRate);
	Wave->NumChannels = 1;
	Wave->Duration = ClipSeconds[Index];
	Wave->bLooping = false;
	const TArray<int16>& PCM = Clips[Index];
	Wave->QueueAudio(reinterpret_cast<const uint8*>(PCM.GetData()), PCM.Num() * sizeof(int16));

	UAudioComponent* Comp = nullptr;
	const float FinalVolume = Volume * MasterVolume;
	if (Location)
	{
		USoundAttenuation* Att = Attenuations.IsValidIndex(AttenuationIndex(Sound)) ? Attenuations[AttenuationIndex(Sound)].Get() : nullptr;
		Comp = UGameplayStatics::SpawnSoundAtLocation(W, Wave, *Location, FRotator::ZeroRotator, FinalVolume, Pitch, 0.f, Att, nullptr, true);
	}
	else
	{
		Comp = UGameplayStatics::SpawnSound2D(W, Wave, FinalVolume, Pitch, 0.f, nullptr, false, true);
	}

	RecentWaves.Add(Wave);
	if (RecentWaves.Num() > 96)
	{
		RecentWaves.RemoveAt(0, 32);
	}

	if (Comp)
	{
		Active.Add(Comp);
		ActiveEnd.Add(Clock + ClipSeconds[Index] / FMath::Max(0.25f, Pitch) + 0.1f);
	}
}

void UTOAudio::Play(ETOSound Sound, const FVector& Location, float Volume, float Pitch)
{
	UWorld* W = World.Get();
	if (!W)
	{
		return;
	}
	ETOSound Final = Sound;
	float Vol = Volume;
	if (APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(W, 0))
	{
		const float Dist = FVector::Dist(Cam->GetCameraLocation(), Location);
		if (Dist > MaxAudibleDistance(Sound))
		{
			return;
		}
		// Far gunfire is replaced by the rolling "distant" layer.
		const bool bGun = AttenuationIndex(Sound) == 0 && Sound != ETOSound::DistantShot;
		if (bGun && Dist > 12000.f)
		{
			Final = ETOSound::DistantShot;
			Vol *= 1.4f;
		}
	}
	const int32 Variant = FMath::RandRange(0, NumVariants(Final) - 1);
	SpawnClip(Final, Variant, &Location, Vol, Pitch * FMath::FRandRange(0.96f, 1.04f));
}

void UTOAudio::Play2D(ETOSound Sound, float Volume, float Pitch)
{
	const int32 Variant = FMath::RandRange(0, NumVariants(Sound) - 1);
	SpawnClip(Sound, Variant, nullptr, Volume, Pitch);
}

void UTOAudio::Tick(float DeltaTime)
{
	Clock += DeltaTime;
	for (int32 i = Active.Num() - 1; i >= 0; --i)
	{
		UAudioComponent* C = Active[i];
		if (!IsValid(C) || Clock >= ActiveEnd[i])
		{
			if (IsValid(C))
			{
				C->Stop();
			}
			Active.RemoveAtSwap(i);
			ActiveEnd.RemoveAtSwap(i);
		}
	}
}
