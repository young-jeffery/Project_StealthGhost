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
    SightConfig->SightRadius = 3000; // 30 meters
    SightConfig->LoseSightRadius = 3500.0f; // 35 meters
    SightConfig->PeripheralVisionAngleDegrees = 70.0f; // 140 degree cone
    SightConfig->SetMaxAge(5.0f); // Memory lasts 5 seconds

    // We check all affiliation boxes so the AI doesn't ignore us by default
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;

    // Create and Configure the Ears
    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
    HearingConfig->HearingRange = 3500.0f; // 35 meters
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

    // Start proximity detection at 4x per second so it doesn't run every frame
    GetWorld()->GetTimerManager().SetTimer(ProximityCheckTimerHandle, this, &AGhostAIController::CheckProximity, 0.25f, true);
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
                    // Player entered vision cone!
                    CurrentVisibleTarget = Actor;
                    CoverMultiplier = 1.0f; // Assume fully visible initially

                    // Start the trace timer (5 times a second)
                    GetWorld()->GetTimerManager().SetTimer(VisibilityTimerHandle, this, &AGhostAIController::UpdateVisibilityGating, 0.2f, true);

                    if (bIsSpooked || BlackboardComp->GetValueAsBool(FName("bIsCombatSearch")))
                    {
                        SuspicionLevel = MaxSuspicion;

                        // Lock onto the player
                        BlackboardComp->SetValueAsObject(FName("TargetActor"), Actor);

                        // Broadcast the alarm immediately from this guard's position
                        UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetPawn()->GetActorLocation(), 1.0f, GetPawn(), 5000.0f, FName("Alarm"));

                        // Wipe the search memory so they don't try to go back to investigating
                        BlackboardComp->ClearValue(FName("bIsCombatSearch"));
                        BlackboardComp->ClearValue(FName("InvestigateLocation"));
                        BlackboardComp->ClearValue(FName("bWillWalkToNoise"));
                    }
                }

                // If the character we see is already dead and hasn't been discovered yet, investigate it!
                else if (SensedCharacter->bIsDead && !SensedCharacter->bHasBeenDiscovered)
                {
                    if (!BlackboardComp->GetValueAsObject(FName("TargetActor")))
                    {
                        float CurrentTime = GetWorld()->GetTimeSeconds();
                        bool bIsSuccessiveBody = (CurrentTime - LastBodyDiscoveredTime) < SuccessiveBodyWindow;

                        if (bIsSuccessiveBody)
                        {
                            // Second dead body found within the window, forget investigation and raise alarm
                            SensedCharacter->bHasBeenDiscovered = true;
                            bIsSpooked = true;
                            SuspicionLevel = MaxSuspicion;
                            SuspicionDecayPauseEndTime = CurrentTime + 20.0f;

                            // Broadcast the alarm immediately from this guard's position
                            UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetPawn()->GetActorLocation(), 1.0f, GetPawn(), 5000.0f, FName("Alarm"));

                            // Combat search from the body's location
                            BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), SensedCharacter->GetActorLocation());
                            BlackboardComp->SetValueAsBool(FName("bIsCombatSearch"), true);
                            BlackboardComp->SetValueAsBool(FName("bWillWalkToNoise"), true);

                            if (AProject_StealthGhostCharacter* MyGhostChar = Cast<AProject_StealthGhostCharacter>(GetPawn()))
                            {
                                MyGhostChar->OnCombatStarted();
                            }

                            if (bShowDebugVisuals)
                            {
                                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Guard: Another one! Raising alarm immediately!"));
                            }
                        }
                        else
                        {
                            // First body — investigate normally, record the time
                            LastBodyDiscoveredTime = CurrentTime;

                            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Guard: Hmm, What's that? Lemme check."));

                            FVector DirectionToBody = (SensedCharacter->GetActorLocation() - GetPawn()->GetActorLocation()).GetSafeNormal();
                            FVector StopLocation = SensedCharacter->GetActorLocation() - (DirectionToBody * 150.0f);

                            BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), StopLocation);
                            BlackboardComp->SetValueAsObject(FName("SpottedBody"), SensedCharacter);
                            bIsSpooked = true;

                            if (AProject_StealthGhostCharacter* MyGhostChar = Cast<AProject_StealthGhostCharacter>(GetPawn()))
                            {
                                MyGhostChar->OnCombatStarted();
                            }
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
                        CoverMultiplier = 1.0f; // Reset

                        // Stop firing the traces!
                        GetWorld()->GetTimerManager().ClearTimer(VisibilityTimerHandle);

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
                            BlackboardComp->SetValueAsBool(FName("bIsCombatSearch"), true);
                            BlackboardComp->SetValueAsBool(FName("bWillWalkToNoise"), true);
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
            // Ignore fellow guards footstep
            if (Actor)
            {
                // Ignore self noise
                if (Actor == GetPawn())
                {
                    return;
                }

                // Is it my teammate?
                if (Actor->ActorHasTag(FName("Guard")))
                {
                    // We completely ignore friendly footsteps or accidental bumps.
                    if (Stimulus.Tag == FName("Footstep") || Stimulus.Tag == FName("Distraction"))
                    {
                        return; // Ignore mundane teammate noises
                    }
                }
            }

			// Sound dampening through walls logic
            FHitResult HitResult;
            FCollisionQueryParams TraceParams;
            TraceParams.AddIgnoredActor(GetPawn()); // Ignore the guard's own body

            // Start the check from the guard's ears
            FVector GuardEars = GetPawn()->GetActorLocation() + FVector(0, 0, 70.0f);
            FVector SoundLocation = Stimulus.StimulusLocation;

            // This prevents their destination from being overwritten by their own pain.
            if (FVector::Dist(GetPawn()->GetActorLocation(), SoundLocation) < 50.0f)
            {
                return; // Abort hearing logic completely!
            }

            // Fire a laser directly at the sound
            bool bHitSomething = GetWorld()->LineTraceSingleByChannel(HitResult, GuardEars, SoundLocation, ECC_WorldStatic, TraceParams);

            bool bIsOccluded = false;

            if (bHitSomething)
            {
                // If the object we hit is more than 50cm away from the sound source, it is a genuine obstructing wall, not just the floor!
                if (FVector::Dist(HitResult.ImpactPoint, SoundLocation) > 50.0f)
                {
                    bIsOccluded = true;
                }
            }

            if (bIsOccluded)
            {
                // If sound passes through a wall, then dampen it
                float WallDampeningMultiplier = 0.3f;

                // Dynamically fetch the guard's max hearing range
                FAISenseID HearingID = UAISense::GetSenseID<UAISense_Hearing>();
                UAISenseConfig_Hearing* ActiveHearingConfig = Cast<UAISenseConfig_Hearing>(AIPerception->GetSenseConfig(HearingID));

                float MaxHearingRange = ActiveHearingConfig ? ActiveHearingConfig->HearingRange : 2000.0f;
                float EffectiveHearingRange = MaxHearingRange * WallDampeningMultiplier;

                // How far away is the actual sound?
                float DistanceToSound = FVector::Dist(GuardEars, SoundLocation);

				// If the sound is more than effectiive hearing range away after dampening, ignore it
                if (DistanceToSound > EffectiveHearingRange)
                {
                    // The sound is too far away to be heard through the wall. Ignore it completely!
                    if (bShowDebugVisuals)
                    {
                        DrawDebugLine(GetWorld(), GuardEars, HitResult.ImpactPoint, FColor::Red, false, 2.0f, 0, 1.0f);
                        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("Guard: (Sound muffled by wall)"));
                    }
                    return; // Abort the hearing logic!
                }

                // If theyhear it through a wall, draw a yellow line
                if (bShowDebugVisuals)
                {
                    DrawDebugLine(GetWorld(), GuardEars, HitResult.ImpactPoint, FColor::Yellow, false, 2.0f, 0, 1.0f);
                }
            }
            else
            {
                // Clear path, heard perfectly! Draw a green line.
                if (bShowDebugVisuals)
                {
                    DrawDebugLine(GetWorld(), GuardEars, SoundLocation, FColor::Green, false, 2.0f, 0, 1.0f);
                }
            }

            // First confirm that they are not chasing a target
            UObject* CurrentTarget = BlackboardComp->GetValueAsObject(FName("TargetActor"));
            if (!CurrentTarget)
            {
                // For sounds tagged as distractions
                if (Stimulus.Tag == FName("Distraction"))
                {
                    // If the guard is already at Max Suspicion skip dice roll, instantly update the location and sprint to the new stone
                    if (SuspicionLevel >= 70.0f || BlackboardComp->GetValueAsBool(FName("bIsCombatSearch")))
                    {
                        BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), SoundLocation);
                        BlackboardComp->SetValueAsBool(FName("bWillWalkToNoise"), true);
                        BlackboardComp->SetValueAsBool(FName("bIsCombatSearch"), true); // Ensures they keep sprinting!

                        if (bShowDebugVisuals) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("Guard: Rushing the distraction!"));
                        return; // Skip the rest of the Tier logic!
                    }


                    // Distraction escalation logic
                    float CurrentTime = GetWorld()->GetTimeSeconds();

                    // Did we hear a noise within the threshold?
                    if (CurrentTime - LastTimeSoundHeard <= RapidNoiseThreshold)
                    {
                        RapidNoiseCount++; // Escalate
                    }
                    else
                    {
                        RapidNoiseCount = 1;
                    }

                    LastTimeSoundHeard = CurrentTime;

                    // Evaluate the levels
                    if (RapidNoiseCount == 1)
                    {
                        // First noise, use distance to determine if they guard just looks or walks to noise source. Closer = More likely to walk.
                        float Distance = FVector::Dist(GetPawn()->GetActorLocation(), SoundLocation);

                        // Fetch the max hearing range (fallback to 2000 if not found)
                        FAISenseID HearingID = UAISense::GetSenseID<UAISense_Hearing>();
                        UAISenseConfig_Hearing* ActiveHearingConfig = Cast<UAISenseConfig_Hearing>(AIPerception->GetSenseConfig(HearingID));
                        float MaxHearingRange = ActiveHearingConfig ? ActiveHearingConfig->HearingRange : 2000.0f;

                        // Base chance is 20%. They gain up to +80% more depending on how close the sound is.
                        float WalkChance = 0.2f + (1.0f - (Distance / MaxHearingRange)) * 0.8f;

                        // Roll the dice!
                        bool bWillWalk = FMath::FRand() <= WalkChance;

                        BlackboardComp->SetValueAsBool(FName("bWillWalkToNoise"), bWillWalk);
                        BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), SoundLocation);

                        if (bShowDebugVisuals)
                        {
                            FString Action = bWillWalk ? TEXT("Look & Walk") : TEXT("Look Only");
                            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, FString::Printf(TEXT("Guard: Tier 1 Distraction (%s)"), *Action));
                        }
                    }
                    else if (RapidNoiseCount == 2)
                    {
                        // Second noise, increase suspicion to 50% and pause decay.
                        // We use FMath::Max so we don't accidentally lower their suspicion if they were already at 80%
                        SuspicionLevel = FMath::Max(SuspicionLevel, MaxSuspicion * 0.5f);
                        SuspicionDecayPauseEndTime = CurrentTime + SuspicionDecayPauseDuration;

                        // They walk to investigate
                        BlackboardComp->SetValueAsBool(FName("bWillWalkToNoise"), true);
                        BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), SoundLocation);

                        if (bShowDebugVisuals)
                        {
                            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Orange, TEXT("Guard: Tier 2 Distraction (Escalating, Decay Paused)"));
                        }
                    }
                    else if (RapidNoiseCount >= 3)
                    {
                        // Third noise, full alert and yell to nearby guards.
                        bIsSpooked = true;
                        SuspicionLevel = MaxSuspicion;
						SuspicionDecayPauseDuration += 10.0f; // Pause for 10 extra seconds
                        SuspicionDecayPauseEndTime = CurrentTime + SuspicionDecayPauseDuration;

                        BlackboardComp->SetValueAsBool(FName("WillWalkToNoise"), true);

                        UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetPawn()->GetActorLocation(), 1.0f, GetPawn(), 2500.0f, FName("Alarm"));

                        if (bShowDebugVisuals)
                        {
                            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Guard: Tier 3 Distraction (Calling for backup!)"));
                        }
                    }
                }
              
				// For sounds tagged as bullet whizzes
                else if (Stimulus.Tag == FName("BulletWhiz"))
                {
                    // Ignore repeat whizzes from the same shot burst.
                    float CurrentTime = GetWorld()->GetTimeSeconds();
                    if (CurrentTime - LastBulletWhizTime < BulletWhizCooldown)
                    {
						return; // Ignore extra whizzes from the same shot
                    }
                    LastBulletWhizTime = CurrentTime;

                    // A bullet passes them instantly spooks them 
                    bIsSpooked = true;
                    SuspicionLevel = FMath::Max(SuspicionLevel, MaxSuspicion * 0.8f); // Instant 80% suspicion
					SuspicionDecayPauseEndTime = CurrentTime + 10.0f;   // Pause decay

                    BlackboardComp->SetValueAsBool(FName("bWillWalkToNoise"), true);
                    BlackboardComp->SetValueAsBool(FName("bIsCombatSearch"), true);

                    // Get the "Body" that this brain is currently controlling
                    APawn* ControlledBody = GetPawn();
                    if (AProject_StealthGhostCharacter* MyGhostChar = Cast<AProject_StealthGhostCharacter>(ControlledBody))
                    {
                        MyGhostChar->OnCombatStarted();
                    }

                    // They will investigate where the bullet hit or passed them by
                    BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), SoundLocation);

                    if (bShowDebugVisuals) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Guard: I'm under fire!"));
                }

                // For footstep sounds
                else if (Stimulus.Tag == FName("Footstep"))
                {
                    // Normal investigation sequence
                    BlackboardComp->SetValueAsBool(FName("bWillWalkToNoise"), true);
                    BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), SoundLocation);

                    if (bShowDebugVisuals) GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("Guard: Heard a footstep..."));
                }

                // For alarms and gunshots
                else if (Stimulus.Tag == FName("Alarm"))
                {
                    bIsSpooked = true;
                    SuspicionLevel = MaxSuspicion; // Max out instantly
                    BlackboardComp->SetValueAsBool(FName("bWillWalkToNoise"), true);
                    BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), SoundLocation);
                    
                    // Add a random offset so the backup guards form a firing line instead of trying to stand on the exact same spot
                    FVector FlankingOffset = FVector(FMath::RandRange(-250.0f, 250.0f), FMath::RandRange(-250.0f, 250.0f), 0.0f);
                    BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), SoundLocation + FlankingOffset);

                    BlackboardComp->SetValueAsBool(FName("bIsCombatSearch"), true);

                    // Get the "Body" that this brain is currently controlling
                    APawn* ControlledBody = GetPawn();
                    if (AProject_StealthGhostCharacter* MyGhostChar = Cast<AProject_StealthGhostCharacter>(ControlledBody))
                    {
                        // It is our character! Tell them to equip the gun!
                        MyGhostChar->OnCombatStarted();
                    }
                }

				// For sounds tagged as SpottedPlayer
                else if (Stimulus.Tag == FName("SpottedPlayer"))
                {
                    // The shouting guard is Actor. Look up their TargetActor to find where the
                    // player actually is, rather than just moving to the shouting guard's position.
                    FVector PlayerLoc = SoundLocation; // fallback — shouting guard's position

                    if (ACharacter* ShoutingGuard = Cast<ACharacter>(Actor))
                    {
                        if (AGhostAIController* ShoutingAI = Cast<AGhostAIController>(ShoutingGuard->GetController()))
                        {
                            if (UBlackboardComponent* ShoutBB = ShoutingAI->GetBlackboardComponent())
                            {
                                if (APawn* SpottedPlayer = Cast<APawn>(ShoutBB->GetValueAsObject(FName("TargetActor"))))
                                {
                                    PlayerLoc = SpottedPlayer->GetActorLocation();
                                }
                            }
                        }
                    }

                    bIsSpooked = true;
                    SuspicionLevel = MaxSuspicion;
                    SuspicionDecayPauseEndTime = GetWorld()->GetTimeSeconds() + 20.0f;

                    // Spread guards out so they don't stack on the same tile
                    FVector FlankingOffset = FVector(FMath::RandRange(-250.0f, 250.0f), FMath::RandRange(-250.0f, 250.0f), 0.0f);

                    BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), PlayerLoc + FlankingOffset);
                    BlackboardComp->SetValueAsBool(FName("bIsCombatSearch"), true);
                    BlackboardComp->SetValueAsBool(FName("bWillWalkToNoise"), true);

                    if (AProject_StealthGhostCharacter* MyGhostChar = Cast<AProject_StealthGhostCharacter>(GetPawn()))
                    {
                        MyGhostChar->OnCombatStarted();
                    }

                    if (bShowDebugVisuals)
                    {
                        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Guard: Player spotted — moving in!"));
                    }
                }

				// For sounds tagged as GuardDown (a guard has been killed)
                else if (Stimulus.Tag == FName("GuardDown"))
                {
					// If the dead guard has already been discovered, ignore this event
                    if (AProject_StealthGhostCharacter* DeadGuard = Cast<AProject_StealthGhostCharacter>(Actor))
                    {
                        if (DeadGuard->bHasBeenDiscovered) return;
                    }


                    bIsSpooked = true;
                    SuspicionLevel = MaxSuspicion;
                    SuspicionDecayPauseEndTime = GetWorld()->GetTimeSeconds() + 20.0f;

                    // Run a LOS check to the player. If clear, they witnessed the kill — chase directly.
                    // If blocked, treat it as a combat search toward the dead guard's position.
                    bool bCanSeeShooter = false;

                    APlayerController* PC = GetWorld()->GetFirstPlayerController();
                    if (PC && PC->GetPawn())
                    {
                        APawn* PlayerPawn = PC->GetPawn();
                        FVector GuardEyes = GetPawn()->GetActorLocation() + FVector(0, 0, 70.0f);

                        FHitResult LOSHit;
                        FCollisionQueryParams LOSParams;
                        LOSParams.AddIgnoredActor(GetPawn());
                        LOSParams.AddIgnoredActor(PlayerPawn);

                        bool bHit = GetWorld()->LineTraceSingleByChannel(
                            LOSHit, GuardEyes, PlayerPawn->GetActorLocation(), ECC_Visibility, LOSParams);
                        bCanSeeShooter = !bHit;

                        if (bCanSeeShooter)
                        {
                            // Witnessed the kill — lock on directly, activates Chase branch
                            BlackboardComp->SetValueAsObject(FName("TargetActor"), PlayerPawn);
                            BlackboardComp->ClearValue(FName("InvestigateLocation"));
                            BlackboardComp->ClearValue(FName("bWillWalkToNoise"));
                            BlackboardComp->ClearValue(FName("bIsCombatSearch"));
                        }
                    }

                    if (!bCanSeeShooter)
                    {
                        // Couldn't see the shooter — investigate where the guard fell
                        BlackboardComp->SetValueAsVector(FName("InvestigateLocation"), SoundLocation);
                        BlackboardComp->SetValueAsBool(FName("bIsCombatSearch"), true);
                        BlackboardComp->SetValueAsBool(FName("bWillWalkToNoise"), true);
                    }

                    if (AProject_StealthGhostCharacter* MyGhostChar = Cast<AProject_StealthGhostCharacter>(GetPawn()))
                    {
                        MyGhostChar->OnCombatStarted();
                    }

                    if (bShowDebugVisuals)
                    {
                        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Guard: Man down!"));
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

            // Scale detection speed based on player stance, distance and angle
            if (AProject_StealthGhostCharacter* StealthPlayer = Cast<AProject_StealthGhostCharacter>(CurrentVisibleTarget))
            {
                // --- ANGLE MODIFIER ---
                // Get the direction from the guard to the player
                FVector DirectionToTarget = (StealthPlayer->GetActorLocation() - GetPawn()->GetActorLocation()).GetSafeNormal();

                // Get where the guard is currently looking
                FVector GuardForward = GetPawn()->GetActorForwardVector();

                // Compare them (1.0 = dead center, 0.5 = edge of 60-degree vision)
                float ViewDotProduct = FVector::DotProduct(GuardForward, DirectionToTarget);

                // Map the dot product to a speed multiplier (Edge = 0.3x speed, Center = 1.2x speed)
                FVector2D InputRange(0.5f, 1.0f);
                FVector2D OutputRange(0.3f, 1.2f);
                float AngleMultiplier = FMath::GetMappedRangeValueClamped(InputRange, OutputRange, ViewDotProduct);

                // Apply the angle multiplier first
                CurrentBuildRate *= AngleMultiplier;

                //Apply the Partial Cover multiplier next
                CurrentBuildRate *= CoverMultiplier;

                // Calculate the 3D distance between the guard (GetPawn) and the player
                float Distance = FVector::Dist(GetPawn()->GetActorLocation(), StealthPlayer->GetActorLocation());

                // --- DISTANCE MODIFIERS ---
                // 1 Unreal Unit = 1 cm. So 200.0f is 2 meters.
                if (Distance < 500.0f)
                {
                    CurrentBuildRate *= 10.0f;
                }
                else if (Distance < 1500.0f)
                {
                    CurrentBuildRate *= 3.0f;
                }
                else
                {
                    CurrentBuildRate *= 1.0f;
                }

                // --- STANCE MODIFIERS ---
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

                // Broadcast from this guard's position using the SpottedPlayer tag.
                // Backup guards receiving this will look up this guard's TargetActor to get the player's exact location
                UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetPawn()->GetActorLocation(), 1.0f, GetPawn(), 5000.0f, FName("SpottedPlayer"));
            }
        }
        else
        {
            // Decay suspicion when target is out of sight
            if (SuspicionLevel > 0.0f && GetWorld()->GetTimeSeconds() > SuspicionDecayPauseEndTime)
            {
                SuspicionLevel -= SuspicionDecayRate * DeltaTime;
                SuspicionLevel = FMath::Clamp(SuspicionLevel, 0.0f, MaxSuspicion);
            }
        }

		if (!bShowDebugVisuals) return;
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

