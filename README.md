# Samoloty

Prototyp sieciowej gry lotniczej arcade tworzony w Unreal Engine 5.8. Projekt rozwija sterowanie inspirowane grami pokroju Ace Online: samolot jest prowadzony kursorem, może wykonywać boczny strafe, zwalniać, korzystać z dopalacza oraz walczyć za pomocą rakiet i karabinu pokładowego.

## Wymagania

- Unreal Engine 5.8
- Visual Studio 2022 z narzędziami C++ dla Unreal Engine
- Git LFS do pobierania assetów `.uasset` i `.umap`

Po sklonowaniu repozytorium:

```powershell
git lfs install
git lfs pull
```

## Sterowanie

| Sterowanie | Działanie |
|---|---|
| Pozycja kursora | Skręt samolotu w poziomie i pionie |
| `A` / `D` | Strafe w lewo / prawo |
| `S` | Płynne zwolnienie do prędkości minimalnej |
| `Spacja` | Boost/dopalacz |
| Lewy przycisk myszy | Ciągły ogień RifleGun |
| Prawy przycisk myszy | Salwa rakiet; przytrzymanie ponawia atak po cooldownie |

Sterowanie zależy od pozycji kursora względem środka ekranu, a nie od szybkości ruchu myszy. Maksymalne prędkości obrotu są ograniczone przez statystyki samolotu.

## Aktualne funkcje

- serwerowo autorytatywny lot do przodu i replikowane wejście gracza,
- sterowanie widocznym kursorem, martwa strefa i krzywa reakcji,
- płynny strafe oraz osobne przechylenia wizualne skrętu i strafe,
- kamera pościgowa z dynamicznym kadrowaniem, FOV i odległością boosta,
- zwalnianie pod `S` z regulowaną prędkością minimalną,
- boost z energią, regeneracją i opóźnieniem regeneracji,
- boost ma priorytet nad zwalnianiem,
- zwrotność zależna od aktualnej prędkości,
- arcade'owy wpływ kierunku lotu na prędkość i tempo jej zmiany: wolniej w górę, szybciej w dół,
- HP, obrażenia i zniszczenie Pawna bez respawnu,
- dwusalwowe bombardowanie rakietowe,
- hitscan RifleGun z kosmetycznym tracerem,
- Niagara dla rakiet, eksplozji, muzzle flash i trafień RifleGun,
- przestrzenne dźwięki broni oraz trzywarstwowy dźwięk silnika,
- debug HP, boosta, prędkości, zwrotności, rakiet i RifleGun.

## Architektura

Głównym aktorem gracza jest `AArcadeJetPawn`. Pawn zbiera lokalny input, zarządza kamerą i przekazuje dane do wyspecjalizowanych komponentów.

```text
ArcadeJetPawn
├── Collision
├── PlaneMesh
├── JetStatsComponent
├── ArcadeFlightComponent
├── JetBoostComponent
├── JetEngineAudioComponent
├── HealthComponent
├── RocketWeaponComponent
├── RifleGunComponent
├── RocketSpawnLeft / RocketSpawnRight
├── GunMuzzleLeft / GunMuzzleRight
├── CameraBoom
└── FollowCamera
```

### `UJetStatsComponent`

Centralne źródło bazowych i efektywnych statystyk lotu oraz uzbrojenia. Obejmuje między innymi:

- prędkość normalną, minimalną i szybkość reakcji prędkości,
- wpływ wznoszenia i nurkowania na prędkość,
- strafe, yaw, pitch i przechylenie,
- mnożniki zwrotności dla małej prędkości i boosta,
- energię i mnożnik prędkości boosta,
- HP,
- parametry Rocket Barrage i RifleGun.

W przyszłości będzie agregował modyfikatory z przedmiotów, perków i drzewa pasywnego.

### `UArcadeFlightComponent`

Serwer wykonuje obrót, ruch, strafe, zmianę prędkości oraz kolizję. Klient przesyła znormalizowane wejście. Pozostali gracze są wyświetlani przez buforowane snapshoty, interpolację i krótką ograniczoną ekstrapolację.

System nie dodaje fizycznej siły grawitacji, opadania ani przeciągnięcia. Kierunek samolotu wpływa wyłącznie na docelową prędkość i `ForwardSpeedResponse`.

### `UJetBoostComponent`

Serwer kontroluje energię oraz stan boosta. Replikowany stan jest wygładzany do `BoostAlpha`, używanego przez lot, kamerę, VFX i audio.

### `UJetEngineAudioComponent`

Lokalny komponent prezentacji odtwarza jednocześnie trzy zapętlone warstwy i płynnie miesza ich głośność:

- `SlowEngineLoop`,
- `NormalEngineLoop`,
- `BoostEngineLoop`.

