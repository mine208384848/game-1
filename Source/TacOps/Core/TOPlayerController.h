// TAC-OPS - player controller: Enhanced Input (built in code, no assets), screen state machine
//
// Default bindings (Delta Force PC layout):
//   WASD move, Mouse look, LMB fire, RMB aim, Shift sprint / hold breath, Space jump/vault,
//   C / Ctrl crouch (slide while sprinting), Z prone, Q/E lean, R reload, F interact (hold),
//   1-4 weapons, wheel cycle, B fire mode, G grenade, 5 grenade type, X operator ability,
//   H quick heal, T flashlight, N night vision, MMB ping, Tab inventory, M map, Esc pause.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Core/TOTypes.h"
#include "TOPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class ATOCharacter;
class ATOLootContainer;
class UTOSaveGame;

UENUM()
enum class ETOScreen : uint8
{
	None,
	Loading,
	Lobby,
	OpsPrep,
	WarfarePrep,
	Gunsmith,
	Settings,
	Pause,
	Inventory,
	Map,
	Death,
	Results,
	Respawn
};

UCLASS()
class TACOPS_API ATOPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATOPlayerController();

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

	// Screens ------------------------------------------------------------------------------
	ETOScreen GetScreen() const { return Screen; }
	void SetScreen(ETOScreen NewScreen);
	bool IsUIScreen() const;
	void OpenSettings();
	void CloseSettings();
	void OpenGunsmith(int32 Target);

	// Game mode callbacks
	void OnMatchReady();
	void OnPlayerSpawned(ATOCharacter* NewCharacter);
	void ShowDeath();
	void ShowResults();
	void OpenLoot(ATOLootContainer* Container);
	void CloseLoot();
	ATOLootContainer* GetLootTarget() const { return LootTarget.Get(); }

	ATOCharacter* GetTOCharacter() const;
	UTOSaveGame* GetSave() const;
	float GetFPS() const { return SmoothedFPS; }
	FVector2D GetMouse() const;
	float GetDeathShownTime() const { return DeathShownTime; }

	/** 0 = primary, 1 = sidearm (gunsmith edits the lobby loadout). */
	int32 GunsmithTarget = 0;
	ETOScreen GunsmithReturn = ETOScreen::OpsPrep;
	ETOScreen SettingsReturn = ETOScreen::Lobby;

	// Warfare respawn choices
	ETOOperator RespawnOperator = ETOOperator::Viper;
	int32 RespawnWeaponIndex = 0;

	// Ping marker
	FVector PingLocation = FVector::ZeroVector;
	float PingTime = -100.f;
	TWeakObjectPtr<ATOCharacter> PingedEnemy;

private:
	void BuildInput();
	void ApplyInputMode();
	UInputAction* MakeAction(FName Name, int32 ValueType, bool bUI);
	bool GameplayBlocked() const;
	float NowSeconds() const;

	// Handlers
	void OnMove(const FInputActionValue& Value);
	void OnMoveStop(const FInputActionValue& Value);
	void OnLook(const FInputActionValue& Value);
	void OnJump();
	void OnSprintStart();
	void OnSprintStop();
	void OnCrouch();
	void OnProne();
	void OnFireStart();
	void OnFireStop();
	void OnAimStart();
	void OnAimStop();
	void OnReload();
	void OnInteractStart();
	void OnInteractStop();
	void OnLeanLeftStart();
	void OnLeanLeftStop();
	void OnLeanRightStart();
	void OnLeanRightStop();
	void OnWeapon1();
	void OnWeapon2();
	void OnWeapon3();
	void OnWeapon4();
	void OnWheel(const FInputActionValue& Value);
	void OnFireMode();
	void OnGrenade();
	void OnGrenadeCycle();
	void OnAbility();
	void OnHeal();
	void OnLight();
	void OnNVG();
	void OnInventory();
	void OnMap();
	void OnEscape();
	void OnClick();
	void OnRightClick();
	void OnPing();

	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TMap<FName, TObjectPtr<UInputAction>> Actions;

	ETOScreen Screen = ETOScreen::Loading;
	TWeakObjectPtr<ATOLootContainer> LootTarget;
	float DeathShownTime = -100.f;
	float SmoothedFPS = 60.f;
	bool bInputBuilt = false;
	bool bAimToggled = false;
};
