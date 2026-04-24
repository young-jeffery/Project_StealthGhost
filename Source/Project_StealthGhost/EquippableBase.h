// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EquippableBase.generated.h"

UCLASS()
class PROJECT_STEALTHGHOST_API AEquippableBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AEquippableBase();

	// Neutral root that takes either of skeletal or static meshes
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USceneComponent* DefaultRoot;

	// For complex items with moving parts
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* ItemMesh;

	// For solid, non-moving items
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UStaticMeshComponent* ItemStaticMesh;


	// The name of the socket on the player that this item should attach to when equipped
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Equipment Setup")
	FName AttachmentSocketName;

	// --- CORE INVENTORY FUNCTIONS ---
	// Attaches the item to the player's hand
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	virtual void Equip(class USceneComponent* TargetParent, FName SocketName);

	// Hides the item or destroys it when put away
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	virtual void Unequip();


	// --- CORE ACTION FUNCTIONS (To be overridden by children) ---

	// Called when the player holds the Right Mouse Button (L2)
	UFUNCTION(BlueprintCallable, Category = "Equipment Actions")
	virtual void StartAiming();

	// Called when the player releases the Right Mouse Button
	UFUNCTION(BlueprintCallable, Category = "Equipment Actions")
	virtual void StopAiming();

	// Called when the player clicks the Left Mouse Button (R2)
	UFUNCTION(BlueprintCallable, Category = "Equipment Actions")
	virtual void UseAction();

	// Called when the player releases the Left Mouse Button
	UFUNCTION(BlueprintCallable, Category = "Equipment Actions")
	virtual void ReleaseAction();




protected:

};
