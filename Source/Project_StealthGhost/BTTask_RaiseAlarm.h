// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_RaiseAlarm.generated.h"

// Forward declaration to avoid circular dependencies
class AProject_StealthGhostCharacter;

UCLASS()
class PROJECT_STEALTHGHOST_API UBTTask_RaiseAlarm : public UBTTaskNode
{
    GENERATED_BODY()

public:
    UBTTask_RaiseAlarm();

    // Built in function that is called when the task starts
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    // Built in function that is called if the task is interrupted before it finishes
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    // How long the guard takes to call for backup (e.g., pulling out a radio)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float AlarmDelay = 4.0f;

protected:
    // Timer to handle the delay
    FTimerHandle AlarmTimerHandle;

    // A cached pointer to the behavior tree component so we can finish the task later
    UBehaviorTreeComponent* CachedOwnerComp;

    // A cached pointer to the dead body we are currently looking at
    AProject_StealthGhostCharacter* TargetBody;

    // The function that runs when the timer finishes
    void FinishAlarm();
};




//#pragma once
//
//#include "CoreMinimal.h"
//#include "BehaviorTree/BTTaskNode.h"
//#include "BTTask_RaiseAlarm.generated.h"
//
///**
// * Custom Behavior Tree task to trigger a noise event
// */
//UCLASS()
//class PROJECT_STEALTHGHOST_API UBTTask_RaiseAlarm : public UBTTaskNode
//{
//	GENERATED_BODY()
//	
//public:
//	// Constructor: Sets default values for this task's properties
//	UBTTask_RaiseAlarm();
//
//	// Built in function that is called when the task is executed
//	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
//};
