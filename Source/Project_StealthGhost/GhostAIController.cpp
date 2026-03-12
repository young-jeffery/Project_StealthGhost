// Fill out your copyright notice in the Description page of Project Settings.

#include "GhostAIController.h"
#include "Perception/AISenseConfig_Hearing.h" // Needed for the ears
#include "GameFramework/Character.h"          // Needed to cast to the player
#include "Engine/Engine.h"                    // Needed for the debug text
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Project_StealthGhostCharacter.h"
//#include "Perception/AISense_Hearing.h"

// Constructor - This runs once when the AI is created to set up its components.
AGhostAIController::AGhostAIController()
{
    // Create the Brain
    AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerception"));
    SetPerceptionComponent(*AIPerception);

    // Create and Configure the Eyes
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
    SightConfig->SightRadius = 1500.0f; // 15 meters
    SightConfig->LoseSightRadius = 1700.0f; // 17 meters
    SightConfig->PeripheralVisionAngleDegrees = 60.0f; // 120 degree cone
    SightConfig->SetMaxAge(5.0f); // Memory lasts 5 seconds

    // We check all affiliation boxes so the AI doesn't ignore us by default
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

    // Create and Configure the Ears
    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
    HearingConfig->HearingRange = 2000.0f; // 20 meters
    HearingConfig->SetMaxAge(5.0f);
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;

    // Plug both senses into the brain
    AIPerception->ConfigureSense(*SightConfig);
    AIPerception->ConfigureSense(*HearingConfig);
    AIPerception->SetDominantSense(SightConfig->GetSenseImplementation());
}

// The BeginPlay function
void AGhostAIController::BeginPlay()
{
    Super::BeginPlay();

    // The Delegate Binding: Tell the brain to call OUR function when it senses something
    if (AIPerception)
    {
        AIPerception->OnTargetPerceptionUpdated.AddDynamic(this, &AGhostAIController::OnTargetDetected);
    }
}

// What sense was triggered
void AGhostAIController::OnTargetDetected(AActor* Actor, FAIStimulus Stimulus)
{
    // Cast to custom character to access IsDead state
    if (AProject_StealthGhostCharacter* SensedCharacter = Cast<AProject_StealthGhostCharacter>(Actor))
    {
        UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
        if (!BlackboardComp) return;

        // Sight Logic
        if (Stimulus.Type == SightConfig->GetSenseID())
        {
            if (Stimulus.WasSuccessfullySensed())
            {
                // Is it the player?
                if (SensedCharacter->IsPlayerControlled())
                {
                    if (!BlackboardComp->GetValueAsObject(FName("TargetActor")))
                    {
                        // Chase and clear investigation points
                        BlackboardComp->SetValueAsObject(FName("TargetActor"), Actor);
                        BlackboardComp->ClearValue(FName("InvestigateLocation"));

                        // Call for backup
                        UAISense_Hearing::ReportNoiseEvent(GetWorld(), SensedCharacter->GetActorLocation(), 1.0f, GetPawn(), 2000.0f, FName("Alarm"));
                    }
                }
                // Not the player. Is it a dead guard?
                else if (SensedCharacter->bIsDead)
                {
                    // Check if there is already a target
                    if (!BlackboardComp->GetValueAsObject(FName("TargetActor")))
                    {
                        // Debug Text
                        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Guard: HEYYY!!! There's a dead body here. Raise the alarm"));

                        // Go to the location
                        BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), SensedCharacter->GetActorLocation());

                        // Alert other nearby guards
                        UAISense_Hearing::ReportNoiseEvent(GetWorld(), SensedCharacter->GetActorLocation(), 1.0f, GetPawn(), 0.0f, FName("Alarm"));
                    }
                }
            }
            else
            {
                // If the player gets out of sight
                if (SensedCharacter->IsPlayerControlled())
                {
                    BlackboardComp->ClearValue(FName("TargetActor"));
                    BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), Actor->GetActorLocation());
                }
            }
        }
        // Hearing Logic
        else if (Stimulus.Type == HearingConfig->GetSenseID())
        {
            if (Stimulus.WasSuccessfullySensed())
            {
                // First confirm that they are not chasing a target
                UObject* CurrentTarget = BlackboardComp->GetValueAsObject(FName("TargetActor"));
                if (!CurrentTarget)
                {
                    // Investigate if any soound is heard
                    BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), Stimulus.StimulusLocation);

                    // Debug to confirm the guards communicate with each other
                    if (Stimulus.Tag == "Alarm")
                    {
                        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("Guard: Alarm!? On my way bro."));
                    }

                }
            }
        }
    }
}
    
    
    //// Did the AI sense a Character?
    //if (ACharacter* SensedCharacter = Cast<ACharacter>(Actor))
    //{
    //    // Prevent the guards from seeing themselves as enemies
    //    if (!SensedCharacter->IsPlayerControlled()) return;
    //    // Gets access to the blackboard
    //    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    //    if (!BlackboardComp) return;

    //    // Sight logic
    //    if (Stimulus.Type == SightConfig->GetSenseID())
    //    {
    //        if (Stimulus.WasSuccessfullySensed())
    //        {
    //            // Chase the player if seen and clear investigation points
    //            BlackboardComp->SetValueAsObject(FName("TargetActor"), Actor);
    //            BlackboardComp->ClearValue(FName("InvestigateLocation"));
    //        }
    //        else
    //        {
    //            // after losing sight, stop chasing but investigate last known location
    //            BlackboardComp->ClearValue(FName("TargetActor"));
    //            BlackboardComp->SetValueAsVector(FName("InvestigateLoccation"), Actor->GetActorLocation());
    //        }
    //    }
    //    // Hearing Logic
    //    else if (Stimulus.Type == HearingConfig->GetSenseID())
    //    {
    //        if (Stimulus.WasSuccessfullySensed())
    //        {
    //            // This enables investigate through hearing only if the player isn't currently being chased
    //            UObject* CurrentTarget = BlackboardComp->GetValueAsObject(FName("TargetActor"));
    //            if (!CurrentTarget)
    //            {
    //                BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), Actor->GetActorLocation());
    //            }
    //        }
    //    }

    //    // Was it a successful detection? (True = entered radius, False = left radius)
    //    if (Stimulus.WasSuccessfullySensed())
    //    {
    //        BlackboardComp->SetValueAsObject(FName("TargetActor"), Actor);
    //        // Check WHICH sense was triggered by comparing the ID
    //        if (Stimulus.Type == SightConfig->GetSenseID())
    //        {
    //            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("AI: I see you!"));
    //            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("AI: CHASING SIGHT"));
    //        }
    //        else if (Stimulus.Type == HearingConfig->GetSenseID())
    //        {
    //            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("AI: I heard something."));
    //            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("AI: CHASING SOUND"));
    //        }
    //    }
    //    else
    //    {
    //        BlackboardComp->ClearValue(FName("TargetActor"));
    //        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("AI: I lost track of them."));
    //    }
    //}

void AGhostAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (GuardBehaviorTree != nullptr)
    {
        RunBehaviorTree(GuardBehaviorTree);
    }
}