// TAC-OPS - shared gameplay types
//
// Everything the different systems (characters, weapons, AI, world, UI) share lives here.
// Static design data (item / weapon / attachment tables) are plain C++ structs that live
// in TODatabase.cpp.  Runtime data that has to be saved or garbage-collection safe is a USTRUCT.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "TOTypes.generated.h"

/** Custom trace channel "Bullet" (see DefaultEngine.ini). */
#define TO_TRACE_BULLET ECC_GameTraceChannel1

class AActor;
class ATOCharacter;

// ---------------------------------------------------------------------------------------------
//  Enums
// ---------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ETOMatchMode : uint8
{
	Menu,
	Operations,   // PvPvE extraction raid (Hazard Operations style)
	Warfare       // large scale attack & defend
};

UENUM(BlueprintType)
enum class ETOTimeOfDay : uint8
{
	Day,
	Sunset,
	Night
};

UENUM(BlueprintType)
enum class ETOBodyPart : uint8
{
	Head,
	Thorax,
	Stomach,
	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg,
	None
};

UENUM(BlueprintType)
enum class ETORarity : uint8
{
	Common,     // white
	Uncommon,   // green
	Rare,       // blue
	Epic,       // purple
	Legendary,  // gold
	Mythic      // red
};

UENUM(BlueprintType)
enum class ETOItemCategory : uint8
{
	Valuable,
	Medical,
	Ammo,
	Grenade,
	Key,
	Armor,
	Helmet,
	Rig,
	Backpack,
	Weapon
};

UENUM(BlueprintType)
enum class ETOWeaponClass : uint8
{
	AssaultRifle,
	SMG,
	DMR,
	Sniper,
	LMG,
	Shotgun,
	Pistol,
	Melee
};

UENUM(BlueprintType)
enum class ETOFireMode : uint8
{
	Semi,
	Burst,
	Auto,
	Bolt
};

UENUM(BlueprintType)
enum class ETOAttachSlot : uint8
{
	Optic,
	Muzzle,
	Barrel,
	Underbarrel,
	Stock,
	Magazine,
	Tactical,
	Count UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ETOOpticType : uint8
{
	Iron,
	RedDot,
	Holo,
	Prism,
	ACOG,
	Sniper
};

UENUM(BlueprintType)
enum class ETOOperator : uint8
{
	Viper,    // Assault  - Adrenal Surge
	Halo,     // Support  - Heal Pulse
	Bastion,  // Engineer - Deployable Shield
	Echo      // Recon    - Recon Pulse
};

UENUM(BlueprintType)
enum class ETOContainerType : uint8
{
	WeaponCrate,
	AmmoBox,
	MedicalCase,
	Toolbox,
	Safe,
	ComputerTower,
	Duffel,
	FilingCabinet,
	ServerRack,
	Locker,
	HighValueCrate,
	Corpse,
	LooseItem
};

UENUM(BlueprintType)
enum class ETOAIRole : uint8
{
	Guard,
	Boss,
	Rival,
	Teammate,
	WarfareBot
};

UENUM(BlueprintType)
enum class ETOStance : uint8
{
	Stand,
	Crouch,
	Prone
};

UENUM(BlueprintType)
enum class ETOGrenadeType : uint8
{
	Frag,
	Smoke,
	Flash
};

UENUM(BlueprintType)
enum class ETOExtractRule : uint8
{
	Always,        // walk in & wait
	Radio,         // call a helicopter first (loud!)
	Paid,          // pay credits at the terminal
	NoBackpack     // only possible without a backpack
};

/** Surface / material ids used by the procedural world, weapons and characters. */
UENUM()
enum class ETOMat : uint8
{
	Grass, GrassDry, Dirt, Rock, RockDark, Sand, Gravel, Snow,
	Asphalt, RoadLineWhite, RoadLineYellow, Sidewalk,
	Concrete, ConcreteDark, ConcreteLight, ConcreteDam, Plaster, PlasterBeige, PlasterBlue, Brick, Tile, Carpet, FloorWood,
	MetalDark, MetalGrey, MetalRust, MetalGreen, MetalBlue, MetalYellow, MetalRed, MetalWhite, Aluminium,
	Wood, WoodDark, Fabric, Canvas, Sandbag, Rubber, Plastic, PlasticTan,
	Glass, Water, WaterRiver,
	FoliageDark, FoliageLight, FoliagePine, Bark,
	LampWarm, LampCold, LampRed, LampGreen, Screen, TracerOrange, TracerRed, Flash,
	Smoke, SmokeGreen, Blood, Black, Gold, Skin,
	SuitGreen, SuitTan, SuitBlack, SuitGrey, SuitBlue, SuitRed, SuitUrban,
	Gun, GunTan, GunOlive,
	Count UMETA(Hidden)
};

// ---------------------------------------------------------------------------------------------
//  Static design data (plain C++)
// ---------------------------------------------------------------------------------------------

struct FTOItemDef
{
	FName Id;
	FString Name;
	FString ShortName;
	ETOItemCategory Category = ETOItemCategory::Valuable;
	ETORarity Rarity = ETORarity::Common;
	int32 W = 1;
	int32 H = 1;
	int32 Value = 1000;