void AGhostAIController::UpdateVisibilityGating()
{
    if (!CurrentVisibleTarget || !GetPawn()) return;

    AProject_StealthGhostCharacter* StealthPlayer = Cast<AProject_StealthGhostCharacter>(CurrentVisibleTarget);
    if (!StealthPlayer) return;

    FVector GuardEyes = GetPawn()->GetActorLocation() + FVector(0, 0, 70.0f);
    FHitResult HitResult;
    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredActor(GetPawn()); // Ignore the guard

    // Fire a single laser at the center of the player's body
    bool bHitCenter = GetWorld()->LineTraceSingleByChannel(HitResult, GuardEyes, StealthPlayer->GetActorLocation(), ECC_Visibility, TraceParams);

	// If it hit the player directly, they are fully exposed, end here and set the multiplier to 1.0 (100% detection speed)
    if (bHitCenter && HitResult.GetActor() == StealthPlayer)
    {
        CoverMultiplier = 1.0f;

        // DEBUG: Draw a solid green line to the center of the player
        if (bShowDebugVisuals)
        {
            DrawDebugLine(GetWorld(), GuardEyes, HitResult.ImpactPoint, FColor::Green, false, 0.25f, 0, 2.0f);
            DrawDebugString(GetWorld(), StealthPlayer->GetActorLocation() + FVector(0, 0, 50.0f), TEXT("Cover: 100% (Center Visible)"), nullptr, FColor::Green, 0.25f, true);
        }

        return;
    }

    // If the center is blocked, fire at individual bones to see how much is exposed.
    TArray<FName> BonesToCheck = {
        FName("head"), FName("spine_02"), FName("spine_03"),
        FName("spine_04"), FName("spine_05"), FName("pelvis"),
        FName("thigh_l"), FName("thigh_r"), FName("hand_r"),
        FName("hand_l"), FName("clavicle_l")
    };

    int32 VisibleBones = 0;

    for (FName BoneName : BonesToCheck)
    {
        FVector TargetLocation = StealthPlayer->GetMesh()->GetSocketLocation(BoneName);
        bool bHitBone = GetWorld()->LineTraceSingleByChannel(HitResult, GuardEyes, TargetLocation, ECC_Visibility, TraceParams);

        if (!bHitBone || HitResult.GetActor() == StealthPlayer)
        {
            VisibleBones++;

            // DEBUG: Draw green lines/spheres for visible bones
            if (bShowDebugVisuals)
            {
                DrawDebugLine(GetWorld(), GuardEyes, TargetLocation, FColor::Green, false, 0.25f, 0, 1.0f);
                DrawDebugSphere(GetWorld(), TargetLocation, 8.0f, 8, FColor::Green, false, 0.25f);
            }
        }
        else
        {
            // DEBUG: Draw red lines stopping at the cover that blocked the bone
            if (bShowDebugVisuals)
            {
                DrawDebugLine(GetWorld(), GuardEyes, HitResult.ImpactPoint, FColor::Red, false, 0.25f, 0, 0.5f);
            }
        }
    }

    // Calculate the ratio. If 5 out of 11 bones are visible, CoverMultiplier becomes ~0.45 (45% detection speed).
    CoverMultiplier = (float)VisibleBones / (float)BonesToCheck.Num();

    // DEBUG: Print the ratio text
    if (bShowDebugVisuals)
    {
        FString DebugText = FString::Printf(TEXT("Cover: %d%% (%d/%d Bones)"), FMath::RoundToInt(CoverMultiplier * 100.0f), VisibleBones, BonesToCheck.Num());
        DrawDebugString(GetWorld(), StealthPlayer->GetActorLocation() + FVector(0, 0, 50.0f), DebugText, nullptr, FColor::Yellow, 0.25f, true);
    }
}

