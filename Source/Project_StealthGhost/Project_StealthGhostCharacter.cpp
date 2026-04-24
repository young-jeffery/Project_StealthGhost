// Copyright Epic Games, Inc. All Rights Reserved.

#include "Project_StealthGhostCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Engine/OverlapResult.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Project_StealthGhost.h"
#include "DrawDebugHelpers.h"
#include "InteractableInterface.h"
#include <Perception/AISense_Hearing.h>
#include "EquippableBase.h"
#include "ThrowableEquipment.h"
#include "TimerManager.h"
#include "WeaponBase.h"
#include "GhostAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"

// Forward declaration — NotifyNearbyGuardsOfDeath is defined later in this file, but TakeDamage needs to call it
static void NotifyNearbyGuardsOfDeath(AProject_StealthGhostCharacter* DeadChar);


AProject_StealthGhostCharacter::AProject_StealthGhostCharacter()
{
	// this enables tick function
	PrimaryActorTick.bCanEverTick = true;

	// this allows the character to be able to crouch
	GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch = true;

	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 300.0f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AProject_StealthGhostCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AProject_StealthGhostCharacter::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AProject_StealthGhostCharacter::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AProject_StealthGhostCharacter::Look);
	}
	else
	{
		UE_LOG(LogProject_StealthGhost, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}

	// Called to bind functionality to input
	Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AProject_StealthGhostCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// Are we in cover?
		if (CurrentState == EPlayerMovementState::VE_InCover)
		{
			// Get the right vector of the wall we are currently touching
			FVector WallRight = FVector::CrossProduct(FVector::UpVector, CurrentWallNormal).GetSafeNormal();

			// Check the raw Y-Axis 
			// -1.0 means they are pulling perfectly straight back
			if (MovementVector.Y < -0.5f)
			{
				// They are pulling back, start the stopwatch
				CurrentBreakawayTime += GetWorld()->GetDeltaSeconds();

				if (CurrentBreakawayTime >= BreakawayHoldTime)
				{
					CurrentBreakawayTime = 0.0f;
					ToggleCover();
					return;
				}
			}
			else
			{
				// They stopped pulling back, reset the stopwatch
				CurrentBreakawayTime = 0.0f;
			}

			// 
			float IntendedMovement = MovementVector.X * -1.0f;

			// If the player is trying to move, check if we are about to hit a corner or reach the end of the wall
			if (FMath::Abs(IntendedMovement) > 0.05f)
			{
				FVector StartLoc = GetActorLocation();
				// Calculate a point slightly ahead of where we want to move
				FVector FeelerOffset = GetActorRightVector() * FMath::Sign(IntendedMovement) * CoverFeelerDistance;
				FVector FeelerStart = StartLoc + FeelerOffset;
				FVector FeelerEnd = FeelerStart - (GetActorForwardVector() * 100.0f);

				FHitResult FeelerHit;
				FCollisionQueryParams Params;
				Params.AddIgnoredActor(this);

				bool bFeelerHit = GetWorld()->LineTraceSingleByChannel(FeelerHit, FeelerStart, FeelerEnd, ECC_Visibility, Params);

				if (bFeelerHit)
				{
					// We hit wall ahead of us. Check if it's a smooth curve or a sharp corner
					FVector NextWallNormal = FeelerHit.Normal;
					NextWallNormal.Z = 0.0f;

					// Compare the upcoming wall to the wall we are currently touching
					float CornerDotProduct = FVector::DotProduct(NextWallNormal, CurrentWallNormal);

					// If the dot product is lower than our threshold, it's a sharp corner so don't move
					if (CornerDotProduct < CoverCornerThreshold)
					{
						IntendedMovement = 0.0f;
					}
				}
				else
				{
					// The feeler hit nothing. We reached the end of the mesh. Don't move
					IntendedMovement = 0.0f;
				}
			}

			// Apply whatever movement survived the checks
			AddMovementInput(GetActorRightVector(), IntendedMovement);
		}
		else {
			
			// route the input
			DoMove(MovementVector.X, MovementVector.Y);
		}
	}	
}

