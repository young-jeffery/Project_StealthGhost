// Fill out your copyright notice in the Description page of Project Settings.

#include "GhostAIController.h"
#include "Perception/AISenseConfig_Hearing.h" // Needed for the ears
#include "GameFramework/Character.h"          // Needed to cast to the player
#include "Engine/Engine.h"                    // Needed for the debug text
#include "BehaviorTree/BehaviorTree.h"

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
    // Did the AI sense a Character?
    if (ACharacter* SensedCharacter = Cast<ACharacter>(Actor))
    {
        // Was it a successful detection? (True = entered radius, False = left radius)
        if (Stimulus.WasSuccessfullySensed())
        {
            // Check WHICH sense was triggered by comparing the ID
            if (Stimulus.Type == SightConfig->GetSenseID())
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("AI: I SEE YOU!"));
            }
            else if (Stimulus.Type == HearingConfig->GetSenseID())
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("AI: I HEARD SOMETHING!"));
            }
        }
        else
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("AI: I lost track of them."));
        }
    }
}

void AGhostAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (GuardBehaviorTree != nullptr)
    {
        RunBehaviorTree(GuardBehaviorTree);
    }
}