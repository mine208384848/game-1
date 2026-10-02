// TAC-OPS - player controller
//
// Unity port mapping: PlayerInput / InputManager axes + UIManager.cs (screen switching) -> ATOPlayerController.
// All Enhanced Input actions and the mapping context are created in code so the project
// runs without any .uasset content.

#include "Core/TOPlayerController.h"
#include "Core/TOGameMode.h"
#include "Core/TOGameInstance.h"
#include "Core/TOSaveGame.h"
#include "Core/TODatabase.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOHealthComponent.h"
#include "Weapons/TOWeaponComponent.h"
#include "Weapons/TOCombatManager.h"
#include "Audio/TOAudio.h"
#include "World/TOLootContainer.h"
#include "AI/TOAIController.h"
#include "UI/TOHUD.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Misc/App.h"
#include "CollisionQueryParams.h"
#include "TacOps.h"

namespace
{
	// Look speed matches the UE templates (old InputYawScale / InputPitchScale of 2.5).
	constexpr float LookScale = 2.5f;
	constexpr float LootCloseDistance = 400.f;
}

ATOPlayerController::ATOPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShouldPerformFullTickWhenPaused = true;
	bShowMouseCursor = false;
	bEnableClickEvents = false;
	bEnableMouseOverEvents = false;
}

void ATOPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (!IsLocalController())
	{
		return;
	}
	BuildInput();
	if (ULocalPlayer* LP = GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
		{
			if (Mapping && !Sub->HasMappingContext(Mapping))
			{
				Sub->AddMappingContext(Mapping, 0);
			}
		}
	}
	Screen = ETOScreen::Loading;
	ApplyInputMode();
}

// ---------------------------------------------------------------------------------------------
//  Input construction
// ---------------------------------------------------------------------------------------------

UInputAction* ATOPlayerController::MakeAction(FName Name, int32 ValueType, bool bUI)
{
	UInputAction* A = NewObject<UInputAction>(this, Name);
	A->ValueType = (EInputActionValueType)ValueType;
	A->bTriggerWhenPaused = bUI;
	// Several actions share keys (LMB = fire + UI click), so nothing swallows input.
	A->bConsumeInput = false;
	Actions.Add(Name, A);
	return A;
}

