// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "Project_StealthGhostCharacter.generated.h"

// This Enum tracks what the player is doing. 
// It's much cleaner than having 5 different booleans.
UENUM(BlueprintType)
enum class EPlayerMovementState : uint8
{
	VE_Default      UMETA(DisplayName = "Default"),
	VE_Crouching    UMETA(DisplayName = "Crouching"),
	VE_InCover      UMETA(DisplayName = "In Cover")
};

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AProject_StealthGhostCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;


public:

	/** Constructor */
	AProject_StealthGhostCharacter();

	virtual void Tick(float DeltaTime) override;

	virtual void Jump() override;

	// The 'UPROPERTY' macro allows us to see and change this in Blueprints.
	// This supports the "easily extendable" criteria.
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stealth")
	EPlayerMovementState CurrentState;

	// Exposes this array to the editor for easy configuration of patrol routes.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard Patrol")
	TArray<AActor*> PatrolRoute;

	// Attempt to stealth kill
	UFUNCTION(BlueprintCallable, Category = "Stealth Action")
	void TryStealthKill();

	// BP event to play animation and lock the camera
	UFUNCTION(BlueprintImplementableEvent, Category = "Stealth Action")
	void PlayStealthKillAnimation(AActor* TargetGuard);

	// Called when this character is assassinated
	UFUNCTION(BlueprintCallable, Category = "Stealth Action")
	void DieSilently();

	// Death animation to be played
	UPROPERTY(EditAnywhere, Category = "Stealth Animation")
	UAnimMontage* DeathMontage;

	// The animation to play when taking non-lethal damage
	UPROPERTY(EditAnywhere, Category = "Combat Animations")
	UAnimMontage* HitReactionMontage;

	// Let the AnimBP the target is dead
	UPROPERTY(BlueprintReadOnly, Category = "Stealth Action")
	bool bIsDead = false;

	// Checks if a dead body has already triggered an alarm
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Stealth Action")
	bool bHasBeenDiscovered = false;

	// Tracks if the player is currently holding the aim button
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	bool bIsAiming = false;

	// Call this when the weapon slot is selected / equipped
	UFUNCTION(BlueprintCallable, Category = "Animation State")
	void SwitchToArmedAnimState();

	// Call this when the weapon is holstered
	UFUNCTION(BlueprintCallable, Category = "Animation State")
	void SwitchToUnarmedAnimState();

	// Triggered the moment the AI enters full combat mode
	UFUNCTION(BlueprintImplementableEvent, Category = "AI Combat")
	void OnCombatStarted();


protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// This is for the "Splinter Cell" cover logic.
	// UFUNCTION(BlueprintCallable) makes this function appear as a node in Blueprints
	UFUNCTION(BlueprintCallable, Category = "Stealth Action")
	void ToggleCover();

	// Blueprint for crouch button
	UFUNCTION(BlueprintCallable, Category = "Stealth Action")
	void ToggleCrouch();

	// Blueprint for cover transition
	UFUNCTION(BlueprintImplementableEvent, Category = "Stealth Action")
	void SmoothSnapToCover(FVector TargetLocation, FRotator TargetRotation);

	// Blueprints for sprint
	UFUNCTION(BlueprintCallable, Category = "Stealth Action")
	void StartSprint();
	UFUNCTION(BlueprintCallable, Category = "Stealth Action")
	void StopSprint();

	// Memeory variable to prevent player from moving away from cover unwantedly
	FVector LastValidCoverLocation;

	// A simple Raycast function to see if a wall is in front of us.
	bool CanTakeCover(FHitResult& OutHit);

	// this sets the stealth kill range
	UPROPERTY(EditAnywhere, Category = "Stealth Action")
	float StealthKillRange = 150.0f;

	// angle of tolerance for determining if the player is behind the guard
	UPROPERTY(EditAnywhere, Category = "Stealth Action")
	float StealthKillAngleTolerance = 0.4f;

	// Fires a laser from the camera to find interactables
	void CheckForInteractables();

	// Called when the player presses the interact button
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	void Interact();

	// Stores the object we are currently looking at so we can pick it up
	UPROPERTY()
	AActor* CurrentInteractable = nullptr;

	// --- INVENTORY ---
	// Tracks how many stones/distractions we currently hold
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 ThrowableCount = 0;

	// Caches weapon ammo when unequipped. Key = Weapon Class, Value = X(Current), Y(Reserve)
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Inventory")
	TMap<TSubclassOf<class AEquippableBase>, FVector2D> SavedAmmoMap;


	// --- EQUIPMENT SYSTEM ---

	// The Array holding our allowed equipment (e.g., Index 0: Pistol, Index 1: Stone)
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TArray<TSubclassOf<class AEquippableBase>> LoadoutClasses;

	// The actual item currently in use by the player
	UPROPERTY(BlueprintReadOnly, Category = "Equipment")
	class AEquippableBase* CurrentEquipment;

	// Spawns the item from the specified array index and puts it in our hand
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void EquipSlot(int32 SlotIndex);

	// Tracks if the currently equipped item is a gun/weapon that requires armed animations
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Equipment")
	bool bIsHoldingWeapon = false;

	// The animation to play when firing the weapon
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Animation")
	class UAnimMontage* ShootMontage;

	// The animation to play when equipping an item
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Animation")
	class UAnimMontage* EquipMontage;

	// The Animation Blueprint to use when completely unarmed
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation State")
	TSubclassOf<UAnimInstance> UnarmedAnimClass;

	// The Animation Blueprint to use when holding a weapon
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation State")
	TSubclassOf<UAnimInstance> ArmedAnimClass;

	// The animation to play when reloading
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment Animation")
	class UAnimMontage* ReloadMontage;



	// --- EQUIPMENT ACTIONS ---
	UFUNCTION(BlueprintCallable, Category = "Equipment Actions")
	void StartAiming();

	UFUNCTION(BlueprintCallable, Category = "Equipment Actions")
	void StopAiming();

	UFUNCTION(BlueprintCallable, Category = "Equipment Actions")
	void ReleaseWeapon();

	UFUNCTION(BlueprintCallable, Category = "Equipment Actions")
	void FireWeapon();

	UFUNCTION(BlueprintCallable, Category = "Equipment Actions")
	void HolsterEquipment();

	UFUNCTION(BlueprintCallable, Category = "Equipment Actions")
	void ReloadWeapon();

	// Timer to handle player smooth movement in cover
	FTimerHandle CoverUpdateTimer;

	// HEALTH SYSTEM
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Health")
	float CurrentHealth = 100.f;

	// Unreal's native damage override
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, AActor* DamageCauser) override;


protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	virtual void Landed(const FHitResult& Hit) override;

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};

