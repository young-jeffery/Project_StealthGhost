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
	float StealthKillAngleTolerance = 0.3f;


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