void ATOPlayerController::BuildInput()
{
	if (bInputBuilt)
	{
		return;
	}
	bInputBuilt = true;

	const int32 Digital = (int32)EInputActionValueType::Boolean;
	const int32 Axis1D = (int32)EInputActionValueType::Axis1D;
	const int32 Axis2D = (int32)EInputActionValueType::Axis2D;

	Mapping = NewObject<UInputMappingContext>(this, TEXT("IMC_TacOps"));

	auto Map = [this](UInputAction* Action, const FKey& Key, bool bSwizzle = false, bool bNegate = false)
	{
		FEnhancedActionKeyMapping& M = Mapping->MapKey(Action, Key);
		if (bSwizzle)
		{
			M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Mapping));
		}
		if (bNegate)
		{
			M.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
		}
	};

	UInputAction* Move = MakeAction(TEXT("IA_Move"), Axis2D, false);
	Map(Move, EKeys::W, true, false);
	Map(Move, EKeys::S, true, true);
	Map(Move, EKeys::D);
	Map(Move, EKeys::A, false, true);
	Map(Move, EKeys::Gamepad_Left2D);

	UInputAction* Look = MakeAction(TEXT("IA_Look"), Axis2D, false);
	Map(Look, EKeys::Mouse2D);

	Map(MakeAction(TEXT("IA_Jump"), Digital, false), EKeys::SpaceBar);
	Map(MakeAction(TEXT("IA_Sprint"), Digital, false), EKeys::LeftShift);
	{
		UInputAction* Crouch = MakeAction(TEXT("IA_Crouch"), Digital, false);
		Map(Crouch, EKeys::C);
		Map(Crouch, EKeys::LeftControl);
	}
	Map(MakeAction(TEXT("IA_Prone"), Digital, false), EKeys::Z);
	Map(MakeAction(TEXT("IA_Fire"), Digital, false), EKeys::LeftMouseButton);
	Map(MakeAction(TEXT("IA_Aim"), Digital, false), EKeys::RightMouseButton);
	Map(MakeAction(TEXT("IA_Reload"), Digital, false), EKeys::R);
	Map(MakeAction(TEXT("IA_Interact"), Digital, false), EKeys::F);
	Map(MakeAction(TEXT("IA_LeanLeft"), Digital, false), EKeys::Q);
	Map(MakeAction(TEXT("IA_LeanRight"), Digital, false), EKeys::E);
	Map(MakeAction(TEXT("IA_Weapon1"), Digital, false), EKeys::One);
	Map(MakeAction(TEXT("IA_Weapon2"), Digital, false), EKeys::Two);
	Map(MakeAction(TEXT("IA_Weapon3"), Digital, false), EKeys::Three);
	Map(MakeAction(TEXT("IA_Weapon4"), Digital, false), EKeys::Four);
	Map(MakeAction(TEXT("IA_Wheel"), Axis1D, true), EKeys::MouseWheelAxis);
	Map(MakeAction(TEXT("IA_FireMode"), Digital, false), EKeys::B);
	Map(MakeAction(TEXT("IA_Grenade"), Digital, false), EKeys::G);
	Map(MakeAction(TEXT("IA_GrenadeCycle"), Digital, false), EKeys::Five);
	Map(MakeAction(TEXT("IA_Ability"), Digital, false), EKeys::X);
	Map(MakeAction(TEXT("IA_Heal"), Digital, false), EKeys::H);
	Map(MakeAction(TEXT("IA_Light"), Digital, false), EKeys::T);
	Map(MakeAction(TEXT("IA_NVG"), Digital, false), EKeys::N);
	Map(MakeAction(TEXT("IA_Ping"), Digital, false), EKeys::MiddleMouseButton);
	{
		UInputAction* Inv = MakeAction(TEXT("IA_Inventory"), Digital, true);
		Map(Inv, EKeys::Tab);
		Map(Inv, EKeys::I);
	}
	Map(MakeAction(TEXT("IA_Map"), Digital, true), EKeys::M);
	{
		UInputAction* Esc = MakeAction(TEXT("IA_Escape"), Digital, true);
		Map(Esc, EKeys::Escape);
		Map(Esc, EKeys::P);
	}
	Map(MakeAction(TEXT("IA_Click"), Digital, true), EKeys::LeftMouseButton);
	Map(MakeAction(TEXT("IA_RightClick"), Digital, true), EKeys::RightMouseButton);
}

void ATOPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	BuildInput();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		UE_LOG(LogTacOps, Error, TEXT("EnhancedInputComponent missing - check DefaultInput.ini"));
		return;
	}
	auto A = [this](const TCHAR* Name) -> UInputAction*
	{
		const TObjectPtr<UInputAction>* Found = Actions.Find(FName(Name));
		return Found ? Found->Get() : nullptr;
	};

	EIC->BindAction(A(TEXT("IA_Move")), ETriggerEvent::Triggered, this, &ATOPlayerController::OnMove);
	EIC->BindAction(A(TEXT("IA_Move")), ETriggerEvent::Completed, this, &ATOPlayerController::OnMoveStop);
	EIC->BindAction(A(TEXT("IA_Look")), ETriggerEvent::Triggered, this, &ATOPlayerController::OnLook);
	EIC->BindAction(A(TEXT("IA_Jump")), ETriggerEvent::Started, this, &ATOPlayerController::OnJump);
	EIC->BindAction(A(TEXT("IA_Sprint")), ETriggerEvent::Started, this, &ATOPlayerController::OnSprintStart);
	EIC->BindAction(A(TEXT("IA_Sprint")), ETriggerEvent::Completed, this, &ATOPlayerController::OnSprintStop);
	EIC->BindAction(A(TEXT("IA_Crouch")), ETriggerEvent::Started, this, &ATOPlayerController::OnCrouch);
	EIC->BindAction(A(TEXT("IA_Prone")), ETriggerEvent::Started, this, &ATOPlayerController::OnProne);
	EIC->BindAction(A(TEXT("IA_Fire")), ETriggerEvent::Started, this, &ATOPlayerController::OnFireStart);
	EIC->BindAction(A(TEXT("IA_Fire")), ETriggerEvent::Completed, this, &ATOPlayerController::OnFireStop);
	EIC->BindAction(A(TEXT("IA_Aim")), ETriggerEvent::Started, this, &ATOPlayerController::OnAimStart);
	EIC->BindAction(A(TEXT("IA_Aim")), ETriggerEvent::Completed, this, &ATOPlayerController::OnAimStop);
	EIC->BindAction(A(TEXT("IA_Reload")), ETriggerEvent::Started, this, &ATOPlayerController::OnReload);
	EIC->BindAction(A(TEXT("IA_Interact")), ETriggerEvent::Started, this, &ATOPlayerController::OnInteractStart);
	EIC->BindAction(A(TEXT("IA_Interact")), ETriggerEvent::Completed, this, &ATOPlayerController::OnInteractStop);
	EIC->BindAction(A(TEXT("IA_LeanLeft")), ETriggerEvent::Started, this, &ATOPlayerController::OnLeanLeftStart);
	EIC->BindAction(A(TEXT("IA_LeanLeft")), ETriggerEvent::Completed, this, &ATOPlayerController::OnLeanLeftStop);
	EIC->BindAction(A(TEXT("IA_LeanRight")), ETriggerEvent::Started, this, &ATOPlayerController::OnLeanRightStart);
	EIC->BindAction(A(TEXT("IA_LeanRight")), ETriggerEvent::Completed, this, &ATOPlayerController::OnLeanRightStop);
	EIC->BindAction(A(TEXT("IA_Weapon1")), ETriggerEvent::Started, this, &ATOPlayerController::OnWeapon1);
	EIC->BindAction(A(TEXT("IA_Weapon2")), ETriggerEvent::Started, this, &ATOPlayerController::OnWeapon2);
	EIC->BindAction(A(TEXT("IA_Weapon3")), ETriggerEvent::Started, this, &ATOPlayerController::OnWeapon3);
	EIC->BindAction(A(TEXT("IA_Weapon4")), ETriggerEvent::Started, this, &ATOPlayerController::OnWeapon4);
	EIC->BindAction(A(TEXT("IA_Wheel")), ETriggerEvent::Triggered, this, &ATOPlayerController::OnWheel);
	EIC->BindAction(A(TEXT("IA_FireMode")), ETriggerEvent::Started, this, &ATOPlayerController::OnFireMode);
	EIC->BindAction(A(TEXT("IA_Grenade")), ETriggerEvent::Started, this, &ATOPlayerController::OnGrenade);
	EIC->BindAction(A(TEXT("IA_GrenadeCycle")), ETriggerEvent::Started, this, &ATOPlayerController::OnGrenadeCycle);
	EIC->BindAction(A(TEXT("IA_Ability")), ETriggerEvent::Started, this, &ATOPlayerController::OnAbility);
	EIC->BindAction(A(TEXT("IA_Heal")), ETriggerEvent::Started, this, &ATOPlayerController::OnHeal);
	EIC->BindAction(A(TEXT("IA_Light")), ETriggerEvent::Started, this, &ATOPlayerController::OnLight);
	EIC->BindAction(A(TEXT("IA_NVG")), ETriggerEvent::Started, this, &ATOPlayerController::OnNVG);
	EIC->BindAction(A(TEXT("IA_Ping")), ETriggerEvent::Started, this, &ATOPlayerController::OnPing);
	EIC->BindAction(A(TEXT("IA_Inventory")), ETriggerEvent::Started, this, &ATOPlayerController::OnInventory);
	EIC->BindAction(A(TEXT("IA_Map")), ETriggerEvent::Started, this, &ATOPlayerController::OnMap);
	EIC->BindAction(A(TEXT("IA_Escape")), ETriggerEvent::Started, this, &ATOPlayerController::OnEscape);
	EIC->BindAction(A(TEXT("IA_Click")), ETriggerEvent::Started, this, &ATOPlayerController::OnClick);
	EIC->BindAction(A(TEXT("IA_RightClick")), ETriggerEvent::Started, this, &ATOPlayerController::OnRightClick);
}

// ---------------------------------------------------------------------------------------------
//  Screens
// ---------------------------------------------------------------------------------------------

bool ATOPlayerController::IsUIScreen() const
{
	return Screen != ETOScreen::None && Screen != ETOScreen::Loading;
}

