// Fill out your copyright notice in the Description page of Project Settings.


#include "EquippableBase.h"
#include "Components/SkeletalMeshComponent.h"

// Sets default values
AEquippableBase::AEquippableBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Create the mesh and set it as the root component
	ItemMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ItemMesh"));
	RootComponent = ItemMesh;

	// Turn off collision by default so the gun doesn't block the player's movement
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

}

void AEquippableBase::Equip(USceneComponent* ParentComponent, FName SocketName)
{
	// Snap the weapon to the hand socket
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(ParentComponent, AttachmentRules, SocketName);
}

void AEquippableBase::Unequip()
{
	// For now, we will just destroy the actor when it's unequipped.
	// Later, you could attach it to a "Holster" socket on the player's hip!
	Destroy();
}

// --- VIRTUAL FUNCTIONS ---
// These are intentionally empty! The Pistol and the Stone will override these 
// to do their specific logic (like firing a bullet or drawing a trajectory arc).

void AEquippableBase::StartAiming()
{
}

void AEquippableBase::StopAiming()
{
}

void AEquippableBase::UseAction()
{
}

void AEquippableBase::ReleaseAction()
{
}

// Called when the game starts or when spawned
void AEquippableBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AEquippableBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

