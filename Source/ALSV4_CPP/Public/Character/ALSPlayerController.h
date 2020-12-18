// Project:         Advanced Locomotion System V4 on C++
// Copyright:       Copyright (C) 2020 Doğa Can Yanıkoğlu
// License:         MIT License (http://www.opensource.org/licenses/mit-license.php)
// Source Code:     https://github.com/dyanikoglu/ALSV4_CPP
// Original Author: Doğa Can Yanıkoğlu
// Contributors:    


#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ALSPlayerController.generated.h"

class AALSPlayerCharacter;

/**
 * Player controller class
 */
UCLASS(Blueprintable, BlueprintType)
class ALSV4_CPP_API AALSPlayerController : public APlayerController
{
	GENERATED_BODY()

	virtual void OnPossess(APawn* NewPawn) override;
	virtual void OnRep_Pawn() override;

	void SetupCamera();
	
protected:
	/** Main character reference */
	UPROPERTY(BlueprintReadOnly, Category = "ALS Player Controller")
	AALSPlayerCharacter* PossessedCharacter = nullptr;
};
