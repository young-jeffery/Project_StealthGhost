// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EquippableBase.h"
#include "ThrowableEquipment.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_STEALTHGHOST_API AThrowableEquipment : public AEquippableBase
{
	GENERATED_BODY()

public:
	AThrowableEquipment();

	// Tick is used to draw the aiming arc every frame while holding aim
	virtual void Tick(float DeltaTime) override;

	// --- OVERRIDES FROM THE BASE CLASS ---
	virtual void StartAiming() override;
	virtual void StopAiming() override;
	virtual void ReleaseAction() override; // Release for throwing, not Use!


protected:
	// What actual item are we throwing?
	UPROPERTY(EditDefaultsOnly, Category = "Throwing")
	TSubclassOf<class AThrowableBase> ThrowableClass;

	// How fast the item is launched
	UPROPERTY(EditDefaultsOnly, Category = "Throwing")
	float ThrowVelocity = 1500.0f;

	// Are we currently holding the aim button?
	bool bIsAiming = false;
	
};
