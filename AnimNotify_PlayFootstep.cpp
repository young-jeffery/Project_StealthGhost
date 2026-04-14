// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimNotify_PlayFootstep.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Kismet/GameplayStatics.h"

void UAnimNotify_PlayFootstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!MeshComp || !MeshComp->GetOwner()) return;

    // Check if the owner is a Character
    if (ACharacter* Character = Cast<ACharacter>(MeshComp->GetOwner()))
    {
        // Are they on the floor?
        if (Character->GetCharacterMovement()->IsMovingOnGround())
        {
			// Play audio file if set in the editor!
            if (FootstepSound)
            {
                UGameplayStatics::PlaySoundAtLocation(MeshComp->GetWorld(), FootstepSound, Character->GetActorLocation());
            }

            // Report the noise to the AI
            UAISense_Hearing::ReportNoiseEvent(
                MeshComp->GetWorld(),
                Character->GetActorLocation(),
                NoiseVolume,
                Character,
                NoiseRange,
                FName("Footstep") // Tag for the AIController
            );
        }
    }
}
