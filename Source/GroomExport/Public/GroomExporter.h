// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Exporters/Exporter.h"
#include "GroomExporter.generated.h"

/**
 * 
 */
UCLASS()
class GROOMEXPORT_API UGroomExporter : public UExporter
{
	GENERATED_BODY()

public:
	UGroomExporter();

	virtual bool ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags = 0) override;
	
};
