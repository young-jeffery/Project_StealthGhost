// Fill out your copyright notice in the Description page of Project Settings.


#include "WeaponBase.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

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
	ACharacter* PlayerChar = Cast<ACharacter>(GetOwner());
	if (!PlayerChar) return;

	UCameraComponent* PlayerCam = PlayerChar->FindComponentByClass<UCameraComponent>();
	if (!PlayerCam) return;

	// Setup the math from the Camera
	FVector CameraLoc = PlayerCam->GetComponentLocation();
	FVector CameraEndLoc = CameraLoc + (PlayerCam->GetForwardVector() * WeaponRange);

	FHitResult CameraHit;
	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(PlayerChar); // Ignore the player
	TraceParams.AddIgnoredActor(this);       // Ignore the gun
	TraceParams.bReturnPhysicalMaterial = true; // Get the Physical Material hit

	FVector TargetPoint = CameraEndLoc; // Default to max range

	// The Line Trace from the camera
	if (GetWorld()->LineTraceSingleByChannel(CameraHit, CameraLoc, CameraEndLoc, ECC_Visibility, TraceParams))
	{
		TargetPoint = CameraHit.ImpactPoint; 
	}

	// Get the tip of the gun
	FVector MuzzleLoc = ItemMesh->GetSocketLocation(FName("MuzzleFlash"));

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
			BulletHit.GetActor()->TakeDamage(FinalDamage, FDamageEvent(), PlayerChar->GetController(), this);
		}
	}

	// Trigger the blueprint event so the editor can play muzzle flashes and sounds!
	OnWeaponFired();
}

