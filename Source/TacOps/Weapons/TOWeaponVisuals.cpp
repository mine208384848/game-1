// TAC-OPS - procedural weapon models

#include "Weapons/TOWeaponVisuals.h"
#include "Core/TODatabase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/StaticMesh.h"

namespace TOWeaponVisuals
{
	UStaticMeshComponent* AddPart(AActor* Owner, USceneComponent* Parent, ETOShape Shape, ETOMat Mat,
		const FVector& Location, const FVector& SizeCm, const FRotator& Rotation, bool bFirstPerson, bool bHideFromOwner)
	{
		if (!Owner || !Parent)
		{
			return nullptr;
		}
		UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(Owner);
		if (!Lib)
		{
			return nullptr;
		}

		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner);
		C->SetStaticMesh(Lib->GetMesh(Shape));
		C->SetMaterial(0, Lib->GetMaterial(Mat));
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetGenerateOverlapEvents(false);
		C->SetCanEverAffectNavigation(false);
		C->bReceivesDecals = false;
		C->SetMobility(EComponentMobility::Movable);
		if (bFirstPerson)
		{
			C->SetCastShadow(false);
			C->SetOnlyOwnerSee(true);
			C->bUseAsOccluder = false;
		}
		else
		{
			C->SetCastShadow(true);
			C->SetOwnerNoSee(bHideFromOwner);
			C->bCastHiddenShadow = true;
			C->SetCullDistance(16000.f);
		}
		C->SetupAttachment(Parent);
		C->SetRelativeLocation(Location);
		C->SetRelativeRotation(Rotation);
		C->SetRelativeScale3D(SizeCm / 100.f);
		C->RegisterComponent();
		return C;
	}

	void DestroyParts(TArray<UStaticMeshComponent*>& Parts)
	{
		for (UStaticMeshComponent* P : Parts)
		{
			if (IsValid(P))
			{
				P->DestroyComponent();
			}
		}
		Parts.Reset();
	}

	// Helpers ------------------------------------------------------------------------------------

	struct FBuilder
	{
		AActor* Owner = nullptr;
		USceneComponent* Parent = nullptr;
		bool bFP = false;
		bool bHide = false;
		FTOGunVisual* Out = nullptr;

		UStaticMeshComponent* Box(ETOMat M, FVector Loc, FVector Size, FRotator Rot = FRotator::ZeroRotator)
		{
			UStaticMeshComponent* C = AddPart(Owner, Parent, ETOShape::Cube, M, Loc, Size, Rot, bFP, bHide);
			if (C) Out->Parts.Add(C);
			return C;
		}

		/** Cylinder along +X with given diameter and length, centered at Loc. */
		UStaticMeshComponent* CylX(ETOMat M, FVector Loc, float Diameter, float Length)
		{
			UStaticMeshComponent* C = AddPart(Owner, Parent, ETOShape::Cylinder, M, Loc, FVector(Diameter, Diameter, Length), FRotator(90.f, 0.f, 0.f), bFP, bHide);
			if (C) Out->Parts.Add(C);
			return C;
		}

		/** Cylinder along Z. */
		UStaticMeshComponent* CylZ(ETOMat M, FVector Loc, float Diameter, float Length, FRotator Rot = FRotator::ZeroRotator)
		{
			UStaticMeshComponent* C = AddPart(Owner, Parent, ETOShape::Cylinder, M, Loc, FVector(Diameter, Diameter, Length), Rot, bFP, bHide);
			if (C) Out->Parts.Add(C);
			return C;
		}

		/** Cylinder along Y (drums, wheels). */
		UStaticMeshComponent* CylY(ETOMat M, FVector Loc, float Diameter, float Length)
		{
			UStaticMeshComponent* C = AddPart(Owner, Parent, ETOShape::Cylinder, M, Loc, FVector(Diameter, Diameter, Length), FRotator(0.f, 0.f, 90.f), bFP, bHide);
			if (C) Out->Parts.Add(C);
			return C;
		}

		/** Hollow square frame along X around the sight line (looks like a red dot / holo window). */
		void Frame(ETOMat M, float X, float Z, float Len, float WinW, float WinH, float Thick)
		{
			Out->OpticParts.Add(Box(M, FVector(X, 0.f, Z + WinH * 0.5f + Thick * 0.5f), FVector(Len, WinW + Thick * 2.f, Thick)));
			Out->OpticParts.Add(Box(M, FVector(X, 0.f, Z - WinH * 0.5f - Thick * 0.5f), FVector(Len, WinW + Thick * 2.f, Thick)));
			Out->OpticParts.Add(Box(M, FVector(X, -WinW * 0.5f - Thick * 0.5f, Z), FVector(Len, Thick, WinH)));
			Out->OpticParts.Add(Box(M, FVector(X, WinW * 0.5f + Thick * 0.5f, Z), FVector(Len, Thick, WinH)));
		}
	};

	void BuildGun(AActor* Owner, USceneComponent* Parent, const FTOWeaponConfig& Config, bool bFirstPerson, bool bHideFromOwner, FTOGunVisual& Out)
	{
		Out = FTOGunVisual();
		const FTOWeaponDef* Def = TODB::FindWeapon(Config.WeaponId);
		if (!Def || !Owner || !Parent)
		{
			return;
		}
		const FTOWeaponStats Stats = TODB::ComputeStats(Config);

		FBuilder B;
		B.Owner = Owner;
		B.Parent = Parent;
		B.bFP = bFirstPerson;
		B.bHide = bHideFromOwner;
		B.Out = &Out;

		const ETOMat Body = Def->BodyMat;
		const ETOMat Furn = Def->FurnitureMat;
		const ETOMat Dark = ETOMat::Gun;
		const FName Optic = Config.GetAttachment(ETOAttachSlot::Optic);
		const FName Muzzle = Config.GetAttachment(ETOAttachSlot::Muzzle);
		const FName Barrel = Config.GetAttachment(ETOAttachSlot::Barrel);
		const FName Under = Config.GetAttachment(ETOAttachSlot::Underbarrel);
		const FName Stock = Config.GetAttachment(ETOAttachSlot::Stock);
		const FName Mag = Config.GetAttachment(ETOAttachSlot::Magazine);
		const FName Tac = Config.GetAttachment(ETOAttachSlot::Tactical);

		float BarrelLen = Def->BarrelLen;
		if (Barrel == FName(TEXT("Brl_Short"))) BarrelLen *= 0.75f;
		if (Barrel == FName(TEXT("Brl_Long"))) BarrelLen *= 1.25f;

		const float RL = Def->RecvLen;
		const float RH = Def->RecvH;
		float FrontX = RL - 10.f;      // where the receiver ends and the barrel starts
		float HandguardLen = BarrelLen * 0.6f;
		float TopZ = RH * 0.4f;        // top of receiver / rail
		float RearX = -10.f;
		bool bHasRail = true;
		bool bIronSights = Optic.IsNone();
		float MagLenMult = 1.f;
		if (Mag == FName(TEXT("Mag_Ext"))) MagLenMult = 1.4f;

		Out.SightHeight = Stats.SightHeight;

		switch (Def->Style)
		{
		// ----------------------------------------------------------------------------- Knife
		case 9:
		{
			B.Box(ETOMat::Rubber, FVector(-3.f, 0.f, 0.f), FVector(10.f, 2.4f, 3.f));
			B.Box(ETOMat::MetalDark, FVector(2.5f, 0.f, 0.f), FVector(1.f, 3.6f, 4.6f));
			B.Box(ETOMat::MetalGrey, FVector(12.f, 0.f, 0.3f), FVector(18.f, 0.4f, 3.f));
			B.Box(ETOMat::MetalGrey, FVector(21.5f, 0.f, 0.8f), FVector(2.f, 0.4f, 1.8f), FRotator(-35.f, 0.f, 0.f));
			Out.MuzzleLocal = FVector(22.f, 0.f, 0.f);
			Out.RightHandLocal = FVector(-3.f, 0.f, 0.f);
			Out.LeftHandLocal = FVector(-3.f, -6.f, -6.f);
			Out.Length = 30.f;
			Out.RearX = -8.f;
			return;
		}
		// ----------------------------------------------------------------------------- Pistol
		case 8:
		{
			const float SlideLen = RL;
			B.Box(Body, FVector(SlideLen * 0.5f - 5.f, 0.f, 1.2f), FVector(SlideLen, 2.8f, 3.2f));
			B.Box(Dark, FVector(SlideLen * 0.45f - 5.f, 0.f, -1.2f), FVector(SlideLen * 0.85f, 2.6f, 1.8f));
			B.Box(Furn, FVector(-2.f, 0.f, -5.8f), FVector(3.2f, 2.7f, 9.f), FRotator(-14.f, 0.f, 0.f));
			B.Box(Dark, FVector(2.f, 0.f, -2.8f), FVector(4.5f, 0.6f, 0.6f));
			Out.Magazine = B.Box(Dark, FVector(-3.2f, 0.f, -10.6f), FVector(3.0f, 2.4f, 1.6f * MagLenMult), FRotator(-14.f, 0.f, 0.f));
			if (Out.Magazine) Out.MagazineRest = Out.Magazine->GetRelativeLocation();
			B.Box(Dark, FVector(SlideLen - 5.6f, 0.f, 3.1f), FVector(0.8f, 0.5f, 0.8f));
			B.Box(Dark, FVector(-4.f, 0.f, 3.1f), FVector(0.8f, 1.6f, 0.8f));
			FrontX = SlideLen - 5.f;
			TopZ = 2.8f;
			RearX = -5.f;
			Out.RightHandLocal = FVector(-2.f, 0.f, -5.f);
			Out.LeftHandLocal = FVector(-1.f, -2.f, -6.f);
			bHasRail = false;
			if (bIronSights)
			{
				Out.SightHeight = 3.6f;
			}
			Out.LightLocal = FVector(FrontX - 4.f, 0.f, -3.2f);
			break;
		}
		// ----------------------------------------------------------------------------- Bolt action
		case 5:
		{
			const ETOMat StockMat = (Furn == ETOMat::Gun) ? ETOMat::Plastic : Furn;
			B.Box(StockMat, FVector(-4.f, 0.f, -3.2f), FVector(62.f, 4.2f, 5.2f));
			B.Box(StockMat, FVector(-28.f, 0.f, -5.f), FVector(16.f, 4.0f, 11.f));
			B.Box(ETOMat::Rubber, FVector(-36.5f, 0.f, -5.f), FVector(1.2f, 4.2f, 11.5f));
			B.Box(StockMat, FVector(-8.f, 0.f, -8.f), FVector(4.f, 3.4f, 7.f), FRotator(-20.f, 0.f, 0.f));
			B.CylX(Body, FVector(7.f, 0.f, 0.6f), 3.4f, 28.f);
			B.CylZ(ETOMat::MetalGrey, FVector(4.f, 3.4f, 0.4f), 1.0f, 4.f, FRotator(0.f, 0.f, 90.f));
			B.Box(ETOMat::MetalGrey, FVector(4.f, 5.4f, -0.6f), FVector(1.6f, 1.6f, 1.6f));
			Out.Magazine = B.Box(Dark, FVector(9.f, 0.f, -6.8f * MagLenMult), FVector(6.f, 3.f, 4.f * MagLenMult));
			if (Out.Magazine) Out.MagazineRest = Out.Magazine->GetRelativeLocation();
			FrontX = 21.f;
			HandguardLen = 0.f;
			TopZ = 2.4f;
			RearX = -37.f;
			Out.RightHandLocal = FVector(-7.f, 0.f, -6.f);
			Out.LeftHandLocal = FVector(20.f, 0.f, -6.f);
			Out.LightLocal = FVector(26.f, 3.f, -2.f);
			break;
		}
		// ----------------------------------------------------------------------------- Bullpup
		case 2:
		{
			B.Box(Body, FVector(-8.f, 0.f, -1.f), FVector(44.f, 5.2f, RH));
			B.Box(Body, FVector(-26.f, 0.f, -2.5f), FVector(10.f, 5.0f, RH + 3.f));
			B.Box(ETOMat::Rubber, FVector(-31.5f, 0.f, -2.5f), FVector(1.2f, 5.2f, RH + 3.f));
			B.Box(Body, FVector(0.f, 0.f, -7.f), FVector(3.4f, 3.f, 9.f), FRotator(-12.f, 0.f, 0.f));
			B.Box(Body, FVector(10.f, 0.f, -6.5f), FVector(3.0f, 3.f, 8.f), FRotator(10.f, 0.f, 0.f));
			Out.Magazine = B.Box(Dark, FVector(-11.f, 0.f, -9.5f * MagLenMult), FVector(3.0f, 2.4f, 13.f * MagLenMult), FRotator(10.f, 0.f, 0.f));
			if (Out.Magazine) Out.MagazineRest = Out.Magazine->GetRelativeLocation();
			FrontX = 14.f;
			HandguardLen = 4.f;
			TopZ = RH * 0.5f;
			RearX = -32.f;
			Out.RightHandLocal = FVector(0.f, 0.f, -5.f);
			Out.LeftHandLocal = FVector(10.f, 0.f, -6.f);
			Out.LightLocal = FVector(16.f, 3.2f, -1.f);
			break;
		}
		// ----------------------------------------------------------------------------- Compact (Vector / P90)
		case 10:
		{
			B.Box(Body, FVector(RL * 0.5f - 14.f, 0.f, -1.f), FVector(RL, 5.2f, RH));
			B.Box(Body, FVector(-2.f, 0.f, -RH * 0.5f - 4.f), FVector(3.2f, 3.f, 8.f), FRotator(-12.f, 0.f, 0.f));
			if (Def->Id == FName(TEXT("P90")))
			{
				Out.Magazine = B.Box(ETOMat::Plastic, FVector(RL * 0.5f - 12.f, 0.f, RH * 0.5f + 0.4f), FVector(RL * 0.75f, 4.6f, 1.8f));
				TopZ = RH * 0.5f + 1.4f;
			}
			else
			{
				Out.Magazine = B.Box(Dark, FVector(7.f, 0.f, -RH * 0.5f - 8.f * MagLenMult), FVector(2.8f, 2.4f, 14.f * MagLenMult));
				TopZ = RH * 0.5f;
			}
			if (Out.Magazine) Out.MagazineRest = Out.Magazine->GetRelativeLocation();
			B.Box(Dark, FVector(-20.f, 0.f, -1.f), FVector(12.f, 3.4f, 5.f));
			FrontX = RL - 14.f;
			HandguardLen = 0.f;
			RearX = -26.f;
			Out.RightHandLocal = FVector(-2.f, 0.f, -6.f);
			Out.LeftHandLocal = FVector(RL - 18.f, 0.f, -RH * 0.5f - 1.f);
			Out.LightLocal = FVector(FrontX - 3.f, 3.2f, 0.f);
			break;
		}
		// ----------------------------------------------------------------------------- Rifles / SMG / DMR / LMG / Shotgun
		default:
		{
			const bool bAK = Def->Style == 1;
			const bool bSMG = Def->Style == 3;
			const bool bDMR = Def->Style == 4;
			const bool bLMG = Def->Style == 6;
			const bool bShotgun = Def->Style == 7;

			// Receiver
			B.Box(Body, FVector(RL * 0.5f - 10.f, 0.f, -RH * 0.1f), FVector(RL, 4.4f, RH));
			if (bHasRail && !bAK && !bShotgun)
			{
				B.Box(Dark, FVector(RL * 0.5f - 10.f, 0.f, TopZ + 0.6f), FVector(RL * 0.9f, 2.2f, 1.2f));
			}
			if (bAK)
			{
				// AK dust cover & gas tube
				B.Box(Dark, FVector(RL * 0.5f - 12.f, 0.f, TopZ + 0.5f), FVector(RL * 0.8f, 3.8f, 1.2f));
			}

			// Pistol grip
			B.Box(bAK ? Furn : Dark, FVector(-3.f, 0.f, -RH * 0.6f - 3.6f), FVector(3.2f, 3.f, 9.f), FRotator(-16.f, 0.f, 0.f));
			// Trigger guard
			B.Box(Dark, FVector(1.5f, 0.f, -RH * 0.6f - 1.4f), FVector(5.f, 0.6f, 0.6f));

			// Magazine
			if (bShotgun)
			{
				Out.Magazine = nullptr;
			}
			else if (Mag == FName(TEXT("Mag_Drum")))
			{
				Out.Magazine = B.CylY(Dark, FVector(5.f, 0.f, -RH * 0.6f - 7.f), 12.f, 5.f);
			}
			else if (bLMG)
			{
				Out.Magazine = B.Box(ETOMat::MetalGreen, FVector(4.f, -1.f, -RH * 0.6f - 6.f), FVector(10.f, 7.f, 10.f));
			}
			else
			{
				const float MagH = (bDMR ? 11.f : (bSMG ? 15.f : 16.f)) * MagLenMult;
				const float MagPitch = bAK ? 18.f : (bSMG ? 12.f : 7.f);
				Out.Magazine = B.Box(Dark, FVector(6.f, 0.f, -RH * 0.6f - MagH * 0.45f), FVector(3.2f, 2.4f, MagH), FRotator(MagPitch, 0.f, 0.f));
			}
			if (Out.Magazine)
			{
				Out.MagazineRest = Out.Magazine->GetRelativeLocation();
			}
			if (Mag == FName(TEXT("Mag_Fast")) && Out.Magazine)
			{
				B.Box(ETOMat::MetalRed, Out.MagazineRest + FVector(0.f, 0.f, -9.f), FVector(1.2f, 2.6f, 1.2f));
			}

			// Handguard
			if (bShotgun)
			{
				B.CylX(Dark, FVector(FrontX + BarrelLen * 0.42f, 0.f, -2.6f), 2.4f, BarrelLen * 0.84f);
				B.Box(Furn, FVector(FrontX + BarrelLen * 0.35f, 0.f, -2.6f), FVector(14.f, 4.2f, 4.2f));
				HandguardLen = 0.f;
			}
			else
			{
				HandguardLen = bSMG ? BarrelLen * 0.75f : BarrelLen * 0.6f;
				B.Box(bAK ? Furn : Body, FVector(FrontX + HandguardLen * 0.5f, 0.f, -0.6f), FVector(HandguardLen, 5.0f, 5.4f));
				if (bAK)
				{
					B.CylX(Dark, FVector(FrontX + HandguardLen * 0.5f, 0.f, 3.2f), 1.6f, HandguardLen + 4.f);
				}
				else if (bHasRail)
				{
					B.Box(Dark, FVector(FrontX + HandguardLen * 0.5f, 0.f, 2.7f), FVector(HandguardLen, 2.2f, 0.8f));
				}
			}

			// Stock
			if (Stock == FName(TEXT("Stk_Light")))
			{
				B.Box(Dark, FVector(-21.f, 0.f, 0.5f), FVector(20.f, 1.2f, 1.2f));
				B.Box(Dark, FVector(-21.f, 0.f, -4.5f), FVector(20.f, 1.2f, 1.2f));
				B.Box(ETOMat::Rubber, FVector(-31.f, 0.f, -2.f), FVector(1.4f, 3.6f, 8.f));
			}
			else if (Stock == FName(TEXT("Stk_Heavy")))
			{
				B.Box(Furn == ETOMat::Gun ? ETOMat::Plastic : Furn, FVector(-23.f, 0.f, -2.5f), FVector(26.f, 4.2f, 9.f));
				B.Box(ETOMat::Rubber, FVector(-36.5f, 0.f, -2.5f), FVector(1.4f, 4.4f, 9.5f));
			}
			else if (bAK || bShotgun || Furn == ETOMat::Wood || Furn == ETOMat::WoodDark)
			{
				B.Box(Furn, FVector(-22.f, 0.f, -3.f), FVector(24.f, 3.8f, 7.f), FRotator(-6.f, 0.f, 0.f));
				B.Box(ETOMat::Rubber, FVector(-34.5f, 0.f, -4.2f), FVector(1.2f, 4.0f, 8.f), FRotator(-6.f, 0.f, 0.f));
			}
			else
			{
				B.CylX(Dark, FVector(-17.f, 0.f, -0.5f), 2.8f, 16.f);
				B.Box(Furn == ETOMat::Gun ? ETOMat::Plastic : Furn, FVector(-24.f, 0.f, -2.f), FVector(14.f, 3.8f, 7.f));
				B.Box(ETOMat::Rubber, FVector(-31.5f, 0.f, -2.f), FVector(1.2f, 4.0f, 7.5f));
			}
			RearX = -34.f;

			if (bLMG)
			{
				// Carry handle
				B.Box(Dark, FVector(RL * 0.35f - 10.f, 0.f, TopZ + 3.4f), FVector(12.f, 1.4f, 1.4f));
				B.Box(Dark, FVector(RL * 0.35f - 15.f, 0.f, TopZ + 1.8f), FVector(1.2f, 1.4f, 3.2f));
				B.Box(Dark, FVector(RL * 0.35f - 5.f, 0.f, TopZ + 1.8f), FVector(1.2f, 1.4f, 3.2f));
			}

			Out.RightHandLocal = FVector(-3.f, 0.f, -RH * 0.6f - 2.f);
			Out.LeftHandLocal = FVector(FrontX + FMath::Max(8.f, HandguardLen * 0.55f), 0.f, -4.f);
			Out.LightLocal = FVector(FrontX + FMath::Max(6.f, HandguardLen * 0.7f), 3.4f, -0.5f);
			break;
		}
		}

		// Barrel ----------------------------------------------------------------------------------
		B.CylX(Dark, FVector(FrontX + BarrelLen * 0.5f, 0.f, 0.f), Def->Style == 8 ? 1.2f : 1.6f, BarrelLen);
		float MuzzleX = FrontX + BarrelLen;

		// Iron sights
		if (bIronSights && Def->Style != 8 && Def->Style != 5)
		{
			B.Box(Dark, FVector(FrontX + FMath::Max(HandguardLen, 6.f) - 1.f, 0.f, Out.SightHeight - 1.6f), FVector(0.8f, 0.5f, 3.4f));
			B.Box(Dark, FVector(FMath::Max(RearX + 22.f, -8.f), 0.f, Out.SightHeight - 1.2f), FVector(1.4f, 2.2f, 2.4f));
		}

		// Muzzle device ---------------------------------------------------------------------------
		if (Muzzle == FName(TEXT("Muz_Supp")))
		{
			B.CylX(ETOMat::MetalDark, FVector(MuzzleX + 8.f, 0.f, 0.f), 3.6f, 17.f);
			MuzzleX += 17.f;
		}
		else if (Muzzle == FName(TEXT("Muz_Comp")))
		{
			B.CylX(Dark, FVector(MuzzleX + 2.5f, 0.f, 0.f), 2.4f, 5.f);
			MuzzleX += 5.f;
		}
		else if (Muzzle == FName(TEXT("Muz_Brake")))
		{
			B.Box(Dark, FVector(MuzzleX + 2.5f, 0.f, 0.f), FVector(5.f, 3.2f, 2.4f));
			MuzzleX += 5.f;
		}
		else if (Muzzle == FName(TEXT("Muz_Flash")))
		{
			B.CylX(Dark, FVector(MuzzleX + 2.2f, 0.f, 0.f), 2.2f, 4.5f);
			MuzzleX += 4.5f;
		}
		Out.MuzzleLocal = FVector(MuzzleX, 0.f, 0.f);
		Out.Length = MuzzleX - RearX;
		Out.RearX = RearX;

		// Optics ----------------------------------------------------------------------------------
		const float SH = Out.SightHeight;
		const float OpticX = (Def->Style == 8) ? 0.f : (Def->Style == 5 ? 6.f : 2.f);
		switch (Stats.Optic)
		{
		case ETOOpticType::RedDot:
			Out.OpticParts.Add(B.Box(Dark, FVector(OpticX, 0.f, SH - 2.6f), FVector(5.f, 3.f, 1.2f)));
			B.Frame(Dark, OpticX + 0.5f, SH, 2.4f, 2.8f, 2.6f, 0.4f);
			break;
		case ETOOpticType::Holo:
			Out.OpticParts.Add(B.Box(Dark, FVector(OpticX, 0.f, SH - 2.9f), FVector(9.f, 3.6f, 1.6f)));
			Out.OpticParts.Add(B.Box(Dark, FVector(OpticX - 2.5f, 0.f, SH - 1.5f), FVector(3.5f, 3.6f, 1.6f)));
			B.Frame(Dark, OpticX + 2.f, SH, 4.f, 3.6f, 3.0f, 0.5f);
			break;
		case ETOOpticType::Prism:
			Out.OpticParts.Add(B.Box(Dark, FVector(OpticX, 0.f, SH - 2.8f), FVector(6.f, 3.f, 1.2f)));
			Out.OpticParts.Add(B.Box(Dark, FVector(OpticX, 0.f, SH), FVector(9.f, 3.6f, 3.8f)));
			break;
		case ETOOpticType::ACOG:
			Out.OpticParts.Add(B.Box(Dark, FVector(OpticX, 0.f, SH - 3.f), FVector(8.f, 3.f, 1.2f)));
			Out.OpticParts.Add(B.CylX(Dark, FVector(OpticX, 0.f, SH), 3.4f, 12.f));
			Out.OpticParts.Add(B.CylX(Dark, FVector(OpticX + 6.5f, 0.f, SH), 4.4f, 3.f));
			Out.OpticParts.Add(B.Box(ETOMat::LampRed, FVector(OpticX + 2.f, 0.f, SH + 2.1f), FVector(1.2f, 0.8f, 0.6f)));
			break;
		case ETOOpticType::Sniper:
			Out.OpticParts.Add(B.Box(Dark, FVector(OpticX - 4.f, 0.f, SH - 3.6f), FVector(2.f, 3.f, 2.4f)));
			Out.OpticParts.Add(B.Box(Dark, FVector(OpticX + 6.f, 0.f, SH - 3.6f), FVector(2.f, 3.f, 2.4f)));
			Out.OpticParts.Add(B.CylX(Dark, FVector(OpticX, 0.f, SH), 3.0f, 28.f));
			Out.OpticParts.Add(B.CylX(Dark, FVector(OpticX + 13.f, 0.f, SH), 4.6f, 7.f));
			Out.OpticParts.Add(B.CylX(Dark, FVector(OpticX - 12.5f, 0.f, SH), 4.0f, 5.f));
			Out.OpticParts.Add(B.CylZ(Dark, FVector(OpticX + 1.f, 0.f, SH + 2.f), 2.2f, 2.f));
			Out.OpticParts.Add(B.CylZ(Dark, FVector(OpticX + 1.f, 2.f, SH), 2.2f, 2.f, FRotator(0.f, 0.f, 90.f)));
			break;
		default:
			break;
		}

		// Underbarrel -----------------------------------------------------------------------------
		const float UbX = Out.LeftHandLocal.X;
		if (Under == FName(TEXT("Ub_Vert")))
		{
			B.Box(Dark, FVector(UbX + 2.f, 0.f, -7.5f), FVector(2.8f, 2.8f, 8.f));
			Out.LeftHandLocal = FVector(UbX + 2.f, 0.f, -8.f);
		}
		else if (Under == FName(TEXT("Ub_Angled")))
		{
			B.Box(Dark, FVector(UbX - 1.f, 0.f, -4.6f), FVector(6.5f, 2.6f, 2.6f), FRotator(28.f, 0.f, 0.f));
		}
		else if (Under == FName(TEXT("Ub_Bipod")))
		{
			B.Box(Dark, FVector(MuzzleX - BarrelLen * 0.45f, 0.f, -2.2f), FVector(3.f, 3.f, 1.6f));
			B.CylX(Dark, FVector(MuzzleX - BarrelLen * 0.45f - 8.f, -1.3f, -3.4f), 0.9f, 16.f);
			B.CylX(Dark, FVector(MuzzleX - BarrelLen * 0.45f - 8.f, 1.3f, -3.4f), 0.9f, 16.f);
		}

		// Tactical --------------------------------------------------------------------------------
		if (Tac == FName(TEXT("Tac_Light")))
		{
			B.CylX(Dark, Out.LightLocal, 2.4f, 6.f);
			B.CylX(ETOMat::LampCold, Out.LightLocal + FVector(3.1f, 0.f, 0.f), 2.0f, 0.4f);
		}
		else if (Tac == FName(TEXT("Tac_Laser")))
		{
			const FVector L = Out.LightLocal * FVector(1.f, -1.f, 1.f);
			B.Box(Dark, L, FVector(4.f, 2.f, 2.f));
			B.Box(ETOMat::LampRed, L + FVector(2.1f, 0.f, 0.f), FVector(0.3f, 0.8f, 0.8f));
			Out.LightLocal = L;
		}
		else if (Tac == FName(TEXT("Tac_Combo")))
		{
			B.Box(Dark, Out.LightLocal, FVector(6.f, 2.8f, 2.8f));
			B.Box(ETOMat::LampCold, Out.LightLocal + FVector(3.1f, 0.f, 0.5f), FVector(0.3f, 1.8f, 1.2f));
			B.Box(ETOMat::LampRed, Out.LightLocal + FVector(3.1f, 0.f, -0.9f), FVector(0.3f, 0.6f, 0.6f));
		}
		Out.LightLocal.X += 3.5f;
	}
}
