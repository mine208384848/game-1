// TAC-OPS - hinged doors, keycard rooms

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/TOTypes.h"
#include "Core/TOInterfaces.h"
#include "TODoor.generated.h"

class UStaticMeshComponent;

UCLASS()
class TACOPS_API ATODoor : public AActor, public ITOInteractable
{
	GENERATED_BODY()

public:
	ATODoor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Width/height in cm; KeyId != None makes it a locked keycard door. */
	void Setup(float InWidth, float InHeight, ETOMat InMat, FName InKeyId);

	// ITOInteractable
	virtual FString GetInteractLabel(const ATOCharacter* User) const override;
	virtual FString GetInteractHint(const ATOCharacter* User) const override;
	virtual bool CanInteract(const ATOCharacter* User) const override { return true; }
	virtual float GetInteractDuration(const ATOCharacter* User) const override;
	virtual void Interact(ATOCharacter* User) override;

	bool IsOpen() const { return bOpen; }
	bool IsLocked() const { return !KeyId.IsNone() && !bUnlocked; }
	void OpenFor(const AActor* By);
	void SetOpen(bool bInOpen, float Direction);
	FName GetKeyId() const { return KeyId; }

protected:
	void Build();

	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USceneComponent> Root;
	UPROPERTY(VisibleAnywhere, Category = "TacOps") TObjectPtr<USceneComponent> Hinge;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Panel;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Reader;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Parts;

	FName KeyId;
	bool bUnlocked = false;
	bool bOpen = false;
	float Angle = 0.f;
	float TargetAngle = 0.f;
	float Width = 110.f;
	float Height = 220.f;
	ETOMat Mat = ETOMat::WoodDark;
	bool bBuilt = false;
};
