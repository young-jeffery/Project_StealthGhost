// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/Services/BTService_BlackboardBase.h"
#include "BTService_CheckAttackRange.generated.h"

/**
 * Custom Service to manage combat range
 */
UCLASS()
class PROJECT_STEALTHGHOST_API UBTService_CheckAttackRange : public UBTService_BlackboardBase
{
	GENERATED_BODY()

public:
	UBTService_CheckAttackRange();


protected:
	virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;

	// Distance to switch from chasing to shooting
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard Combat")
	float AttackRange = 500.0f;

	// Distance to switch from shooting to chasing
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Guard Combat")
	float LoseAttackRange = 1000.0f;
	
};