void ATOPlayerController::SetScreen(ETOScreen NewScreen)
{
	const ETOScreen Old = Screen;
	if (Old == ETOScreen::Inventory && NewScreen != ETOScreen::Inventory)
	{
		CloseLoot();
	}
	if (NewScreen != ETOScreen::None)
	{
		if (ATOCharacter* C = GetTOCharacter())
		{
			// Keep walking while the bag / map is open (Delta Force style), stop everything else.
			C->SetFireInput(false);
			C->SetAimInput(false);
			C->SetInteractInput(false);
			C->SetLeanInput(0.f);
			if (NewScreen != ETOScreen::Inventory && NewScreen != ETOScreen::Map)
			{
				C->CancelActions();
			}
		}
	}
	bAimToggled = false;
	Screen = NewScreen;
	ApplyInputMode();
	if (ATOHUD* H = Cast<ATOHUD>(GetHUD()))
	{
		H->OnScreenChanged(Old, NewScreen);
	}
}

void ATOPlayerController::ApplyInputMode()
{
	if (!IsLocalController())
	{
		return;
	}
	if (IsUIScreen())
	{
		FInputModeGameAndUI ModeUI;
		ModeUI.SetHideCursorDuringCapture(false);
		ModeUI.SetLockMouseToViewportBehavior(EMouseLockMode::LockAlways);
		SetInputMode(ModeUI);
		bShowMouseCursor = true;
	}
	else
	{
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
	}

	// Single player: the pause menu (and settings opened from it) really pauses the battle.
	const bool bWantPause = Screen == ETOScreen::Pause || (Screen == ETOScreen::Settings && SettingsReturn == ETOScreen::Pause);
	if (IsPaused() != bWantPause)
	{
		SetPause(bWantPause);
	}
}

void ATOPlayerController::OpenSettings()
{
	if (Screen != ETOScreen::Settings)
	{
		SettingsReturn = Screen;
	}
	SetScreen(ETOScreen::Settings);
}

void ATOPlayerController::CloseSettings()
{
	UTOGameInstance* GI = UTOGameInstance::Get(this);
	UTOSaveGame* Save = GetSave();
	if (GI && Save)
	{
		GI->ApplyGraphicsQuality(Save->GraphicsQuality);
		GI->WriteSave();
		if (ATOCharacter* C = GetTOCharacter())
		{
			C->SetBaseFOV(Save->FieldOfView);
		}
		if (ATOCombatManager* CM = ATOCombatManager::Get(this))
		{
			if (CM->GetAudio())
			{
				CM->GetAudio()->MasterVolume = Save->MasterVolume;
			}
		}
	}
	SetScreen(SettingsReturn == ETOScreen::Settings ? ETOScreen::Lobby : SettingsReturn);
}

void ATOPlayerController::OpenGunsmith(int32 Target)
{
	GunsmithTarget = FMath::Clamp(Target, 0, 1);
	if (Screen != ETOScreen::Gunsmith)
	{
		GunsmithReturn = Screen;
	}
	SetScreen(ETOScreen::Gunsmith);
}

void ATOPlayerController::OnMatchReady()
{
	ATOGameMode* GM = ATOGameMode::Get(this);
	if (UTOSaveGame* Save = GetSave())
	{
		if (ATOCombatManager* CM = ATOCombatManager::Get(this))
		{
			if (CM->GetAudio())
			{
				CM->GetAudio()->MasterVolume = Save->MasterVolume;
			}
		}
	}
	if (!GM || GM->GetMatchMode() == ETOMatchMode::Menu)
	{
		SetScreen(ETOScreen::Lobby);
		return;
	}
	if (GM->GetMatchMode() == ETOMatchMode::Warfare)
	{
		RespawnOperator = GM->GetSession().Loadout.Operator;
	}
	SetScreen(ETOScreen::None);
}

void ATOPlayerController::OnPlayerSpawned(ATOCharacter* NewCharacter)
{
	if (!NewCharacter)
	{
		return;
	}
	if (UTOSaveGame* Save = GetSave())
	{
		NewCharacter->SetBaseFOV(Save->FieldOfView);
	}
	DeathShownTime = -100.f;
	PingTime = -100.f;
	PingedEnemy.Reset();
	SetViewTarget(NewCharacter);
	if (Screen != ETOScreen::Loading)
	{
		SetScreen(ETOScreen::None);
	}
}

