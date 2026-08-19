#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FlightHUDWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UBorder;
class UHorizontalBox;
class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;

enum class ERadarContactIcon : uint8
{
	Level,
	Above,
	Below
};

struct FRadarDisplayContact
{
	FVector2D NormalizedPosition = FVector2D::ZeroVector;
	ERadarContactIcon Icon = ERadarContactIcon::Level;
	bool bSelected = false;
};

struct FDamageNumberDisplay
{
	FVector2D Position = FVector2D::ZeroVector;
	FString Text;
	float Opacity = 1.0f;
	float Scale = 1.0f;
};

/** Local-only combat HUD. A Blueprint widget can derive from this class later. */
UCLASS(Blueprintable)
class SAMOLOTY_API UFlightHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFlightHUDWidget(const FObjectInitializer& ObjectInitializer);
	virtual bool Initialize() override;

	void SetCombatElementsVisible(bool bVisible);
	void SetLockMarkerPosition(bool bVisible, const FVector2D& WidgetPosition);
	void SetRadarContacts(bool bVisible, const TArray<FRadarDisplayContact>& Contacts);
	void SetSelectedTargetIndicator(bool bHasTarget, bool bTargetOnScreen,
		const FVector2D& WidgetPosition, const FVector2D& ScreenDirection,
		const FVector2D& WidgetViewportSize, float DistanceCentimeters);
	void SetAttackerDirections(const TArray<FVector2D>& ScreenDirections,
		const FVector2D& WidgetViewportSize);
	void SetDamageNumbers(const TArray<FDamageNumberDisplay>& DamageNumbers);
	void SetPlayerStatus(float CurrentHealth, float MaxHealth,
		float CurrentBoost, float MaxBoost, FName GunName,
		float GunCooldownRemaining, float GunCooldownDuration,
		FName MissileName, float MissileCooldownRemaining,
		float MissileCooldownDuration, float SpeedCentimetersPerSecond);
	void SetBoundaryWarning(float WarningAlpha, bool bAutomaticReturn);
	void SetAbilityCooldowns(float QuickReversalRemaining,
		float BackwardDashRemaining, float ForwardDashRemaining,
		float BlinkRemaining, float EvasiveRollRemaining);
	void SetPerformanceStats(bool bShowFPS, float FPS, bool bShowPing,
		float PingMilliseconds);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Textures")
	TObjectPtr<UTexture2D> CrosshairTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Textures")
	TObjectPtr<UTexture2D> LockMarkerTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Textures")
	TObjectPtr<UTexture2D> RadarBackgroundTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Textures")
	TObjectPtr<UTexture2D> RadarFrameTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Textures")
	TObjectPtr<UTexture2D> RadarPlayerTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Textures")
	TObjectPtr<UTexture2D> RadarContactTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Textures")
	TObjectPtr<UTexture2D> RadarAboveTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Textures")
	TObjectPtr<UTexture2D> RadarBelowTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Textures")
	TObjectPtr<UTexture2D> RadarTargetTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Targeting|Textures")
	TObjectPtr<UTexture2D> SelectedTargetDirectionTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Targeting|Textures")
	TObjectPtr<UTexture2D> AttackerDirectionTexture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Layout",
		meta=(ClampMin="1"))
	FVector2D CrosshairSize = FVector2D(376.0f, 256.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Targeting|Layout",
		meta=(ClampMin="1"))
	FVector2D WorldTargetMarkerSize = FVector2D(72.0f, 72.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Targeting|Layout",
		meta=(ClampMin="1"))
	FVector2D SelectedTargetDirectionSize = FVector2D(92.0f, 92.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Targeting|Layout",
		meta=(ClampMin="1"))
	FVector2D AttackerDirectionSize = FVector2D(216.0f, 216.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Targeting|Layout",
		meta=(ClampMin="0.05", ClampMax="0.48"))
	float SelectedTargetDirectionRadius = 0.30f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Targeting|Layout",
		meta=(ClampMin="1"))
	float SelectedTargetDistanceFontSize = 22.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Targeting|Layout")
	FVector2D SelectedTargetDistanceOffset = FVector2D(0.0f, 48.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Targeting|Layout",
		meta=(ClampMin="0.05", ClampMax="0.48"))
	float AttackerDirectionRadius = 0.23f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Layout",
		meta=(ClampMin="64"))
	FVector2D RadarSize = FVector2D(320.0f, 320.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Layout",
		meta=(ClampMin="0"))
	FVector2D RadarScreenMargin = FVector2D(32.0f, 32.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Layout",
		meta=(ClampMin="1"))
	FVector2D RadarContactSize = FVector2D(24.0f, 24.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Layout",
		meta=(ClampMin="1"))
	FVector2D RadarSelectedSize = FVector2D(38.0f, 38.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flight HUD|Radar|Layout",
		meta=(ClampMin="1"))
	FVector2D RadarPlayerSize = FVector2D(34.0f, 34.0f);

private:
	void BuildNativeWidgetTree();
	void BuildRadarWidgetTree();
	void BuildStatusWidgetTree();
	void UpdateWeaponCooldown(UProgressBar* Bar, UTextBlock* Text,
		FName WeaponName, float Remaining, float Duration);
	void EnsureRadarContactPool(int32 RequiredCount);
	void EnsureAttackerDirectionPool(int32 RequiredCount);
	void EnsureDamageNumberPool(int32 RequiredCount);
	UTexture2D* GetRadarContactTexture(ERadarContactIcon Icon) const;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CrosshairImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> LockMarkerImage;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> LockMarkerSlot;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SelectedTargetMarkerImage;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> SelectedTargetMarkerSlot;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SelectedTargetDirectionImage;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> SelectedTargetDirectionSlot;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SelectedTargetDistanceText;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> SelectedTargetDistanceSlot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> AttackerDirectionImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCanvasPanelSlot>> AttackerDirectionSlots;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> DamageNumberTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCanvasPanelSlot>> DamageNumberSlots;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RadarCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeedText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> BoostBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BoostText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> GunCooldownBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GunCooldownText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> MissileCooldownBar;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MissileCooldownText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BoundaryWarningText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PerformanceStatsText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> AbilityCooldownContainer;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> AbilityCooldownBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> AbilityCooldownTexts;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RadarSelectedImage;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> RadarSelectedSlot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> RadarContactImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCanvasPanelSlot>> RadarContactSlots;

	bool bCombatElementsVisible = true;
};