void AProject_StealthGhostCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AProject_StealthGhostCharacter::DoMove(float Right, float Forward)
{
	if (GetController() != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AProject_StealthGhostCharacter::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AProject_StealthGhostCharacter::Jump()
{
	// If we are in cover, completely ignore the jump command
	if (CurrentState == EPlayerMovementState::VE_InCover) return;

	// Otherwise, do the normal Unreal jump
	Super::Jump();
}

void AProject_StealthGhostCharacter::Landed(const FHitResult& Hit)
{
	// declaring this first ensures we allow the default landing physics take place
	Super::Landed(Hit);

	UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 0.5f, this, 1200.0f, FName("Landing noise"));
}

void AProject_StealthGhostCharacter::DoJumpStart()
{
	// If character is in cover, don't jump
	if (CurrentState == EPlayerMovementState::VE_InCover) return;

	// signal the character to jump
	Super::Jump();
}

void AProject_StealthGhostCharacter::DoJumpEnd()
{
	// signal the character to stop jumping
	StopJumping();
}

// My additions

// Wall detection logic
bool AProject_StealthGhostCharacter::CanTakeCover(FHitResult& OutHit)
{
	// If the character is in the air then the cover logic should fail
	if (GetCharacterMovement()->IsFalling()) return false;

	// The size of our detection "beach ball" (in cm)
	float SphereRadius = 150.0f;
	// Reach of the Sphere
	float SweepDistance = 150.0f;
	
	FVector StartLocation = GetActorLocation();
	// We trace just a tiny bit forward, but rely on the sphere's width to do the work.
	FVector EndLocation = StartLocation + (GetActorForwardVector() * SweepDistance);

	FHitResult HitResult;
	FCollisionQueryParams CollisionParams;
	CollisionParams.AddIgnoredActor(this);

	// Create the shape we want to cast
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);

	TArray<FHitResult> HitResults;

	// Perform the Sweep (Shoots the sphere out)
	bool bHit = GetWorld()->SweepMultiByChannel(
		HitResults,
		StartLocation,
		EndLocation,
		FQuat::Identity, // No rotation needed for a sphere
		ECC_Visibility,
		SphereShape,
		CollisionParams
	);

	// This checks waht we hit and determines if a wall was hit
	// FMath::Abs gets the absolute value. If Z is close to 0, it's a vertical wall.
	if (bHit)
	{
		for (const FHitResult& Hit : HitResults)
		{
			if (FMath::Abs(Hit.Normal.Z) < 0.2f)
			{
				OutHit = Hit;
				// Draw a green dot exactly where the wall is
				DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 45.0f, 12, FColor::Green, false, 2.0f);
				return true;
			}
		}
	}

	// If no wall was hit then return false. The first line makes the debug sphere red
	DrawDebugSphere(GetWorld(), StartLocation, SphereRadius, 12, FColor::Red, false, 2.0f);
	return false;
}

// Toggle Cover logic
void AProject_StealthGhostCharacter::ToggleCover()
{
	// If we are already in cover, pressing the button takes us out of cover.
	if (CurrentState == EPlayerMovementState::VE_InCover)
	{
		CurrentState = EPlayerMovementState::VE_Default;
		GetCharacterMovement()->bOrientRotationToMovement = true;

		// Stop cover update timer when leaving cover
		//GetWorldTimerManager().ClearTimer(CoverUpdateTimer);
	}
	// If we are NOT in cover, check if there is a wall in front of us.
	else
	{
		FHitResult HitResult;

		// If a wall is hit, this fills HitResult with the walls data
		if (CanTakeCover(HitResult))
		{
			// This just set the character to the InCover state
			CurrentState = EPlayerMovementState::VE_InCover;

			// Memorize the exact wall angle the moment we touch it
			CurrentWallNormal = HitResult.Normal;

			GetCharacterMovement()->bOrientRotationToMovement = false;

			// Auto crouch when getting into cover
			Crouch();

			// This is for the player's rotation
			// The normal points away from the wall, so the character is rotated to face the normal so their back is to the wall
			FRotator WallRotation = HitResult.Normal.Rotation();
			//SetActorRotation(WallRotation);
			// This moves the player slightly outrward to avoid clipping with the wall
			FVector CoverLocation = HitResult.ImpactPoint + (HitResult.Normal * 40.0f);
			// Keeps the players Z height to prevent sinking into the floor
			CoverLocation.Z = GetActorLocation().Z;

			//SetActorLocation(CoverLocation);

			LastValidCoverLocation = CoverLocation;

			SmoothSnapToCover(CoverLocation, WallRotation);
		}
	}
}



