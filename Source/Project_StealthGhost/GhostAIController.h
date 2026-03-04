// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AIPerceptionTypes.h"
#include "GhostAIController.generated.h"

/**
 * GhostAIController: The brain of our NPC guards.
 * Inheriting from AAIController allows us to use the Behavior Tree and Perception systems.
 */
UCLASS()
class PROJECT_STEALTHGHOST_API AGhostAIController : public AAIController
{
	GENERATED_BODY()

public:
    // Constructor: Used to initialize components
    AGhostAIController();
 
    // The AIs brain configuration
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI")
    UAIPerceptionComponent* AIPerception;

    // The AIs sight configuration
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI")
    UAISenseConfig_Sight* SightConfig;

    // The AIs hearing configuration
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI")
    UAISenseConfig_Hearing* HearingConfig;

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // This runs when the AIController takes over the guard pawn
    virtual void OnPossess(APawn* InPawn) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI");
    class UBehaviorTree* GuardBehaviorTree;


    // This enables us to know which sense was triggered
    UFUNCTION()
    void OnTargetDetected(AActor* Actor, FAIStimulus Stimulus);

};
