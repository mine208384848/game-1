// TAC-OPS - HUD and menus (canvas based, no UMG assets needed)
//
// Everything on screen is drawn here in immediate mode: the Delta Force style combat HUD
// (compass, minimap, vitals, ammo, kill feed, hit markers, extraction, Warfare tickets),
// the grid inventory with loot containers, the tactical map, and all menus (lobby,
// operation / warfare preparation, gunsmith, settings, pause, death, results, respawn).
//
// Layout works in "reference units": the screen is always 1080 units high and as wide as
// the aspect ratio requires (1920 for 16:9, 2520 for 21:9 ...).
//
// Unity port mapping: UIManager.cs / HUDController.cs / Crosshair.cs / MainMenu.cs -> ATOHUD

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Core/TOTypes.h"
#include "Core/TOPlayerController.h"
#include "TOHUD.generated.h"

class ATOCharacter;
class ATOGameMode;
class ATOLootContainer;
class UTOInventoryComponent;
class UTOSaveGame;
class UTOGameInstance;
class UFont;
class UTexture;

enum class ETOAlign : uint8
{
	Left,
	Center,
	Right
};

struct FTOUIButton
{
	FBox2D Rect = FBox2D(ForceInit);
	TFunction<void()> OnClick;
	TFunction<void()> OnRightClick;
};

/** Identifies an item shown in the inventory screen. */
struct FTOItemRef
{
	enum ESource : uint8 { None, PlayerGrid, Loot, Gear, WeaponSlot };
	ESource Source = None;
	int32 Index = 0;   // gear: ETOItemCategory, weapon: slot
	int32 Uid = 0;

	bool IsSet() const { return Source != None; }
	bool operator==(const FTOItemRef& O) const { return Source == O.Source && Index == O.Index && Uid == O.Uid; }
};

UCLASS()
class TACOPS_API ATOHUD : public AHUD
{
	GENERATED_BODY()

public:
	ATOHUD();

	virtual void DrawHUD() override;

	/** Mouse click forwarded by the controller (viewport pixels). True when a widget used it. */
	bool HandleClick(const FVector2D& Pos, bool bRight);
	void HandleScroll(float Delta);
	void OnScreenChanged(ETOScreen Old, ETOScreen New);

private:
	// Frame context -------------------------------------------------------------------------
	ATOPlayerController* GetPC() const;
	ATOCharacter* GetPlayerChar() const;
	ATOGameMode* GetGM() const;
	UTOGameInstance* GetGI() const;
	UTOSaveGame* GetSaveGame() const;
	float WorldNow() const;
	float RealNow() const;
	void PlayUISound(bool bConfirm = false) const;

	// Primitives (reference units) ------------------------------------------------------------
	void Rect(float X, float Y, float W, float H, const FLinearColor& C);
	void Frame(float X, float Y, float W, float H, const FLinearColor& C, float T = 1.f);
	void Line(float X1, float Y1, float X2, float Y2, const FLinearColor& C, float T = 1.f);
	void Tri(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Col);
	void Disc(float CX, float CY, float R, const FLinearColor& C, int32 Segments = 24);
	void Ring(float CX, float CY, float R0, float R1, const FLinearColor& C, float Start01 = 0.f, float End01 = 1.f, int32 Segments = 48);
	void Diamond(float CX, float CY, float R, const FLinearColor& C);
	void Arrow(float CX, float CY, float Size, float AngleDeg, const FLinearColor& C);
	float Text(const FString& Str, float X, float Y, float Size, const FLinearColor& C, ETOAlign Align = ETOAlign::Left, bool bShadow = true);
	float TextWidth(const FString& Str, float Size);
	FString Fit(const FString& Str, float Size, float MaxW);
	void Tex(UTexture* Texture, float X, float Y, float W, float H, float U = 0.f, float V = 0.f, float UW = 1.f, float VH = 1.f, const FLinearColor& C = FLinearColor::White);
	bool Hover(float X, float Y, float W, float H) const;
	void Panel(float X, float Y, float W, float H, float Alpha = 0.82f);
	void Header(const FString& Str, float X, float Y, float W, float Size = 20.f);
	void Bar(float X, float Y, float W, float H, float Pct, const FLinearColor& Fill, float BackAlpha = 0.45f);
	bool ProjectRef(const FVector& World, FVector2D& Out) const;

	// Widgets ------------------------------------------------------------------------------
	bool Button(float X, float Y, float W, float H, const FString& Label, TFunction<void()> OnClick, bool bEnabled = true, bool bSelected = false, float Size = 18.f);
	void Region(float X, float Y, float W, float H, TFunction<void()> OnClick, TFunction<void()> OnRightClick = nullptr);
	void Stepper(float X, float Y, float W, const FString& Label, const FString& Value, TFunction<void()> OnDec, TFunction<void()> OnInc, const FString& Extra = FString(), bool bCanDec = true, bool bCanInc = true);
	void Slider(float X, float Y, float W, const FString& Label, float Value, float Min, float Max, float Step, TFunction<void(float)> OnSet, const FString& ValueText);
	void Toggle(float X, float Y, float W, const FString& Label, bool bValue, TFunction<void()> OnFlip);
	void Choice(float X, float Y, float W, const FString& Label, const TArray<FString>& Options, int32 Selected, TFunction<void(int32)> OnPick);
	void OperatorCards(float X, float Y, float W, ETOOperator Selected, TFunction<void(ETOOperator)> OnPick);
	void DrawBackdrop(float Alpha);
	void DrawTitle(const FString& Title, const FString& Sub);

