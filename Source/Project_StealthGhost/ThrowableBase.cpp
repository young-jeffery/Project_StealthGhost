// Fill out your copyright notice in the Description page of Project Settings.


#include "ThrowableBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Perception/AISense_Hearing.h" // Required to broadcast noise to the AI
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
AThrowableBase::AThrowableBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// Setup Collision Sphere
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(10.0f);

	// Using the standard PhysicsActor profile so it hits walls and floors
	CollisionComp->SetCollisionProfileName(TEXT("PhysicsActor"));
	// Make the	engine trigger the OnHit event when this component hits something
	CollisionComp->SetNotifyRigidBodyCollision(true);
	// Use Continuous Collision Detection to prevent tunneling at high speeds
	CollisionComp->BodyInstance.bUseCCD = true;

	// Bind our custom OnHit function to the engine's built-in hit event
	CollisionComp->OnComponentHit.AddDynamic(this, &AThrowableBase::OnHit);
	RootComponent = CollisionComp;

	// Setup Mesh
	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	ItemMesh->SetupAttachment(RootComponent);
	// Turn off mesh collision. The invisible sphere handles the physics much cheaper.
	ItemMesh->SetCollisionProfileName(TEXT("NoCollision"));

	// Setup Projectile Movement
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileComp"));
	ProjectileMovement->UpdatedComponent = CollisionComp;

	// Speed Setup 
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 2000.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true; // Allows the stone to skip on the ground
	ProjectileMovement->Bounciness = 0.4f;
}

// Called when the game starts or when spawned
void AThrowableBase::BeginPlay()
{
	Super::BeginPlay();

	// Apply the Blueprint's custom physics settings to the projectile component
	if (ProjectileMovement)
	{
		ProjectileMovement->Bounciness = ObjectBounciness;
		ProjectileMovement->Friction = ObjectFriction;
	}

}

// Called every frame
void AThrowableBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AThrowableBase::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Only make a distraction noise on the very first bounce.
	// We don't want the AI investigating the 3rd tiny bounce instead of the initial impact.
	if (!bHasMadeNoise && OtherActor != this)
	{
		// Get how fast the object was moving at the moment of impact. We can use this to make louder noises for faster throws, and softer noises for gentle drops
		float ImpactSpeed = GetVelocity().Size();

		
		if (ImpactSpeed < 200.0f)
		{
			return; // make no sound if the imppact is too soft.
		}

		bHasMadeNoise = true;

		// Dynamic Loudness Math
		// We divide the speed by 1500 (a standard hard throw speed) to get a percentage.
		// Then we clamp it between 0.3 (30%) and 1.0 (100%) so it's never too quiet or absurdly loud.
		float SpeedMultiplier = FMath::Clamp(ImpactSpeed / 1500.0f, 0.3f, 1.0f);

		float FinalLoudness = DistractionLoudness * SpeedMultiplier;
		float FinalRange = MaxRange * SpeedMultiplier;

		// Get the player pawn as the instigator of the noise
		AActor* NoiseInstigator = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);

		// Broadcast the noise. 
		// We tag it "Distraction" so the AI can distinguish it from footsteps or alarms later.
		UAISense_Hearing::ReportNoiseEvent(GetWorld(), GetActorLocation(), FinalLoudness, NoiseInstigator, FinalRange, FName("Distraction"));

		// Debug visual
		if (GEngine)
		{
			FString DebugMsg = FString::Printf(TEXT("Stone Hit! Speed: %f | Range: %f"), ImpactSpeed, FinalRange);
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Cyan, DebugMsg);
		}
	}
}