void ATOPlayerController::ShowDeath()
{
	DeathShownTime = NowSeconds();
	SetScreen(ETOScreen::Death);
}

void ATOPlayerController::ShowResults()
{
	SetScreen(ETOScreen::Results);
}

void ATOPlayerController::OpenLoot(ATOLootContainer* Container)
{
	if (!Container)
	{
		return;
	}
	if (Screen != ETOScreen::None && Screen != ETOScreen::Inventory)
	{
		return;
	}
	if (LootTarget.IsValid() && LootTarget.Get() != Container)
	{
		CloseLoot();
	}
	LootTarget = Container;
	Container->SetBeingSearched(true);
	if (Screen != ETOScreen::Inventory)
	{
		SetScreen(ETOScreen::Inventory);
	}
	else if (ATOHUD* H = Cast<ATOHUD>(GetHUD()))
	{
		H->OnScreenChanged(ETOScreen::Inventory, ETOScreen::Inventory);
	}
}

void ATOPlayerController::CloseLoot()
{
	if (ATOLootContainer* C = LootTarget.Get())
	{
		C->SetBeingSearched(false);
	}
	LootTarget.Reset();
}

ATOCharacter* ATOPlayerController::GetTOCharacter() const
{
	return Cast<ATOCharacter>(GetPawn());
}

UTOSaveGame* ATOPlayerController::GetSave() const
{
	UTOGameInstance* GI = UTOGameInstance::Get(this);
	return GI ? GI->GetSave() : nullptr;
}

FVector2D ATOPlayerController::GetMouse() const
{
	float X = 0.f, Y = 0.f;
	if (!GetMousePosition(X, Y))
	{
		return FVector2D(-1.f, -1.f);
	}
	return FVector2D(X, Y);
}

float ATOPlayerController::NowSeconds() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

bool ATOPlayerController::GameplayBlocked() const
{
	if (Screen != ETOScreen::None)
	{
		return true;
	}
	const ATOGameMode* GM = ATOGameMode::Get(this);
	if (!GM || GM->GetState() != ETOGMState::Playing)
	{
		return true;
	}
	const ATOCharacter* C = GetTOCharacter();
	return !C || !C->IsAlive();
}

// ---------------------------------------------------------------------------------------------
//  Tick
// ---------------------------------------------------------------------------------------------

void ATOPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	const float RealDt = (float)FApp::GetDeltaTime();
	if (RealDt > KINDA_SMALL_NUMBER)
	{
		SmoothedFPS = FMath::Lerp(SmoothedFPS, 1.f / RealDt, FMath::Clamp(RealDt * 3.f, 0.f, 1.f));
	}

	ATOGameMode* GM = ATOGameMode::Get(this);
	ATOCharacter* C = GetTOCharacter();

	// Loot window follows the container: walk away and it closes.
	if (Screen == ETOScreen::Inventory)
	{
		ATOLootContainer* Loot = LootTarget.Get();
		if (LootTarget.IsStale() || (Loot && C && FVector::Dist(Loot->GetActorLocation(), C->GetActorLocation()) > LootCloseDistance))
		{
			CloseLoot();
		}
	}

	// Bag / map cannot stay open while downed or dead.
	if ((Screen == ETOScreen::Inventory || Screen == ETOScreen::Map) && (!C || !C->IsAlive() || C->IsDowned()))
	{
		SetScreen(ETOScreen::None);
	}

	// Warfare: after a short kill-cam the deploy screen appears.
	if (GM && GM->GetMatchMode() == ETOMatchMode::Warfare && GM->GetState() == ETOGMState::Playing && GM->IsPlayerAwaitingRespawn())
	{
		const bool bCanSwitch = Screen == ETOScreen::None || Screen == ETOScreen::Inventory || Screen == ETOScreen::Map;
		if (bCanSwitch && GM->GetRespawnWait() < 4.f)
		{
			SetScreen(ETOScreen::Respawn);
		}
	}
}

// ---------------------------------------------------------------------------------------------
//  Gameplay handlers
// ---------------------------------------------------------------------------------------------