	// Armor / helmet level, ammo penetration tier, backpack / rig tier.
	int32 Level = 0;
	// Armor / helmet max durability.
	float Durability = 0.f;
	// Rig / backpack capacity.
	int32 GridW = 0;
	int32 GridH = 0;

	// Medical
	float Heal = 0.f;
	int32 Uses = 1;
	bool bStopBleed = false;
	bool bFixFracture = false;
	float UseTime = 2.f;
	float Painkiller = 0.f;

	// Ammo
	FName Caliber;
	int32 StackMax = 1;

	// Weapon item
	FName WeaponId;

	// Keycard
	FName KeyId;

	// Grenade
	ETOGrenadeType GrenadeType = ETOGrenadeType::Frag;

	// Valuable sub-type: electronics / data (found in computers & server racks)
	bool bTech = false;

	FString Description;
};

struct FTOWeaponDef
{
	FName Id;
	FString Name;
	ETOWeaponClass Class = ETOWeaponClass::AssaultRifle;
	FName Caliber;

	float Damage = 30.f;
	float RPM = 700.f;
	int32 MagSize = 30;
	float ReloadTime = 2.2f;
	float ReloadEmptyTime = 2.8f;
	float Velocity = 850.f;      // m/s
	float RecoilV = 0.45f;       // degrees per shot
	float RecoilH = 0.25f;
	float HipSpread = 2.5f;      // degrees
	float AdsSpread = 0.15f;
	float AdsTime = 0.25f;       // seconds
	float Range = 60.f;          // effective range (m)
	float ArmorDamage = 1.f;     // armor durability damage multiplier
	int32 Pellets = 1;
	float MoveMult = 1.f;
	float Noise = 1.f;
	int32 Price = 30000;
	TArray<ETOFireMode> Modes;

	// Procedural visuals (cm)
	uint8 Style = 0;             // 0 AR, 1 AK, 2 bullpup, 3 SMG, 4 DMR, 5 bolt, 6 LMG, 7 shotgun, 8 pistol, 9 knife, 10 P90/Vector
	float RecvLen = 30.f;
	float RecvH = 7.f;
	float BarrelLen = 36.f;
	ETOMat BodyMat = ETOMat::Gun;
	ETOMat FurnitureMat = ETOMat::Gun;

	// Inventory footprint
	int32 InvW = 4;
	int32 InvH = 2;
};

struct FTOAttachmentDef
{
	FName Id;
	FString Name;
	ETOAttachSlot Slot = ETOAttachSlot::Optic;
	uint32 ClassMask = 0;         // bit per ETOWeaponClass
	int32 Price = 3000;

	float RecoilVMult = 1.f;
	float RecoilHMult = 1.f;
	float AdsTimeMult = 1.f;
	float HipSpreadMult = 1.f;
	float RangeMult = 1.f;
	float VelocityMult = 1.f;
	float ReloadMult = 1.f;
	float NoiseMult = 1.f;
	float MagMult = 1.f;
	float MoveMult = 1.f;

