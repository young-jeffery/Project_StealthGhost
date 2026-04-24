// Fill out your copyright notice in the Description page of Project Settings.


#include "ThrowableEquipment.h"
#include "ThrowableBase.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"


AThrowableEquipment::AThrowableEquipment()
{
	// Turn Tick on so we can draw the arc every frame
	PrimaryActorTick.bCanEverTick = true;
}

void AThrowableEquipment::StartAiming()
{
	bIsAiming = true;
}

void AThrowableEquipment::StopAiming()
{
	bIsAiming = false;
	UpdateAimMarker(FVector::ZeroVector, false);
}

void AThrowableEquipment::ReleaseAction()
{
	// If we weren't aiming, don't throw anything
	if (!bIsAiming || !ThrowableClass) return;

	bIsAiming = false;
	UpdateAimMarker(FVector::ZeroVector, false);

	ACharacter* PlayerChar = Cast<ACharacter>(GetOwner());
	if (PlayerChar)
	{
		// To make the throw perfectly accurate to the crosshair, we throw from the camera
		UCameraComponent* PlayerCam = PlayerChar->FindComponentByClass<UCameraComponent>();
		if (PlayerCam)
		{
			FVector StartLocation = ItemMesh->GetComponentLocation();
			FVector LaunchVelocity = PlayerCam->GetForwardVector() * ThrowVelocity;

			// Spawn the physical stone
			FActorSpawnParameters SpawnParams;
			SpawnParams.Instigator = PlayerChar;
			SpawnParams.Owner = PlayerChar;
			// Prevents the stone from colliding with the player's body right as it spawns
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			AThrowableBase* ThrownItem = GetWorld()->SpawnActor<AThrowableBase>(ThrowableClass, StartLocation, PlayerCam->GetComponentRotation(), SpawnParams);

			// Give it the velocity
			if (ThrownItem)
			{
				// Get the root physical collision of the stone
				UPrimitiveComponent* ItemRoot = Cast<UPrimitiveComponent>(ThrownItem->GetRootComponent());
				if (ItemRoot)
				{
					// Tell the stone to completely ignore the player character's collision
					ItemRoot->IgnoreActorWhenMoving(PlayerChar, true);
				}

				// Push the Projectile Movement Component, not the raw physics root!
				UProjectileMovementComponent* ProjComp = ThrownItem->FindComponentByClass<UProjectileMovementComponent>();
				if (ProjComp)
				{
					ProjComp->Velocity = LaunchVelocity;
				}
			}

			// Optional: Here is where you would subtract 1 from the player's ThrowableCount!
		}
	}
}

void AThrowableEquipment::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// If we are holding the aim button, draw the trajectory arc!
	if (bIsAiming)
	{
		ACharacter* PlayerChar = Cast<ACharacter>(GetOwner());
		if (PlayerChar)
		{
			UCameraComponent* PlayerCam = PlayerChar->FindComponentByClass<UCameraComponent>();
			if (PlayerCam)
			{
				// The start location is the mesh's location, but the launch direction is based on the camera's forward vector
				FVector StartLocation = ItemMesh->GetComponentLocation();
				FVector LaunchVelocity = PlayerCam->GetForwardVector() * ThrowVelocity;

				// Setup the math prediction
				FPredictProjectilePathParams PredictParams;
				PredictParams.StartLocation = StartLocation;
				PredictParams.LaunchVelocity = LaunchVelocity;
				PredictParams.bTraceWithCollision = true; // Stop the arc when it hits a wall/floor
				PredictParams.ProjectileRadius = 5.0f;
				PredictParams.MaxSimTime = 5.0f;

				PredictParams.DrawDebugType = EDrawDebugTrace::None; // Draw it just for this frame
				PredictParams.TraceChannel = ECC_Visibility;
				PredictParams.ActorsToIgnore.Add(this);
				PredictParams.ActorsToIgnore.Add(PlayerChar); // Don't hit ourselves

				FPredictProjectilePathResult PredictResult;

				// Fire the fake physics simulation!
				if (UGameplayStatics::PredictProjectilePath(GetWorld(), PredictParams, PredictResult))
				{
					// Tell the Blueprint where it hit
					UpdateAimMarker(PredictResult.HitResult.ImpactPoint, true);
				}
				else
				{
					// If aiming into the sky where it doesn't hit anything, hide the marker
					UpdateAimMarker(FVector::ZeroVector, false);
				}
			}
		}
	}
}

