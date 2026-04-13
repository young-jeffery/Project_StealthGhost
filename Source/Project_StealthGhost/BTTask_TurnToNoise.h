// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_TurnToNoise.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_STEALTHGHOST_API UBTTask_TurnToNoise : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
    UBTTask_TurnToNoise();

    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    // How long the guard pauses to stare at the noise before walking
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
    float ObservationDuration = 1.0f;

protected:
    float TimeSpentWaiting;
	
};
