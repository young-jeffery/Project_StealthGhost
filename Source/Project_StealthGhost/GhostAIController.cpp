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
#include "Perception/AISense_Sight.h"
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
    // If the noise or sight is from me, ignore it
    if (Actor == GetPawn()) return;

    UBlackboardComponent* BlackboardComp = GetBlackboardComponent();
    if (!BlackboardComp) return;

    
    // SIGHT LOGIC
    if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
    {
		// Cast to custom character to access IsDead state and player control status. If the cast fails, this means we sensed something that isn't a character, so we skip the sight logic and go straight to the hearing logic below
        if (AProject_StealthGhostCharacter* SensedCharacter = Cast<AProject_StealthGhostCharacter>(Actor))
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
                    FName VisibleBone = NAME_None;

                    TArray<FName> BonesToCheck = {
                        FName("head"), FName("spine_02"), FName("spine_03"),
                        FName("spine_04"), FName("spine_05"), FName("pelvis"),
                        FName("thigh_l"), FName("thigh_r"), FName("hand_r"),
                        FName("hand_l"), FName("clavicle_l")
                    };

                    for (FName BoneName : BonesToCheck)
                    {
                        FVector TargetLocation = SensedCharacter->GetMesh()->GetSocketLocation(BoneName);

                        bool bHitSomething = GetWorld()->LineTraceSingleByChannel(
                            HitResult, GuardEyes, TargetLocation, ECC_Visibility, TraceParams
                        );

                        if (!bHitSomething || (HitResult.GetActor() == SensedCharacter))
                        {
                            bHasTrueLOS = true;
                            VisibleBone = BoneName;
                            break;
                        }
                    }

                    // DEBUG VISUALS
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
                            FVector HeadLocation = SensedCharacter->GetMesh()->GetSocketLocation(FName("head"));
                            DrawDebugLine(GetWorld(), GuardEyes, HeadLocation, FColor::Red, false, 2.0f, 0, 1.0f);
                        }
                    }

                    if (bHasTrueLOS)
                    {
                        CurrentVisibleTarget = Actor; // Start building suspicion!
                    }
                }

                // If the character we see is already dead and hasn't been discovered yet, investigate it!
                else if (SensedCharacter->bIsDead && !SensedCharacter->bHasBeenDiscovered)
                {
                    if (!BlackboardComp->GetValueAsObject(FName("TargetActor")))
                    {
                        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Guard: Hmm, What's that? Lemme check."));

                        FVector DirectionToBody = (SensedCharacter->GetActorLocation() - GetPawn()->GetActorLocation()).GetSafeNormal();
                        FVector StopLocation = SensedCharacter->GetActorLocation() - (DirectionToBody * 150.0f);

                        BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), StopLocation);
                        BlackboardComp->SetValueAsObject(FName("SpottedBody"), SensedCharacter);
                        bIsSpooked = true;

                        // Get the "Body" that this brain is currently controlling
                        APawn* ControlledBody = GetPawn();

                        // Check if the body exists, and "Cast" it to ensure it is specifically your custom StealthGhost character
                        if (AProject_StealthGhostCharacter* MyGhostChar = Cast<AProject_StealthGhostCharacter>(ControlledBody))
                        {
                            // It is our character! Tell them to equip the gun!
                            MyGhostChar->OnCombatStarted();
                        }
                    }
                }
            }
            else
            {
                // Player got out of sight
                if (APawn* SensedPawn = Cast<APawn>(Actor))
                {
                    if (SensedPawn->IsPlayerControlled())
                    {
                        CurrentVisibleTarget = nullptr;

                        // If they weren't fully detected yet but suspicion is high, investigate!
                        if (!BlackboardComp->GetValueAsObject(FName("TargetActor")) && SuspicionLevel > 50.0f)
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
    }
    
    // HEARING LOGIC
    else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
    {
        if (Stimulus.WasSuccessfullySensed())
        {
            // First confirm that they are not chasing a target
            UObject* CurrentTarget = BlackboardComp->GetValueAsObject(FName("TargetActor"));
            if (!CurrentTarget)
            {
                // Investigate the noise location
                BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), Stimulus.StimulusLocation);

                // Check the tags we set up earlier
                if (Stimulus.Tag == "Alarm")
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("Guard: Alarm!? On my way bro."));
                    bIsSpooked = true;

                    // Get the "Body" that this brain is currently controlling
                    APawn* ControlledBody = GetPawn();

                    // Check if the body exists, and "Cast" it to ensure it is specifically your custom StealthGhost character
                    if (AProject_StealthGhostCharacter* MyGhostChar = Cast<AProject_StealthGhostCharacter>(ControlledBody))
                    {
                        // It is our character! Tell them to equip the gun!
                        MyGhostChar->OnCombatStarted();
                    }
                }
                else if (Stimulus.Tag == "Distraction")
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Guard: What was that noise?"));
                }
                else if (Stimulus.Tag == "Footstep")
                {
                    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, TEXT("Guard: Did I hear footsteps?"));
                }
                else
                {
                    // Fallback in case a noise without a tag, unspecified tag, or misspelt tag is made
                    FString Tag = Stimulus.Tag.ToString();
                    GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::White, FString::Printf(TEXT("Guard: Heard noise: %s"), *Tag));
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

                // Get the "Body" that this brain is currently controlling
                APawn* ControlledBody = GetPawn();

                // Check if the body exists, and "Cast" it to ensure it is specifically your custom StealthGhost character
                if (AProject_StealthGhostCharacter* MyGhostChar = Cast<AProject_StealthGhostCharacter>(ControlledBody))
                {
                    // It is our character! Tell them to equip the gun!
                    MyGhostChar->OnCombatStarted();
                }

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

	// Get the active hearing config from the perception component. 
    // This is necessary because the config can be changed at runtime, so we can't just rely on the default values we set in the constructor.
    FAISenseID HearingID = UAISense::GetSenseID<UAISense_Hearing>();
    UAISenseConfig_Hearing* ActiveHearingConfig = Cast<UAISenseConfig_Hearing>(AIPerception->GetSenseConfig(HearingID));

    // Draws a yellow wireframe sphere around the guard representing their 20m hearing radius
    if (ActiveHearingConfig)
    {
        DrawDebugSphere(GetWorld(), ControlledPawn->GetActorLocation(), ActiveHearingConfig->HearingRange, 64, FColor::Yellow, false, -1.0f, 0, 2.0f);
    }
    

    // --- DEBUG SIGHT RANGE (Green Cone) ---

	// Get the active sight config from the perception component.
    FAISenseID SightID = UAISense::GetSenseID<UAISense_Sight>();
    UAISenseConfig_Sight* ActiveSightConfig = Cast<UAISenseConfig_Sight>(AIPerception->GetSenseConfig(SightID));

    if (ActiveSightConfig)
    {
        FVector EyeLocation;
        FRotator EyeRotation;
        ControlledPawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);

        // Draws a green cone representing the distance and peripheral angle of their vision
        DrawDebugCone(
            GetWorld(),
            EyeLocation,
            EyeRotation.Vector(),
            ActiveSightConfig->SightRadius,
            FMath::DegreesToRadians(ActiveSightConfig->PeripheralVisionAngleDegrees),
            FMath::DegreesToRadians(ActiveSightConfig->PeripheralVisionAngleDegrees),
            64,
            FColor::Green,
            false,
            -1.0f,
            0,
            2.0f
        );
    }
    

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

// Helper function to check if the guard is currently alerted (chasing player, investigating, or raising alarm)
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

bool AGhostAIController::IsInvestigating() const
{
    // If we don't have a Blackboard, we aren't investigating
    if (!CachedBlackboard) return false;

    // Returns TRUE if the vector is set to a real location, and FALSE if it is Invalid
    return CachedBlackboard->GetValueAsVector(FName("InvestigateLocation")) != FAISystem::InvalidLocation;
}