void ATOPlayerController::OnMove(const FInputActionValue& Value)
{
	ATOCharacter* C = GetTOCharacter();
	if (!C)
	{
		return;
	}
	const bool bMoveAllowed = Screen == ETOScreen::None || Screen == ETOScreen::Inventory || Screen == ETOScreen::Map;
	const ATOGameMode* GM = ATOGameMode::Get(this);
	if (!bMoveAllowed || !GM || GM->GetState() != ETOGMState::Playing)
	{
		C->InputMove(FVector2D::ZeroVector);
		return;
	}
	C->InputMove(Value.Get<FVector2D>());
}

void ATOPlayerController::OnMoveStop(const FInputActionValue& Value)
{
	if (ATOCharacter* C = GetTOCharacter())
	{
		C->InputMove(FVector2D::ZeroVector);
	}
}

void ATOPlayerController::OnLook(const FInputActionValue& Value)
{
	if (GameplayBlocked())
	{
		return;
	}
	ATOCharacter* C = GetTOCharacter();
	const UTOSaveGame* Save = GetSave();
	const FVector2D V = Value.Get<FVector2D>();
	float Sens = Save ? Save->MouseSensitivity : 1.f;
	const UTOWeaponComponent* W = C->GetWeapons();
	if (W && W->GetADSAlpha() > 0.5f)
	{
		// Zoomed views turn slower so the same mouse distance covers the same screen distance.
		const float FovRatio = FMath::Clamp(C->GetFOVForView() / FMath::Max(1.f, C->GetBaseFOV()), 0.05f, 1.f);
		Sens *= (Save ? Save->ADSSensitivity : 0.8f) * FovRatio;
	}
	const float Yaw = V.X * LookScale * Sens;
	float Pitch = V.Y * LookScale * Sens;
	if (Save && Save->bInvertY)
	{
		Pitch = -Pitch;
	}
	C->AddLookInput(Yaw, Pitch);
}

void ATOPlayerController::OnJump()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputJump();
	}
}

void ATOPlayerController::OnSprintStart()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->SetSprint(true);
	}
}

void ATOPlayerController::OnSprintStop()
{
	if (ATOCharacter* C = GetTOCharacter())
	{
		C->SetSprint(false);
	}
}

void ATOPlayerController::OnCrouch()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->ToggleCrouch();
	}
}

void ATOPlayerController::OnProne()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->ToggleProne();
	}
}

void ATOPlayerController::OnFireStart()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->SetFireInput(true);
	}
}

void ATOPlayerController::OnFireStop()
{
	if (ATOCharacter* C = GetTOCharacter())
	{
		C->SetFireInput(false);
	}
}

void ATOPlayerController::OnAimStart()
{
	if (GameplayBlocked())
	{
		return;
	}
	const UTOSaveGame* Save = GetSave();
	if (Save && Save->bToggleADS)
	{
		bAimToggled = !bAimToggled;
		GetTOCharacter()->SetAimInput(bAimToggled);
		return;
	}
	GetTOCharacter()->SetAimInput(true);
}

void ATOPlayerController::OnAimStop()
{
	const UTOSaveGame* Save = GetSave();
	if (Save && Save->bToggleADS)
	{
		return;
	}
	if (ATOCharacter* C = GetTOCharacter())
	{
		C->SetAimInput(false);
	}
}

void ATOPlayerController::OnReload()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputReload();
	}
}

void ATOPlayerController::OnInteractStart()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->SetInteractInput(true);
	}
}

void ATOPlayerController::OnInteractStop()
{
	if (ATOCharacter* C = GetTOCharacter())
	{
		C->SetInteractInput(false);
	}
}

void ATOPlayerController::OnLeanLeftStart()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->SetLeanInput(-1.f);
	}
}

void ATOPlayerController::OnLeanLeftStop()
{
	if (ATOCharacter* C = GetTOCharacter())
	{
		C->SetLeanInput(IsInputKeyDown(EKeys::E) && !GameplayBlocked() ? 1.f : 0.f);
	}
}

void ATOPlayerController::OnLeanRightStart()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->SetLeanInput(1.f);
	}
}

