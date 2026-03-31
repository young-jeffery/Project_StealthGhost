// Fill out your copyright notice in the Description page of Project Settings.

#include "GhostAIController.h"
#include "AITypes.h"
#include "Perception/AISenseConfig_Hearing.h" // Needed for the ears
#include "GameFramework/Character.h"          // Needed to cast to the player
#include "Engine/Engine.h"                    // Needed for the debug text
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "DrawDebugHelpers.h"
#include "Project_StealthGhostCharacter.h"
#include "Perception/AISense_Hearing.h"

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

    // Enable ticking so we can draw our debug visuals every frame
    PrimaryActorTick.bCanEverTick = true;
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
    // If the noise is from me then ignore it
	if (Actor == GetPawn()) return;

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
                if (SensedCharacter->IsPlayerControlled())
                {
                        // --- ADVANCED LINE OF SIGHT VERIFICATION (Multi-Bone Trace) ---
                        FHitResult HitResult;
                        FCollisionQueryParams TraceParams;
                        TraceParams.AddIgnoredActor(GetPawn()); // Guard ignores himself 

                        FVector GuardEyes = GetPawn()->GetActorLocation() + FVector(0, 0, 70.0f);
                        bool bHasTrueLOS = false;
						FName VisibleBone = NAME_None; // Variable to store which bone we can see (for debugging purposes)

                        // Array of bones to check. 
                        TArray<FName> BonesToCheck = {
                            FName("head"),
                            FName("spine_02"), // Main body/Chest
                            FName("spine_03"), // Main body/Chest
                            FName("spine_04"), // Main body/Chest
                            FName("spine_05"), // Main body/Chest
                            FName("pelvis"),   // Hips
                            FName("thigh_l"),  // Left Thigh
                            FName("thigh_r"),  // Right Thigh
                            FName("hand_r"),    // Right Hand
                            FName("hand_l"),    // Left Hand
                            FName("clavicle_l")    // Clavicle
                        };

                        for (FName BoneName : BonesToCheck)
                        {
                            // Get the location of the current bone we are checking
                            FVector TargetLocation = SensedCharacter->GetMesh()->GetSocketLocation(BoneName);

                            bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
                                HitResult,
                                GuardEyes,
                                TargetLocation,
                                ECC_Visibility,
                                TraceParams
                            );

                            // If we hit nothing, or the thing we hit was the player, we can see this bone!
                            if (!bHitSomething || (HitResult.GetActor() == SensedCharacter))
                            {
                                bHasTrueLOS = true;
								VisibleBone = BoneName; // Store the name of the bone we can see for debugging
                                // We saw at least one part of them, so we break the loop to save performance
                                break;
                            }
                        }

                        // DEBUG VISUALS
                            // If your debug bool is checked in the editor, draw the lasers!
                        if (bShowDebugVisuals)
                        {
                            if (bHasTrueLOS)
                            {
                                FVector ConfirmedBoneLocation = SensedCharacter->GetMesh()->GetSocketLocation(VisibleBone);
                                DrawDebugLine(GetWorld(), GuardEyes, ConfirmedBoneLocation, FColor::Green, false, 2.0f, 0, 1.0f);
                                DrawDebugSphere(GetWorld(), ConfirmedBoneLocation, 10.0f, 8, FColor::Green, false, 2.0f);
                            }
                            else
                            {
                                // No LOS - draw a single red line to the head as indicator
                                FVector HeadLocation = SensedCharacter->GetMesh()->GetSocketLocation(FName("head"));
                                DrawDebugLine(GetWorld(), GuardEyes, HeadLocation, FColor::Red, false, 2.0f, 0, 1.0f);
                            }
                        }
               
                        if (bHasTrueLOS)
                        {
                            CurrentVisibleTarget = Actor; // Start building suspicion!
                        }
                }
                
				// If the character we see is already dead and we haven't discovered the body yet, investigate it!
                    else if (SensedCharacter->bIsDead && !SensedCharacter->bHasBeenDiscovered)
                    {
                        if (!BlackboardComp->GetValueAsObject(FName("TargetActor")))
                        {
                            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Guard: Hmm, What's that? Lemme check."));

                            // Get the direction pointing from the Guard to the Dead Body
                            FVector DirectionToBody = (SensedCharacter->GetActorLocation() - GetPawn()->GetActorLocation()).GetSafeNormal();

                            // Calculate a point 150 units (1.5 meters) BACKWARDS from the body along that direction line
                            FVector StopLocation = SensedCharacter->GetActorLocation() - (DirectionToBody * 150.0f);

                            BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), StopLocation);
                            BlackboardComp->SetValueAsObject(FName("SpottedBody"), SensedCharacter);

							bIsSpooked = true; // Finding a dead body spooks the guard, making them more alert in the future
                        }
                    }
            }
            else
            {
                // Player got out of sight
                // FIX: Cast Actor to APawn so we can safely check IsPlayerControlled()
                if (APawn* SensedPawn = Cast<APawn>(Actor))
                {
                    if (SensedPawn->IsPlayerControlled())
                    {
                        CurrentVisibleTarget = nullptr;

                        // If they weren't fully detected yet but suspicion is high, investigate!
                        if (!BlackboardComp->GetValueAsObject(FName("TargetActor")) && SuspicionLevel > 30.0f)
                        {
                            BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), Actor->GetActorLocation());
                            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Guard: Did I see something?"));
                        }

                        // If they WERE fully detected, go to their last known location
                        if (BlackboardComp->GetValueAsObject(FName("TargetActor")) == Actor)
                        {
                            BlackboardComp->ClearValue(FName("TargetActor"));
                            BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), Actor->GetActorLocation());
                        }
                    }
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
					//// Check where we are currentlty investigating
					//FVector CurrentInvestigateLocation = BlackboardComp->GetValueAsVector(FName("InvestigateLocation"));

     //               //If the noise is at our location or we are already on our way there, then ignore it
					//if (FVector::Dist(CurrentInvestigateLocation, Stimulus.StimulusLocation) < 50.0f) return

					// Investigate the noise location
                    BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), Stimulus.StimulusLocation);

                    // Debug to confirm the guards communicate with each other
                    if (Stimulus.Tag == "Alarm")
                    {
                        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("Guard: Alarm!? On my way bro."));
						bIsSpooked = true; // Hearing an alarm spooks the guard, making them more alert in the future
                    }

                }
            }
        }
    }
}

