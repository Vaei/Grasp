// Copyright (c) Jared Taylor

using UnrealBuildTool;

public class Grasp : ModuleRules
{
	public Grasp(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"GameplayTags",
				"GameplayTasks",
				"GameplayAbilities",
				"TargetingSystem",
				"DeveloperSettings",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"NetCore",
				"AIModule",
				"UMG",
			}
			);
	}
}
