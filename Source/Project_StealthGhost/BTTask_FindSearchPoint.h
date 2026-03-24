// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_FindSearchPoint.generated.h"

UCLASS()
class PROJECT_STEALTHGHOST_API UBTTask_FindSearchPoint : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_FindSearchPoint();

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	// How far around the "InvestigateLocation" the guard should wander
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Search")
	float SearchRadius = 500.0f; // 5 meters

	// The key where we store the base location we are investigating
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Search")
	FBlackboardKeySelector InvestigateLocationKey;

	// The new key where we will store the random point to walk to
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Search")
	FBlackboardKeySelector SearchPointKey;
};