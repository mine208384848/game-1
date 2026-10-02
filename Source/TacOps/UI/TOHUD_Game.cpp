// TAC-OPS - combat HUD (Delta Force style) and the tactical map

#include "UI/TOHUD.h"
#include "UI/TOHUDStyle.h"
#include "Core/TOGameMode.h"
#include "Core/TOGameInstance.h"
#include "Core/TODatabase.h"
#include "Characters/TOCharacter.h"
#include "Characters/TOHealthComponent.h"
#include "Characters/TOInventoryComponent.h"
#include "Weapons/TOWeaponComponent.h"
#include "World/TOWorldGenerator.h"
#include "World/TOExtractionZone.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

namespace
{
	float Bearing(const FVector& From, const FVector& To)
	{
		const FVector D = To - From;
		return FRotator::ClampAxis(FMath::RadiansToDegrees(FMath::Atan2(D.Y, D.X)));
	}

	const TCHAR* CompassLabel(int32 Deg)
	{
		switch (Deg)
		{
		case 0: return TEXT("N");
		case 45: return TEXT("NE");
		case 90: return TEXT("E");
		case 135: return TEXT("SE");
		case 180: return TEXT("S");
		case 225: return TEXT("SW");
		case 270: return TEXT("W");
		case 315: return TEXT("NW");
		default: return nullptr;
		}
	}

	const TCHAR* GrenadeName(ETOGrenadeType T)
	{
		switch (T)
		{
		case ETOGrenadeType::Frag: return TEXT("FRAG");
		case ETOGrenadeType::Smoke: return TEXT("SMOKE");
		default: return TEXT("FLASH");
		}
	}

	const TCHAR* RuleName(ETOExtractRule R)
	{
		switch (R)
		{
		case ETOExtractRule::Radio: return TEXT("RADIO");
		case ETOExtractRule::Paid: return TEXT("PAID");
		case ETOExtractRule::NoBackpack: return TEXT("NO BAG");
		default: return TEXT("OPEN");
		}
	}

	FLinearColor PoiColor(int32 Tier)
	{
		switch (FMath::Clamp(Tier, 0, 3))
		{
		case 0: return FLinearColor(0.85f, 0.88f, 0.9f);
		case 1: return FLinearColor(0.45f, 0.85f, 1.f);
		case 2: return FLinearColor(1.f, 0.75f, 0.3f);
		default: return FLinearColor(1.f, 0.35f, 0.3f);
		}
	}
}

// ---------------------------------------------------------------------------------------------
//  Main combat HUD
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawGameplay()
{
	ATOCharacter* C = GetPlayerChar();
	ATOGameMode* GM = GetGM();
	ATOPlayerController* PC = GetPC();
	if (!GM || !PC)
	{
		return;
	}
	const bool bOverlay = PC->GetScreen() != ETOScreen::None;

	if (C && C->IsAlive())
	{
		DrawScreenEffects(C);
		const UTOWeaponComponent* W = C->GetWeapons();
		if (W && W->IsScoped())
		{
			DrawScopeOverlay(C);
		}
		if (!bOverlay)
		{
			DrawWorldMarkers(C);
			DrawCrosshair(C);
			DrawHitMarker(C);
			DrawDamageIndicators(C);
			DrawInteraction(C);
		}
		DrawCompass(C);
		DrawMinimap(C);
		DrawVitals(C);
		DrawWeaponInfo(C);
		DrawExtraction(C);
		if (C->IsDowned())
		{
			DrawDowned(C);
		}
	}
	if (GM->GetMatchMode() == ETOMatchMode::Warfare)
	{
		DrawWarfareStatus();
	}
	DrawKillFeed();
	DrawMessages();
}

void ATOHUD::DrawScreenEffects(ATOCharacter* C)
{
	const UTOHealthComponent* H = C->GetHealth();
	const float HP01 = H ? H->GetHealth01() : 1.f;
	// Low health: pulsing red edges.
	if (HP01 < 0.45f || C->IsDowned())
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(WorldNow() * (C->IsDowned() ? 3.f : 5.f));
		const float A = FMath::Clamp((0.45f - HP01) / 0.45f, 0.f, 1.f) * (0.35f + 0.25f * Pulse) + (C->IsDowned() ? 0.25f : 0.f);
		for (int32 i = 0; i < 6; ++i)
		{
			const float T = 26.f * (i + 1);
			const FLinearColor Col(0.55f, 0.f, 0.f, A * (1.f - i / 6.f) * 0.35f);
			Rect(0.f, 0.f, RefW, T, Col);
			Rect(0.f, RefH - T, RefW, T, Col);
			Rect(0.f, T, T, RefH - 2.f * T, Col);
			Rect(RefW - T, T, T, RefH - 2.f * T, Col);
		}
	}
	// Suppression: darkened edges.
	if (C->SuppressionAlpha > 0.02f)
	{
		for (int32 i = 0; i < 5; ++i)
		{
			const float T = 40.f * (i + 1);
			const FLinearColor Col(0.f, 0.f, 0.f, C->SuppressionAlpha * 0.16f * (1.f - i / 5.f));
			Rect(0.f, 0.f, RefW, T, Col);
			Rect(0.f, RefH - T, RefW, T, Col);
			Rect(0.f, T, T, RefH - 2.f * T, Col);
			Rect(RefW - T, T, T, RefH - 2.f * T, Col);
		}
	}
	// Flashbang
	if (C->FlashAlpha > 0.01f)
	{
		Rect(0.f, 0.f, RefW, RefH, FLinearColor(1.f, 1.f, 0.97f, FMath::Clamp(C->FlashAlpha, 0.f, 1.f)));
	}
}

// ---------------------------------------------------------------------------------------------
//  Compass
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawCompass(ATOCharacter* C)
{
	const ATOPlayerController* PC = GetPC();
	ATOGameMode* GM = GetGM();
	if (!PC || !GM)
	{
		return;
	}
	const float Yaw = FRotator::ClampAxis(PC->GetControlRotation().Yaw);
	const float CX = RefW * 0.5f;
	const float Y = 16.f;
	const float HalfW = 330.f;
	const float Span = 80.f;
	Rect(CX - HalfW, Y, HalfW * 2.f, 34.f, FLinearColor(0.f, 0.f, 0.f, 0.32f));

	for (int32 Deg = 0; Deg < 360; Deg += 5)
	{
		const float Delta = FRotator::NormalizeAxis(Deg - Yaw);
		if (FMath::Abs(Delta) > Span)
		{
			continue;
		}
		const float X = CX + Delta / Span * HalfW;
		const float Fade = 1.f - FMath::Square(Delta / Span) * 0.85f;
		const TCHAR* Label = CompassLabel(Deg);
		if (Label)
		{
			const bool bNorth = Deg == 0;
			Text(Label, X, Y + 6.f, 18.f, (bNorth ? TOStyle::Accent : TOStyle::Text).CopyWithNewOpacity(Fade), ETOAlign::Center);
		}
		else if (Deg % 15 == 0)
		{
			Rect(X - 0.75f, Y + 4.f, 1.5f, 10.f, FLinearColor(1.f, 1.f, 1.f, 0.8f * Fade));
			Text(FString::Printf(TEXT("%d"), Deg), X, Y + 16.f, 11.f, TOStyle::Dim.CopyWithNewOpacity(Fade), ETOAlign::Center);
		}
		else
		{
			Rect(X - 0.5f, Y + 4.f, 1.f, 6.f, FLinearColor(1.f, 1.f, 1.f, 0.45f * Fade));
		}
	}
	// Center needle + heading
	Tri(FVector2D(CX - 7.f, Y + 36.f + 10.f), FVector2D(CX + 7.f, Y + 36.f + 10.f), FVector2D(CX, Y + 36.f), TOStyle::Accent);
	Text(FString::Printf(TEXT("%03d"), FMath::RoundToInt(Yaw) % 360), CX, Y + 48.f, 14.f, TOStyle::Accent, ETOAlign::Center);

	// Markers on the strip
	const FVector Me = C->GetActorLocation();
	auto Marker = [&](const FVector& Where, const FLinearColor& Col, bool bDiamond)
	{
		const float Delta = FRotator::NormalizeAxis(Bearing(Me, Where) - Yaw);
		const float X = CX + FMath::Clamp(Delta / Span, -1.f, 1.f) * HalfW;
		if (bDiamond)
		{
			Diamond(X, Y - 2.f, 6.f, Col);
		}
		else
		{
			Disc(X, Y - 2.f, 4.f, Col, 10);
		}
	};
	const float Now = WorldNow();
	if (Now - PC->PingTime < 12.f)
	{
		Marker(PC->PingedEnemy.IsValid() ? PC->PingedEnemy->GetActorLocation() : PC->PingLocation, PC->PingedEnemy.IsValid() ? TOStyle::Enemy : TOStyle::Money, true);
	}
	if (GM->GetMatchMode() == ETOMatchMode::Operations)
	{
		for (const TWeakObjectPtr<ATOExtractionZone>& Z : GM->GetExtractionZones())
		{
			if (Z.IsValid() && Z->IsAvailable())
			{
				Marker(Z->GetActorLocation(), TOStyle::Extract.CopyWithNewOpacity(Z->IsReady() ? 1.f : 0.45f), true);
			}
		}
	}
	else
	{
		for (const TWeakObjectPtr<ATOCapturePoint>& P : GM->GetCapturePoints())
		{
			if (P.IsValid() && P->bActive)
			{
				Marker(P->GetActorLocation(), TOStyle::TeamColor(P->OwnerTeam, GM->GetPlayerTeam()), true);
			}
		}
	}
	for (const TWeakObjectPtr<ATOCharacter>& Ptr : GM->GetCharacters())
	{
		const ATOCharacter* O = Ptr.Get();
		if (O && O != C && O->IsAlive() && O->TeamId == C->TeamId && O->SquadId == C->SquadId)
		{
			Marker(O->GetActorLocation(), TOStyle::Friendly, false);
		}
	}
}