	ETOOpticType Optic = ETOOpticType::Iron;
	float Zoom = 1.f;
	bool bSuppressor = false;
	bool bLight = false;
	bool bLaser = false;
	bool bBipod = false;
};

/** Final weapon stats after attachments are applied. */
struct FTOWeaponStats
{
	const FTOWeaponDef* Def = nullptr;
	float Damage = 0.f;
	float RPM = 600.f;
	int32 MagSize = 30;
	float ReloadTime = 2.f;
	float ReloadEmptyTime = 2.5f;
	float Velocity = 800.f;
	float RecoilV = 0.4f;
	float RecoilH = 0.2f;
	float HipSpread = 2.f;
	float AdsSpread = 0.1f;
	float AdsTime = 0.25f;
	float Range = 50.f;
	float Noise = 1.f;
	float MoveMult = 1.f;
	float Zoom = 1.f;
	ETOOpticType Optic = ETOOpticType::Iron;
	bool bSuppressed = false;
	bool bLight = false;
	bool bLaser = false;
	bool bBipod = false;
	float SightHeight = 6.f;
};

struct FTOOperatorDef
{
	ETOOperator Id = ETOOperator::Viper;
	FString Name;
	FString Role;
	FString AbilityName;
	FString AbilityDesc;
	float Cooldown = 40.f;
	ETOMat SuitMat = ETOMat::SuitGreen;
	FLinearColor UIColor = FLinearColor::White;
};

/** Everything needed to resolve a hit on something damageable. */
struct FTODamageInfo
{
	float Damage = 0.f;
	int32 PenLevel = 0;
	float ArmorDamage = 1.f;
	ETOBodyPart Part = ETOBodyPart::Thorax;
	FVector HitLocation = FVector::ZeroVector;
	FVector HitDirection = FVector::ForwardVector;
	TWeakObjectPtr<AActor> Instigator;
	int32 InstigatorTeam = -1;
	FName WeaponId;
	bool bExplosive = false;
	bool bMelee = false;
	bool bFall = false;
	bool bBleed = false;
	float Distance = 0.f;
};

struct FTODamageResult
{
	float Applied = 0.f;
	bool bKilled = false;
	bool bDowned = false;
	bool bArmorHit = false;
	bool bArmorBroken = false;
	bool bHeadshot = false;
};

// ---------------------------------------------------------------------------------------------
//  Runtime data (USTRUCT so it can be saved and is GC aware)
// ---------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FTOWeaponConfig
{
	GENERATED_BODY()

	UPROPERTY() FName WeaponId;
	/** One entry per ETOAttachSlot (NAME_None = empty). */
	UPROPERTY() TArray<FName> Attachments;
	/** Ammo penetration tier used by this weapon (1..6). */
	UPROPERTY() int32 AmmoTier = 3;

	bool IsValid() const { return !WeaponId.IsNone(); }
	FName GetAttachment(ETOAttachSlot Slot) const
	{
		const int32 Idx = (int32)Slot;
		return Attachments.IsValidIndex(Idx) ? Attachments[Idx] : NAME_None;
	}
	void SetAttachment(ETOAttachSlot Slot, FName Id)
	{
		const int32 Idx = (int32)Slot;
		if (Attachments.Num() < (int32)ETOAttachSlot::Count)
		{
			Attachments.SetNum((int32)ETOAttachSlot::Count);
		}
		Attachments[Idx] = Id;
	}
};

USTRUCT(BlueprintType)
struct FTOItemInstance
{
	GENERATED_BODY()

	UPROPERTY() FName ItemId;
	/** Stack size for ammo, remaining uses for medical items. */
	UPROPERTY() int32 Count = 1;
	/** Current durability for armor / helmets. */
	UPROPERTY() float Durability = 0.f;
	UPROPERTY() int32 X = 0;
	UPROPERTY() int32 Y = 0;
	UPROPERTY() bool bRotated = false;
	/** Containers reveal items progressively while being searched. */
	UPROPERTY() bool bRevealed = true;
	UPROPERTY() int32 Uid = 0;
	/** Only used when this item is a weapon. */
	UPROPERTY() FTOWeaponConfig Weapon;

	bool IsValid() const { return !ItemId.IsNone(); }
};

USTRUCT(BlueprintType)
struct FTOGrid
{
	GENERATED_BODY()

	UPROPERTY() FName GridName;
	UPROPERTY() int32 W = 0;
	UPROPERTY() int32 H = 0;
	UPROPERTY() TArray<FTOItemInstance> Items;

	void Init(FName InName, int32 InW, int32 InH) { GridName = InName; W = InW; H = InH; Items.Reset(); }
	bool IsEnabled() const { return W > 0 && H > 0; }

	static void GetFootprint(const FTOItemInstance& Item, int32& OutW, int32& OutH);
	bool CanPlaceAt(int32 IW, int32 IH, int32 PX, int32 PY, int32 IgnoreUid = -1) const;
	bool FindSpot(int32 IW, int32 IH, int32& OutX, int32& OutY, bool& bOutRotated) const;
	/** Merges stacks and places the item; returns how many units could NOT be added (0 = all added). */
	int32 TryAdd(const FTOItemInstance& Item);
	int32 IndexOfUid(int32 Uid) const;
	int64 TotalValue() const;
	int32 CountItem(FName ItemId) const;
};

/** Player loadout selected in the lobby. */
USTRUCT(BlueprintType)
struct FTOLoadout
{
	GENERATED_BODY()

