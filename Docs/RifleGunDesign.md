# RifleGun — założenia prototypu

## Stan implementacji

- [x] Ciągły ogień pod lewym przyciskiem myszy
- [x] Dwa regulowane punkty luf strzelające naprzemiennie
- [x] Serwerowy hitscan i obrażenia
- [x] Nieskończona amunicja, bez przeładowania i przegrzania
- [x] Lekki lokalny `ARifleTracerVisual` bez kolizji i replikacji
- [x] Niagara muzzle flash przypinana do aktualnej lufy
- [x] Niagara efektu trafienia pozostająca w świecie
- [x] Przestrzenny dźwięk strzału i trafienia
- [x] Debug strzałów, trafień i linii trace
- [ ] Dokładne hitboxy samolotu i kanał `WeaponTrace`
- [ ] Hit marker potwierdzany przez serwer
- [ ] Server rewind

## Statystyki początkowe

- Obrażenia: 50
- Szybkostrzelność: 20 strzałów/s
- Zasięg: 200000 cm (2000 m)
- Rozrzut: 0.25 stopnia
- Prędkość wizualnego tracera: 200000 cm/s
- Długość tracera: 500 cm
- Szerokość tracera: 5 cm

## Sieć

- Właściciel natychmiast tworzy przewidywany efekt kosmetyczny.
- Serwer niezależnie wykonuje właściwy trace na kanale `Visibility` i zadaje obrażenia.
- Niezawodna zmiana stanu przycisku uruchamia/zatrzymuje ogień na serwerze.
- Niezawodność nie jest używana dla kosmetyki każdego strzału; `Unreliable Multicast` przekazuje tor pozostałym klientom.
- Tracer jest lokalnym aktorem prezentacji i nigdy nie steruje trafieniem ani damage.
- Obecna kapsuła Pawna musi blokować `Visibility`.

## Konfiguracja w `BP_PlayerPlane`

Komponent `RifleGun` udostępnia:

```text
Tracer Visual Class → BP_RifleTracer
Muzzle Flash Effect → NS_MuzzleFlashRifle
Impact Effect → NS_ImpactRifle
Rifle Fire Sound → SW_RifleFire lub Sound Cue
Rifle Impact Sound → SW_RifleImpact lub Sound Cue
```

`GunMuzzleLeft` i `GunMuzzleRight` powinny znajdować się na końcach luf. Ich lokalna czerwona oś X wskazuje kierunek strzału.

Muzzle flash jest tworzony przez `SpawnSystemAttached`, automatycznie usuwany i podąża za właściwą lufą. Dla cząstek, które również mają poruszać się razem z lufą, emiter Niagara powinien używać `Local Space`. Impact jest tworzony w miejscu trafienia w przestrzeni świata.

## Tracer

`BP_RifleTracer` dziedziczy po `ARifleTracerVisual`. Jego `TracerMesh` używa cube'a skierowanego długością w lokalnej osi X oraz materiału `Unlit/Emissive`. Kod steruje długością i szerokością, przesuwa czoło tracera z ustawioną prędkością i zatrzymuje je dokładnie w punkcie wyniku hitscanu.

## Assety

- `Content/Main/Blueprints/BP_RifleTracer`
- `Content/Main/SystemNiagara/NS_MuzzleFlashRifle`
- `Content/Main/SystemNiagara/NS_ImpactRifle`
- `Content/Main/Audio/Weapons/SW_RifleFire`
- `Content/Main/Audio/Weapons/SW_RifleImpact`
- `Content/Main/Audio/Weapons/ATT_Global`
