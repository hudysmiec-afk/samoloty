# Unreal Engine Project

This is an Unreal Engine 5.8 project.

## Rules
- Prefer existing Unreal patterns in this project.
- Do not modify binary assets in Content unless explicitly asked.
- For gameplay code, use UE C++ conventions: UCLASS, UPROPERTY, UFUNCTION, TObjectPtr where appropriate.
- Avoid changing generated files, Intermediate, Binaries, Saved, or DerivedDataCache.

## Build
Use UnrealBuildTool or the generated Visual Studio solution when available.

## Tests
If possible, verify C++ changes by building the editor target.