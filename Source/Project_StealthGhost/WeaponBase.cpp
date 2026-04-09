// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponBase.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Kismet/GameplayStatics.h"
#include "AIController.h"
#include "Perception/AISense_Hearing.h"

void AWeaponBase::StartAiming()
{
	// Trigger the blueprint event so the editor can zoom the camera in!
	OnAimStateChanged(true);
}

void AWeaponBase::StopAiming()
{
	// Trigger the blueprint event so the editor can zoom the camera back out!
	OnAimStateChanged(false);
}

// Weapon firing logic
void AWeaponBase::UseAction()
{
	// If the gun is empty, stop the fire function immediately
	if (!CanFire())
	{
		return;
	}

	// Subtract one bullet from the magazine before firing the shot.
	ConsumeAmmo();

	// Get the player character and camera to setup our traces
	ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar) return;

	

	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(OwnerChar); // Ignore the owner
	TraceParams.AddIgnoredActor(this);       // Ignore the gun
	TraceParams.bReturnPhysicalMaterial = true; // Get the Physical Material hit

	// Get the tip of the gun
	FVector MuzzleLoc = ItemMesh->GetSocketLocation(FName("MuzzleFlash"));
	FVector TargetPoint; // Where the bullet will travel to


	// Player & AI Fire Logic
	if (OwnerChar->IsPlayerControlled())
	{
		// Player trace from the Camera
		UCameraComponent* PlayerCam = OwnerChar->FindComponentByClass<UCameraComponent>();
		if (PlayerCam)
		{
			FVector CameraLoc = PlayerCam->GetComponentLocation();
			FVector CamForward = PlayerCam->GetForwardVector();

			// Prevent wall clipping
			// Find out exactly how far the camera is sitting behind the player
			float DistanceToPlayer = FVector::Dist(CameraLoc, OwnerChar->GetActorLocation());

			// Push the start of the laser forward so it begins at the player's position
			FVector AdjustedStartLoc = CameraLoc + (CamForward * DistanceToPlayer);

			// Calculate the end point from our new start location
			FVector CameraEndLoc = AdjustedStartLoc + (CamForward * WeaponRange);

			FHitResult CameraHit;

			if (GetWorld()->LineTraceSingleByChannel(CameraHit, AdjustedStartLoc, CameraEndLoc, ECC_Visibility, TraceParams))
			{
				TargetPoint = CameraHit.ImpactPoint;
			}
			else
			{
				TargetPoint = CameraEndLoc; // Default to max range if we hit the sky
			}
		}
	}
	else
	{
		// Trace directly to the target they are focusing on
		FVector DirectionToTarget;

		// Who are they currently looking at
		AAIController* AICon = Cast<AAIController>(OwnerChar->GetController());
		if (AICon && AICon->GetFocusActor())
		{
			// Get the direction from the muzzle directly to the center of the player
			FVector TargetCenter = AICon->GetFocusActor()->GetActorLocation();
			DirectionToTarget = (TargetCenter - MuzzleLoc).GetSafeNormal();
		}
		else
		{
			// Fallback just in case they aren't focusing on anyone
			DirectionToTarget = ItemMesh->GetSocketRotation(FName("MuzzleFlash")).Vector();
		}
		

		// Convert our spread angle to radians for the math
		float HalfConeAngle = FMath::DegreesToRadians(AIWeaponSpread);
		// Generate a random direction inside that cone
		FVector InaccurateDirection = FMath::VRandCone(DirectionToTarget, HalfConeAngle);

		TargetPoint = MuzzleLoc + (InaccurateDirection * WeaponRange);
	}

	FHitResult BulletHit;
	FCollisionShape BulletShape = FCollisionShape::MakeSphere(MagnetismRadius); // The Bullet Magnetism (Thick Sphere Trace)

	bool bHit = GetWorld()->SweepSingleByChannel(BulletHit, MuzzleLoc, TargetPoint, FQuat::Identity, ECC_Visibility, BulletShape, TraceParams);

	// --- DEBUG VISUALS ---
	// Draw a thin red laser showing the exact center of our crosshair
	DrawDebugLine(GetWorld(), MuzzleLoc, TargetPoint, FColor::Red, false, 2.0f, 0, 1.0f);

	if (bHit && BulletHit.GetActor())
	{
		// Make sure we actually hit a Character and not a wall
		if (BulletHit.GetActor()->IsA(ACharacter::StaticClass()))
		{
			// Does body damage by default
			float FinalDamage = BaseDamage;

			// Check if we hit a valid physical material
			if (BulletHit.PhysMaterial.IsValid() && BulletHit.PhysMaterial->SurfaceType == SurfaceType1) // SurfaceType1 is Head
			{
				FinalDamage *= HeadshotMultiplier;
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("HEADSHOT!"));
			}
			else
			{
				if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("BODY SHOT"));
			}

			// Draw a green sphere where the thick bullet hit
			DrawDebugSphere(GetWorld(), BulletHit.ImpactPoint, MagnetismRadius, 12, FColor::Green, false, 2.0f);

			// Apply Damage to the enemy
			BulletHit.GetActor()->TakeDamage(FinalDamage, FDamageEvent(), OwnerChar->GetController(), this);
		}
	}

	// --- AUDIO & NEAR MISS SYSTEM ---

	// The boom of the gun (Only if not silenced)
	if (!bIsSilenced)
	{
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), MuzzleLoc, 1.0f, OwnerChar, 2000.0f, FName("Alarm"));
	}

	// The crack of the bullet hitting a surface (Near Miss!)
	if (bHit)
	{
		// If a bullet hits the wall next to a guard's head, they will hear it!
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), BulletHit.ImpactPoint, 1.0f, OwnerChar, 500.0f, FName("Distraction"));
	}

	// Trigger the blueprint event so the editor can play muzzle flashes and sounds!
	OnWeaponFired();
}


bool AWeaponBase::CanFire() const
{
	return CurrentAmmo > 0; // We can only fire if we have bullets in the magazine
}

void AWeaponBase::ConsumeAmmo()
{
	if (CurrentAmmo > 0)
	{
		CurrentAmmo--;
	}
}

void AWeaponBase::Reload()
{
	if (CurrentAmmo == MagazineSize || TotalReserveAmmo <= 0) return;

	int32 BulletsNeeded = MagazineSize - CurrentAmmo;
	int32 BulletsToReload = FMath::Min(BulletsNeeded, TotalReserveAmmo);

	CurrentAmmo += BulletsToReload;
	TotalReserveAmmo -= BulletsToReload;

	// You can trigger a reload sound or notify here later
}