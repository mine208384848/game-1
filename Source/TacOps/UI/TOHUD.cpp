// TAC-OPS - HUD core: canvas primitives, widgets, click routing and screen dispatch

#include "UI/TOHUD.h"
#include "UI/TOHUDStyle.h"
#include "Core/TOGameMode.h"
#include "Core/TOGameInstance.h"
#include "Core/TOSaveGame.h"
#include "Core/TODatabase.h"
#include "Characters/TOCharacter.h"
#include "Weapons/TOCombatManager.h"
#include "Audio/TOAudio.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "CanvasItem.h"
#include "RenderUtils.h"
#include "TextureResource.h"

ATOHUD::ATOHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

// ---------------------------------------------------------------------------------------------
//  Context
// ---------------------------------------------------------------------------------------------

ATOPlayerController* ATOHUD::GetPC() const
{
	return Cast<ATOPlayerController>(GetOwningPlayerController());
}

ATOCharacter* ATOHUD::GetPlayerChar() const
{
	const ATOPlayerController* PC = GetPC();
	return PC ? PC->GetTOCharacter() : nullptr;
}

ATOGameMode* ATOHUD::GetGM() const
{
	return ATOGameMode::Get(this);
}

UTOGameInstance* ATOHUD::GetGI() const
{
	return UTOGameInstance::Get(this);
}

UTOSaveGame* ATOHUD::GetSaveGame() const
{
	UTOGameInstance* GI = GetGI();
	return GI ? GI->GetSave() : nullptr;
}

float ATOHUD::WorldNow() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

float ATOHUD::RealNow() const
{
	return GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.f;
}

void ATOHUD::PlayUISound(bool bConfirm) const
{
	if (ATOCombatManager* CM = ATOCombatManager::Get(this))
	{
		if (UTOAudio* A = CM->GetAudio())
		{
			A->Play2D(ETOSound::UIClick, bConfirm ? 0.6f : 0.4f, bConfirm ? 0.8f : 1.f);
		}
	}
}

// ---------------------------------------------------------------------------------------------
//  Frame
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || Canvas->ClipY < 8.f)
	{
		return;
	}

	S = Canvas->ClipY / 1080.f;
	RefH = 1080.f;
	RefW = Canvas->ClipX / S;
	if (!Font)
	{
		Font = GEngine ? GEngine->GetLargeFont() : nullptr;
	}
	if (Font && FontBaseH <= 0.f)
	{
		float W = 0.f, H = 0.f;
		GetTextSize(TEXT("Wg"), W, H, Font, 1.f);
		FontBaseH = FMath::Max(1.f, H);
	}

	if (const ATOPlayerController* PC = GetPC())
	{
		Mouse = PC->GetMouse() / S;
	}
	NewButtons.Reset();
	HoverItem = FTOItemRef();

	const ATOPlayerController* PC = GetPC();
	const ETOScreen Screen = PC ? PC->GetScreen() : ETOScreen::Loading;
	const ATOGameMode* GM = GetGM();
	const bool bInMatch = GM && GM->GetMatchMode() != ETOMatchMode::Menu && GM->GetState() != ETOGMState::Loading;

	// Combat HUD stays visible under the bag, map and pause overlays.
	if (bInMatch && (Screen == ETOScreen::None || Screen == ETOScreen::Pause || Screen == ETOScreen::Map || Screen == ETOScreen::Inventory || Screen == ETOScreen::Settings))
	{
		DrawGameplay();
	}

	switch (Screen)
	{
	case ETOScreen::Loading: DrawLoading(); break;
	case ETOScreen::Lobby: DrawLobby(); break;
	case ETOScreen::OpsPrep: DrawOpsPrep(); break;
	case ETOScreen::WarfarePrep: DrawWarfarePrep(); break;
	case ETOScreen::Gunsmith: DrawGunsmith(); break;
	case ETOScreen::Settings: DrawSettings(); break;
	case ETOScreen::Pause: DrawPause(); break;
	case ETOScreen::Inventory: DrawInventory(); break;
	case ETOScreen::Map: DrawMapScreen(); break;
	case ETOScreen::Death: DrawDeath(); break;
	case ETOScreen::Results: DrawResults(); break;
	case ETOScreen::Respawn: DrawRespawn(); break;
	default: break;
	}

	if (const UTOSaveGame* Save = GetSaveGame())
	{
		if (Save->bShowFPS && PC)
		{
			const float Fps = PC->GetFPS();
			const FLinearColor C = Fps >= 55.f ? TOStyle::Good : (Fps >= 30.f ? TOStyle::Warn : TOStyle::Danger);
			Text(FString::Printf(TEXT("%.0f FPS  %.1f ms"), Fps, 1000.f / FMath::Max(1.f, Fps)), RefW - 12.f, RefH - 24.f, 14.f, C, ETOAlign::Right);
		}
	}

	Buttons = MoveTemp(NewButtons);
	NewButtons.Reset();
}

