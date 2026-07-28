# Enemy Plane AI

Pierwszy przeciwnik jest ręcznie ustawianym na mapie `BP_EnemyPlane`, dziedziczącym po `AEnemyPlanePawn`. Blueprint pozwala przypisać mesh, ustawić `Box Extent`, pozycje obu luf oraz kosmetykę `RifleGun`.

## Zachowanie

`UEnemyPlaneAIComponent` działa wyłącznie autorytatywnie na serwerze i używa lekkiej maszyny stanów:

- `Roaming` — swobodny lot wokół miejsca ustawienia,
- `AttackRun` — lot w stronę aktualnej pozycji gracza i ciągły ogień, gdy cel jest z przodu,
- `Extend` — krótki odlot po minięciu celu,
- `TurnBack` — nawrót i przygotowanie kolejnego nalotu.

NPC wybiera najbliższego żywego Pawna kontrolowanego przez gracza. Zachowuje cel do jego śmierci lub przekroczenia `LoseTargetRange`. RifleGun jest hitscanem, dlatego AI nie stosuje wyprzedzenia zależnego od prędkości celu.

## Sieć i wydajność

- decyzje AI i obrażenia wykonywane są tylko na serwerze,
- wyszukiwanie celu i przeszkód odbywa się okresowo,
- różne NPC otrzymują losowe początkowe przesunięcia interwałów,
- ruch nie korzysta z NavMesh ani fizyki,
- standardowe `ReplicateMovement` jest wyłączone,
- komponent wysyła kompaktową pozycję, obrót i prędkość,
- klienci interpolują oraz krótko ekstrapolują prezentację NPC.

## Profil testowy

- `MaxHealth`: 500,
- `ForwardSpeed`: 2250 cm/s,
- `Rifle Damage`: 20,
- `ShotsPerSecond`: 10,
- `SpreadAngleDegrees`: 1,
- `FlyPastDistance`: 20 m,
- `ExtendDistance`: 50 m.

Parametry są ustawialne w komponentach `JetStats`, `EnemyAI` i `RifleGun`, dzięki czemu kolejne archetypy mogą dziedziczyć po tej samej klasie C++.