// ---------------------------------------------------------------------------------------------
//  Minimap / tactical map
// ---------------------------------------------------------------------------------------------

FVector2D ATOHUD::MapToScreen(const FVector& World, float X, float Y, float Size, float U0, float V0, float UVSize) const
{
	const ATOGameMode* GM = GetGM();
	const ATOWorldGenerator* WG = GM ? GM->GetWorldGen() : nullptr;
	if (!WG)
	{
		return FVector2D(X, Y);
	}
	const FVector2D UV = WG->WorldToMapUV(World);
	return FVector2D(X + (UV.X - U0) / UVSize * Size, Y + (UV.Y - V0) / UVSize * Size);
}

void ATOHUD::DrawMapContents(float X, float Y, float Size, float U0, float V0, float UVSize, bool bFull)
{
	ATOGameMode* GM = GetGM();
	ATOWorldGenerator* WG = GM ? GM->GetWorldGen() : nullptr;
	ATOPlayerController* PC = GetPC();
	if (!WG || !PC)
	{
		return;
	}
	Rect(X, Y, Size, Size, FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Tex(WG->GetMapTexture(), X, Y, Size, Size, U0, V0, UVSize, UVSize, FLinearColor(1.f, 1.f, 1.f, 0.92f));
	Frame(X, Y, Size, Size, FLinearColor(1.f, 1.f, 1.f, 0.25f));

	auto Inside = [&](const FVector2D& P, float Pad = 0.f)
	{
		return P.X >= X + Pad && P.X <= X + Size - Pad && P.Y >= Y + Pad && P.Y <= Y + Size - Pad;
	};
	auto ClampIn = [&](const FVector2D& P, float Pad)
	{
		return FVector2D(FMath::Clamp(P.X, X + Pad, X + Size - Pad), FMath::Clamp(P.Y, Y + Pad, Y + Size - Pad));
	};
	const float K = bFull ? 1.f : 0.75f;
	const ATOCharacter* C = GetPlayerChar();
	const int32 MyTeam = C ? C->TeamId : GM->GetPlayerTeam();

	// Grid (100 m squares on the full map)
	if (bFull)
	{
		const float Cells = 25.f;
		for (int32 i = 1; i < (int32)Cells; ++i)
		{
			const float T = i / Cells * Size;
			Rect(X + T, Y, 1.f, Size, FLinearColor(0.f, 0.f, 0.f, 0.12f));
			Rect(X, Y + T, Size, 1.f, FLinearColor(0.f, 0.f, 0.f, 0.12f));
		}
		for (const FTOPOI& P : WG->POIs)
		{
			const FVector2D SP = MapToScreen(P.Location, X, Y, Size, U0, V0, UVSize);
			if (!Inside(SP))
			{
				continue;
			}
			const FLinearColor Col = PoiColor(P.Tier);
			Rect(SP.X - 3.f, SP.Y - 3.f, 6.f, 6.f, Col);
			Text(P.Name.ToUpper(), SP.X, SP.Y + 6.f, P.bMajor ? 16.f : 13.f, Col, ETOAlign::Center);
		}
	}

	if (GM->GetMatchMode() == ETOMatchMode::Operations)
	{
		for (const TWeakObjectPtr<ATOExtractionZone>& ZP : GM->GetExtractionZones())
		{
			const ATOExtractionZone* Z = ZP.Get();
			if (!Z)
			{
				continue;
			}
			FVector2D SP = MapToScreen(Z->GetActorLocation(), X, Y, Size, U0, V0, UVSize);
			if (!Inside(SP, 6.f))
			{
				if (!Z->IsAvailable() || bFull)
				{
					continue;
				}
				SP = ClampIn(SP, 8.f);
			}
			const FLinearColor Col = Z->IsAvailable() ? (Z->IsReady() ? TOStyle::Extract : TOStyle::Warn) : FLinearColor(0.5f, 0.5f, 0.5f, 0.7f);
			Diamond(SP.X, SP.Y, 9.f * K, FLinearColor(0.f, 0.f, 0.f, 0.6f));
			Diamond(SP.X, SP.Y, 7.f * K, Col);
			if (bFull)
			{
				Text(Z->GetZoneName(), SP.X, SP.Y + 10.f, 14.f, Col, ETOAlign::Center);
				Text(Z->IsAvailable() ? FString::Printf(TEXT("[%s] %s"), RuleName(Z->GetRule()), *Z->GetStatusText()) : FString(TEXT("CLOSED")), SP.X, SP.Y + 28.f, 12.f, TOStyle::Dim, ETOAlign::Center);
			}
		}
	}
	else
	{
		for (const TWeakObjectPtr<ATOCapturePoint>& PP : GM->GetCapturePoints())
		{
			const ATOCapturePoint* P = PP.Get();
			if (!P || (!P->bActive && !bFull))
			{
				continue;
			}
			const FVector2D SP = MapToScreen(P->GetActorLocation(), X, Y, Size, U0, V0, UVSize);
			if (!Inside(SP, 6.f))
			{
				continue;
			}
			const FLinearColor Col = TOStyle::TeamColor(P->OwnerTeam, GM->GetPlayerTeam()).CopyWithNewOpacity(P->bActive ? 1.f : 0.4f);
			Disc(SP.X, SP.Y, 11.f * K, FLinearColor(0.f, 0.f, 0.f, 0.6f), 16);
			Disc(SP.X, SP.Y, 9.f * K, Col, 16);
			if (P->bActive && P->Capture > 0.f && P->Capture < 1.f)
			{
				Ring(SP.X, SP.Y, 11.f * K, 14.f * K, TOStyle::Text, 0.f, P->Capture, 24);
			}
			Text(P->Label, SP.X, SP.Y - 8.f * K, 13.f * K, TOStyle::TextOnAccent, ETOAlign::Center, false);
		}
	}

	// Characters: squad mates, revealed enemies
	for (const TWeakObjectPtr<ATOCharacter>& Ptr : GM->GetCharacters())
	{
		const ATOCharacter* O = Ptr.Get();
		if (!O || O == C || !O->IsAlive())
		{
			continue;
		}
		const bool bFriend = O->TeamId == MyTeam;
		const bool bShow = bFriend ? (GM->GetMatchMode() == ETOMatchMode::Operations ? O->SquadId == (C ? C->SquadId : -2) : true) : O->IsRevealed();
		if (!bShow)
		{
			continue;
		}
		const FVector2D SP = MapToScreen(O->GetActorLocation(), X, Y, Size, U0, V0, UVSize);
		if (!Inside(SP, 3.f))
		{
			continue;
		}
		const FLinearColor Col = bFriend ? (O->IsDowned() ? TOStyle::Warn : TOStyle::Friendly) : TOStyle::Enemy;
		if (bFriend)
		{
			Arrow(SP.X, SP.Y, 6.f * K, O->GetActorRotation().Yaw, Col);
		}
		else
		{
			Diamond(SP.X, SP.Y, 5.f * K, Col);
		}
	}

	// Ping
	if (WorldNow() - PC->PingTime < 20.f)
	{
		const FVector Where = PC->PingedEnemy.IsValid() ? PC->PingedEnemy->GetActorLocation() : PC->PingLocation;
		const FVector2D SP = MapToScreen(Where, X, Y, Size, U0, V0, UVSize);
		if (Inside(SP, 4.f))
		{
			Diamond(SP.X, SP.Y, 7.f * K, PC->PingedEnemy.IsValid() ? TOStyle::Enemy : TOStyle::Money);
		}
	}

	// Player
	if (C)
	{
		const FVector2D SP = MapToScreen(C->GetActorLocation(), X, Y, Size, U0, V0, UVSize);
		if (Inside(SP, 2.f))
		{
			Arrow(SP.X, SP.Y, 10.f * K, PC->GetControlRotation().Yaw, FLinearColor(0.f, 0.f, 0.f, 0.7f));
			Arrow(SP.X, SP.Y, 8.f * K, PC->GetControlRotation().Yaw, TOStyle::Accent);
		}
	}
}

void ATOHUD::DrawMinimap(ATOCharacter* C)
{
	ATOGameMode* GM = GetGM();
	ATOWorldGenerator* WG = GM ? GM->GetWorldGen() : nullptr;
	if (!WG || !WG->GetMapTexture())
	{
		return;
	}
	const float X = 24.f;
	const float Y = 24.f;
	const float Size = 250.f;
	const float ViewMeters = 360.f;
	const float UVSize = (ViewMeters * 100.f) / (2.f * WG->GetHalfSize());
	const FVector2D UV = WG->WorldToMapUV(C->GetActorLocation());
	const float U0 = FMath::Clamp(UV.X - UVSize * 0.5f, 0.f, 1.f - UVSize);
	const float V0 = FMath::Clamp(UV.Y - UVSize * 0.5f, 0.f, 1.f - UVSize);
	DrawMapContents(X, Y, Size, U0, V0, UVSize, false);
	Text(TEXT("N"), X + Size * 0.5f, Y + 4.f, 14.f, TOStyle::Accent, ETOAlign::Center);

	// Raid timer / mode line
	float TY = Y + Size + 8.f;
	if (GM->GetMatchMode() == ETOMatchMode::Operations)
	{
		const float Left = GM->GetRaidTimeLeft();
		const FLinearColor Col = Left < 300.f ? TOStyle::Danger : TOStyle::Text;
		Rect(X, TY, Size, 30.f, FLinearColor(0.f, 0.f, 0.f, 0.45f));
		Text(TEXT("RAID"), X + 10.f, TY + 7.f, 14.f, TOStyle::Dim);
		Text(TOUtil::FormatTime(Left), X + Size - 10.f, TY + 4.f, 20.f, Col, ETOAlign::Right);
		TY += 38.f;

		// Open extractions with distance
		const FVector Me = C->GetActorLocation();
		int32 Shown = 0;
		for (const TWeakObjectPtr<ATOExtractionZone>& ZP : GM->GetExtractionZones())
		{
			const ATOExtractionZone* Z = ZP.Get();
			if (!Z || !Z->IsAvailable() || Shown >= 4)
			{
				continue;
			}
			const float Dist = FVector::Dist2D(Me, Z->GetActorLocation()) / 100.f;
			const FLinearColor Col2 = Z->IsReady() ? TOStyle::Extract : TOStyle::Warn;
			Rect(X, TY, 4.f, 20.f, Col2);
			Text(Fit(Z->GetZoneName(), 14.f, Size - 80.f), X + 10.f, TY + 2.f, 14.f, TOStyle::Text);
			Text(FString::Printf(TEXT("%.0fm"), Dist), X + Size - 4.f, TY + 2.f, 14.f, Col2, ETOAlign::Right);
			TY += 24.f;
			++Shown;
		}
	}
	else
	{
		Rect(X, TY, Size, 30.f, FLinearColor(0.f, 0.f, 0.f, 0.45f));
		Text(TEXT("WARFARE"), X + 10.f, TY + 7.f, 14.f, TOStyle::Dim);
		Text(TOUtil::FormatTime(GM->GetMatchTime()), X + Size - 10.f, TY + 4.f, 20.f, TOStyle::Text, ETOAlign::Right);
	}
}

void ATOHUD::DrawMapScreen()
{
	ATOGameMode* GM = GetGM();
	ATOWorldGenerator* WG = GM ? GM->GetWorldGen() : nullptr;
	ATOPlayerController* PC = GetPC();
	if (!WG || !PC)
	{
		return;
	}
	Rect(0.f, 0.f, RefW, RefH, FLinearColor(0.01f, 0.012f, 0.015f, 0.9f));
	const float Size = RefH - 120.f;
	const float X = FMath::Max(40.f, RefW * 0.5f - Size * 0.5f - 160.f);
	const float Y = 70.f;
	Text(FString::Printf(TEXT("%s  -  TACTICAL MAP"), *WG->MapName.ToUpper()), X, 26.f, 28.f, TOStyle::Text);
	Text(TEXT("[M] close    [LMB] place marker"), X + Size, 34.f, 15.f, TOStyle::Dim, ETOAlign::Right);
	DrawMapContents(X, Y, Size, 0.f, 0.f, 1.f, true);

	// Click to place a waypoint marker.
	const float Half = WG->GetHalfSize();
	Region(X, Y, Size, Size, [this, PC, WG, X, Y, Size, Half]()
	{
		const float U = (ClickPos.X - X) / Size;
		const float V = (ClickPos.Y - Y) / Size;
		const float WX = Half - V * 2.f * Half;
		const float WY = U * 2.f * Half - Half;
		PC->PingLocation = FVector(WX, WY, WG->GetTerrainHeight(WX, WY));
		PC->PingTime = WorldNow();
		PC->PingedEnemy.Reset();
		PlayUISound(false);
	});

	// Legend / extraction list
	const float LX = X + Size + 30.f;
	const float LW = FMath::Max(300.f, RefW - LX - 40.f);
	Panel(LX, Y, LW, Size);
	float LY = Y + 16.f;
	if (GM->GetMatchMode() == ETOMatchMode::Operations)
	{
		Header(TEXT("Extraction"), LX + 18.f, LY, LW - 36.f);
		LY += 46.f;
		const ATOCharacter* C = GetPlayerChar();
		for (const TWeakObjectPtr<ATOExtractionZone>& ZP : GM->GetExtractionZones())
		{
			const ATOExtractionZone* Z = ZP.Get();
			if (!Z)
			{
				continue;
			}
			const FLinearColor Col = Z->IsAvailable() ? (Z->IsReady() ? TOStyle::Extract : TOStyle::Warn) : FLinearColor(0.5f, 0.5f, 0.5f, 1.f);
			Rect(LX + 18.f, LY, 4.f, 44.f, Col);
			Text(Z->GetZoneName(), LX + 30.f, LY + 2.f, 17.f, Z->IsAvailable() ? TOStyle::Text : TOStyle::Dim);
			Text(Z->IsAvailable() ? Fit(Z->GetStatusText(), 13.f, LW - 70.f) : FString(TEXT("Closed this raid")), LX + 30.f, LY + 24.f, 13.f, TOStyle::Dim);
			if (C)
			{
				Text(FString::Printf(TEXT("%.0fm"), FVector::Dist2D(C->GetActorLocation(), Z->GetActorLocation()) / 100.f), LX + LW - 18.f, LY + 2.f, 15.f, Col, ETOAlign::Right);
			}
			LY += 54.f;
		}
		LY += 10.f;
		Header(TEXT("Zones"), LX + 18.f, LY, LW - 36.f);
		LY += 46.f;
		const TCHAR* TierNames[] = { TEXT("Low risk"), TEXT("Medium"), TEXT("High value"), TEXT("Extreme") };
		for (int32 T = 0; T < 4; ++T)
		{
			Rect(LX + 18.f, LY + 4.f, 10.f, 10.f, PoiColor(T));
			Text(TierNames[T], LX + 36.f, LY, 15.f, TOStyle::Text);
			LY += 24.f;
		}
	}
	else
	{
		Header(TEXT("Front line"), LX + 18.f, LY, LW - 36.f);
		LY += 46.f;
		Text(FString::Printf(TEXT("Sector %d / %d"), GM->GetCurrentSector() + 1, GM->GetNumSectors()), LX + 18.f, LY, 18.f, TOStyle::Text);
		Text(GM->GetSectorName(), LX + 18.f, LY + 26.f, 15.f, TOStyle::Dim);
		LY += 64.f;
		for (const TWeakObjectPtr<ATOCapturePoint>& PP : GM->GetCapturePoints())
		{
			const ATOCapturePoint* P = PP.Get();
			if (!P || !P->bActive)
			{
				continue;
			}
			const FLinearColor Col = TOStyle::TeamColor(P->OwnerTeam, GM->GetPlayerTeam());
			Rect(LX + 18.f, LY, 4.f, 30.f, Col);
			Text(FString::Printf(TEXT("Objective %s"), *P->Label), LX + 30.f, LY + 4.f, 17.f, TOStyle::Text);
			Bar(LX + LW * 0.55f, LY + 12.f, LW * 0.45f - 18.f, 6.f, P->Capture, TOStyle::Warn);
			LY += 40.f;
		}
	}
	LY = Y + Size - 110.f;
	Text(TEXT("Legend"), LX + 18.f, LY, 15.f, TOStyle::Dim);
	Arrow(LX + 28.f, LY + 36.f, 8.f, 0.f, TOStyle::Accent);
	Text(TEXT("You"), LX + 44.f, LY + 28.f, 14.f, TOStyle::Text);
	Arrow(LX + 118.f, LY + 36.f, 7.f, 0.f, TOStyle::Friendly);
	Text(TEXT("Squad"), LX + 132.f, LY + 28.f, 14.f, TOStyle::Text);
	Diamond(LX + 218.f, LY + 36.f, 6.f, TOStyle::Enemy);
	Text(TEXT("Spotted"), LX + 232.f, LY + 28.f, 14.f, TOStyle::Text);
	Diamond(LX + 28.f, LY + 70.f, 6.f, TOStyle::Money);
	Text(TEXT("Marker"), LX + 44.f, LY + 62.f, 14.f, TOStyle::Text);
	Diamond(LX + 118.f, LY + 70.f, 6.f, TOStyle::Extract);
	Text(TEXT("Extraction"), LX + 132.f, LY + 62.f, 14.f, TOStyle::Text);
}

// ---------------------------------------------------------------------------------------------
//  Vitals (bottom left)
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawVitals(ATOCharacter* C)
{
	const UTOHealthComponent* H = C->GetHealth();
	const UTOInventoryComponent* Inv = C->GetInventory();
	if (!H || !Inv)
	{
		return;
	}
	const float X = 24.f;
	const float W = 430.f;
	const float Y = RefH - 168.f;
	Rect(X, Y, W, 144.f, FLinearColor(0.f, 0.f, 0.f, 0.38f));
	Rect(X, Y, 3.f, 144.f, TOStyle::Accent.CopyWithNewOpacity(0.7f));

	// Operator + ability
	const FTOOperatorDef& Op = TODB::GetOperator(C->Operator);
	Text(Op.Name.ToUpper(), X + 14.f, Y + 8.f, 16.f, Op.UIColor);
	const float CD = C->GetAbilityCooldownRemaining();
	const float CDT = FMath::Max(1.f, C->GetAbilityCooldownTotal());
	const FString AbilityText = CD > 0.f ? FString::Printf(TEXT("[X] %s  %.0fs"), *Op.AbilityName, CD) : FString::Printf(TEXT("[X] %s  READY"), *Op.AbilityName);
	Text(AbilityText, X + W - 12.f, Y + 9.f, 14.f, CD > 0.f ? TOStyle::Dim : TOStyle::Accent, ETOAlign::Right);
	Bar(X + 14.f, Y + 30.f, W - 28.f, 2.f, 1.f - CD / CDT, CD > 0.f ? TOStyle::Dim : TOStyle::Accent, 0.3f);

	// Health
	DisplayedHealth = FMath::FInterpTo(DisplayedHealth, H->GetHealth(), GetWorld()->GetDeltaSeconds(), 6.f);
	const float HP01 = H->GetHealth01();
	const FLinearColor HPCol = HP01 > 0.5f ? TOStyle::Health : (HP01 > 0.25f ? TOStyle::Warn : TOStyle::Danger);
	Text(FString::Printf(TEXT("%.0f"), FMath::CeilToFloat(H->GetHealth())), X + 14.f, Y + 38.f, 40.f, HPCol);
	const float BX = X + 96.f;
	const float BW = W - 110.f;
	Rect(BX, Y + 52.f, BW, 14.f, FLinearColor(0.f, 0.f, 0.f, 0.5f));
	Rect(BX, Y + 52.f, BW * FMath::Clamp(DisplayedHealth / H->GetMaxHealth(), 0.f, 1.f), 14.f, TOStyle::Danger.CopyWithNewOpacity(0.7f));
	Rect(BX, Y + 52.f, BW * HP01, 14.f, HPCol);
	for (int32 i = 1; i < 10; ++i)
	{
		Rect(BX + BW * i / 10.f, Y + 52.f, 1.f, 14.f, FLinearColor(0.f, 0.f, 0.f, 0.4f));
	}
	// Stamina
	Bar(BX, Y + 70.f, BW, 3.f, C->GetStamina01(), FLinearColor(0.9f, 0.9f, 0.7f, 0.8f), 0.35f);

	// Armor / helmet
	auto GearBar = [&](float GY, const TCHAR* Label, int32 Level, float Pct)
	{
		Text(Label, X + 14.f, GY - 2.f, 13.f, TOStyle::Dim);
		if (Level <= 0)
		{
			Text(TEXT("NONE"), X + 96.f, GY - 2.f, 13.f, TOStyle::Dim);
			return;
		}
		Rect(X + 70.f, GY - 1.f, 22.f, 15.f, TOStyle::Armor.CopyWithNewOpacity(0.85f));
		Text(FString::Printf(TEXT("%d"), Level), X + 81.f, GY - 2.f, 13.f, TOStyle::TextOnAccent, ETOAlign::Center, false);
		Bar(BX, GY + 3.f, BW - 50.f, 7.f, Pct, Pct > 0.3f ? TOStyle::Armor : TOStyle::Danger, 0.5f);
		Text(FString::Printf(TEXT("%.0f%%"), Pct * 100.f), X + W - 12.f, GY - 2.f, 13.f, TOStyle::Text, ETOAlign::Right);
	};
	GearBar(Y + 82.f, TEXT("ARMOR"), Inv->GetArmorLevel(), Inv->GetArmor01());
	GearBar(Y + 102.f, TEXT("HELMET"), Inv->GetHelmetLevel(), Inv->GetHelmet01());

	// Status effects
	float SX = X + 14.f;
	const float SY = Y + 122.f;
	auto Chip = [&](const FString& Str, const FLinearColor& Col)
	{
		const float TW = TextWidth(Str, 12.f) + 14.f;
		Rect(SX, SY, TW, 18.f, Col.CopyWithNewOpacity(0.85f));
		Text(Str, SX + 7.f, SY + 2.f, 12.f, TOStyle::TextOnAccent, ETOAlign::Left, false);
		SX += TW + 6.f;
	};
	if (H->IsBleeding()) Chip(TEXT("BLEEDING"), TOStyle::Danger);
	if (H->HasLegFracture()) Chip(TEXT("LEG FRACTURE"), TOStyle::Warn);
	if (H->HasArmFracture()) Chip(TEXT("ARM FRACTURE"), TOStyle::Warn);
	if (H->IsPainkillerActive()) Chip(FString::Printf(TEXT("PAINKILLER %.0fs"), H->GetPainkillerTime()), TOStyle::Good);
	if (C->IsAdrenalineActive()) Chip(TEXT("ADRENALINE"), TOStyle::Accent);
	if (C->IsNVGOn()) Chip(TEXT("NVG"), TOStyle::Good);
	if (C->GetStance() != ETOStance::Stand) Chip(C->GetStance() == ETOStance::Crouch ? TEXT("CROUCH") : TEXT("PRONE"), FLinearColor(0.75f, 0.78f, 0.8f));

	// Item use progress
	if (C->IsUsingItem())
	{
		const float P = C->GetUseProgress();
		const float UW = 360.f;
		const float UX = RefW * 0.5f - UW * 0.5f;
		const float UY = RefH * 0.5f + 150.f;
		Rect(UX, UY, UW, 44.f, FLinearColor(0.f, 0.f, 0.f, 0.55f));
		Text(FString::Printf(TEXT("USING  %s"), *C->GetUsingItemName().ToUpper()), UX + 12.f, UY + 6.f, 15.f, TOStyle::Text);
		Bar(UX + 12.f, UY + 30.f, UW - 24.f, 6.f, P, TOStyle::Good, 0.5f);
	}
}

// ---------------------------------------------------------------------------------------------
//  Weapon / ammo (bottom right)
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawWeaponInfo(ATOCharacter* C)
{
	UTOWeaponComponent* W = C->GetWeapons();
	UTOInventoryComponent* Inv = C->GetInventory();
	const UTOHealthComponent* H = C->GetHealth();
	if (!W || !Inv || !H)
	{
		return;
	}
	const float Wd = 400.f;
	const float X = RefW - Wd - 24.f;
	const float Y = RefH - 168.f;
	Rect(X, Y, Wd, 144.f, FLinearColor(0.f, 0.f, 0.f, 0.38f));
	Rect(X + Wd - 3.f, Y, 3.f, 144.f, TOStyle::Accent.CopyWithNewOpacity(0.7f));

	Text(W->GetActiveName().ToUpper(), X + Wd - 14.f, Y + 8.f, 18.f, TOStyle::Text, ETOAlign::Right);
	if (!W->IsMeleeActive())
	{
		const int32 Mag = W->GetMagAmmo();
		const int32 MagSize = FMath::Max(1, W->GetMagSize());
		const FLinearColor MagCol = Mag == 0 ? TOStyle::Danger : (Mag <= MagSize / 4 ? TOStyle::Warn : TOStyle::Text);
		const FString Reserve = W->bInfiniteReserve ? FString(TEXT("INF")) : FString::Printf(TEXT("%d"), W->GetReserveAmmo());
		const float RW = Text(Reserve, X + Wd - 14.f, Y + 48.f, 22.f, TOStyle::Dim, ETOAlign::Right);
		Text(TEXT("/"), X + Wd - 22.f - RW, Y + 46.f, 24.f, TOStyle::Dim, ETOAlign::Right);
		Text(FString::Printf(TEXT("%d"), Mag), X + Wd - 40.f - RW, Y + 30.f, 46.f, MagCol, ETOAlign::Right);
		// fire mode + ammo tier
		const FString Mode = TODB::FireModeName(W->GetFireMode());
		const float MW = TextWidth(Mode, 13.f) + 14.f;
		Rect(X + 14.f, Y + 40.f, MW, 20.f, FLinearColor(1.f, 1.f, 1.f, 0.14f));
		Text(Mode, X + 21.f, Y + 42.f, 13.f, TOStyle::Text);
		if (const FTOWeaponConfig* Cfg = W->GetActiveConfig())
		{
			const int32 Tier = W->GetSlot(W->GetActiveSlotIndex()).LoadedTier;
			Rect(X + 20.f + MW, Y + 40.f, 34.f, 20.f, TODB::RarityColor((ETORarity)FMath::Clamp(Tier - 1, 0, 5)).CopyWithNewOpacity(0.85f));
			Text(FString::Printf(TEXT("T%d"), Tier), X + 37.f + MW, Y + 42.f, 13.f, TOStyle::TextOnAccent, ETOAlign::Center, false);
			(void)Cfg;
		}
		// Magazine pips
		const int32 Pips = FMath::Min(MagSize, 40);
		const float PW = (Wd - 28.f) / Pips;
		for (int32 i = 0; i < Pips; ++i)
		{
			const bool bFull = i < FMath::CeilToInt((float)Mag / MagSize * Pips);
			Rect(X + 14.f + i * PW, Y + 86.f, FMath::Max(1.f, PW - 2.f), 4.f, bFull ? MagCol.CopyWithNewOpacity(0.8f) : FLinearColor(1.f, 1.f, 1.f, 0.12f));
		}
		if (W->IsReloading())
		{
			Bar(X + 14.f, Y + 94.f, Wd - 28.f, 3.f, W->GetReloadProgress(), TOStyle::Accent, 0.3f);
			Text(TEXT("RELOADING"), X + 14.f, Y + 64.f, 13.f, TOStyle::Accent);
		}
		else if (Mag == 0)
		{
			Text(W->GetReserveAmmo() > 0 || W->bInfiniteReserve ? TEXT("[R] RELOAD") : TEXT("NO AMMO"), X + 14.f, Y + 64.f, 13.f, TOStyle::Danger);
		}
	}

	// Grenades + quick heal row
	float GX = X + 14.f;
	const float GY = Y + 108.f;
	for (int32 g = 0; g < 3; ++g)
	{
		const ETOGrenadeType T = (ETOGrenadeType)g;
		const int32 Count = Inv->CountGrenades(T);
		const bool bSel = C->GetSelectedGrenade() == T;
		const FString Str = FString::Printf(TEXT("%s %d"), GrenadeName(T), Count);
		const float TW = TextWidth(Str, 13.f) + 12.f;
		Rect(GX, GY, TW, 22.f, bSel ? TOStyle::Accent.CopyWithNewOpacity(Count > 0 ? 0.85f : 0.35f) : FLinearColor(1.f, 1.f, 1.f, 0.08f));
		Text(Str, GX + 6.f, GY + 3.f, 13.f, bSel ? TOStyle::TextOnAccent : (Count > 0 ? TOStyle::Text : TOStyle::Dim), ETOAlign::Left, !bSel);
		GX += TW + 5.f;
	}
	const FTOItemInstance* Med = Inv->FindBestMedical(H->IsBleeding(), H->HasLegFracture() || H->HasArmFracture(), H->GetHealth01() < 1.f);
	const FString MedStr = Med ? FString::Printf(TEXT("[H] %s"), *TODB::ItemName(*Med, true)) : FString(TEXT("[H] -"));
	Text(MedStr, X + Wd - 14.f, GY + 3.f, 13.f, Med ? TOStyle::Good : TOStyle::Dim, ETOAlign::Right);
	if (W->IsLightOn())
	{
		Text(TEXT("LIGHT"), X + Wd - 14.f, Y + 64.f, 12.f, TOStyle::Money, ETOAlign::Right);
	}
}

// ---------------------------------------------------------------------------------------------
//  Kill feed & messages
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawKillFeed()
{
	ATOGameMode* GM = GetGM();
	const ATOCharacter* C = GetPlayerChar();
	if (!GM)
	{
		return;
	}
	const float Now = WorldNow();
	const int32 MyTeam = C ? C->TeamId : GM->GetPlayerTeam();
	float Y = 24.f;
	for (int32 i = GM->KillFeed.Num() - 1; i >= 0; --i)
	{
		const FTOKillFeedEntry& E = GM->KillFeed[i];
		const float Age = Now - E.Time;
		if (Age > 9.f)
		{
			continue;
		}
		const float A = FMath::Clamp((9.f - Age) / 1.f, 0.f, 1.f);
		const FString Killer = E.Killer.IsEmpty() ? FString(TEXT("")) : E.Killer;
		const FString Mid = E.bDowned ? FString(TEXT(" [DOWNED] ")) : FString::Printf(TEXT(" [%s%s] "), E.Weapon.IsEmpty() ? TEXT("KIA") : *E.Weapon, E.bHeadshot ? TEXT(" HS") : TEXT(""));
		const float VW = TextWidth(E.Victim, 15.f);
		const float MW = TextWidth(Mid, 13.f);
		const float KW = TextWidth(Killer, 15.f);
		const float Total = KW + MW + VW + 20.f;
		const float X = RefW - 24.f - Total;
		const bool bMine = E.Killer == TEXT("YOU") || E.Victim == TEXT("YOU");
		Rect(X, Y, Total, 24.f, FLinearColor(0.f, 0.f, 0.f, (bMine ? 0.6f : 0.35f) * A));
		if (bMine)
		{
			Rect(X, Y, 3.f, 24.f, TOStyle::Accent.CopyWithNewOpacity(A));
		}
		float TX = X + 10.f;
		if (!Killer.IsEmpty())
		{
			Text(Killer, TX, Y + 3.f, 15.f, TOStyle::TeamColor(E.KillerTeam, MyTeam).CopyWithNewOpacity(A));
		}
		TX += KW;
		Text(Mid, TX, Y + 5.f, 13.f, (E.bHeadshot ? TOStyle::Money : TOStyle::Dim).CopyWithNewOpacity(A));
		TX += MW;
		Text(E.Victim, TX, Y + 3.f, 15.f, TOStyle::TeamColor(E.VictimTeam, MyTeam).CopyWithNewOpacity(A));
		Y += 28.f;
	}
}

void ATOHUD::DrawMessages()
{
	ATOGameMode* GM = GetGM();
	if (!GM)
	{
		return;
	}
	const float Now = WorldNow();
	const bool bWarfare = GM->GetMatchMode() == ETOMatchMode::Warfare;
	float Y = bWarfare ? 178.f : 96.f;
	int32 Shown = 0;
	for (int32 i = GM->Messages.Num() - 1; i >= 0 && Shown < 4; --i)
	{
		const FTOMessage& M = GM->Messages[i];
		const float Age = Now - M.Time;
		if (Age > 6.f || Age < 0.f)
		{
			continue;
		}
		const float A = FMath::Clamp((6.f - Age) / 1.f, 0.f, 1.f) * FMath::Clamp(Age * 6.f, 0.f, 1.f);
		const float TW = TextWidth(M.Text, 17.f) + 30.f;
		Rect(RefW * 0.5f - TW * 0.5f, Y, TW, 28.f, FLinearColor(0.f, 0.f, 0.f, 0.45f * A));
		Text(M.Text, RefW * 0.5f, Y + 4.f, 17.f, M.Color.CopyWithNewOpacity(A), ETOAlign::Center);
		Y += 32.f;
		++Shown;
	}
}

// ---------------------------------------------------------------------------------------------
//  Crosshair, scope, hit markers, damage direction
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawCrosshair(ATOCharacter* C)
{
	const UTOWeaponComponent* W = C->GetWeapons();
	if (!W || C->IsDowned())
	{
		return;
	}
	const float CX = RefW * 0.5f;
	const float CY = RefH * 0.5f;
	const float ADS = W->GetADSAlpha();
	const FTOWeaponStats& St = W->GetStats();

	if (ADS > 0.85f)
	{
		if (W->IsScoped())
		{
			return;
		}
		// Reflex / holographic reticles are projected at the screen center.
		switch (St.Optic)
		{
		case ETOOpticType::RedDot:
			Disc(CX, CY, 2.2f, FLinearColor(1.f, 0.1f, 0.08f, 0.95f), 12);
			break;
		case ETOOpticType::Holo:
			Ring(CX, CY, 13.f, 14.5f, FLinearColor(1.f, 0.15f, 0.1f, 0.9f), 0.f, 1.f, 40);
			Disc(CX, CY, 1.8f, FLinearColor(1.f, 0.15f, 0.1f, 0.95f), 10);
			break;
		case ETOOpticType::Prism:
		case ETOOpticType::ACOG:
			Tri(FVector2D(CX, CY), FVector2D(CX - 5.f, CY + 9.f), FVector2D(CX + 5.f, CY + 9.f), FLinearColor(1.f, 0.35f, 0.1f, 0.95f));
			Line(CX, CY + 9.f, CX, CY + 40.f, FLinearColor(0.f, 0.f, 0.f, 0.85f), 1.5f);
			break;
		default:
			break;
		}
		return;
	}
	if (C->IsSprinting() || W->IsMeleeActive())
	{
		Disc(CX, CY, 1.6f, FLinearColor(1.f, 1.f, 1.f, 0.5f), 8);
		return;
	}
	// Spread (degrees) -> screen distance.
	const float FOV = FMath::Max(10.f, C->GetFOVForView());
	const float Px = FMath::Tan(FMath::DegreesToRadians(W->GetSpreadDegrees())) / FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f)) * (RefW * 0.5f);
	const float Gap = FMath::Clamp(Px, 4.f, 140.f) * (1.f - ADS);
	const float Len = 10.f;
	const FLinearColor Col(1.f, 1.f, 1.f, 0.9f * (1.f - ADS));
	const FLinearColor Shadow(0.f, 0.f, 0.f, 0.5f * (1.f - ADS));
	auto Seg = [&](float X1, float Y1, float X2, float Y2)
	{
		Line(X1 + 1.f, Y1 + 1.f, X2 + 1.f, Y2 + 1.f, Shadow, 2.f);
		Line(X1, Y1, X2, Y2, Col, 2.f);
	};
	Seg(CX - Gap - Len, CY, CX - Gap, CY);
	Seg(CX + Gap, CY, CX + Gap + Len, CY);
	Seg(CX, CY + Gap, CX, CY + Gap + Len);
	if (St.Def && St.Def->Class == ETOWeaponClass::Shotgun)
	{
		Ring(CX, CY, Gap, Gap + 1.5f, Col, 0.f, 1.f, 40);
	}
	else
	{
		Seg(CX, CY - Gap - Len, CX, CY - Gap);
	}
	Disc(CX, CY, 1.5f, Col, 8);
}