bool ATOHUD::HandleClick(const FVector2D& Pos, bool bRight)
{
	ClickPos = Pos / FMath::Max(S, 0.001f);
	for (int32 i = Buttons.Num() - 1; i >= 0; --i)
	{
		const FTOUIButton& B = Buttons[i];
		if (!B.Rect.IsInside(ClickPos))
		{
			continue;
		}
		TFunction<void()> Fn = bRight ? B.OnRightClick : B.OnClick;
		if (!Fn)
		{
			continue;
		}
		// The handler may change screens (and with that the button list) - work on copies.
		Buttons.Reset();
		Fn();
		return true;
	}
	if (bMenuOpen)
	{
		bMenuOpen = false;
		return true;
	}
	return false;
}

void ATOHUD::HandleScroll(float Delta)
{
	Scroll = FMath::Max(0.f, Scroll - Delta * 60.f);
}

void ATOHUD::OnScreenChanged(ETOScreen Old, ETOScreen New)
{
	if (Old != New)
	{
		Scroll = 0.f;
		ScreenOpenTime = RealNow();
		AbandonConfirmUntil = -1.f;
	}
	bMenuOpen = false;
	MenuItem = FTOItemRef();
	Buttons.Reset();
}

// ---------------------------------------------------------------------------------------------
//  Primitives
// ---------------------------------------------------------------------------------------------

void ATOHUD::Rect(float X, float Y, float W, float H, const FLinearColor& C)
{
	if (W <= 0.f || H <= 0.f || C.A <= 0.001f)
	{
		return;
	}
	DrawRect(C, X * S, Y * S, W * S, H * S);
}

void ATOHUD::Frame(float X, float Y, float W, float H, const FLinearColor& C, float T)
{
	Rect(X, Y, W, T, C);
	Rect(X, Y + H - T, W, T, C);
	Rect(X, Y + T, T, H - 2.f * T, C);
	Rect(X + W - T, Y + T, T, H - 2.f * T, C);
}

void ATOHUD::Line(float X1, float Y1, float X2, float Y2, const FLinearColor& C, float T)
{
	DrawLine(X1 * S, Y1 * S, X2 * S, Y2 * S, C, FMath::Max(1.f, T * S));
}

