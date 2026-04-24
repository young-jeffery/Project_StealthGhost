// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InteractableInterface.h"
#include "ThrowableBase.generated.h"

// Forward declarations to improve compilation speed
class UStaticMeshComponent;
class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class PROJECT_STEALTHGHOST_API AThrowableBase : public AActor, public IInteractableInterface
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AThrowableBase();

	// How the stone answers the interface for text
	virtual FString GetInteractText_Implementation() override;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// --- COMPONENTS ---
	// The collision sphere acts as the root so it bounces cleanly off the environment
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionComp;

	// The visual representation (the stone or bottle mesh)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* ItemMesh;

	// Handles the physics arc, gravity, and bouncing automatically
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

	// --- SETTINGS ---
	// How loud the distraction is (multiplier for the AI hearing system)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Throwable Settings")
	float DistractionLoudness = 1.0f;

	// Maximum range the sound travels in Unreal Units (1500 = 15 meters)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Throwable Settings")
	float MaxRange = 1500.0f;

	// A boolean lock to ensure the item only distracts the AI on the FIRST bounce
	bool bHasMadeNoise = false;

	// --- PHYSICS SETTINGS FOR BLUEPRINTS ---

	// How bouncy the object is (0.0 = no bounce, 1.0 = rubber ball)
	UPROPERTY(EditDefaultsOnly, Category = "Throwable Physics")
	float ObjectBounciness = 0.2f;

	// How much it slides on the floor (0.0 = ice, 1.0 = sandpaper)
	UPROPERTY(EditDefaultsOnly, Category = "Throwable Physics")
	float ObjectFriction = 0.6f;

	// --- FUNCTIONS ---
	// The function bound to the physics collision event
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