void AProject_StealthGhostCharacter::Tick(float DeltaTime)
{
	// Constantly check what the camera is pointing at
	CheckForInteractables();

	Super::Tick(DeltaTime);

	if (CurrentState == EPlayerMovementState::VE_InCover)
	{
		FVector StartLocation = GetActorLocation();

		// We need to find the right vector of the wall so we can shoot traces from the shoulders
		FVector WallRight = FVector::CrossProduct(FVector::UpVector, CurrentWallNormal).GetSafeNormal();

		// We shoot the traces backward against the wall's normal
		FVector TraceDirection = CurrentWallNormal * -80.0f;

		// Setup our 3 points: Center, Left Shoulder, Right Shoulder
		FVector RightShoulder = StartLocation + (WallRight * 35.0f);
		FVector LeftShoulder = StartLocation - (WallRight * 35.0f);

		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActor(this);

		FHitResult CenterHit, RightHit, LeftHit;
		bool bHitCenter = GetWorld()->LineTraceSingleByChannel(CenterHit, StartLocation, StartLocation + TraceDirection, ECC_Visibility, CollisionParams);
		bool bHitRight = GetWorld()->LineTraceSingleByChannel(RightHit, RightShoulder, RightShoulder + TraceDirection, ECC_Visibility, CollisionParams);
		bool bHitLeft = GetWorld()->LineTraceSingleByChannel(LeftHit, LeftShoulder, LeftShoulder + TraceDirection, ECC_Visibility, CollisionParams);

		if (bHitCenter || bHitRight || bHitLeft)
		{
			LastValidCoverLocation = GetActorLocation();

			// Use the shoulders to average out bumps
			FVector TargetNormal = CurrentWallNormal; // Default to what we had last frame

			if (bHitRight && bHitLeft) { TargetNormal = (RightHit.Normal + LeftHit.Normal).GetSafeNormal(); }
			else if (bHitRight) { TargetNormal = RightHit.Normal; }
			else if (bHitLeft) { TargetNormal = LeftHit.Normal; }
			else if (bHitCenter) { TargetNormal = CenterHit.Normal; } // Fallback

			TargetNormal.Z = 0.0f; // Keep it flat
			CurrentWallNormal = TargetNormal; // Save it

			// Smoothly apply the rotation
			FRotator TargetRotation = CurrentWallNormal.Rotation();
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, 10.0f));

			// Use the Center Hit for our distance to the wall
			if (bHitCenter)
			{
				FVector TargetLocation = CenterHit.ImpactPoint + (CurrentWallNormal * 40.0f);
				FVector CurrentLocation = GetActorLocation();

				// Interpolate X and Y. Because we used the center chest hit, this adjusts 
				// depth and will not pull the character laterally
				FVector2D SmoothXY = FMath::Vector2DInterpTo(FVector2D(CurrentLocation.X, CurrentLocation.Y), FVector2D(TargetLocation.X, TargetLocation.Y), DeltaTime, 10.0f);
				SetActorLocation(FVector(SmoothXY.X, SmoothXY.Y, CurrentLocation.Z));
			}
		}
		else
		{
			// We fell off the wall completely
			SetActorLocation(LastValidCoverLocation);
			GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}
	}

	//if (CurrentState == EPlayerMovementState::VE_InCover)
	//{
	//	FVector StartLocation = GetActorLocation();
	//	FVector ForwardOffset = GetActorForwardVector() * 80.0f;

	//	// Sphere size
	//	float SphereRadius = 35.0f;

	//	FCollisionQueryParams CollisionParams;
	//	CollisionParams.AddIgnoredActor(this);
	//	FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);

	//	TArray<FHitResult> HitResults;

	//	bool bHit = GetWorld()->SweepMultiByChannel(HitResults, StartLocation, EndLocation, FQuat::Identity, ECC_Visibility, SphereShape, CollisionParams);

	//	bool bFoundWall = false;

	//	if (bHit)
	//	{
	//		for (const FHitResult& Hit : HitResults)
	//		{
	//			// checks if this is a vertical wall
	//			if (FMath::Abs(Hit.Normal.Z) < 0.2f)
	//			{
	//				bFoundWall = true;
	//				// Save the current location
	//				LastValidCoverLocation = GetActorLocation();

	//				// smoothly roatate the character to match the wall's new curve
	//				FRotator TargetRotation = Hit.Normal.Rotation();
	//				FRotator SmoothRotation = FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, 10.0f);
	//				SetActorRotation(SmoothRotation);

	//				// calculate a constant distance between the player and the wall
	//				FVector HugWallLocation = Hit.ImpactPoint + (Hit.Normal * 40.0f);
	//				// Get our current location
	//				FVector CurrentLocation = GetActorLocation();

	//				// Isolate X and Y axis from Z to avoid falling bug
	//				FVector2D CurrentXY(CurrentLocation.X, CurrentLocation.Y);
	//				FVector2D TargetXY(HugWallLocation.X, HugWallLocation.Y);

	//				// This interpolates only the 2D plane
	//				FVector2D SmoothXY = FMath::Vector2DInterpTo(CurrentXY, TargetXY, DeltaTime, 10.0f);

	//				// Combine the new X and Y with the Z we didn't touch
	//				FVector SmoothLocation(SmoothXY.X, SmoothXY.Y, CurrentLocation.Z);

	//				SetActorLocation(SmoothLocation);

	//				break; // stop loop once a wall is found
	//			}
	//		}
	//	}
	//	// if no wall is found after looping
	//	if (!bFoundWall)
	//	{
	//		SetActorLocation(LastValidCoverLocation);
	//		GetCharacterMovement()->Velocity = FVector::ZeroVector;
	//	}
	//}

	// Footstep Logic

	// Are we moving on the ground?
	if (GetVelocity().SizeSquared() > 0.0f && GetCharacterMovement()->IsMovingOnGround())
	{
		// No sound when crouching or in cover
		if (CurrentState == EPlayerMovementState::VE_Crouching || CurrentState == EPlayerMovementState::VE_InCover)
		{
			AccumulatedStepDistance = 0.0f;
		}
		else
		{
			// We are walking or sprinting. Add the distance moved.
			AccumulatedStepDistance += GetVelocity().Size() * DeltaTime;

			// Check if we are sprinting
			bool bIsSprinting = GetCharacterMovement()->MaxWalkSpeed > 500.0f;

			// Sprinting means longer strides, so we increase the required distance before a step triggers
			float CurrentRequiredDistance = bIsSprinting ? (BaseStepDistance * 1.3f) : BaseStepDistance;

			// Have we moved far enough to trigger a step?
			if (AccumulatedStepDistance >= CurrentRequiredDistance)
			{
				if (FootstepSound)
				{
					UGameplayStatics::PlaySoundAtLocation(GetWorld(), FootstepSound, GetActorLocation());
				}

				// Set exact volumes and ranges based on movement state
				float Loudness = bIsSprinting ? 0.7f : 0.5f;
				float Range = bIsSprinting ? 1800.0f : 500.0f;

				// Send the isolated "Footstep" tag to the AI
				UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), Loudness, this, Range, FName("Footstep"));

				// Reset distance
				AccumulatedStepDistance -= CurrentRequiredDistance;
			}
		}
	}
	else
	{
		// We stopped moving or jumped. Reset the counter.
		AccumulatedStepDistance = 0.0f;
	}
}