void ATOHUD::DrawScopeOverlay(ATOCharacter* C)
{
	const float CX = RefW * 0.5f;
	const float CY = RefH * 0.5f;
	const float R = RefH * 0.46f;
	const FLinearColor Black(0.f, 0.f, 0.f, 1.f);
	Rect(0.f, 0.f, CX - R, RefH, Black);
	Rect(CX + R, 0.f, RefW - (CX + R), RefH, Black);
	Rect(CX - R, 0.f, 2.f * R, CY - R, Black);
	Rect(CX - R, CY + R, 2.f * R, RefH - (CY + R), Black);
	Ring(CX, CY, R - 1.f, R * 1.45f, Black, 0.f, 1.f, 96);
	Ring(CX, CY, R - 14.f, R, FLinearColor(0.f, 0.f, 0.f, 0.55f), 0.f, 1.f, 96);

	// Mil-dot reticle
	const FLinearColor Ret(0.f, 0.f, 0.f, 0.95f);
	Line(CX - R, CY, CX - 26.f, CY, Ret, 3.f);
	Line(CX + 26.f, CY, CX + R, CY, Ret, 3.f);
	Line(CX, CY + 26.f, CX, CY + R, Ret, 3.f);
	Line(CX, CY - R, CX, CY - 26.f, Ret, 1.5f);
	Line(CX - 26.f, CY, CX + 26.f, CY, Ret, 1.f);
	Line(CX, CY - 26.f, CX, CY + 26.f, Ret, 1.f);
	for (int32 i = 1; i <= 5; ++i)
	{
		const float O = i * 40.f;
		Disc(CX - O, CY, 2.5f, Ret, 8);
		Disc(CX + O, CY, 2.5f, Ret, 8);
		Disc(CX, CY + O, 2.5f, Ret, 8);
	}
	Disc(CX, CY, 1.6f, FLinearColor(1.f, 0.1f, 0.05f, 0.95f), 8);
	if (const UTOWeaponComponent* W = C->GetWeapons())
	{
		Text(FString::Printf(TEXT("%.1fx"), W->GetStats().Zoom), CX + R * 0.7f, CY + R * 0.62f, 16.f, FLinearColor(0.85f, 0.85f, 0.85f, 0.7f));
	}
	Text(TEXT("[SHIFT] HOLD BREATH"), CX, CY + R - 50.f, 13.f, FLinearColor(0.8f, 0.8f, 0.8f, 0.5f), ETOAlign::Center);
}

