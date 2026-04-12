// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AIPerceptionTypes.h"
#include "BehaviorTree/BlackboardComponent.h"
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

    // Cached Blackboard Reference
    UPROPERTY(BlueprintReadOnly, Category = "AI")
    UBlackboardComponent* CachedBlackboard;

    // Allows this AI Controller to update every frame
    virtual void Tick(float DeltaTime) override;

    // A toggle switch you can easily click in the Unreal Editor
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
    bool bShowDebugVisuals = false;

    // Checks the Blackboard to see if the AI is currently busy with combat or investigation
    UFUNCTION(BlueprintCallable, Category = "AI State")
    bool IsAlerted() const;

    // Checks if the guard is currently investigating a location
    UFUNCTION(BlueprintPure, Category = "AI State")
    bool IsInvestigating() const;

    // --- SUSPICION SYSTEM ---
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI Suspicion")
    float SuspicionLevel = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Suspicion")
    float MaxSuspicion = 100.0f;

    // How fast suspicion builds per second
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Suspicion")
    float SuspicionBuildRate = 45.0f;

    // How fast suspicion drops per second when player is out of sight
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Suspicion")
    float SuspicionDecayRate = 20.0f;

    // The current target the guard is looking at
    UPROPERTY()
    AActor* CurrentVisibleTarget = nullptr;

    // --- MEMORY & ALERT STATE ---

    // Tracks if the guard has been permanently put on high alert
    UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "AI Memory")
    bool bIsSpooked = false;

    // How much faster suspicion builds when the guard is spooked (2.0 = twice as fast)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Memory")
    float SpookedBuildMultiplier = 2.0f;

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

    // This runs when the AIController takes over the guard pawn
    virtual void OnPossess(APawn* InPawn) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI");
    class UBehaviorTree* GuardBehaviorTree;

    // --- PARTIAL COVER OPTIMIZATION ---

    // Timer handle to gate how often we check for partial cover
    FTimerHandle VisibilityTimerHandle;

    // The function that runs on the timer (e.g., 5 times a second)
    void UpdateVisibilityGating();

    // The multiplier applied to detection speed (1.0 = fully exposed, 0.5 = half covered)
    float CoverMultiplier = 1.0f;



    // This enables us to know which sense was triggered
    UFUNCTION()
    void OnTargetDetected(AActor* Actor, FAIStimulus Stimulus);

};
