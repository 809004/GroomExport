// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GroomExport : ModuleRules
{
	public GroomExport(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
			);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Imath",
				"AlembicLib",
				"Core",
				"Engine",
				"HairStrandsCore",
				"HairStrandsEditor"
			});
	}
}
