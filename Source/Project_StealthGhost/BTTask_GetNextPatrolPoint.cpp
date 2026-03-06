// Fill out your copyright notice in the Description page of Project Settings.


#include "BTTask_GetNextPatrolPoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "Project_StealthGhostCharacter.h" // This links to our guards parent class

UBTTask_GetNextPatrolPoint::UBTTask_GetNextPatrolPoint()
{
	NodeName = TEXT("Get Next Patrol Point");
}

EBTNodeResult::Type UBTTask_GetNextPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	// Gets the AIcontroller and Guard Bluprint
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController) return EBTNodeResult::Failed;

	AProject_StealthGhostCharacter* GhostGuard = Cast<AProject_StealthGhostCharacter>(AIController->GetPawn());
	if (!GhostGuard) return EBTNodeResult::Failed;

	// Fail to avoid crashing if no waypoint was assigned
	if (GhostGuard->PatrolRoute.Num() == 0) return EBTNodeResult::Failed;

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp) return EBTNodeResult::Failed;

	// checks our bookmark to see what array number we're on
	int32 CurrentIndex = BlackboardComp->GetValueAsInt(PatrolIndexKey.SelectedKeyName);

	if (CurrentIndex < 0 || CurrentIndex >= GhostGuard->PatrolRoute.Num())
	{
		CurrentIndex = 0;
	}

	// Get the Target Point from the array
	AActor* TargetPoint = GhostGuard->PatrolRoute[CurrentIndex];
	if (TargetPoint)
	{
		// Write the Target Point location to the blackboard for the guard
		BlackboardComp->SetValueAsVector(GetSelectedBlackboardKey(), TargetPoint->GetActorLocation());

		// Add 1 to the bookmark to get to the next point and then loop back once it reaches the end
		//int32 NextIndex = (CurrentIndex + 1) % GhostGuard->PatrolRoute.Num();
		//BlackboardComp->SetValueAsInt(PatrolIndexKey.SelectedKeyName, NextIndex);

		return EBTNodeResult::Succeeded;
	}

	return EBTNodeResult::Failed;
}