// Crouch Logic
void AProject_StealthGhostCharacter::ToggleCrouch()
{
	// This prevents tampering with InCover logic
	if (CurrentState == EPlayerMovementState::VE_InCover) return;

	// bIsCrouched is a built in variable that checks if crouch is active or not
	if (bIsCrouched)
	{
		UnCrouch();
		CurrentState = EPlayerMovementState::VE_Default;
	}
	else
	{
		Crouch();
		CurrentState = EPlayerMovementState::VE_Crouching;
	}
}

void AProject_StealthGhostCharacter::StartSprint()
{
	if (CurrentState == EPlayerMovementState::VE_InCover)
	{
		return;
	}
	else if (bIsCrouched)
	{
		UnCrouch();
		CurrentState = EPlayerMovementState::VE_Default;
		GetCharacterMovement()->MaxWalkSpeed = 600.0f;
		return;
	}

	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
}

void AProject_StealthGhostCharacter::StopSprint()
{
	GetCharacterMovement()->MaxWalkSpeed = 300.0f;
}

// Attempt Kill Logic
void AProject_StealthGhostCharacter::TryStealthKill()
{
	// 1. Ask our new scanner if there is a valid target in front of us
	if (AProject_StealthGhostCharacter* TargetGuard = CheckForStealthKillTarget())
	{
		// 2. We got a green light! Execute the kill.
		TargetGuard->DieSilently();
		PlayStealthKillAnimation(TargetGuard);

		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("Kill Successful"));

		// Roll for stealth kill health restore
		if (FMath::FRand() <= StealthKillHealChance)
		{
			RestoreHealth(StealthKillHealAmount, TEXT("Stealth Kill"));
		}
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Kill Failed. No valid target."));
	}
}

// Checks if we can stealth kill
AProject_StealthGhostCharacter* AProject_StealthGhostCharacter::CheckForStealthKillTarget()
{
	// Send a small sphere in front of the player
	FVector StartLoc = GetActorLocation();
	FVector EndLoc = StartLoc + (GetActorForwardVector() * StealthKillRange);

	// Creating the sphere and ignoring hits on the player
	FHitResult HitResult;
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(50.0f);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);


	// look for guards
	bool bHit = GetWorld()->SweepSingleByObjectType(HitResult, StartLoc, EndLoc, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), SphereShape, QueryParams);

	if (bHit && HitResult.GetActor())
	{
		AActor* TargetGuard = HitResult.GetActor();

		// Math to check if player is behind the guard using Dot Product
		FVector PlayerForward = GetActorForwardVector();
		FVector GuardForward = TargetGuard->GetActorForwardVector();

		// 1 means they are facing the same way, -1 means facing each other
		float FacingAlignment = FVector::DotProduct(PlayerForward, GuardForward);

		// Cast to specific target class
		if (AProject_StealthGhostCharacter* Guard = Cast<AProject_StealthGhostCharacter>(TargetGuard))
		{
			if (Guard->bIsDead)
			{
				return nullptr; // Guard is dead, return nothing
			}

			// check if alignment is greater than tolerance angle
			if (FacingAlignment > StealthKillAngleTolerance)
			{
				return Guard; // SUCCESS! We are perfectly behind a living guard.
			}
		}
	}
	return nullptr; // No valid guard found
}

// Enemy Death Logic
void AProject_StealthGhostCharacter::DieSilently()
{
	// Roll chance for stealth kill loot
	if (FMath::FRand() <= AmmoDropChance)
	{
		SpawnDroppedAmmo(1); // Normal kill yields 1 ammo
	}

	bIsDead = true; // tell the state machine that this character is dead

	// Notify nearby guards that another guard has died
	if (!IsPlayerControlled())
	{
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 1.0f, this, 700.0f, FName("GuardDown"));
	}

	// Kill the guards movement
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCharacterMovement()->SetAvoidanceEnabled(false);

	AController* VictimController = Controller;

	// Server AI controller from the guard
	if (VictimController)
	{
		// Stop the AI brain from running any logic on the guard
		VictimController->UnPossess();

		// Destroy the controller to free up resources.
		VictimController->Destroy();
	}

	// Turn off collision capsule 
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Keep mesh collision. (Might use physics ragdoll)
	GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));

	// Change the actor's tag if it's used for any 
	Tags.Add(FName("Dead Body"));
	Tags.Remove(FName("Guard"));

	// Disable the actor's Tick
	SetActorTickEnabled(false);

	// Play the death animation
	if (DeathMontage && GetMesh()->GetAnimInstance())
	{
		GetMesh()->GetAnimInstance()->Montage_Play(DeathMontage);
	}
}

// --- INTERACTION SYSTEM ---

