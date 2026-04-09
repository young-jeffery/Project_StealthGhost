// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EquippableBase.h"
#include "WeaponBase.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_STEALTHGHOST_API AWeaponBase : public AEquippableBase
{
	GENERATED_BODY()

public:

	// --- OVERRIDES FROM BASE CLASS ---
	virtual void StartAiming() override;
	virtual void StopAiming() override;
	virtual void UseAction() override; 

	// Returns true if we have bullets to shoot
	bool CanFire() const;

	// Call this to reload
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Reload();

	// Deducts ammo when firing
	void ConsumeAmmo();



protected:
	// --- WEAPON STATS ---
	UPROPERTY(EditDefaultsOnly, Category = "Weapon Stats")
	float WeaponRange = 5000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon Stats")
	float MagnetismRadius = 25.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon Stats")
	float BaseDamage = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon Stats")
	float HeadshotMultiplier = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon Stats")
	bool bIsSilenced = false;

	// How inaccurate the AI is (in degrees). Higher number = more missing.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon Stats - AI")
	float AIWeaponSpread = 3.0f;


	// --- BLUEPRINT VISUAL HOOKS ---
	// C++ calls these, but the Blueprint Editor decides what they actually do!

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon Events")
	void OnWeaponFired();

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon Events")
	void OnAimStateChanged(bool bIsAiming);


	// Ammo variables
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon Ammo")
	int32 MagazineSize = 30;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon Ammo")
	int32 CurrentAmmo = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Weapon Ammo")
	int32 TotalReserveAmmo = 90;

	
};