void ATOPlayerController::OnLeanRightStop()
{
	if (ATOCharacter* C = GetTOCharacter())
	{
		C->SetLeanInput(IsInputKeyDown(EKeys::Q) && !GameplayBlocked() ? -1.f : 0.f);
	}
}

void ATOPlayerController::OnWeapon1()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputSelectWeapon(UTOWeaponComponent::SlotPrimary);
	}
}

void ATOPlayerController::OnWeapon2()
{
	if (!GameplayBlocked())
	{
		ATOCharacter* C = GetTOCharacter();
		// Slot 2 is the secondary long gun if one was picked up, otherwise the sidearm.
		const bool bSecondary = C->GetWeapons()->HasWeapon(UTOWeaponComponent::SlotSecondary);
		C->InputSelectWeapon(bSecondary ? UTOWeaponComponent::SlotSecondary : UTOWeaponComponent::SlotSidearm);
	}
}

void ATOPlayerController::OnWeapon3()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputSelectWeapon(UTOWeaponComponent::SlotSidearm);
	}
}

void ATOPlayerController::OnWeapon4()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputSelectWeapon(UTOWeaponComponent::SlotMelee);
	}
}

void ATOPlayerController::OnWheel(const FInputActionValue& Value)
{
	const float Delta = Value.Get<float>();
	if (FMath::IsNearlyZero(Delta))
	{
		return;
	}
	if (IsUIScreen())
	{
		if (ATOHUD* H = Cast<ATOHUD>(GetHUD()))
		{
			H->HandleScroll(Delta);
		}
		return;
	}
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputCycleWeapon(Delta > 0.f ? -1 : 1);
	}
}

void ATOPlayerController::OnFireMode()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputFireMode();
	}
}

void ATOPlayerController::OnGrenade()
{
	ATOCharacter* C = GetTOCharacter();
	if (Screen == ETOScreen::None && C && C->IsDowned())
	{
		// Downed: [G] gives up instead of waiting for a revive.
		if (ATOGameMode* GM = ATOGameMode::Get(this))
		{
			GM->PlayerGiveUp();
		}
		return;
	}
	if (!GameplayBlocked())
	{
		C->InputGrenade();
	}
}

void ATOPlayerController::OnGrenadeCycle()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputCycleGrenade();
	}
}

void ATOPlayerController::OnAbility()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputAbility();
	}
}

void ATOPlayerController::OnHeal()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputQuickHeal();
	}
}

void ATOPlayerController::OnLight()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputLight();
	}
}

void ATOPlayerController::OnNVG()
{
	if (!GameplayBlocked())
	{
		GetTOCharacter()->InputNVG();
	}
}

void ATOPlayerController::OnPing()
{
	if (GameplayBlocked())
	{
		return;
	}
	ATOCharacter* C = GetTOCharacter();
	ATOGameMode* GM = ATOGameMode::Get(this);
	const FVector Start = C->GetEyeLocation();
	const FVector Dir = C->GetAimDirection();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TOPing), true, C);
	FHitResult Hit;
	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * 40000.f, ECC_Visibility, Params))
	{
		return;
	}
	PingLocation = Hit.ImpactPoint;
	PingTime = NowSeconds();
	PingedEnemy.Reset();

	// Spot an enemy close to the ping line (Delta Force style "spotting").
	if (GM)
	{
		float BestScore = 0.f;
		for (const TWeakObjectPtr<ATOCharacter>& Ptr : GM->GetCharacters())
		{
			ATOCharacter* Other = Ptr.Get();
			if (!Other || !Other->IsAlive() || !C->IsHostileTo(Other))
			{
				continue;
			}
			const FVector ToOther = Other->GetChestLocation() - Start;
			const float Dist = ToOther.Size();
			if (Dist > Hit.Distance + 300.f || Dist < 1.f)
			{
				continue;
			}
			const float Dot = FVector::DotProduct(ToOther / Dist, Dir);
			if (Dot > 0.995f && Dot > BestScore)
			{
				BestScore = Dot;
				PingedEnemy = Other;
			}
		}
	}

	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (CM->GetAudio())
		{
			CM->GetAudio()->Play2D(ETOSound::Beep, 0.5f, PingedEnemy.IsValid() ? 1.4f : 1.f);
		}
	}

	if (!GM)
	{
		return;
	}
	if (ATOCharacter* Enemy = PingedEnemy.Get())
	{
		GM->AlertSquad(C->SquadId, Enemy, Enemy->GetActorLocation(), C);
		GM->PushMessage(FString::Printf(TEXT("Enemy spotted - %.0f m"), FVector::Dist(Start, Enemy->GetActorLocation()) / 100.f), FLinearColor(1.f, 0.35f, 0.3f));
		return;
	}
	// Ground ping: AI squad mates move up to the marked position.
	int32 Ordered = 0;
	for (const TWeakObjectPtr<ATOCharacter>& Ptr : GM->GetCharacters())
	{
		ATOCharacter* Mate = Ptr.Get();
		if (!Mate || Mate == C || !Mate->IsAlive() || Mate->IsDowned() || Mate->TeamId != C->TeamId || Mate->SquadId != C->SquadId)
		{
			continue;
		}
		if (ATOAIController* AI = Cast<ATOAIController>(Mate->GetController()))
		{
			if (AI->GetRole() == ETOAIRole::Teammate || AI->GetRole() == ETOAIRole::WarfareBot)
			{
				AI->CommandMoveTo(PingLocation);
				++Ordered;
			}
		}
	}
	if (Ordered > 0 && GM->GetMatchMode() == ETOMatchMode::Operations)
	{
		GM->PushMessage(TEXT("Squad: moving to marker"), FLinearColor(0.5f, 0.85f, 1.f));
	}
}

