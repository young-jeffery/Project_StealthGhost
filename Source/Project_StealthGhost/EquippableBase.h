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

	// The visual mesh of the item (Skeletal so guns can have moving slides/triggers)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class USkeletalMeshComponent* ItemMesh;

	// --- CORE INVENTORY FUNCTIONS ---
	// Attaches the item to the player's hand
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	virtual void Equip(class USceneComponent* ParentComponent, FName SocketName);

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
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
