// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InteractableInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UInteractableInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interface class where functions live
 */

class PROJECT_STEALTHGHOST_API IInteractableInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	// The main function called when the player presses the interact button.
	// BlueprintNativeEvent means C++ handles it by default, but Blueprints can override it.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void Interact(AActor* Interactor);

	// Tells the item to show its floating UI
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void ShowPrompt();

	// Tells the item to hide its floating UI
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void HidePrompt();

	// A helper function so the UI knows what text to display (e.g., "Press E to Pick Up Stone")
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	FString GetInteractText();

	// Checks if the item requires the player to hold the button
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	bool DoesRequireHold();
};