void AProject_StealthGhostCharacter::CheckForInteractables()
{
	// Only run this if we are the actual player.
	if (!IsPlayerControlled())
	{
		CurrentInteractable = nullptr;
		return;
	}

	// Set the Origin to the Character, but the Direction to the Camera
	FVector PlayerLoc = GetActorLocation();
	FVector CamForward = FollowCamera->GetForwardVector();

	// How far the player can reach (e.g., 200 cm / 2 meters)
	float ReachRadius = 200.0f;
	// The width of our interaction cone. 90 degrees = a full 180 degree half-circle in front of us.
	float MaxAngleDegrees = 90.0f;

	// Grab EVERYTHING within our reach
	TArray<FOverlapResult> OverlapResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bool bOverlap = GetWorld()->OverlapMultiByChannel(
		OverlapResults,
		PlayerLoc,
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(ReachRadius),
		QueryParams
	);

	AActor* BestInteractable = nullptr;
	float BestDotProduct = -1.0f; // Tracks which item is closest to the center of the screen

	if (bOverlap)
	{
		for (const FOverlapResult& Result : OverlapResults)
		{
			AActor* HitActor = Result.GetActor();

			// Does it have the interface and is it alive?
			if (IsValid(HitActor) && HitActor->Implements<UInteractableInterface>())
			{
				// Get the direction pointing from the player to the item
				FVector DirectionToItem = (HitActor->GetActorLocation() - PlayerLoc).GetSafeNormal();

				// Compares the Camera direction to the Item direction
				// 1.0 = Dead center. 0.0 = 90 degrees to the side. -1.0 = Directly behind the camera.
				float DotProduct = FVector::DotProduct(CamForward, DirectionToItem);

				// Convert Max Angle into a Dot Product threshold
				float AngleThreshold = FMath::Cos(FMath::DegreesToRadians(MaxAngleDegrees));

				// If the item is inside our 180-degree cone...
				if (DotProduct >= AngleThreshold)
				{
					// ...and if it is closer to the center of our screen than the last item we checked...
					if (DotProduct > BestDotProduct)
					{
						BestDotProduct = DotProduct;
						BestInteractable = HitActor; // Make it our new target!
					}
				}
			}
		}
	}

	// Update our memory
	if (BestInteractable != CurrentInteractable)
	{
		// 1. We looked away! Tell the OLD item to hide its UI.
		if (CurrentInteractable)
		{
			IInteractableInterface::Execute_HidePrompt(CurrentInteractable);
		}

		// 2. Update our memory to the new item (this might be an item, or it might be nullptr if we looked at nothing)
		CurrentInteractable = BestInteractable;

		// 3. We looked at something new! Tell the NEW item to show its UI.
		if (CurrentInteractable)
		{
			IInteractableInterface::Execute_ShowPrompt(CurrentInteractable);
		}
	}
}

void AProject_StealthGhostCharacter::Interact()
{
	// If we are looking at something that has the interface...
	if (CurrentInteractable)
	{
		// Pass the Execute_Interact command at the object and the object decides what to do with it
		IInteractableInterface::Execute_Interact(CurrentInteractable, this);

		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, TEXT("Player: Sent Interaction Command!"));
	}
}

// --- EQUIPMENT SYSTEM ---

// This is called when the player presses the button to equip an item
void AProject_StealthGhostCharacter::EquipSlot(int32 SlotIndex)
{
	// Ensure the index exist as a valid slot
	if (LoadoutClasses.IsValidIndex(SlotIndex))
	{
		// Get the class from that slot
		TSubclassOf<AEquippableBase> SelectedClass = LoadoutClasses[SlotIndex];

		// If the slot is empty (None), or we selected what we are already holding, holster and stop.
		if (!SelectedClass || (CurrentEquipment && CurrentEquipment->IsA(SelectedClass)))
		{
			// ONly works for the player to prevent AI from accidentally holstering themselves when trying to equip
			if (IsPlayerControlled())
			{
				HolsterEquipment();
			}

			return;
		}

		// Put away whatever we are currently holding
		HolsterEquipment();

		// Special check: If it's a throwable, do we actually have ammo for it?
		if (SelectedClass->IsChildOf(AThrowableEquipment::StaticClass()) && ThrowableCount <= 0)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("No stones left!"));
			return; // Cancel the equip
		}

		// Spawn and attach the equipment
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;

		// Spawn the equipment actor
		CurrentEquipment = GetWorld()->SpawnActor<AEquippableBase>(SelectedClass, GetActorLocation(), GetActorRotation(), SpawnParams);

		if (CurrentEquipment)
		{
			// If it's a gun, we check if we have saved ammo for it and restore it
			if (AWeaponBase* Weapon = Cast<AWeaponBase>(CurrentEquipment))
			{
				if (SavedAmmoMap.Contains(SelectedClass))
				{
					FVector2D SavedAmmo = SavedAmmoMap[SelectedClass];
					Weapon->SetAmmo(SavedAmmo.X, SavedAmmo.Y);
				}
			}

			// Attach it to the set socket
			CurrentEquipment->Equip(GetMesh(), CurrentEquipment->AttachmentSocketName);

			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("Equipped Item"));

			// If the item is a gun, play the montage
			if (Cast<AWeaponBase>(CurrentEquipment))
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("Equipped Gun"));

				// Switch to the armed anim if holding a gun
				SwitchToArmedAnimState();
			}
			else
			{
				// Ensure we stay or switch the unarmed state for throwables
				SwitchToUnarmedAnimState();
			}
		}
	}
}

