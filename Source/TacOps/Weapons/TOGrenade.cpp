// TAC-OPS - grenades and deployable shield

#include "Weapons/TOGrenade.h"
#include "Weapons/TOCombatManager.h"
#include "Characters/TOCharacter.h"
#include "Core/TOGameMode.h"
#include "Audio/TOAudio.h"
#include "World/TOMaterialLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ATOGrenade::ATOGrenade()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		Mesh->SetStaticMesh(SphereMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.09f, 0.09f, 0.11f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(TO_TRACE_BULLET, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetNotifyRigidBodyCollision(true);
	Mesh->SetLinearDamping(0.15f);
	Mesh->SetAngularDamping(0.8f);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->BodyInstance.bUseCCD = true;
}

void ATOGrenade::BeginPlay()
{
	Super::BeginPlay();
	if (UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this))
	{
		Mesh->SetMaterial(0, Lib->GetMaterial(ETOMat::MetalGreen));
	}
	Mesh->SetMassOverrideInKg(NAME_None, 0.45f, true);
	Mesh->OnComponentHit.AddDynamic(this, &ATOGrenade::OnHit);
}

void ATOGrenade::Launch(ETOGrenadeType InType, const FVector& Velocity, ATOCharacter* InThrower)
{
	Type = InType;
	Thrower = InThrower;
	Team = InThrower ? InThrower->TeamId : -1;
	bLaunched = true;
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;
	switch (Type)
	{
	case ETOGrenadeType::Smoke: FuseEnd = Now + 1.6f; break;
	case ETOGrenadeType::Flash: FuseEnd = Now + 1.8f; break;
	default: FuseEnd = Now + 3.2f; break;
	}
	if (UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this))
	{
		Mesh->SetMaterial(0, Lib->GetMaterial(Type == ETOGrenadeType::Smoke ? ETOMat::MetalGrey : (Type == ETOGrenadeType::Flash ? ETOMat::MetalBlue : ETOMat::MetalGreen)));
	}
	Mesh->SetPhysicsLinearVelocity(Velocity);
	Mesh->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-600.f, 600.f), FMath::FRandRange(-600.f, 600.f), FMath::FRandRange(-600.f, 600.f)));
}

void ATOGrenade::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Now = World->GetTimeSeconds();
	if (Now - LastBounce > 0.2f && Mesh->GetPhysicsLinearVelocity().Size() > 150.f)
	{
		LastBounce = Now;
		if (ATOCombatManager* CM = ATOCombatManager::Get(this))
		{
			if (CM->GetAudio())
			{
				CM->GetAudio()->Play(ETOSound::GrenadeBounce, GetActorLocation(), 0.7f);
			}
		}
	}
}

void ATOGrenade::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bLaunched || bDetonated)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.f;

	// Let nearby AI react to live frags.
	if (Type == ETOGrenadeType::Frag && Now - LastWarn > 0.5f)
	{
		LastWarn = Now;
		if (ATOGameMode* GM = ATOGameMode::Get(this))
		{
			GM->ReportGrenade(GetActorLocation(), Team);
		}
	}

	if (Now >= FuseEnd)
	{
		Detonate();
	}
}

void ATOGrenade::Detonate()
{
	if (bDetonated)
	{
		return;
	}
	bDetonated = true;
	ATOCombatManager* CM = ATOCombatManager::Get(this);
	const FVector Loc = GetActorLocation();
	if (CM)
	{
		switch (Type)
		{
		case ETOGrenadeType::Frag:
			CM->Explode(Loc, 750.f, 150.f, Thrower.Get(), Team, FName(TEXT("Frag Grenade")));
			break;
		case ETOGrenadeType::Smoke:
			CM->SpawnSmoke(Loc, false, 28.f, 12);
			if (CM->GetAudio())
			{
				CM->GetAudio()->Play(ETOSound::Heal, Loc, 1.f, 0.6f);
			}
			break;
		case ETOGrenadeType::Flash:
			CM->Flashbang(Loc, Thrower.Get());
			break;
		}
	}
	SetLifeSpan(Type == ETOGrenadeType::Smoke ? 3.f : 0.05f);
	Mesh->SetVisibility(Type == ETOGrenadeType::Smoke);
}

// =============================================================================================
//  Deployable shield
// =============================================================================================

ATODeployableShield::ATODeployableShield()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.5f;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void ATODeployableShield::BeginPlay()
{
	Super::BeginPlay();
	const UWorld* World = GetWorld();
	SpawnTime = World ? World->GetTimeSeconds() : 0.f;
	UTOMaterialLibrary* Lib = UTOMaterialLibrary::Get(this);
	if (!Lib)
	{
		return;
	}
	auto AddPart = [&](ETOMat Mat, const FVector& Loc, const FVector& Size, bool bSolid)
	{
		UStaticMeshComponent* M = NewObject<UStaticMeshComponent>(this);
		M->SetStaticMesh(Lib->GetMesh(ETOShape::Cube));
		M->SetMaterial(0, Lib->GetMaterial(Mat));
		M->SetupAttachment(Root);
		M->SetRelativeLocation(Loc);
		M->SetRelativeScale3D(Size / 100.f);
		if (bSolid)
		{
			M->SetCollisionProfileName(TEXT("BlockAll"));
			M->SetCollisionResponseToChannel(TO_TRACE_BULLET, ECR_Block);
			M->ComponentTags.Add(FName(TEXT("Metal")));
		}
		else
		{
			M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		M->SetCanEverAffectNavigation(false);
		M->RegisterComponent();
		Parts.Add(M);
	};
	AddPart(ETOMat::MetalDark, FVector(0.f, 0.f, 50.f), FVector(10.f, 160.f, 100.f), true);
	AddPart(ETOMat::Glass, FVector(0.f, 0.f, 112.f), FVector(6.f, 60.f, 22.f), false);
	AddPart(ETOMat::MetalDark, FVector(0.f, -55.f, 112.f), FVector(10.f, 50.f, 24.f), true);
	AddPart(ETOMat::MetalDark, FVector(0.f, 55.f, 112.f), FVector(10.f, 50.f, 24.f), true);
	AddPart(ETOMat::MetalYellow, FVector(0.f, 0.f, 4.f), FVector(40.f, 170.f, 8.f), false);
	AddPart(ETOMat::LampGreen, FVector(-6.f, 70.f, 95.f), FVector(2.f, 4.f, 4.f), false);
}

void ATODeployableShield::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() - SpawnTime > LifeTime)
	{
		Destroy();
	}
}

void ATODeployableShield::ReceiveTODamage(const FTODamageInfo& Info)
{
	HP -= Info.Damage * (Info.bExplosive ? 3.f : 1.f);
	if (HP <= 0.f)
	{
		if (ATOCombatManager* CM = ATOCombatManager::Get(this))
		{
			CM->LightFlash(GetActorLocation() + FVector(0.f, 0.f, 60.f), FLinearColor(1.f, 0.8f, 0.5f), 20000.f, 600.f, 0.15f);
			if (CM->GetAudio())
			{
				CM->GetAudio()->Play(ETOSound::GrenadeBounce, GetActorLocation(), 1.f, 0.5f);
			}
		}
		Destroy();
	}
}