Warstwa slow reaguje na `S`, a boost ma nad nią priorytet. Na serwerze dedykowanym komponent nie tworzy audio.

### Walka

- `UHealthComponent` — replikowane HP i śmierć.
- `URocketWeaponComponent` — salwy, cooldown, spawn rakiet i dźwięk wystrzału salwy.
- `ARocketProjectile` — ruch, sweep, proximity, obrażenia, Niagara i dźwięk eksplozji.
- `URifleGunComponent` — szybkostrzelność, serwerowy hitscan, obrażenia i synchronizacja kosmetyki.
- `ARifleTracerVisual` — lekki, niereplikowany wizualny pocisk bez kolizji i damage.

Dokładne założenia znajdują się w:

- [`Docs/RocketBarrageDesign.md`](Docs/RocketBarrageDesign.md)
- [`Docs/RifleGunDesign.md`](Docs/RifleGunDesign.md)
- [`Docs/FlightAndAudio.md`](Docs/FlightAndAudio.md)

## Konfiguracja `BP_PlayerPlane`

Blueprint gracza dziedziczy po `ArcadeJetPawn`:

```text
Content/Main/Blueprints/BP_PlayerPlane
```

Najważniejsze miejsca konfiguracji:

```text
JetStats → Base Flight Stats
JetStats → Base Combat Stats
JetStats → Base Rocket Barrage Stats
JetStats → Base Rifle Gun Stats
JetEngineAudio → Engine loops
RocketWeapon → Rocket class i launch sound
RifleGun → tracer, Niagara i audio
```

Główna kapsuła jest obecnie zarówno kolizją ruchu, jak i uproszczonym celem broni. Musi blokować kanał `Visibility`, ponieważ RifleGun wykonuje trace na tym kanale. Dokładne hitboxy kadłuba i skrzydeł są zadaniem późniejszym.

## Multiplayer

- serwer jest autorytatywny dla ruchu, boosta, rakiet, hitscanu, obrażeń i HP,
- klient nie zmienia autorytatywnej pozycji ani energii,
- efekty wizualne i audio są lokalne; serwer rozsyła dane potrzebne do ich odtworzenia,
- proste rakiety odtwarzają ruch lokalnie z replikowanych danych startowych,
- wystrzelone rakiety pozostają aktywne po śmierci właściciela,
- server rewind dla hitscanu nie jest jeszcze zaimplementowany.

Do testu dwóch graczy w edytorze ustaw `Number of Players = 2`, `Play As Listen Server`, osobne okna i umieść na mapie co najmniej dwa rozstawione `Player Start`.

## Budowanie

```powershell
X:\Programy\EpicCore\UE_5.8\Engine\Build\BatchFiles\Build.bat SamolotyEditor Win64 Development -Project="X:\Dane\Projekty_UE\Samoloty\Samoloty.uproject" -WaitMutex -FromMsBuild
```

Przed pełną kompilacją zamknij Unreal Editor albo wyłącz Live Coding. Nie dodawaj ręcznie folderów `Binaries`, `Intermediate`, `Saved` ani `DerivedDataCache`.

## Praca z Git

Przed pracą:

```powershell
git pull
git lfs pull
```

Po pracy:

```powershell
git add .
git commit -m "Opis zmian"
git push
```

Nie edytuj równocześnie tego samego `.uasset` lub `.umap` na kilku branchach — Git nie potrafi automatycznie scalać binarnych assetów Unreal.

## Planowane systemy

- [x] Sterowanie kursorem, strafe i kamera pościgowa
- [x] Zwalnianie, boost i zwrotność zależna od prędkości
- [x] Podstawowa replikacja ruchu i boostu
- [x] HP i serwerowe obrażenia
- [x] Rocket Barrage
- [x] RifleGun hitscan
- [x] Podstawowe VFX i audio uzbrojenia
- [x] Trzywarstwowe audio silnika
- [ ] Własny celownik i HUD
- [ ] Pasek HP i energii boostu
- [ ] Dokładne hitboxy oraz kanał `WeaponTrace`
- [ ] Hit marker potwierdzany przez serwer
- [ ] Efekt śmierci samolotu i informacja o zabójcy
- [ ] Optymalizacja dużej liczby rakiet
- [ ] Drużyny i blokada friendly fire
- [ ] Rakiety naprowadzane
- [ ] Server rewind dla szybkiej broni
- [ ] Przeciwnicy AI
- [ ] Przedmioty, perki i drzewko pasywne

## Status

Projekt jest grywalnym wczesnym prototypem multiplayer. Fundamenty lotu, boosta, HP, dwóch typów uzbrojenia, podstawowych VFX i audio są zaimplementowane. Kolejny etap to HUD, czytelność walki, dokładniejsze hitboxy oraz testy i optymalizacja pod większą liczbę graczy.
