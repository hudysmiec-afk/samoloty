# Stan projektu

Ostatnia aktualizacja dokumentu: 2026-08-21.

## Punkt odniesienia

- Silnik: Unreal Engine 5.8 (wersja source przeniesiona na nowy dysk).
- Workspace: `U:\Dane\Projekty_UE\Samoloty`.
- Branch roboczy: `flight-network-prediction`.
- Zweryfikowany `HEAD`: `982292844e9a27972d35842a486d9577207bb710` — „Dodaj predykcje lotu i systemy testow sieciowych”.
- Branch jest zsynchronizowany z `origin/flight-network-prediction`, ale working tree zawiera niezacommitowane zmiany konfiguracji i binarnych assetów oraz nowe assety. Przed kolejnym checkpointem trzeba je świadomie przejrzeć; nie wolno ich automatycznie przywracać ani usuwać.

## Aktualny stan gry

Projekt jest grywalnym prototypem sieciowej, arcade'owej gry lotniczej. Docelowa architektura ma obsłużyć co najmniej 64 graczy. Najważniejszym wymaganiem jest natychmiastowa reakcja lokalnego samolotu przy zachowaniu autorytetu serwera.

### Lot i sieć

- `UArcadeFlightComponent` korzysta z Unreal `NetworkPrediction` i stałego kroku 60 Hz.
- Lokalny właściciel przewiduje input natychmiast; serwer potwierdza wynik, a rozbieżności są naprawiane przez rewind i resymulację.
- Obce samoloty korzystają z wygładzania/interpolacji prezentacji.
- Standardowe `ReplicateMovement` nie może równolegle sterować transformem samolotu.
- Przewidywany stan obejmuje lot, strafe, bank, boost, hover, granice mapy oraz ruchy specjalne: roll, zawrót, ruch wsteczny, forward dash i blink.
- Obecny kierunek, bank i wizualna pozycja samolotu były długo dostrajane; feeling jest zaakceptowany i wymaga ostrożnych, małych zmian z checkpointami.

Sterowanie obejmuje kursorem, `A`/`D` strafe, `S` zwolnienie, boost, hover oraz ruchy specjalne. Szczegóły modelu znajdują się w [NetworkMovementArchitecture.md](NetworkMovementArchitecture.md) i [FlightAndAudio.md](FlightAndAudio.md).

### Walka i uzbrojenie

- Replikowane statystyki/HP, serwerowe obrażenia i śmierć.
- Wspólny system wyposażenia i przełączania slotów `Gun` oraz `Missile`, wraz z cooldownem po wyposażeniu.
- Dostępne uzbrojenie: RifleGun, Shotgun, Rocket Barrage, homing missiles i Ground Barrage.
- Trafienia i impacty korzystają ze wspólnego semantycznego zdarzenia, serwerowego potwierdzania i historii hitboxów do rewindu.
- Klient może natychmiast odtworzyć przewidywaną kosmetykę, ale serwer pozostaje jedynym źródłem obrażeń.

Szczegóły: [WeaponSystem.md](WeaponSystem.md), [NetworkImpactArchitecture.md](NetworkImpactArchitecture.md), [RifleGunDesign.md](RifleGunDesign.md), [ShotgunDesign.md](ShotgunDesign.md) i [RocketBarrageDesign.md](RocketBarrageDesign.md).

### AI i przebieg gry

- Latający oraz naziemny przeciwnik AI.
- Tryby agresywny i defensywny, radar/agro, gun oraz homing.
- Spawner przeciwników i respawn gracza.
- Ruch latającego AI jest wykonywany na serwerze, a klienci wygładzają jego prezentację.

### HUD i menu

- Radar, selected/locked target, wskaźniki atakujących i nameplates.
- HP, boost, cooldowny, prędkość, FPS i ping.
- Ustawienia grafiki i audio.
- Menu główne oraz menu pod `Esc`, również podczas hovera.

### Kolizje i prezentacja

Kolizje zostały przebudowane na osobny `MovementCollision` oraz hierarchię hitboxów i elementów wizualnych. Wcześniej występowały problemy z boxami, zaczepianiem o otoczenie i pozycją wizualnego mesha. Zmiany w tej hierarchii trzeba testować zarówno pod kątem ruchu, jak i rewindu trafień.

## Konfiguracja wydajnościowa

Projekt celuje w starsze komputery:

- DX11,
- Nanite, ray tracing, Lumen i VSM wyłączone,
- motion blur wyłączony,
- domyślnie FXAA.

Opcje regulowane przez gracza nie powinny być twardo wymuszane w `DefaultEngine.ini`. Opcje wymagające cooka lub restartu pozostają ustawieniami projektowymi.

`bGenerateNaniteFallbackMeshes` powinno pozostać `True` przy obsłudze DX11, nawet gdy Nanite projektu jest wyłączony, jeżeli używane assety zawierają dane Nanite.

## Znane ostrzeżenia i zależności

- `r.AntiAliasingMethod` ustawione jako `SetByProjectSetting` blokuje zmianę AA przez `GameUserSettings`; zadanie naprawcze opisuje [ROADMAP.md](ROADMAP.md).
- Ostrzeżenia `EditorDataStorageUI` są edytorowe i niekrytyczne.
- `DefaultMap_HLOD0_Instancing` odwołuje się do starej lub niedostępnej klasy `WorldPartitionHLODUtilities`.
- Legacy `BP_Pilot` zgłasza warningi `InputAxis`/`InputAction`.
- Nie wolno wykluczać całego `F16Control` z cooka: istnieje twarda zależność `BP_PlayerPlane -> AB_Jet -> BP_Pilot`.
- Referencyjne źródła starej gry mają pozostać poza kontrolą wersji i produkcyjnym cookiem. Nie wolno przenosić chronionych nazw do klas ani assetów produkcyjnych.

## Packaging

- `bCookAll=False`.
- `AlwaysCook` obejmował `/NNEDenoiser`, `/Game/Main/UI` i `/Game/Main/Maps`.
- Trzeba potwierdzić, czy `NNEDenoiser`/DirectML jest nadal potrzebny.
- PDB klienta stanowił większość rozmiaru paczki i nie jest potrzebny testerom.