	// Menus (TOHUD_Menus.cpp) ----------------------------------------------------------------
	void DrawLoading();
	void DrawLobby();
	void DrawOpsPrep();
	void DrawWarfarePrep();
	void DrawGunsmith();
	void DrawSettings();
	void DrawPause();
	void DrawDeath();
	void DrawResults();
	void DrawRespawn();
	void DrawProfilePanel(float X, float Y, float W);
	void DrawWeaponStats(float X, float Y, float W, const FTOWeaponConfig& Config, const FTOWeaponConfig* Compare);

	// Combat HUD (TOHUD_Game.cpp) --------------------------------------------------------
	void DrawGameplay();
	void DrawScreenEffects(ATOCharacter* C);
	void DrawWorldMarkers(ATOCharacter* C);
	void DrawCompass(ATOCharacter* C);
	void DrawMinimap(ATOCharacter* C);
	void DrawVitals(ATOCharacter* C);
	void DrawWeaponInfo(ATOCharacter* C);
	void DrawKillFeed();
	void DrawMessages();
	void DrawCrosshair(ATOCharacter* C);
	void DrawScopeOverlay(ATOCharacter* C);
	void DrawHitMarker(ATOCharacter* C);
	void DrawDamageIndicators(ATOCharacter* C);
	void DrawInteraction(ATOCharacter* C);
	void DrawExtraction(ATOCharacter* C);
	void DrawWarfareStatus();
	void DrawDowned(ATOCharacter* C);
	void DrawMapScreen();
	void DrawMapContents(float X, float Y, float Size, float U0, float V0, float UVSize, bool bFull);
	FVector2D MapToScreen(const FVector& World, float X, float Y, float Size, float U0, float V0, float UVSize) const;

	// Inventory (TOHUD_Inventory.cpp) ------------------------------------------------------
	void DrawInventory();
	void DrawGrid(const FTOGrid& Grid, float X, float Y, float Cell, FTOItemRef::ESource Source, int32 GridIndex, bool bShowSearch);
	void DrawItemCell(const FTOItemInstance& Item, float X, float Y, float W, float H, bool bHovered, bool bSelected);
	void DrawGearSlot(float X, float Y, float W, float H, ETOItemCategory Category, const FString& Label);
	void DrawWeaponSlot(float X, float Y, float W, float H, int32 Slot, const FString& Label);
	void DrawContextMenu();
	void DrawTooltip();
	void OpenItemMenu(const FTOItemRef& Ref);
	const FTOItemInstance* ResolveItem(const FTOItemRef& Ref) const;
	void ItemQuickMove(const FTOItemRef& Ref);
	void ItemTake(const FTOItemRef& Ref);
	void ItemStore(const FTOItemRef& Ref);
	void ItemEquip(const FTOItemRef& Ref);
	void ItemUse(const FTOItemRef& Ref);
	void ItemToSafe(const FTOItemRef& Ref, bool bIntoSafe);
	void ItemDrop(const FTOItemRef& Ref);
	bool ExtractItem(const FTOItemRef& Ref, FTOItemInstance& Out);
	void GiveOrDrop(const FTOItemInstance& Item, bool bPreferContainer);
	void DropToWorld(const TArray<FTOItemInstance>& Items, const FString& Name, bool bBag);
	void InventoryMessage(const FString& Msg, const FLinearColor& Color = FLinearColor(1.f, 0.8f, 0.3f));

	// State ---------------------------------------------------------------------------------
	UPROPERTY() TObjectPtr<UFont> Font;
	float FontBaseH = 0.f;
	float S = 1.f;
	float RefW = 1920.f;
	float RefH = 1080.f;
	FVector2D Mouse = FVector2D::ZeroVector;
	FVector2D ClickPos = FVector2D::ZeroVector;

	TArray<FTOUIButton> Buttons;      // last completed frame (used for clicks)
	TArray<FTOUIButton> NewButtons;   // being registered this frame

	// Menu state
	float ScreenOpenTime = 0.f;
	float Scroll = 0.f;
	int32 GunsmithClassTab = 0;
	float AbandonConfirmUntil = -1.f;
	float DeployErrorUntil = -1.f;
	FString DeployError;

	// Inventory state
	FTOItemRef HoverItem;
	FTOItemRef MenuItem;
	FVector2D MenuPos = FVector2D::ZeroVector;
	bool bMenuOpen = false;
	FString InvMessage;
	FLinearColor InvMessageColor = FLinearColor::White;
	float InvMessageTime = -100.f;

	// Gameplay HUD state
	float LastHitSeen = -10.f;
	float DisplayedHealth = 100.f;
};
