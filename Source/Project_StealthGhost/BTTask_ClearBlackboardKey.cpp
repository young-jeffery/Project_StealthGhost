// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_ClearBlackboardKey.h"
#include "BehaviorTree/BlackboardComponent.h"

UBTTask_ClearBlackboardKey::UBTTask_ClearBlackboardKey()
{
	// The name that will appear in your Behavior Tree right-click menu
	NodeName = TEXT("Clear Blackboard Key");
}

EBTNodeResult::Type UBTTask_ClearBlackboardKey::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// Grab the Blackboard attached to this AI
	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();

	if (BlackboardComp)
	{
		// GetSelectedBlackboardKey() automatically grabs whatever variable 
		// you selected in the node's Details Panel dropdown.
		BlackboardComp->ClearValue(GetSelectedBlackboardKey());

		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}