void AGhostAIController::CheckProximity()
{
    if (!GetPawn()) return;

    UBlackboardComponent* BB = GetBlackboardComponent();
    if (!BB) return;

    // Already chasing, no need for proximity detection
    if (BB->GetValueAsObject(FName("TargetActor"))) return;

	// Get the player controller and pawn
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC || !PC->GetPawn()) return; // If the player controller or pawn is null, exit early

    APawn* PlayerPawn = PC->GetPawn();
    AProject_StealthGhostCharacter* PlayerChar = Cast<AProject_StealthGhostCharacter>(PlayerPawn);

    // Don't trigger on a dead player
    if (!PlayerChar || PlayerChar->bIsDead) return;

    float DistToPlayer = FVector::Dist(GetPawn()->GetActorLocation(), PlayerPawn->GetActorLocation());

    if (DistToPlayer <= ProximityAlertRadius)
    {
        // Player is too close, instant full alert
        bIsSpooked = true;
        SuspicionLevel = MaxSuspicion;
        SuspicionDecayPauseEndTime = GetWorld()->GetTimeSeconds() + 20.0f;

        BB->SetValueAsObject(FName("TargetActor"), PlayerPawn);
        BB->ClearValue(FName("InvestigateLocation"));
        BB->ClearValue(FName("bWillWalkToNoise"));
        BB->ClearValue(FName("bIsCombatSearch"));

        // Update visible target so the suspicion meter tick doesn't fight with the alert
        CurrentVisibleTarget = PlayerPawn;

        if (AProject_StealthGhostCharacter* MyGhostChar = Cast<AProject_StealthGhostCharacter>(GetPawn()))
        {
            MyGhostChar->OnCombatStarted();
        }

        if (bShowDebugVisuals)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Guard: TOO CLOSE!"));
        }
    }
}