void ATOHUD::DrawHitMarker(ATOCharacter* C)
{
	const float Age = WorldNow() - C->LastHitMarkerTime;
	if (Age > 0.35f || Age < 0.f)
	{
		return;
	}
	const float A = 1.f - Age / 0.35f;
	const float CX = RefW * 0.5f;
	const float CY = RefH * 0.5f;
	FLinearColor Col = FLinearColor(1.f, 1.f, 1.f, A);
	if (C->bLastHitKill)
	{
		Col = FLinearColor(1.f, 0.2f, 0.15f, A);
	}
	else if (C->bLastHitHead)
	{
		Col = FLinearColor(1.f, 0.8f, 0.25f, A);
	}
	else if (C->bLastHitArmor)
	{
		Col = FLinearColor(0.45f, 0.75f, 1.f, A);
	}
	const float In = 8.f + (C->bLastHitKill ? 4.f : 0.f) * (1.f - A);
	const float Out = In + (C->bLastHitKill ? 14.f : 10.f);
	const float T = C->bLastHitKill ? 3.f : 2.f;
	Line(CX - In, CY - In, CX - Out, CY - Out, Col, T);
	Line(CX + In, CY - In, CX + Out, CY - Out, Col, T);
	Line(CX - In, CY + In, CX - Out, CY + Out, Col, T);
	Line(CX + In, CY + In, CX + Out, CY + Out, Col, T);
	if (C->bLastHitKill)
	{
		Text(C->bLastHitHead ? TEXT("HEADSHOT") : TEXT("ELIMINATED"), CX, CY + 40.f, 15.f, Col, ETOAlign::Center);
	}
}

