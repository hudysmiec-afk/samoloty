# Lot, prędkość i audio silnika

## Model prędkości

Samolot zawsze porusza się do przodu. `S` płynnie zmienia prędkość z `ForwardSpeed` do `MinForwardSpeed`; po puszczeniu prędkość wraca do normalnej. Boost ma priorytet nad `S`.

Tempo dochodzenia do prędkości docelowej określa `ForwardSpeedResponse`. Jest to współczynnik interpolacji `FInterpTo`, a nie przyspieszenie wyrażone w `cm/s²`.

## Wpływ kierunku lotu

System nie symuluje fizycznej grawitacji. Używa iloczynu skalarnego kierunku samolotu i światowego wektora góry:

```text
VerticalDirection = dot(ActorForward, WorldUp)
```

- `+1` oznacza pionowy lot w górę,
- `0` lot poziomy,
- `-1` pionowe nurkowanie.

Wartości domyślne:

| Kierunek | Mnożnik prędkości | Mnożnik response |
|---|---:|---:|
| Pionowo w górę | 0.75 | 0.65 |
| Poziomo | 1.00 | 1.00 |
| Pionowo w dół | 1.25 | 1.35 |

Mnożnik jest płynnie interpolowany i stosowany do końcowej prędkości wynikającej z lotu normalnego, `S` lub boosta. Nie istnieje opadanie, przeciągnięcie ani automatyczne opuszczanie nosa.

## Zwrotność

Prędkości obrotu wynikają z:

```text
YawRate = CursorX × MaxYawTurnRate × CurrentTurnRateMultiplier
PitchRate = CursorY × MaxPitchTurnRate × CurrentTurnRateMultiplier
```

Przy prędkości minimalnej używany jest `MinSpeedTurnMultiplier` (domyślnie 1.25), przy normalnej 1.0, a przy pełnej prędkości boosta `BoostSpeedTurnMultiplier` (domyślnie 0.7). Pitch pozostaje ograniczony przez `MaxPitch`.

## `UJetEngineAudioComponent`

Komponent jest czysto lokalną prezentacją i nie replikuje strumieni audio. Każdy klient odtwarza dźwięk każdego istotnego samolotu na podstawie zreplikowanego stanu lotu i boosta.

Trzy pętle są uruchamiane raz i płynnie mieszane:

```text
SlowEngineLoop
NormalEngineLoop
BoostEngineLoop
```

`CrossfadeSpeed` kontroluje szybkość zmian głośności, a `MasterVolume` wspólny poziom warstw. Boost ma priorytet nad slow. Serwer dedykowany wyłącza tick komponentu i nie tworzy `AudioComponent`.

Każda Fala dźwiękowa powinna mieć włączone zapętlanie oraz przypisane tłumienie 3D. Zalecany format źródłowy to WAV 48 kHz mono, PCM 16- lub 24-bit.

## Debug boosta

`JetBoost → Show Boost Debug` pokazuje:

- energię,
- aktualną prędkość w m/s,
- yaw, pitch oraz limit pitchu,
- aktualne maksymalne prędkości yaw/pitch w stopniach na sekundę,
- mnożnik zwrotności,
- stan serwera i `BoostAlpha`.