void AGhostAIController::OnWitnessedGuardDeath(AProject_StealthGhostCharacter* DeadGuard)
{
    if (!GetPawn() || !DeadGuard) return;

    UBlackboardComponent* BB = GetBlackboardComponent();
    if (!BB) return;

    // Mark the body as discovered
    DeadGuard->bHasBeenDiscovered = true;

    // Already chasing the player, spook and keep the existing TargetActor, don't overwrite
    bIsSpooked = true;
    SuspicionLevel = MaxSuspicion;
    SuspicionDecayPauseEndTime = GetWorld()->GetTimeSeconds() + 20.0f;

    if (!BB->GetValueAsObject(FName("TargetActor")))
    {
        // We saw the guard drop but can't confirm the shooter yet.
        // Do a LOS check to the player right now.
        bool bCanSeeShooter = false;

        APlayerController* PC = GetWorld()->GetFirstPlayerController();
        if (PC && PC->GetPawn())
        {
            APawn* PlayerPawn = PC->GetPawn();
            FVector GuardEyes = GetPawn()->GetActorLocation() + FVector(0, 0, 70.0f);

            FHitResult LOSHit;
            FCollisionQueryParams LOSParams;
            LOSParams.AddIgnoredActor(GetPawn());
            LOSParams.AddIgnoredActor(PlayerPawn);

            bool bHit = GetWorld()->LineTraceSingleByChannel(LOSHit, GuardEyes, PlayerPawn->GetActorLocation(), ECC_Visibility, LOSParams);
            bCanSeeShooter = !bHit;

            if (bCanSeeShooter)
            {
                // Saw the kill happen and can see the shooter
                BB->SetValueAsObject(FName("TargetActor"), PlayerPawn);
                BB->ClearValue(FName("InvestigateLocation"));
                BB->ClearValue(FName("bWillWalkToNoise"));
                BB->ClearValue(FName("bIsCombatSearch"));
            }
        }

        if (!bCanSeeShooter)
        {
            // Saw the guard drop but shooter is behind cover — investigate the body's position
            BB->SetValueAsVector(FName("InvestigateLocation"), DeadGuard->GetActorLocation());
            BB->SetValueAsBool(FName("bIsCombatSearch"), true);
            BB->SetValueAsBool(FName("bWillWalkToNoise"), true);
        }
    }

    if (AProject_StealthGhostCharacter* MyGhostChar = Cast<AProject_StealthGhostCharacter>(GetPawn()))
    {
        MyGhostChar->OnCombatStarted();
    }

    if (bShowDebugVisuals)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, TEXT("Guard: I saw that! My teammate is down!"));
    }
}