// This is called when the player holds down the button to aim their throwable item
void AProject_StealthGhostCharacter::StartAiming()
{
	bIsAiming = true;

	if (CurrentEquipment)
	{
		CurrentEquipment->StartAiming();
	}
}

// This is called when the player releases the button to stop aiming their throwable item
void AProject_StealthGhostCharacter::StopAiming()
{
	bIsAiming = false;

	if (CurrentEquipment)
	{
		CurrentEquipment->StopAiming();
	}
}

// This is called when the player releases the button to throw their throwable item
void AProject_StealthGhostCharacter::ReleaseWeapon()
{
	if (CurrentEquipment)
	{
		if (!bIsAiming)
		{
			return; // Must be aiming to release
		}

		// Tell the item to do its release action
		CurrentEquipment->ReleaseAction();

		// Safely check if the thing we just used was a throwable item
		if (Cast<AThrowableEquipment>(CurrentEquipment))
		{
			ThrowableCount--;
			
			bIsAiming = false; // Stop aiming after release

			if (ThrowableCount <= 0)
			{
				HolsterEquipment();
			}

		}
	}
}

// This is called when the player presses the button to holster their currently equipped item without using it
void AProject_StealthGhostCharacter::HolsterEquipment()
{
	if (CurrentEquipment)
	{
		// If it's a gun, we save the ammo count before holstering
		if (AWeaponBase* Weapon = Cast<AWeaponBase>(CurrentEquipment))
		{
			// We store CurrentAmmo in X, and ReserveAmmo in Y
			SavedAmmoMap.Add(Weapon->GetClass(), FVector2D(Weapon->GetCurrentAmmo(), Weapon->GetReserveAmmo()));
		}

		// Set the variable to false
		bIsHoldingWeapon = false;

		// Cancel aiming just in case
		CurrentEquipment->StopAiming();

		// Destroy it without decreasing the inventory count
		CurrentEquipment->Unequip();
		CurrentEquipment = nullptr;

		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Equipment Holstered!"));
	}

	// Switch to the unarmed animation state when holstering
	SwitchToUnarmedAnimState();
}

void AProject_StealthGhostCharacter::FireWeapon()
{
	// If the reload montage is currently playing, abort the fire command immediately!
	if (ReloadMontage && GetMesh()->GetAnimInstance())
	{
		if (GetMesh()->GetAnimInstance()->Montage_IsPlaying(ReloadMontage))
		{
			return;
		}
	}


	if (IsPlayerControlled())
	{
		// Must be aiming to fire
		if (!bIsAiming)
		{
			return;
		}
		// If we are crouching, we must be stationary to shoot
		if (CurrentState == EPlayerMovementState::VE_Crouching || CurrentState == EPlayerMovementState::VE_InCover)
		{
			// We check if the velocity length is greater than a tiny number (0.1f) 
			if (GetVelocity().Length() > 0.1f)
			{
				GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("Must be stationary to shoot while crouching!"));
				return;
			}
		}
	}

	// If we survived the checks above, we are allowed to act!
	if (CurrentEquipment)
	{
		// If it is a gun, check the ammo!
		if (AWeaponBase* Weapon = Cast<AWeaponBase>(CurrentEquipment))
		{
			if (!Weapon->CanFire())
			{
				// Gun is empty! Trigger the reload reflex instead of shooting.
				ReloadWeapon();
				return;
			}
		}

		// If it's a throwable or a gun with bullets, use it normally.
		CurrentEquipment->UseAction();
	}
}

void AProject_StealthGhostCharacter::SwitchToArmedAnimState()
{
	// Verify the character is actually holding a WeaponBase
	AWeaponBase* EquippedGun = Cast<AWeaponBase>(CurrentEquipment);

	// If the cast fails abort!
	if (!EquippedGun)
	{
		return;
	}

	// Delay the swap until the very next frame to prevent animation evaluation crashes
	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
		{
			USkeletalMeshComponent* PlayerMesh = GetMesh();

			// Ensure we have a mesh and the Armed class is assigned in the editor
			if (PlayerMesh && ArmedAnimClass)
			{
				// Only swap if we aren't already using it (prevents unnecessary resets)
				if (PlayerMesh->GetAnimInstance()->GetClass() != ArmedAnimClass)
				{
					PlayerMesh->SetAnimInstanceClass(ArmedAnimClass);
				}
			}
		});
}

void AProject_StealthGhostCharacter::SwitchToUnarmedAnimState()
{
	// Delay the swap until the very next frame to prevent animation evaluation crashes
	GetWorld()->GetTimerManager().SetTimerForNextTick([this]()
		{
			USkeletalMeshComponent* PlayerMesh = GetMesh();

			// Ensure we have a mesh and the Unarmed class is assigned in the editor
			if (PlayerMesh && UnarmedAnimClass)
			{
				// Only swap if we aren't already using it
				if (PlayerMesh->GetAnimInstance()->GetClass() != UnarmedAnimClass)
				{
					PlayerMesh->SetAnimInstanceClass(UnarmedAnimClass);
				}
			}
		});
}