void ATOHUD::DrawDamageIndicators(ATOCharacter* C)
{
	const ATOPlayerController* PC = GetPC();
	if (!PC)
	{
		return;
	}
	const float Now = WorldNow();
	const float Yaw = PC->GetControlRotation().Yaw;
	const float CX = RefW * 0.5f;
	const float CY = RefH * 0.5f;
	for (const FTODamageIndicator& D : C->DamageIndicators)
	{
		const float Age = Now - D.Time;
		if (Age > 2.f || Age < 0.f)
		{
			continue;
		}
		const float A = (1.f - Age / 2.f) * FMath::Clamp(D.Strength, 0.3f, 1.f);
		const float Rel = FRotator::NormalizeAxis(Bearing(C->GetActorLocation(), D.From) - Yaw);
		const float T = Rel / 360.f;
		Ring(CX, CY, 120.f, 134.f, FLinearColor(0.9f, 0.08f, 0.05f, 0.75f * A), T - 0.045f, T + 0.045f, 16);
	}
}

// ---------------------------------------------------------------------------------------------
//  World markers
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawWorldMarkers(ATOCharacter* C)
{
	ATOGameMode* GM = GetGM();
	ATOPlayerController* PC = GetPC();
	if (!GM || !PC)
	{
		return;
	}
	const FVector Eye = C->GetEyeLocation();
	const float Now = WorldNow();
	const bool bWarfare = GM->GetMatchMode() == ETOMatchMode::Warfare;
	FVector2D SP;

	for (const TWeakObjectPtr<ATOCharacter>& Ptr : GM->GetCharacters())
	{
		const ATOCharacter* O = Ptr.Get();
		if (!O || O == C || !O->IsAlive())
		{
			continue;
		}
		const float Dist = FVector::Dist(Eye, O->GetActorLocation()) / 100.f;
		if (O->TeamId == C->TeamId)
		{
			const bool bSquad = O->SquadId == C->SquadId;
			if (!bSquad && (!bWarfare || Dist > 60.f))
			{
				continue;
			}
			if (!ProjectRef(O->GetHeadLocation() + FVector(0.f, 0.f, 45.f), SP))
			{
				continue;
			}
			if (O->IsDowned())
			{
				Disc(SP.X, SP.Y, 11.f, FLinearColor(0.f, 0.f, 0.f, 0.6f), 16);
				Rect(SP.X - 2.f, SP.Y - 7.f, 4.f, 14.f, TOStyle::Warn);
				Rect(SP.X - 7.f, SP.Y - 2.f, 14.f, 4.f, TOStyle::Warn);
				Text(FString::Printf(TEXT("REVIVE %s  %.0fm"), *O->DisplayName, Dist), SP.X, SP.Y + 14.f, 13.f, TOStyle::Warn, ETOAlign::Center);
				continue;
			}
			Diamond(SP.X, SP.Y, 5.f, TOStyle::Friendly);
			if (Dist < 120.f)
			{
				Text(O->DisplayName, SP.X, SP.Y - 24.f, 13.f, TOStyle::Friendly, ETOAlign::Center);
			}
		}
		else if (O->IsRevealed())
		{
			if (ProjectRef(O->GetChestLocation(), SP))
			{
				Diamond(SP.X, SP.Y, 7.f, TOStyle::Enemy.CopyWithNewOpacity(0.85f));
				Text(FString::Printf(TEXT("%.0fm"), Dist), SP.X, SP.Y + 10.f, 12.f, TOStyle::Enemy, ETOAlign::Center);
			}
		}
	}

	// Ping / spotted enemy
	if (Now - PC->PingTime < 12.f)
	{
		const ATOCharacter* Enemy = PC->PingedEnemy.Get();
		const FVector Where = Enemy ? Enemy->GetHeadLocation() + FVector(0.f, 0.f, 40.f) : PC->PingLocation;
		if ((!Enemy || Enemy->IsAlive()) && ProjectRef(Where, SP))
		{
			const float Pulse = 1.f + 0.15f * FMath::Sin(Now * 8.f);
			const FLinearColor Col = Enemy ? TOStyle::Enemy : TOStyle::Money;
			Diamond(SP.X, SP.Y, 9.f * Pulse, FLinearColor(0.f, 0.f, 0.f, 0.6f));
			Diamond(SP.X, SP.Y, 7.f * Pulse, Col);
			Text(FString::Printf(TEXT("%s %.0fm"), Enemy ? TEXT("ENEMY") : TEXT("MARK"), FVector::Dist(Eye, Where) / 100.f), SP.X, SP.Y + 12.f, 13.f, Col, ETOAlign::Center);
		}
	}

	if (!bWarfare)
	{
		for (const TWeakObjectPtr<ATOExtractionZone>& ZP : GM->GetExtractionZones())
		{
			const ATOExtractionZone* Z = ZP.Get();
			if (!Z || !Z->IsAvailable())
			{
				continue;
			}
			const float Dist = FVector::Dist(Eye, Z->GetActorLocation()) / 100.f;
			if (Dist > 900.f || !ProjectRef(Z->GetActorLocation() + FVector(0.f, 0.f, 400.f), SP))
			{
				continue;
			}
			const FLinearColor Col = Z->IsReady() ? TOStyle::Extract : TOStyle::Warn;
			Diamond(SP.X, SP.Y, 6.f, Col.CopyWithNewOpacity(0.85f));
			Text(FString::Printf(TEXT("EXFIL  %.0fm"), Dist), SP.X, SP.Y + 10.f, 12.f, Col.CopyWithNewOpacity(0.85f), ETOAlign::Center);
		}
	}
	else
	{
		for (const TWeakObjectPtr<ATOCapturePoint>& PP : GM->GetCapturePoints())
		{
			const ATOCapturePoint* P = PP.Get();
			if (!P || !P->bActive || !ProjectRef(P->GetActorLocation() + FVector(0.f, 0.f, 600.f), SP))
			{
				continue;
			}
			const FLinearColor Col = TOStyle::TeamColor(P->OwnerTeam, GM->GetPlayerTeam());
			Disc(SP.X, SP.Y, 16.f, FLinearColor(0.f, 0.f, 0.f, 0.55f), 20);
			Disc(SP.X, SP.Y, 13.f, Col.CopyWithNewOpacity(0.85f), 20);
			if (P->Capture > 0.f && P->Capture < 1.f)
			{
				Ring(SP.X, SP.Y, 16.f, 20.f, TOStyle::Text, 0.f, P->Capture, 32);
			}
			Text(P->Label, SP.X, SP.Y - 9.f, 15.f, TOStyle::TextOnAccent, ETOAlign::Center, false);
			Text(FString::Printf(TEXT("%.0fm"), FVector::Dist(Eye, P->GetActorLocation()) / 100.f), SP.X, SP.Y + 20.f, 12.f, Col, ETOAlign::Center);
		}
	}
}