void ATOHUD::Tri(const FVector2D& A, const FVector2D& B, const FVector2D& C, const FLinearColor& Col)
{
	if (!Canvas)
	{
		return;
	}
	FCanvasTriangleItem Item(A * S, B * S, C * S, GWhiteTexture);
	Item.SetColor(Col);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ATOHUD::Disc(float CX, float CY, float R, const FLinearColor& C, int32 Segments)
{
	Ring(CX, CY, 0.f, R, C, 0.f, 1.f, Segments);
}

void ATOHUD::Ring(float CX, float CY, float R0, float R1, const FLinearColor& C, float Start01, float End01, int32 Segments)
{
	if (!Canvas || End01 <= Start01 || R1 <= 0.f)
	{
		return;
	}
	const int32 N = FMath::Max(3, FMath::CeilToInt(Segments * (End01 - Start01)));
	TArray<FCanvasUVTri> Tris;
	Tris.Reserve(N * 2);
	auto P = [&](float Radius, float T) -> FVector2D
	{
		// 0 = up, clockwise
		const float A = (T * 2.f * PI) - PI * 0.5f;
		return FVector2D(CX + FMath::Cos(A) * Radius, CY + FMath::Sin(A) * Radius) * S;
	};
	for (int32 i = 0; i < N; ++i)
	{
		const float T0 = FMath::Lerp(Start01, End01, (float)i / N);
		const float T1 = FMath::Lerp(Start01, End01, (float)(i + 1) / N);
		FCanvasUVTri A;
		A.V0_Pos = P(R1, T0);
		A.V1_Pos = P(R1, T1);
		A.V2_Pos = P(R0, T0);
		A.V0_Color = A.V1_Color = A.V2_Color = C;
		Tris.Add(A);
		if (R0 > 0.f)
		{
			FCanvasUVTri B;
			B.V0_Pos = P(R0, T0);
			B.V1_Pos = P(R1, T1);
			B.V2_Pos = P(R0, T1);
			B.V0_Color = B.V1_Color = B.V2_Color = C;
			Tris.Add(B);
		}
	}
	FCanvasTriangleItem Item(FVector2D::ZeroVector, FVector2D::ZeroVector, FVector2D::ZeroVector, GWhiteTexture);
	Item.TriangleList = MoveTemp(Tris);
	Item.BlendMode = SE_BLEND_Translucent;
	Canvas->DrawItem(Item);
}

void ATOHUD::Diamond(float CX, float CY, float R, const FLinearColor& C)
{
	Tri(FVector2D(CX, CY - R), FVector2D(CX + R, CY), FVector2D(CX, CY + R), C);
	Tri(FVector2D(CX, CY - R), FVector2D(CX, CY + R), FVector2D(CX - R, CY), C);
}

void ATOHUD::Arrow(float CX, float CY, float Size, float AngleDeg, const FLinearColor& C)
{
	// AngleDeg: 0 = pointing up (screen), clockwise
	const float A = FMath::DegreesToRadians(AngleDeg);
	auto Rot = [&](float X, float Y)
	{
		return FVector2D(CX + X * FMath::Cos(A) - Y * FMath::Sin(A), CY + X * FMath::Sin(A) + Y * FMath::Cos(A));
	};
	Tri(Rot(0.f, -Size), Rot(Size * 0.7f, Size * 0.8f), Rot(0.f, Size * 0.4f), C);
	Tri(Rot(0.f, -Size), Rot(0.f, Size * 0.4f), Rot(-Size * 0.7f, Size * 0.8f), C);
}

float ATOHUD::TextWidth(const FString& Str, float Size)
{
	if (!Font || Str.IsEmpty())
	{
		return 0.f;
	}
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, Font, Size / FontBaseH);
	return W;
}

float ATOHUD::Text(const FString& Str, float X, float Y, float Size, const FLinearColor& C, ETOAlign Align, bool bShadow)
{
	if (!Canvas || !Font || Str.IsEmpty())
	{
		return 0.f;
	}
	const float Scale = (Size * S) / FontBaseH;
	float W = 0.f, H = 0.f;
	GetTextSize(Str, W, H, Font, Scale);
	float PX = X * S;
	if (Align == ETOAlign::Center)
	{
		PX -= W * 0.5f;
	}
	else if (Align == ETOAlign::Right)
	{
		PX -= W;
	}
	FCanvasTextItem Item(FVector2D(PX, Y * S), FText::FromString(Str), Font, C);
	Item.Scale = FVector2D(Scale, Scale);
	Item.BlendMode = SE_BLEND_Translucent;
	if (bShadow)
	{
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.75f * C.A), FVector2D(1.f, 1.f) * FMath::Max(1.f, S));
	}
	Canvas->DrawItem(Item);
	return W / S;
}

FString ATOHUD::Fit(const FString& Str, float Size, float MaxW)
{
	if (TextWidth(Str, Size) <= MaxW)
	{
		return Str;
	}
	FString Out = Str;
	while (Out.Len() > 1 && TextWidth(Out + TEXT("..."), Size) > MaxW)
	{
		Out.LeftChopInline(1);
	}
	return Out + TEXT("...");
}

void ATOHUD::Tex(UTexture* Texture, float X, float Y, float W, float H, float U, float V, float UW, float VH, const FLinearColor& C)
{
	if (!Texture)
	{
		return;
	}
	DrawTexture(Texture, X * S, Y * S, W * S, H * S, U, V, UW, VH, C, BLEND_Translucent);
}

