// Fill out your copyright notice in the Description page of Project Settings.

#include "BTTask_FindSearchPoint.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"

UBTTask_FindSearchPoint::UBTTask_FindSearchPoint()
{
	NodeName = TEXT("Find Search Point");
}


EBTNodeResult::Type UBTTask_FindSearchPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
	if (!BB) return EBTNodeResult::Failed;

	// We need the AI Controller to get the guard's current location
	AAIController* AIController = OwnerComp.GetAIOwner();
	if (!AIController || !AIController->GetPawn()) return EBTNodeResult::Failed;

	// Get the center of our search area (The Last Known Location)
	FVector SearchOrigin = BB->GetValueAsVector(InvestigateLocationKey.SelectedKeyName);
	FVector GuardLocation = AIController->GetPawn()->GetActorLocation();

	// Access the Unreal Navigation System
	UNavigationSystemV1* NavSystem = UNavigationSystemV1::GetCurrent(GetWorld());
	if (!NavSystem) return EBTNodeResult::Failed;

	FNavLocation RandomLocation;
	bool bFoundValidSpot = false;

	// Loop up to 10 times to find a good spot
	for (int32 Attempt = 0; Attempt < 10; Attempt++)
	{
		// Pick a random point around the original noise/body
		if (NavSystem->GetRandomReachablePointInRadius(SearchOrigin, SearchRadius, RandomLocation))
		{
			// Measure the distance from WHERE THE GUARD IS STANDING to the NEW POINT
			float DistanceFromGuard = FVector::Dist(GuardLocation, RandomLocation.Location);

			// Failsafe: Ensure the new point is at least 150 units (1.5 meters) away from the guard
			if (DistanceFromGuard >= 150.0f)
			{
				bFoundValidSpot = true;
				break;
			}
		}
	}

	if (bFoundValidSpot)
	{
		// Save that valid random point to the blackboard
		BB->SetValueAsVector(SearchPointKey.SelectedKeyName, RandomLocation.Location);
		return EBTNodeResult::Succeeded;
	}

	// If we couldn't find a spot after 10 tries (maybe they are stuck in a tiny corner), fail gracefully
	return EBTNodeResult::Failed;
};