// ---------------------------------------------------------------------------------------------
//  Interaction / extraction / Warfare / downed
// ---------------------------------------------------------------------------------------------

void ATOHUD::DrawInteraction(ATOCharacter* C)
{
	const FString Label = C->GetFocusLabel();
	if (Label.IsEmpty() || C->IsDowned())
	{
		return;
	}
	const float CX = RefW * 0.5f + 60.f;
	const float CY = RefH * 0.5f + 40.f;
	const float P = C->GetInteractProgress();
	Rect(CX - 16.f, CY - 16.f, 32.f, 32.f, FLinearColor(0.f, 0.f, 0.f, 0.6f));
	Frame(CX - 16.f, CY - 16.f, 32.f, 32.f, TOStyle::Text.CopyWithNewOpacity(0.7f));
	Text(TEXT("F"), CX, CY - 10.f, 18.f, TOStyle::Text, ETOAlign::Center);
	if (P > 0.f)
	{
		Ring(CX, CY, 20.f, 24.f, TOStyle::Accent, 0.f, P, 32);
	}
	Text(Label, CX + 26.f, CY - 12.f, 18.f, TOStyle::Text);
	const FString Hint = C->GetFocusHint();
	if (!Hint.IsEmpty())
	{
		Text(Hint, CX + 26.f, CY + 10.f, 13.f, TOStyle::Dim);
	}
}

