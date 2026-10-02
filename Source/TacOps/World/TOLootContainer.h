// TAC-OPS - lootable containers (crates, safes, computers, server racks, bodies, loose items)
//
// Delta Force style searching: opening a container reveals its items one by one; rarer
// items take longer to identify and play a special chime.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TOTypes.h"
#include "Core/TOInterfaces.h"
#include "TOLootContainer.generated.h"

class UStaticMeshComponent;
class UBoxComponent;

UCLASS()
class TACOPS_API ATOLootContainer : public AActor, public ITOInteractable
{
	GENERATED_BODY()

public:
	ATOLootContainer();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Rolls random loot for this container type and zone tier. */
	void InitRandom(ETOContainerType InType, int32 InTier, int32 Seed);
	void InitWithItems(ETOContainerType InType, const FString& InName, const TArray<FTOItemInstance>& Items);

	// ITOInteractable
	virtual FString GetInteractLabel(const ATOCharacter* User) const override;
	virtual void Interact(ATOCharacter* User) override;
	virtual FString GetInteractHint(const ATOCharacter* User) const override;

	/** Called while the loot UI is open. */
	void SetBeingSearched(bool bSearched);
	bool IsFullySearched() const;
	bool HasUnrevealed() const;
	float GetRevealProgress() const;

	FTOGrid& GetGrid() { return Grid; }
	const FTOGrid& GetGrid() const { return Grid; }
	ETOContainerType GetContainerType() const { return Type; }
	FString GetDisplayName() const { return DisplayName; }
	int32 GetTier() const { return Tier; }
	bool IsEmpty() const { return Grid.Items.Num() == 0; }
	bool WasOpened() const { return bOpened; }

	/** Revision counter for UI refresh. */
	int32 Revision = 0;

protected:
	void BuildVisual();
	void RevealNext();

	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<UBoxComponent> InteractBox;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;
	UPROPERTY() FTOGrid Grid;

	ETOContainerType Type = ETOContainerType::Duffel;
	int32 Tier = 0;
	FString DisplayName;
	bool bBeingSearched = false;
	bool bOpened = false;
	float NextRevealTime = 0.f;
	float RevealStart = 0.f;
	float RevealDuration = 0.f;
	bool bVisualBuilt = false;
};
