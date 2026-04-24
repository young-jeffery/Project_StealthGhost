// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_ScanArea.generated.h"

/**
 * 
 */
UCLASS()
class PROJECT_STEALTHGHOST_API UBTTask_ScanArea : public UBTTaskNode
{
	GENERATED_BODY()


public:
    UBTTask_ScanArea();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
    virtual void TickTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
    virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

    UPROPERTY(EditAnywhere, Category = "Scan")
    float ScanDuration = 3.0f;

private:
    float TimeRemaining;
    float TimeUntilNextShift;
	
};