bool ATOHUD::Hover(float X, float Y, float W, float H) const
{
	return Mouse.X >= X && Mouse.X <= X + W && Mouse.Y >= Y && Mouse.Y <= Y + H;
}

void ATOHUD::Panel(float X, float Y, float W, float H, float Alpha)
{
	Rect(X, Y, W, H, TOStyle::Panel.CopyWithNewOpacity(Alpha));
	Rect(X, Y, W, 2.f, TOStyle::Accent.CopyWithNewOpacity(0.55f));
	Frame(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, 0.06f));
}

void ATOHUD::Header(const FString& Str, float X, float Y, float W, float Size)
{
	Rect(X, Y + 4.f, 4.f, Size - 2.f, TOStyle::Accent);
	Text(Str.ToUpper(), X + 12.f, Y, Size, TOStyle::Text);
	Rect(X, Y + Size + 8.f, W, 1.f, FLinearColor(1.f, 1.f, 1.f, 0.12f));
}

void ATOHUD::Bar(float X, float Y, float W, float H, float Pct, const FLinearColor& Fill, float BackAlpha)
{
	Rect(X, Y, W, H, FLinearColor(0.f, 0.f, 0.f, BackAlpha));
	Rect(X, Y, W * FMath::Clamp(Pct, 0.f, 1.f), H, Fill);
}

bool ATOHUD::ProjectRef(const FVector& World, FVector2D& Out) const
{
	if (!Canvas)
	{
		return false;
	}
	const FVector P = Canvas->Project(World, false);
	if (P.Z <= 0.f)
	{
		return false;
	}
	Out = FVector2D(P.X, P.Y) / S;
	return true;
}

// ---------------------------------------------------------------------------------------------
//  Widgets
// ---------------------------------------------------------------------------------------------

void ATOHUD::Region(float X, float Y, float W, float H, TFunction<void()> OnClick, TFunction<void()> OnRightClick)
{
	FTOUIButton B;
	B.Rect = FBox2D(FVector2D(X, Y), FVector2D(X + W, Y + H));
	B.OnClick = MoveTemp(OnClick);
	B.OnRightClick = MoveTemp(OnRightClick);
	NewButtons.Add(MoveTemp(B));
}

bool ATOHUD::Button(float X, float Y, float W, float H, const FString& Label, TFunction<void()> OnClick, bool bEnabled, bool bSelected, float Size)
{
	const bool bHover = bEnabled && Hover(X, Y, W, H);
	FLinearColor Bg = bSelected ? TOStyle::Accent.CopyWithNewOpacity(0.92f) : TOStyle::Button;
	if (bHover && !bSelected)
	{
		Bg = TOStyle::ButtonHover;
	}
	if (!bEnabled)
	{
		Bg = TOStyle::ButtonDisabled;
	}
	Rect(X, Y, W, H, Bg);
	if (bHover)
	{
		Rect(X, Y + H - 2.f, W, 2.f, TOStyle::Accent);
	}
	Frame(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, bHover ? 0.25f : 0.08f));
	const FLinearColor TextC = bSelected ? TOStyle::TextOnAccent : (bEnabled ? TOStyle::Text : TOStyle::Dim);
	Text(Fit(Label, Size, W - 12.f), X + W * 0.5f, Y + (H - Size) * 0.5f - 1.f, Size, TextC, ETOAlign::Center, !bSelected);
	if (bEnabled && OnClick)
	{
		TFunction<void()> Wrapped = [this, Fn = MoveTemp(OnClick)]()
		{
			PlayUISound(false);
			Fn();
		};
		Region(X, Y, W, H, MoveTemp(Wrapped));
	}
	else
	{
		// Swallow clicks on disabled buttons so nothing underneath reacts.
		Region(X, Y, W, H, []() {});
	}
	return bHover;
}

