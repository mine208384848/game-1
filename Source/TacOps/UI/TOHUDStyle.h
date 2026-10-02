// TAC-OPS - HUD colour palette (dark tactical panels with a lime accent, Delta Force inspired)

#pragma once

#include "CoreMinimal.h"
#include "Core/TOTypes.h"

namespace TOStyle
{
	inline const FLinearColor Panel(0.018f, 0.022f, 0.026f, 0.82f);
	inline const FLinearColor PanelLight(0.07f, 0.08f, 0.09f, 0.88f);
	inline const FLinearColor Accent(0.78f, 1.0f, 0.24f, 1.f);
	inline const FLinearColor Text(0.93f, 0.95f, 0.96f, 1.f);
	inline const FLinearColor TextOnAccent(0.03f, 0.04f, 0.02f, 1.f);
	inline const FLinearColor Dim(0.58f, 0.62f, 0.65f, 1.f);
	inline const FLinearColor Good(0.35f, 0.95f, 0.45f, 1.f);
	inline const FLinearColor Warn(1.0f, 0.72f, 0.2f, 1.f);
	inline const FLinearColor Danger(1.0f, 0.28f, 0.22f, 1.f);
	inline const FLinearColor Money(1.0f, 0.84f, 0.36f, 1.f);
	inline const FLinearColor Friendly(0.35f, 0.72f, 1.0f, 1.f);
	inline const FLinearColor Enemy(1.0f, 0.3f, 0.25f, 1.f);
	inline const FLinearColor Button(0.09f, 0.10f, 0.11f, 0.92f);
	inline const FLinearColor ButtonHover(0.16f, 0.18f, 0.17f, 0.95f);
	inline const FLinearColor ButtonDisabled(0.06f, 0.06f, 0.065f, 0.75f);
	inline const FLinearColor Health(0.86f, 0.92f, 0.88f, 1.f);
	inline const FLinearColor Armor(0.36f, 0.66f, 1.0f, 1.f);
	inline const FLinearColor Extract(0.3f, 1.0f, 0.55f, 1.f);

	inline FLinearColor TeamColor(int32 Team, int32 PlayerTeam)
	{
		return Team == PlayerTeam ? Friendly : Enemy;
	}
}
