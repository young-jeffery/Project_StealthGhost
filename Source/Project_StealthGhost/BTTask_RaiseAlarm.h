// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_RaiseAlarm.generated.h"

/**
 * Custom Behavior Tree task to trigger a noise event
 */
UCLASS()
class PROJECT_STEALTHGHOST_API UBTTask_RaiseAlarm : public UBTTaskNode
{
	GENERATED_BODY()
	
public:
	// Constructor: Sets default values for this task's properties
	UBTTask_RaiseAlarm();

	// Built in function that is called when the task is executed
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