// ---------------------------------------------------------------------------------------------
//  UI handlers
// ---------------------------------------------------------------------------------------------

void ATOPlayerController::OnInventory()
{
	if (Screen == ETOScreen::Inventory)
	{
		SetScreen(ETOScreen::None);
		return;
	}
	if (Screen == ETOScreen::None || Screen == ETOScreen::Map)
	{
		const ATOGameMode* GM = ATOGameMode::Get(this);
		const ATOCharacter* C = GetTOCharacter();
		if (GM && GM->GetState() == ETOGMState::Playing && C && C->IsAlive() && !C->IsDowned())
		{
			SetScreen(ETOScreen::Inventory);
		}
	}
}

void ATOPlayerController::OnMap()
{
	if (Screen == ETOScreen::Map)
	{
		SetScreen(ETOScreen::None);
		return;
	}
	if (Screen == ETOScreen::None || Screen == ETOScreen::Inventory)
	{
		const ATOGameMode* GM = ATOGameMode::Get(this);
		const ATOCharacter* C = GetTOCharacter();
		if (GM && GM->GetState() == ETOGMState::Playing && C && C->IsAlive() && !C->IsDowned())
		{
			SetScreen(ETOScreen::Map);
		}
	}
}

void ATOPlayerController::OnEscape()
{
	const ATOGameMode* GM = ATOGameMode::Get(this);
	switch (Screen)
	{
	case ETOScreen::None:
		if (GM && GM->GetState() == ETOGMState::Playing)
		{
			SetScreen(ETOScreen::Pause);
		}
		break;
	case ETOScreen::Pause:
	case ETOScreen::Inventory:
	case ETOScreen::Map:
		SetScreen(ETOScreen::None);
		break;
	case ETOScreen::Settings:
		CloseSettings();
		break;
	case ETOScreen::Gunsmith:
		SetScreen(GunsmithReturn);
		break;
	case ETOScreen::OpsPrep:
	case ETOScreen::WarfarePrep:
		SetScreen(ETOScreen::Lobby);
		break;
	case ETOScreen::Death:
		SetScreen(ETOScreen::Results);
		break;
	case ETOScreen::Respawn:
		SetScreen(ETOScreen::Pause);
		break;
	default:
		break;
	}
}

void ATOPlayerController::OnClick()
{
	if (!IsUIScreen())
	{
		return;
	}
	if (ATOHUD* H = Cast<ATOHUD>(GetHUD()))
	{
		H->HandleClick(GetMouse(), false);
	}
}

void ATOPlayerController::OnRightClick()
{
	if (!IsUIScreen())
	{
		return;
	}
	if (ATOHUD* H = Cast<ATOHUD>(GetHUD()))
	{
		H->HandleClick(GetMouse(), true);
	}
}
