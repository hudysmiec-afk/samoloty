# Samoloty

Prototyp sieciowej gry lotniczej w stylu arcade, tworzony w Unreal Engine 5.8.

Projekt skupia się obecnie na responsywnym sterowaniu samolotem za pomocą kursora, bocznym strafe, dopalaczu oraz przygotowaniu fundamentów pod rozgrywkę multiplayer.

## Wymagania

- Unreal Engine 5.8
- Visual Studio 2022 z narzędziami C++ dla Unreal Engine
- Git LFS do pobierania assetów `.uasset` i `.umap`

Po sklonowaniu repozytorium należy pobrać pliki LFS:

```powershell
git lfs install
git lfs pull
```

## Sterowanie

| Sterowanie | Działanie |
|---|---|
| Pozycja kursora | Kierowanie samolotem w poziomie i pionie |
| `A` | Strafe w lewo |
| `D` | Strafe w prawo |
| `Spacja` | Boost/dopalacz |

Sterowanie kierunkiem zależy od pozycji kursora względem środka ekranu, a nie od szybkości poruszania myszą. Maksymalna prędkość obrotu jest ograniczona przez statystyki samolotu.

## Aktualne funkcje

- automatyczny lot do przodu,
- sterowanie kierunkiem za pomocą widocznego kursora,
- ograniczona prędkość skrętu,
- martwa strefa kursora i krzywa reakcji,
- płynny strafe na `A/D`,
- osobne kąty przechylenia dla skrętu i strafe,
- priorytet przechylenia strafe,
- kamera pościgowa z dynamicznym kadrowaniem,
- płynny boost pod `Spacją`,
- rozszerzenie FOV i odsunięcie kamery podczas boostu,
- energia boostu, zużycie, regeneracja i opóźnienie regeneracji,
- event Blueprint do sterowania smugą, dźwiękiem i innymi efektami boostu,
- podstawowa replikacja ruchu i boostu przygotowana pod multiplayer,
- system Niagara odpowiedzialny za smugę samolotu.

## Architektura

Głównym aktorem gracza jest `AArcadeJetPawn`. Pawn zbiera input lokalnego gracza, zarządza kamerą i przekazuje dane do wyspecjalizowanych komponentów.

```text
ArcadeJetPawn
├── Collision
├── PlaneMesh
├── JetStatsComponent
├── ArcadeFlightComponent
├── JetBoostComponent
├── CameraBoom
└── FollowCamera
```

### `UJetStatsComponent`

Centralne źródło parametrów samolotu. Przechowuje bazowe i końcowe statystyki wykorzystywane przez pozostałe systemy.

Obecne statystyki obejmują między innymi:

- prędkość lotu i strafe,
- responsywność strafe,
- maksymalną prędkość yaw i pitch,
- wygładzanie wejścia,
- maksymalny pitch,
- przechylenie wizualne i przechylenie strafe,
- mnożnik prędkości boostu,
- pojemność, zużycie i regenerację energii boostu.

W przyszłości komponent będzie agregował modyfikatory pochodzące z przedmiotów, perków i drzewa pasywnego.

### `UArcadeFlightComponent`

Odpowiada za:

- wygładzanie wejścia lotu,
- obrót samolotu,
- ruch do przodu,
- strafe,
- przechylenie,
- kolizje i ślizganie po powierzchni,
- wysyłanie wejścia lokalnego gracza do serwera.

Lokalny gracz symuluje ruch natychmiast, aby sterowanie pozostało responsywne. Serwer otrzymuje znormalizowane wejście i wykonuje autorytatywną symulację.

### `UJetBoostComponent`

Odpowiada za:

- żądanie rozpoczęcia i zakończenia boostu,
- serwerową weryfikację dostępnej energii,
- zużywanie i regenerację energii,
- replikację stanu boostu,
- płynny parametr `BoostAlpha`,
- eventy dla efektów wizualnych i HUD.

Event `On Boost State Changed` w `BP_PlayerPlane` może aktywować lub dezaktywować komponent Niagara, dźwięk silnika i inne efekty.

## Konfiguracja samolotu

Blueprint gracza powinien dziedziczyć po:

```text
ArcadeJetPawn
```

W projekcie jest używany:

```text
Content/Main/Blueprints/BP_PlayerPlane
```

Podstawowe parametry lotu i boostu ustawia się na komponencie:

```text
JetStats → Base Flight Stats
```

