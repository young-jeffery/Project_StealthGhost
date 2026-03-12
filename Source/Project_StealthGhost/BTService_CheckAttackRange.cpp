// Fill out your copyright notice in the Description page of Project Settings.


#include "BTService_CheckAttackRange.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AIController.h"

UBTService_CheckAttackRange::UBTService_CheckAttackRange()
{
	NodeName = TEXT("Check Attack Range");
}

void UBTService_CheckAttackRange::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);

	UBlackboardComponent* BlackboardComp = OwnerComp.GetBlackboardComponent();
	if (!BlackboardComp) return;

	AAIController* AIController = OwnerComp.GetAIOwner();
	AActor* TargetActor = Cast<AActor>(BlackboardComp->GetValueAsObject(GetSelectedBlackboardKey()));

	if (AIController && TargetActor)
	{
		APawn* AIPawn = AIController->GetPawn();
		if (AIPawn)
		{
			// Calculate 3D distance between the guard and the target
			float Distance = FVector::Dist(AIPawn->GetActorLocation(), TargetActor->GetActorLocation());

			// Check if the guard is currently attacking
			bool bCurrentlyAttacking = BlackboardComp->GetValueAsBool(FName("CanAttack"));

			// Not attacking and player is in range then attack
			if (!bCurrentlyAttacking && Distance <= AttackRange)
			{
				BlackboardComp->SetValueAsBool(FName("CanAttack"), true);
			}

			// Attacking and player goes out of range then stop attacking
			if (bCurrentlyAttacking && Distance > AttackRange)
			{
				BlackboardComp->SetValueAsBool(FName("CanAttack"), false);
			}
		}
	}
}
