// Copyright Epic Games, Inc. All Rights Reserved.

#include "Project_StealthGhostCharacter.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Project_StealthGhost.h"
#include "DrawDebugHelpers.h" // This draws visual lines for testing

AProject_StealthGhostCharacter::AProject_StealthGhostCharacter()
{
	//Default Speeds
	//GetCharacterMovement()->MaxWalkSpeed = 200.0f;
	//GetCharacterMovement()->MaxWalkSpeedCrouched = 150.0f;

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
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
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
			// Get the Dot Product of the camera's right and the character's right
			float DirectionCheck = FVector::DotProduct(GetActorRightVector(), GetFollowCamera()->GetRightVector());

			// FMathe::Sign returns 1if they match and -1 if they are opposite
			float MoveSign = FMath::Sign(DirectionCheck);

			// Y Vector movement (foward/backward) is ignored. GetActorRightVector() is used to allow the player slide along the wall
			AddMovementInput(GetActorRightVector(), MovementVector.X * MoveSign);
		}
		else {
			
			// route the input
			DoMove(MovementVector.X, MovementVector.Y);
		}
	}

	// Varying sound levels based on movement type
	if (MovementVector.Length() > 0.0f)
	{
		if (bIsCrouched)
		{
			// Do nothing
		}
		else
		{
			// Loudness is 1 when speed is above 300 and 0.5 otherwise
			float CurrentLoudness = (GetCharacterMovement()->MaxWalkSpeed > 500.0f) ? 0.7f : 0.3f;
			MakeNoise(CurrentLoudness, this, GetActorLocation());
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

	MakeNoise(0.5f, this, GetActorLocation());
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
//bool AProject_StealthGhostCharacter::CanTakeCover()
//{
//	//Laser starts at the center of the player 
//	FVector StartLocation = GetActorLocation();
//
//	//Points towards where the character is facing
//	FVector ForwardVector = GetActorForwardVector();
//
//	//The laser extends 100 units foward, which is equivalent to 1 meter
//	FVector EndLocation = StartLocation + (ForwardVector * 200.0f);
//
//	FHitResult HitResult; // Stores data on what was hit
//	FCollisionQueryParams CollisionParams;
//	CollisionParams.AddIgnoredActor(this); // This tells the laser not to hit the player's own body
//
//	// This fires the laser
//	// ECC_Visibility means it will hit anything that blocks the camera/sight
//	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, StartLocation, EndLocation, ECC_Visibility, CollisionParams);
//
//	// This is just visual debugging to ensure the math works properly
//	if (bHit)
//	{
//		// If we hit a wall, draw a GREEN line for 2 seconds
//		DrawDebugLine(GetWorld(), StartLocation, HitResult.ImpactPoint, FColor::Green, false, 2.0f, 0, 2.0f);
//	}
//	else
//	{
//		// If we hit nothing, draw a RED line for 2 seconds
//		DrawDebugLine(GetWorld(), StartLocation, EndLocation, FColor::Red, false, 2.0f, 0, 2.0f);
//	}
//
//	return bHit;
//}

void AProject_StealthGhostCharacter::ToggleCover()
{
	// If we are already in cover, pressing the button takes us out of cover.
	if (CurrentState == EPlayerMovementState::VE_InCover)
	{
		CurrentState = EPlayerMovementState::VE_Default;
		// NOTE: We will add the logic to un-stick from the wall here later.

		GetCharacterMovement()->bOrientRotationToMovement = true;
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
	Super::Tick(DeltaTime);

	if (CurrentState == EPlayerMovementState::VE_InCover)
	{
		FVector StartLocation = GetActorLocation();
		// Trace backwards from the player using a negative forward vector
		FVector EndLocation = StartLocation - (GetActorForwardVector() * 80.0f);

		// Sphere size
		float SphereRadius = 35.0f;

		FCollisionQueryParams CollisionParams;
		CollisionParams.AddIgnoredActor(this);
		FCollisionShape SphereShape = FCollisionShape::MakeSphere(SphereRadius);

		TArray<FHitResult> HitResults;

		bool bHit = GetWorld()->SweepMultiByChannel(HitResults, StartLocation, EndLocation, FQuat::Identity, ECC_Visibility, SphereShape, CollisionParams);

		bool bFoundWall = false;

		if (bHit)
		{
			for (const FHitResult& Hit : HitResults)
			{
				// checks if this is a vertical wall
				if (FMath::Abs(Hit.Normal.Z) < 0.2f)
				{
					bFoundWall = true;
					// Save the current location
					LastValidCoverLocation = GetActorLocation();

					// smoothly roatate the character to match the wall's new curve
					FRotator TargetRotation = Hit.Normal.Rotation();
					FRotator SmoothRotation = FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaTime, 10.0f);
					SetActorRotation(SmoothRotation);

					// calculate a constant distance between the player and the wall
					FVector HugWallLocation = Hit.ImpactPoint + (Hit.Normal * 40.0f);
					// Get our current location
					FVector CurrentLocation = GetActorLocation();

					// Isolate X and Y axis from Z to avoid falling bug
					FVector2D CurrentXY(CurrentLocation.X, CurrentLocation.Y);
					FVector2D TargetXY(HugWallLocation.X, HugWallLocation.Y);

					// This interpolates only the 2D plane
					FVector2D SmoothXY = FMath::Vector2DInterpTo(CurrentXY, TargetXY, DeltaTime, 10.0f);

					// Combine the new X and Y with the Z we didn't touch
					FVector SmoothLocation(SmoothXY.X, SmoothXY.Y, CurrentLocation.Z);

					// This keeps the height stable
					//HugWallLocation.Z = GetActorLocation().Z;

					//// VInterpTo is used to pull the character to that distance
					//FVector2D SmoothLocation = FMath::VInterpTo(GetActorLocation(), HugWallLocation, DeltaTime, 10.0f);

					SetActorLocation(SmoothLocation);

					break; // stop loop once a wall is found
				}
			}
		}
		// if no wall is found after looping
		if (!bFoundWall)
		{
			SetActorLocation(LastValidCoverLocation);
			GetCharacterMovement()->Velocity = FVector::ZeroVector;
		}
	}
}

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
	if (CurrentState == EPlayerMovementState::VE_InCover || bIsCrouched) return;

	GetCharacterMovement()->MaxWalkSpeed = 600.0f;
}

void AProject_StealthGhostCharacter::StopSprint()
{
	GetCharacterMovement()->MaxWalkSpeed = 200.0f;
}