	UPROPERTY() ETOOperator Operator = ETOOperator::Viper;
	UPROPERTY() FTOWeaponConfig Primary;
	UPROPERTY() FTOWeaponConfig Sidearm;
	UPROPERTY() int32 ArmorLevel = 3;
	UPROPERTY() int32 HelmetLevel = 3;
	UPROPERTY() int32 RigTier = 2;
	UPROPERTY() int32 BackpackTier = 2;
	UPROPERTY() int32 MedTier = 1;
	UPROPERTY() int32 Frags = 1;
	UPROPERTY() int32 Smokes = 1;
	UPROPERTY() int32 Flashes = 0;
	UPROPERTY() int32 SpareMags = 4;
};

/** What the next level load should do (lives in the game instance). */
USTRUCT(BlueprintType)
struct FTOSession
{
	GENERATED_BODY()

	UPROPERTY() ETOMatchMode Mode = ETOMatchMode::Menu;
	UPROPERTY() ETOTimeOfDay Time = ETOTimeOfDay::Day;
	UPROPERTY() int32 Difficulty = 0;       // 0 normal, 1 hazard
	UPROPERTY() bool bSquad = true;         // two AI squad mates in Operations
	UPROPERTY() int32 WarfareSide = 0;      // 0 attack, 1 defend
	UPROPERTY() int32 Seed = 1337;
	UPROPERTY() bool bDeploy = false;
	/** Operations: deploy with the free starter kit instead of the paid loadout. */
	UPROPERTY() bool bFreeKit = false;
	UPROPERTY() FTOLoadout Loadout;
};

/** Summary shown after a raid / battle. */
USTRUCT(BlueprintType)
struct FTOMatchResult
{
	GENERATED_BODY()

	UPROPERTY() bool bValid = false;
	UPROPERTY() ETOMatchMode Mode = ETOMatchMode::Operations;
	UPROPERTY() bool bSurvived = false;
	UPROPERTY() bool bExtracted = false;
	UPROPERTY() bool bVictory = false;
	UPROPERTY() int64 ValueExtracted = 0;
	UPROPERTY() int64 LoadoutCost = 0;
	UPROPERTY() int32 Kills = 0;
	UPROPERTY() int32 Headshots = 0;
	UPROPERTY() int32 Captures = 0;
	UPROPERTY() float Duration = 0.f;
	UPROPERTY() FString Title;
	UPROPERTY() FString KilledBy;
	UPROPERTY() FString ExtractName;
	UPROPERTY() TArray<FString> TopItems;
};

// ---------------------------------------------------------------------------------------------
//  Small helpers
// ---------------------------------------------------------------------------------------------

namespace TOUtil
{
	inline bool IsHostile(int32 TeamA, int32 TeamB) { return TeamA != TeamB && TeamA >= 0 && TeamB >= 0; }

	inline FString FormatMoney(int64 Value)
	{
		const bool bNeg = Value < 0;
		uint64 V = (uint64)(bNeg ? -Value : Value);
		FString Digits = FString::Printf(TEXT("%llu"), V);
		FString Out;
		int32 Count = 0;
		for (int32 i = Digits.Len() - 1; i >= 0; --i)
		{
			Out.InsertAt(0, Digits[i]);
			if (++Count % 3 == 0 && i > 0)
			{
				Out.InsertAt(0, TEXT(','));
			}
		}
		return (bNeg ? TEXT("-") : TEXT("")) + Out;
	}

	inline FString FormatTime(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::FloorToInt(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), S / 60, S % 60);
	}

	inline const TCHAR* BodyPartName(ETOBodyPart Part)
	{
		switch (Part)
		{
		case ETOBodyPart::Head: return TEXT("Head");
		case ETOBodyPart::Thorax: return TEXT("Thorax");
		case ETOBodyPart::Stomach: return TEXT("Stomach");
		case ETOBodyPart::LeftArm: return TEXT("L.Arm");
		case ETOBodyPart::RightArm: return TEXT("R.Arm");
		case ETOBodyPart::LeftLeg: return TEXT("L.Leg");
		case ETOBodyPart::RightLeg: return TEXT("R.Leg");
		default: return TEXT("-");
		}
	}
}