void ATOHUD::DrawExtraction(ATOCharacter* C)
{
	ATOGameMode* GM = GetGM();
	if (!GM || GM->GetMatchMode() != ETOMatchMode::Operations)
	{
		return;
	}
	const ATOExtractionZone* Z = GM->GetPlayerExtractZone();
	if (!Z)
	{
		return;
	}
	const float W = 460.f;
	const float X = RefW * 0.5f - W * 0.5f;
	const float Y = RefH * 0.5f - 210.f;
	const float P = GM->GetExtractProgress();
	Rect(X, Y, W, 74.f, FLinearColor(0.f, 0.f, 0.f, 0.5f));
	Rect(X, Y, W, 2.f, TOStyle::Extract);
	Text(Z->GetZoneName().ToUpper(), X + W * 0.5f, Y + 8.f, 16.f, TOStyle::Extract, ETOAlign::Center);
	if (P > 0.f)
	{
		const float Left = FMath::Max(0.f, GM->GetExtractDuration() - P);
		Text(FString::Printf(TEXT("EXTRACTING  %.1f"), Left), X + W * 0.5f, Y + 30.f, 22.f, TOStyle::Text, ETOAlign::Center);
		Bar(X + 16.f, Y + 60.f, W - 32.f, 5.f, P / GM->GetExtractDuration(), TOStyle::Extract, 0.5f);
	}
	else
	{
		const FString Reason = GM->GetExtractBlockReason();
		Text(Reason.IsEmpty() ? Z->GetStatusText() : Reason, X + W * 0.5f, Y + 34.f, 16.f, Reason.IsEmpty() ? TOStyle::Text : TOStyle::Danger, ETOAlign::Center);
	}
	if (GM->GetPendingExtractionCost() > 0)
	{
		Text(FString::Printf(TEXT("Extraction fee $%s"), *TOUtil::FormatMoney(GM->GetPendingExtractionCost())), X + W * 0.5f, Y + 80.f, 13.f, TOStyle::Money, ETOAlign::Center);
	}
	(void)C;
}

