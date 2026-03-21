// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_ClearBlackboardKey.generated.h"

/**
 * Custom Utility Task to clear a specific Blackboard Key.
 * Built to ensure memory safety and explicit state resets.
 */
UCLASS()
class PROJECT_STEALTHGHOST_API UBTTask_ClearBlackboardKey : public UBTTask_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTTask_ClearBlackboardKey();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};