// Scans nearby guards and directly notifies any that have LOS to this character.
// Used on death because the UE5 sight system doesn't re-fire for bIsDead state changes
// on actors already within an active vision cone.
static void NotifyNearbyGuardsOfDeath(AProject_StealthGhostCharacter* DeadChar)
{
	if (!DeadChar || !DeadChar->GetWorld()) return;

	// Gather all pawns within a generous radius around the dead guard
	TArray<AActor*> NearbyActors;
	UGameplayStatics::GetAllActorsOfClass(DeadChar->GetWorld(), AProject_StealthGhostCharacter::StaticClass(), NearbyActors);

	FVector DeadLocation = DeadChar->GetActorLocation();
	const float CheckRadius = 3000.0f; // 30 meters

	for (AActor* Actor : NearbyActors)
	{
		AProject_StealthGhostCharacter* NearbyChar = Cast<AProject_StealthGhostCharacter>(Actor);
		if (!NearbyChar || NearbyChar == DeadChar) continue;
		if (NearbyChar->bIsDead) continue; // Dead guards don't react
		if (!NearbyChar->IsPlayerControlled() == false) continue; // Only AI guards

		AGhostAIController* AICon = Cast<AGhostAIController>(NearbyChar->GetController());
		if (!AICon || !AICon->GetPawn()) continue;

		// Range check
		float Dist = FVector::Dist(NearbyChar->GetActorLocation(), DeadLocation);
		if (Dist > CheckRadius) continue;

		// did this guard actually have a clear view of the dying guard?
		FVector GuardEyes = NearbyChar->GetActorLocation() + FVector(0, 0, 70.0f);
		FHitResult LOSHit;
		FCollisionQueryParams LOSParams;
		LOSParams.AddIgnoredActor(NearbyChar);
		LOSParams.AddIgnoredActor(DeadChar);

		bool bHit = DeadChar->GetWorld()->LineTraceSingleByChannel(LOSHit, GuardEyes, DeadLocation, ECC_Visibility, LOSParams);

		if (!bHit)
		{
			// Clear LOS to the dead guard — they witnessed it, notify directly
			AICon->OnWitnessedGuardDeath(DeadChar);
		}
	}
}

// Restores health to the character
void AProject_StealthGhostCharacter::RestoreHealth(float Amount, const FString& Source)
{
	if (bIsDead || Amount <= 0.f) return;

	float OldHealth = CurrentHealth;
	CurrentHealth = FMath::Clamp(CurrentHealth + Amount, 0.f, MaxHealth);
	float ActualGain = CurrentHealth - OldHealth;

	if (GEngine && ActualGain > 0.f)
	{
		GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
			FString::Printf(TEXT("+%.0f HP (%s)"), ActualGain, *Source));
	}
}

// Damage Logic
float AProject_StealthGhostCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// If the person who shot this character is NOT a player controller, ignore the damage entirely!
	if (!IsPlayerControlled() && EventInstigator && !EventInstigator->IsPlayerController())
	{
		return 0.f;
	}

	// Call the parent class rule first
	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (ActualDamage > 0.f)
	{
		CurrentHealth -= ActualDamage;
		CurrentHealth = FMath::Clamp(CurrentHealth, 0.f, MaxHealth);

		// DEBUG: Print the health to the screen!
		if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, FString::Printf(TEXT("Player Health: %f"), CurrentHealth));

		// If they took damage but are not dead yet, play the flinch!
		if (CurrentHealth > 0.f)
		{
			// Play the hit reaction montage if it exists and the mesh has an animation instance
			if (HitReactionMontage && GetMesh()->GetAnimInstance())
			{
				GetMesh()->GetAnimInstance()->Montage_Play(HitReactionMontage);
			}
			// If we are an AI, instantly lock onto whoever shot us!
			if (AGhostAIController* AICon = Cast<AGhostAIController>(GetController()))
			{
				if (EventInstigator && EventInstigator->GetPawn())
				{
					APawn* ShooterPawn = EventInstigator->GetPawn(); // Get shooter
					FVector ShooterLocation = ShooterPawn->GetActorLocation(); // Get shooter location

					AICon->bIsSpooked = true;
					AICon->SuspicionLevel = AICon->MaxSuspicion; // Max out suspicion
					// Lock their suspicion at 100%
					AICon->SuspicionDecayPauseEndTime = GetWorld()->GetTimeSeconds() + 20.0f;

					if (UBlackboardComponent* BB = AICon->GetBlackboardComponent())
					{
						// Check if the guard has a clear line of sight to the shooter right now.
						FHitResult LOSHit;
						FCollisionQueryParams LOSParams;
						LOSParams.AddIgnoredActor(this);          // Ignore the guard's own body
						LOSParams.AddIgnoredActor(ShooterPawn);   // We want to know if the path is clear

						FVector GuardEyes = GetActorLocation() + FVector(0, 0, 70.0f);
						bool bHit = GetWorld()->LineTraceSingleByChannel(
							LOSHit, GuardEyes, ShooterLocation, ECC_Visibility, LOSParams);

						// bHit == false means nothing blocked the path so clear LOS to shooter
						bool bHasLOS = !bHit;

						if (bHasLOS)
						{
							// Guard can see the shooter — lock on directly, activate Chase branch
							BB->SetValueAsObject(FName("TargetActor"), ShooterPawn);
							BB->ClearValue(FName("InvestigateLocation"));
							BB->ClearValue(FName("bWillWalkToNoise"));
							BB->ClearValue(FName("bIsCombatSearch"));
						}
						else
						{
							// Guard can't see the shooter — send them to the shot origin
							BB->SetValueAsVector(FName("InvestigateLocation"), ShooterLocation);
							BB->SetValueAsBool(FName("bIsCombatSearch"), true);
							BB->SetValueAsBool(FName("bWillWalkToNoise"), true);
						}

						OnCombatStarted();
					}
				}
			}
		}

		if (CurrentHealth <= 0.f)
		{
			GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Character Died!"));

			// Only run this if the character dying is NOT the player
			if (!IsPlayerControlled())
			{
				// Probability chack
				if (FMath::FRand() <= AmmoDropChance)
				{
					bool bIsHeadshot = false;

					// Check if the damage was a body or head shot
					if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
					{
						const FPointDamageEvent* PointDamageEvent = (FPointDamageEvent*)&DamageEvent;

						if (PointDamageEvent->HitInfo.BoneName == FName("head"))
						{
							bIsHeadshot = true;
						}
					}

					// 2 for headshot, 1 for normal body shot
					int32 AmmoAmount = bIsHeadshot ? 2 : 1;

					// Sapwn the ammo
					SpawnDroppedAmmo(AmmoAmount);
				}
			}

			bIsDead = true;
			NotifyNearbyGuardsOfDeath(this);

			// Notify nearby guards that another guard has died
			if (!IsPlayerControlled())
			{
				UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), 1.0f, this, 500.0f, FName("GuardDown"));
			}

			// Kill the guards movement
			GetCharacterMovement()->StopMovementImmediately();
			GetCharacterMovement()->DisableMovement();
			GetCharacterMovement()->SetAvoidanceEnabled(false);

			AController* VictimController = Controller;

			// Server AI controller from the guard
			if (VictimController)
			{
				// --- NEW: THE FIX ---
				if (IsPlayerControlled())
				{
					// If it is the player, leave the controller alone so the camera works, and trigger the UI!
					OnPlayerDied();
				}
				else
				{
					// If it is an AI guard, stop their brain and destroy it to save memory!
					VictimController->UnPossess();
					VictimController->Destroy();
				}
			}

			// Turn off collision capsule 
			GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			// Keep mesh collision. (Might use physics ragdoll)
			GetMesh()->SetCollisionProfileName(TEXT("Ragdoll"));

			// Change the actor's tag if it's used for any 
			Tags.Add(FName("Dead Body"));
			Tags.Remove(FName("Guard"));

			// Disable the actor's Tick
			SetActorTickEnabled(false);

			// Play the death animation
			if (DeathMontage && GetMesh()->GetAnimInstance())
			{
				GetMesh()->GetAnimInstance()->Montage_Play(DeathMontage);
			}
		}
	}

	return ActualDamage;
}