void ATOHUD::Stepper(float X, float Y, float W, const FString& Label, const FString& Value, TFunction<void()> OnDec, TFunction<void()> OnInc, const FString& Extra, bool bCanDec, bool bCanInc)
{
	const float H = 34.f;
	Rect(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, Hover(X, Y, W, H) ? 0.06f : 0.03f));
	Text(Label, X + 10.f, Y + 8.f, 16.f, TOStyle::Dim);
	const float BW = 30.f;
	const float ValueW = FMath::Min(260.f, W * 0.48f);
	const float VX = X + W - ValueW - (Extra.IsEmpty() ? 0.f : 110.f);
	Button(VX, Y + 3.f, BW, H - 6.f, TEXT("<"), MoveTemp(OnDec), bCanDec, false, 16.f);
	Text(Fit(Value, 16.f, ValueW - 2.f * BW - 8.f), VX + ValueW * 0.5f, Y + 8.f, 16.f, TOStyle::Text, ETOAlign::Center);
	Button(VX + ValueW - BW, Y + 3.f, BW, H - 6.f, TEXT(">"), MoveTemp(OnInc), bCanInc, false, 16.f);
	if (!Extra.IsEmpty())
	{
		Text(Extra, X + W - 8.f, Y + 9.f, 15.f, TOStyle::Money, ETOAlign::Right);
	}
}

void ATOHUD::Slider(float X, float Y, float W, const FString& Label, float Value, float Min, float Max, float Step, TFunction<void(float)> OnSet, const FString& ValueText)
{
	const float H = 40.f;
	Rect(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, Hover(X, Y, W, H) ? 0.06f : 0.03f));
	Text(Label, X + 12.f, Y + 10.f, 17.f, TOStyle::Text);
	const float BarX = X + W * 0.42f;
	const float BarW = W * 0.40f;
	const float Pct = (Value - Min) / FMath::Max(0.0001f, Max - Min);
	Bar(BarX, Y + 17.f, BarW, 6.f, Pct, TOStyle::Accent, 0.6f);
	Rect(BarX + BarW * FMath::Clamp(Pct, 0.f, 1.f) - 4.f, Y + 11.f, 8.f, 18.f, TOStyle::Text);
	Text(ValueText, X + W - 12.f, Y + 10.f, 17.f, TOStyle::Accent, ETOAlign::Right);

	TFunction<void(float)> SetFn = OnSet;
	Region(BarX - 6.f, Y + 4.f, BarW + 12.f, H - 8.f, [this, SetFn, BarX, BarW, Min, Max, Step]()
	{
		const float T = FMath::Clamp((ClickPos.X - BarX) / BarW, 0.f, 1.f);
		float V = FMath::Lerp(Min, Max, T);
		V = FMath::GridSnap(V, Step);
		SetFn(FMath::Clamp(V, Min, Max));
		PlayUISound(false);
	});
	Button(BarX - 40.f, Y + 6.f, 28.f, 28.f, TEXT("-"), [SetFn, Value, Min, Max, Step]() { SetFn(FMath::Clamp(Value - Step, Min, Max)); }, Value > Min + KINDA_SMALL_NUMBER, false, 16.f);
	Button(BarX + BarW + 12.f, Y + 6.f, 28.f, 28.f, TEXT("+"), [SetFn, Value, Min, Max, Step]() { SetFn(FMath::Clamp(Value + Step, Min, Max)); }, Value < Max - KINDA_SMALL_NUMBER, false, 16.f);
}

void ATOHUD::Toggle(float X, float Y, float W, const FString& Label, bool bValue, TFunction<void()> OnFlip)
{
	const float H = 40.f;
	Rect(X, Y, W, H, FLinearColor(1.f, 1.f, 1.f, Hover(X, Y, W, H) ? 0.06f : 0.03f));
	Text(Label, X + 12.f, Y + 10.f, 17.f, TOStyle::Text);
	const float SX = X + W - 74.f;
	Rect(SX, Y + 9.f, 60.f, 22.f, bValue ? TOStyle::Accent.CopyWithNewOpacity(0.85f) : FLinearColor(0.2f, 0.21f, 0.22f, 0.9f));
	Rect(bValue ? SX + 38.f : SX + 2.f, Y + 11.f, 20.f, 18.f, bValue ? TOStyle::TextOnAccent : TOStyle::Dim);
	Text(bValue ? TEXT("ON") : TEXT("OFF"), SX - 10.f, Y + 11.f, 15.f, bValue ? TOStyle::Accent : TOStyle::Dim, ETOAlign::Right);
	Region(X, Y, W, H, [this, Fn = MoveTemp(OnFlip)]()
	{
		PlayUISound(false);
		Fn();
	});
}

