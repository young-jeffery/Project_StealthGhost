// Fill out your copyright notice in the Description page of Project Settings.


#include "EquippableBase.h"
#include "Components/SkeletalMeshComponent.h"

// Sets default values
AEquippableBase::AEquippableBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Create the blank neutral root
	DefaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultRoot"));
	RootComponent = DefaultRoot;

	// Create the Skeletal Mesh and attach it
	ItemMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(RootComponent);
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Create the Static Mesh and attach it
	ItemStaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemStaticMesh"));
	ItemStaticMesh->SetupAttachment(RootComponent);
	ItemStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Default fallback socket
	AttachmentSocketName = FName("hand_r");

}

void AEquippableBase::Equip(USceneComponent* TargetParent, FName SocketName)
{
	// Snap the weapon to the hand socket
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(TargetParent, AttachmentRules, SocketName);
}

void AEquippableBase::Unequip()
{
	Destroy();
}

// --- VIRTUAL FUNCTIONS ---

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