Parametry kamery, kadrowania oraz kursora pozostają w `BP_PlayerPlane → Class Defaults`.

GameMode powinien dziedziczyć po `ArcadeFlightGameMode` i wskazywać `BP_PlayerPlane` jako `Default Pawn Class`.

## Efekt smugi

Projekt zawiera:

```text
Content/Main/Materials/M_PlaneTrail
Content/Main/SystemNiagara/NS_PlaneTrail
```

Komponent Niagara w `BP_PlayerPlane` powinien mieć wyłączone `Auto Activate`. Event `On Boost State Changed` przekazuje wartość `bBoosting` do `Set Active`, dzięki czemu smuga działa wyłącznie podczas boostu.

## Rocket Barrage

Prototyp uzbrojenia pod prawym przyciskiem myszy obejmuje serwerowe HP, dwie salwy prostych rakiet, cooldown, proximity damage, efekty Niagara i debug multiplayer. Pełne założenia, wartości początkowe oraz checklista znajdują się w [`Docs/RocketBarrageDesign.md`](Docs/RocketBarrageDesign.md).

Najważniejsze klasy:

```text
HealthComponent
RocketWeaponComponent
RocketProjectile
```

## Multiplayer

Aktualny model sieciowy jest podstawą prototypową:

- serwer wykonuje autorytatywną symulację ruchu,
- tylko serwer zmienia pozycję samolotu i oblicza zużycie boostu,
- klient wysyła do serwera znormalizowane wejście sterowania,
- każdy klient buforuje snapshoty serwera i interpoluje pozycję, rotację oraz prędkość,
- energia i stan boostu są kontrolowane przez serwer,
- kamera, FOV, kursor i HUD są lokalne,
- efekty boostu reagują na replikowany stan.

W tej prostej wersji nie ma lokalnej predykcji ani korekcji ruchu, więc wejście klienta jest opóźnione o czas transmisji do serwera. Snapshoty są wyświetlane z krótkim stałym opóźnieniem, a krótkie braki pakietów pokrywa ograniczona ekstrapolacja. Dzięki temu nie powstają dwie niezależne wersje pozycji lub energii boostu. Rewind trafień będzie osobnym systemem dodanym razem z uzbrojeniem.

### Test dwóch graczy w edytorze

W ustawieniach `Play`:

1. Ustaw `Number of Players` na `2`.
2. Ustaw `Net Mode` na `Play As Listen Server`.
3. Wybierz uruchamianie w osobnych oknach.
4. Upewnij się, że mapa zawiera co najmniej dwa obiekty `Player Start`.

## Budowanie

Target edytora można zbudować przez UnrealBuildTool:

```powershell
X:\Programy\EpicCore\UE_5.8\Engine\Build\BatchFiles\Build.bat SamolotyEditor Win64 Development -Project="X:\Dane\Projekty_UE\Samoloty\Samoloty.uproject" -WaitMutex -FromMsBuild
```

Przed pełną kompilacją należy zamknąć Unreal Editor albo wyłączyć aktywne Live Coding.

Nie należy ręcznie modyfikować ani dodawać do repozytorium folderów:

- `Binaries`,
- `Intermediate`,
- `Saved`,
- `DerivedDataCache`.

## Praca z Git

Przed rozpoczęciem pracy:

```powershell
git pull
git lfs pull
```

Po zakończeniu:

```powershell
git add .
git commit -m "Opis zmian"
git push
```

Assety Unreal są plikami binarnymi. Nie należy równocześnie edytować tego samego `.uasset` lub `.umap` na kilku branchach, ponieważ Git nie potrafi ich automatycznie scalać.

## Planowane systemy

- [x] Sterowanie kursorem
- [x] Strafe i przechylenie samolotu
- [x] Kamera pościgowa
- [x] Boost i energia
- [x] Podstawowa architektura komponentowa
- [x] Podstawowa replikacja ruchu i boostu
- [ ] Własny celownik i HUD
- [ ] Pasek energii boostu
- [ ] Broń pokładowa
- [ ] Rakiety i namierzanie celu
- [ ] Punkty życia, obrażenia i zniszczenie samolotu
- [ ] Predykcja, korekcja i testy przy wysokim pingu
- [ ] Przeciwnicy AI
- [ ] Przedmioty, perki i drzewko pasywne

## Status

Projekt jest na etapie wczesnego prototypu. Priorytetem jest ustabilizowanie feelingu lotu oraz fundamentów multiplayer przed rozbudową systemu walki.