// Triggers the Behavior Tree
void AGhostAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    if (GuardBehaviorTree != nullptr)
    {
        RunBehaviorTree(GuardBehaviorTree);

		// Cache the blackboard for easy access in other functions
		CachedBlackboard = GetBlackboardComponent();
    }
}

// Debug Visuals Tick function
void AGhostAIController::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn) return;

    UBlackboardComponent* BB = GetBlackboardComponent();

    // --- SUSPICION METER LOGIC ---
    if (BB)
    {
        if (CurrentVisibleTarget)
        {
            float CurrentBuildRate = SuspicionBuildRate;

            // Scale detection speed based on player stance and distance
            if (AProject_StealthGhostCharacter* StealthPlayer = Cast<AProject_StealthGhostCharacter>(CurrentVisibleTarget))
            {
                // Calculate the 3D distance between the guard (GetPawn) and the player
                float Distance = FVector::Dist(GetPawn()->GetActorLocation(), StealthPlayer->GetActorLocation());

                // --- DISTANCE MODIFIERS ---
                // 1 Unreal Unit = 1 cm. So 200.0f is 2 meters.
                if (Distance < 500.0f)
                {
                    CurrentBuildRate *= 20.0f;
                }
                else if (Distance < 700.0f)
                {
                    CurrentBuildRate *= 12.0f;
                }
                else if (Distance > 1000.0f)
                {
                    CurrentBuildRate *= 1.0f;
                }

                // --- STANCE MODIFIERS ---
                // We check the stance AFTER distance. This means if they are crouching 
                // but only 1 meter away, the 20.0x multiplier still makes them get caught quickly!
                if (StealthPlayer->CurrentState == EPlayerMovementState::VE_Crouching)
                {
                    // Crouching reduces visibility build up by 60% (multiplying by 0.4)
                    CurrentBuildRate *= 0.4f;
                }

                // --- MEMORY MODIFIER ---
                // If the guard has been spooked before, they catch you much faster!
                if (bIsSpooked)
                {
                    CurrentBuildRate *= SpookedBuildMultiplier;
                }
            }

            // Increase suspicion
            SuspicionLevel += CurrentBuildRate * DeltaTime;
            SuspicionLevel = FMath::Clamp(SuspicionLevel, 0.0f, MaxSuspicion);

            // Full Detection Trigger
            if (SuspicionLevel >= MaxSuspicion && !BB->GetValueAsObject(FName("TargetActor")))
            {
                BB->SetValueAsObject(FName("TargetActor"), CurrentVisibleTarget);
                BB->ClearValue(FName("InvestigateLocation"));
				bIsSpooked = true; // Once fully detected, the guard is permanently spooked and will be more alert in the future!
                UAISense_Hearing::ReportNoiseEvent(GetWorld(), CurrentVisibleTarget->GetActorLocation(), 1.0f, GetPawn(), 2000.0f, FName("Alarm"));
            }
        }
        else
        {
            // Decay suspicion when target is out of sight
            if (SuspicionLevel > 0.0f)
            {
                SuspicionLevel -= SuspicionDecayRate * DeltaTime;
                SuspicionLevel = FMath::Clamp(SuspicionLevel, 0.0f, MaxSuspicion);
            }
        }

		if (!bShowDebugVisuals) return;

        // Draw Suspicion Text
        if (BB)
        {
            FVector TextLoc = GetPawn()->GetActorLocation() + FVector(0, 0, 130.0f);
            FString SpookedTag = bIsSpooked ? TEXT(" (SPOOKED)") : TEXT("");
            FString SuspicionText = FString::Printf(TEXT("Suspicion: %d%%%s"), FMath::RoundToInt((SuspicionLevel / MaxSuspicion) * 100.0f), *SpookedTag);

            // Turn the text Red if spooked, otherwise keep it Cyan
            FColor TextColor = bIsSpooked ? FColor::Red : FColor::Cyan;

            DrawDebugString(GetWorld(), TextLoc, SuspicionText, nullptr, TextColor, DeltaTime, true);
        }
    }

    // --- DEBUG HEARING RANGE (Yellow Sphere) ---
    // Draws a yellow wireframe sphere around the guard representing their 20m hearing radius
    DrawDebugSphere(GetWorld(), ControlledPawn->GetActorLocation(), HearingConfig->HearingRange, 64, FColor::Yellow, false, -1.0f, 0, 2.0f);

    // --- DEBUG SIGHT RANGE (Green Cone) ---
    FVector EyeLocation;
    FRotator EyeRotation;
    ControlledPawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);

    // Draws a green cone representing the distance and peripheral angle of their vision
    DrawDebugCone(
        GetWorld(),
        EyeLocation,
        EyeRotation.Vector(),
        SightConfig->SightRadius,
        FMath::DegreesToRadians(SightConfig->PeripheralVisionAngleDegrees),
        FMath::DegreesToRadians(SightConfig->PeripheralVisionAngleDegrees),
        64,
        FColor::Green,
        false,
        -1.0f,
        0,
        2.0f
    );

    // --- DEBUG INTERACTION STATE (Floating Text) ---
    if (BB)
    {
        FString CurrentState = TEXT("Patrolling");
        FColor TextColor = FColor::White;

        // Check the blackboard to see what the AI is currently prioritizing
        if (BB->GetValueAsObject(FName("TargetActor")))
        {
            CurrentState = TEXT("CHASING PLAYER!");
            TextColor = FColor::Red;
        }
        else if (BB->GetValueAsBool(FName("bIsRaisingAlarm")))
        {
            CurrentState = TEXT("RAISING ALARM!");
            TextColor = FColor::Orange;
        }
        // Check if the vector is NOT empty/invalid
        else if (BB->GetValueAsVector(FName("InvestigateLocation")) != FAISystem::InvalidLocation)
        {
            CurrentState = TEXT("INVESTIGATING");
            TextColor = FColor::Yellow;
        }

        // Draw the text 100 units above the guard's head
        FVector TextLocation = ControlledPawn->GetActorLocation() + FVector(0, 0, 100.0f);
        DrawDebugString(GetWorld(), TextLocation, CurrentState, nullptr, TextColor, DeltaTime, true);
    }
}

bool AGhostAIController::IsAlerted() const
{
    // If we don't have a Blackboard, assume we aren't alerted
    if (!CachedBlackboard) return false;

    // Are we chasing the player?
    if (CachedBlackboard->GetValueAsObject(FName("TargetActor")) != nullptr) return true;

    // Are we investigating a noise or a body? 
    // FAISystem::InvalidLocation checks if the vector is set
    if (CachedBlackboard->GetValueAsVector(FName("InvestigateLocation")) != FAISystem::InvalidLocation) return true;

    // Are we actively raising an alarm?
    if (CachedBlackboard->GetValueAsBool(FName("bIsRaisingAlarm"))) return true;

    // If all checks fail, the guard is calm and patrolling normally
    return false;
}