void AProject_StealthGhostCharacter::ReloadWeapon()
{
	// Ensure we are holding a weapon and not a rock
	if (AWeaponBase* Weapon = Cast<AWeaponBase>(CurrentEquipment))
	{
		// Don't restart the animation if we are already reloading!
		if (GetMesh()->GetAnimInstance() && GetMesh()->GetAnimInstance()->Montage_IsPlaying(ReloadMontage))
		{
			return;
		}

		// Tell the weapon to reload. If it returns true, play the animation
		if (Weapon->Reload())
		{
			if (ReloadMontage)
			{
				GetMesh()->GetAnimInstance()->Montage_Play(ReloadMontage);
			}
		}
		else
		{
			// Optional: Print a message or play a "click" sound if you want!
			GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("Cannot Reload: Gun is full or out of ammo!"));
		}
	}
}

bool AProject_StealthGhostCharacter::AddAmmoToInventory(TSubclassOf<AEquippableBase> WeaponClass, float AmmoAmount, float MaxCapacity)
{
	if (!WeaponClass) return false;

	// If we are actively holding the weapon, update it directly
	if (CurrentEquipment && CurrentEquipment->IsA(WeaponClass))
	{
		if (AWeaponBase* Weapon = Cast<AWeaponBase>(CurrentEquipment))
		{
			float CurrentReserve = Weapon->GetReserveAmmo();

			if (CurrentReserve >= MaxCapacity) return false; // Ammo is full!

			// Clamp ensures we don't accidentally go over the max capacity
			float NewReserve = FMath::Clamp(CurrentReserve + AmmoAmount, 0.0f, MaxCapacity);
			Weapon->SetAmmo(Weapon->GetCurrentAmmo(), NewReserve);
			return true;
		}
	}

	// 2. If the weapon is holstered in our pocket (SavedAmmoMap)
	if (SavedAmmoMap.Contains(WeaponClass))
	{
		FVector2D AmmoData = SavedAmmoMap[WeaponClass];

		if (AmmoData.Y >= MaxCapacity) return false; // Ammo is full!

		AmmoData.Y = FMath::Clamp(AmmoData.Y + AmmoAmount, 0.0f, MaxCapacity);
		SavedAmmoMap.Add(WeaponClass, AmmoData); // Overwrites the old value with the new math
		return true;
	}

	// 3. We don't have the gun yet, but let's save the ammo for when we find it!
	SavedAmmoMap.Add(WeaponClass, FVector2D(0.0f, FMath::Clamp(AmmoAmount, 0.0f, MaxCapacity)));
	return true;
}