void ATOHUD::DrawWarfareStatus()
{
	ATOGameMode* GM = GetGM();
	if (!GM)
	{
		return;
	}
	const float CX = RefW * 0.5f;
	const float Y = 92.f;
	const bool bAttacking = GM->GetPlayerTeam() == GM->GetAttackerTeam();
	const float W = 560.f;
	Rect(CX - W * 0.5f, Y, W, 72.f, FLinearColor(0.f, 0.f, 0.f, 0.42f));

	// Tickets
	const float T01 = (float)GM->GetTickets() / FMath::Max(1, GM->GetMaxTickets());
	const FLinearColor AttCol = bAttacking ? TOStyle::Friendly : TOStyle::Enemy;
	Text(bAttacking ? TEXT("ATTACK") : TEXT("DEFEND"), CX - W * 0.5f + 12.f, Y + 6.f, 15.f, bAttacking ? TOStyle::Friendly : TOStyle::Accent);
	Text(FString::Printf(TEXT("SECTOR %d/%d  %s"), GM->GetCurrentSector() + 1, GM->GetNumSectors(), *GM->GetSectorName().ToUpper()), CX + W * 0.5f - 12.f, Y + 6.f, 14.f, TOStyle::Dim, ETOAlign::Right);
	Bar(CX - W * 0.5f + 12.f, Y + 28.f, W - 110.f, 8.f, T01, AttCol, 0.5f);
	Text(FString::Printf(TEXT("%d"), GM->GetTickets()), CX + W * 0.5f - 12.f, Y + 22.f, 18.f, AttCol, ETOAlign::Right);

	// Objectives
	float OX = CX - W * 0.5f + 12.f;
	for (const TWeakObjectPtr<ATOCapturePoint>& PP : GM->GetCapturePoints())
	{
		const ATOCapturePoint* P = PP.Get();
		if (!P || !P->bActive)
		{
			continue;
		}
		const FLinearColor Col = TOStyle::TeamColor(P->OwnerTeam, GM->GetPlayerTeam());
		Rect(OX, Y + 44.f, 22.f, 22.f, Col.CopyWithNewOpacity(0.9f));
		Text(P->Label, OX + 11.f, Y + 46.f, 15.f, TOStyle::TextOnAccent, ETOAlign::Center, false);
		Bar(OX + 28.f, Y + 52.f, 110.f, 6.f, P->Capture, TOStyle::Warn, 0.4f);
		const int32 Mine = GM->GetPlayerTeam() == 0 ? P->CountTeam0 : P->CountTeam1;
		const int32 Theirs = GM->GetPlayerTeam() == 0 ? P->CountTeam1 : P->CountTeam0;
		Text(FString::Printf(TEXT("%d v %d"), Mine, Theirs), OX + 146.f, Y + 47.f, 13.f, TOStyle::Text);
		OX += 230.f;
	}
}

void ATOHUD::DrawDowned(ATOCharacter* C)
{
	const UTOHealthComponent* H = C->GetHealth();
	if (!H)
	{
		return;
	}
	const float Left = FMath::Max(0.f, H->BleedOutTime - H->GetDownedTime());
	const float CY = RefH * 0.5f + 90.f;
	Text(TEXT("YOU ARE DOWNED"), RefW * 0.5f, CY, 34.f, TOStyle::Danger, ETOAlign::Center);
	Bar(RefW * 0.5f - 200.f, CY + 46.f, 400.f, 6.f, Left / FMath::Max(1.f, H->BleedOutTime), TOStyle::Danger, 0.5f);
	Text(FString::Printf(TEXT("Bleeding out in %.0fs  -  wait for a squad mate or hold on"), Left), RefW * 0.5f, CY + 60.f, 15.f, TOStyle::Text, ETOAlign::Center);
	Text(TEXT("[G] GIVE UP"), RefW * 0.5f, CY + 86.f, 15.f, TOStyle::Dim, ETOAlign::Center);
}