void ATOHUD::Choice(float X, float Y, float W, const FString& Label, const TArray<FString>& Options, int32 Selected, TFunction<void(int32)> OnPick)
{
	const float H = 40.f;
	Text(Label, X, Y + 10.f, 16.f, TOStyle::Dim);
	const float LabelW = Label.IsEmpty() ? 0.f : W * 0.30f;
	const int32 N = FMath::Max(1, Options.Num());
	const float Gap = 6.f;
	const float BW = (W - LabelW - Gap * (N - 1)) / N;
	for (int32 i = 0; i < Options.Num(); ++i)
	{
		Button(X + LabelW + i * (BW + Gap), Y + 2.f, BW, H - 4.f, Options[i], [OnPick, i]() { OnPick(i); }, true, i == Selected, 16.f);
	}
}

void ATOHUD::OperatorCards(float X, float Y, float W, ETOOperator Selected, TFunction<void(ETOOperator)> OnPick)
{
	const int32 N = TODB::NumOperators();
	const float Gap = 10.f;
	const float CW = (W - Gap * (N - 1)) / N;
	const float CH = 150.f;
	for (int32 i = 0; i < N; ++i)
	{
		const ETOOperator Op = (ETOOperator)i;
		const FTOOperatorDef& D = TODB::GetOperator(Op);
		const float CX = X + i * (CW + Gap);
		const bool bSel = Op == Selected;
		const bool bHover = Hover(CX, Y, CW, CH);
		Rect(CX, Y, CW, CH, bSel ? FLinearColor(0.10f, 0.12f, 0.08f, 0.95f) : FLinearColor(0.05f, 0.055f, 0.06f, bHover ? 0.95f : 0.85f));
		Rect(CX, Y, CW, 4.f, D.UIColor);
		if (bSel)
		{
			Frame(CX, Y, CW, CH, TOStyle::Accent, 2.f);
		}
		// Stylised operator silhouette
		const float PX = CX + CW - 40.f;
		Disc(PX, Y + 34.f, 12.f, D.UIColor.CopyWithNewOpacity(0.55f), 16);
		Rect(PX - 14.f, Y + 48.f, 28.f, 34.f, D.UIColor.CopyWithNewOpacity(0.4f));
		Text(D.Name.ToUpper(), CX + 12.f, Y + 12.f, 22.f, bSel ? TOStyle::Accent : TOStyle::Text);
		Text(D.Role, CX + 12.f, Y + 40.f, 14.f, TOStyle::Dim);
		Text(Fit(D.AbilityName, 15.f, CW - 24.f), CX + 12.f, Y + 92.f, 15.f, D.UIColor);
		Text(Fit(D.AbilityDesc, 13.f, CW - 24.f), CX + 12.f, Y + 114.f, 13.f, TOStyle::Dim);
		Region(CX, Y, CW, CH, [this, OnPick, Op]()
		{
			PlayUISound(false);
			OnPick(Op);
		});
	}
}

void ATOHUD::DrawBackdrop(float Alpha)
{
	Rect(0.f, 0.f, RefW, RefH, FLinearColor(0.01f, 0.012f, 0.015f, Alpha));
	// Subtle scan lines / vignette bands for the Delta Force menu look.
	Rect(0.f, 0.f, RefW, 90.f, FLinearColor(0.f, 0.f, 0.f, 0.35f * Alpha));
	Rect(0.f, RefH - 60.f, RefW, 60.f, FLinearColor(0.f, 0.f, 0.f, 0.35f * Alpha));
}

void ATOHUD::DrawTitle(const FString& Title, const FString& Sub)
{
	Rect(48.f, 34.f, 6.f, 40.f, TOStyle::Accent);
	Text(Title.ToUpper(), 66.f, 28.f, 40.f, TOStyle::Text);
	if (!Sub.IsEmpty())
	{
		Text(Sub, 68.f, 72.f, 16.f, TOStyle::Dim);
	}
	if (UTOGameInstance* GI = GetGI())
	{
		Text(FString::Printf(TEXT("$ %s"), *TOUtil::FormatMoney(GI->GetCredits())), RefW - 48.f, 38.f, 26.f, TOStyle::Money, ETOAlign::Right);
		Text(TEXT("CREDITS"), RefW - 48.f, 70.f, 13.f, TOStyle::Dim, ETOAlign::Right);
	}
}
