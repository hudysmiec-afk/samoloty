# Rocket Barrage - założenia prototypu

## Stan implementacji

- [x] Statystyki HP i Rocket Barrage w JetStatsComponent
- [x] Replikowany, autorytatywny HealthComponent
- [x] Dwa regulowane punkty startowe pod skrzydłami
- [x] Sterowanie RMB: kliknięcie i ciągły ogień po przytrzymaniu
- [x] Dwie salwy po dziesięć rakiet oraz cooldown
- [x] Prosty lot, losowy kontrolowany rozrzut i maksymalny dystans
- [x] Losowana na nowo dla każdej salwy separacja do jednego wspólnego okręgu przed środkiem samolotu
- [x] Serwerowe collision sweep, proximity i obrażenia
- [x] Replikowane trafienie, wybuch i zniszczenie rakiety
- [x] Blueprint rakiety, Niagara trail i Niagara explosion
- [x] Przestrzenny dźwięk wystrzału odtwarzany raz na salwę
- [x] Przestrzenny dźwięk eksplozji odtwarzany przez każdą rakietę
- [x] Przekazywanie ProximityRadius do `User.ExplosionRadius`
- [x] Debug HP, salw, cooldownu oraz liczby rakiet
- [x] Build SamolotyEditor
- [x] Test obciążenia około 1000 rakiet w Standalone
- [ ] Optymalizacja wielu rakiet przez batch simulation i wspólny renderer
- [ ] Drużyny oraz blokada friendly fire
- [ ] Naprowadzane warianty rakiet

## Sieć

- Serwer jest autorytatywny dla użycia broni, salw, rakiet, trafień, HP i śmierci.
- Klient wysyła niezawodne zmiany stanu prawego przycisku myszy.
- Ruch prostych rakiet jest odtwarzany lokalnie z replikowanych danych startowych; serwer rozstrzyga kolizje.
- Wystrzelone rakiety pozostają aktywne po śmierci właściciela.

## Statystyki początkowe

- Max HP: 10000
- Salwy: 2
- Rakiety na salwę: 10
- Odstęp salw: 0.4 s
- Cooldown po ostatniej salwie: 4 s
- Prędkość rakiety: 3000 cm/s
- Obrażenia rakiety: 100
- Maksymalny dystans: 200000 cm
- Promień zapalnika: 2000 cm
- Rozrzut: 1 stopień
- Sprawdzenie proximity: co 0.1 s

## Zachowanie

- Jedno kliknięcie uruchamia całą sekwencję salw.
- Przytrzymanie RMB ponawia sekwencję natychmiast po cooldownie.
- Puszczenie RMB nie anuluje rozpoczętej sekwencji.
- Śmierć anuluje niewystrzelone salwy i niszczy Pawn bez respawnu.
- Dziesięć rakiet salwy startuje jednocześnie: pięć z lewego i pięć z prawego punktu sceny.
- Kierunki są losowane przez serwer w jednym kontrolowanym stożku; lewa i prawa grupa zachowują swoje strony.
- Rakieta najpierw zmierza po indywidualnej krzywej Béziera do punktu we wspólnym okręgu przed środkiem samolotu, a następnie leci prosto i nie jest naprowadzana.
- Rakiety ignorują właściciela i inne rakiety.
- Pierwszy cel z HealthComponent w promieniu otrzymuje pełne obrażenia; ściany nie blokują zapalnika.
- Trafienie otoczenia albo zadanie obrażeń powoduje wybuch i zniszczenie rakiety.
- Rakieta wybucha bez zadawania obrażeń po osiągnięciu maksymalnego przebytego dystansu.

## Rozszerzenia późniejsze

- Tryby ruchu Straight i Homing korzystają ze wspólnej bazy rakiety.
- Team ID i blokada friendly fire zostaną dodane razem z drużynami.
- Dużą liczbę rakiet będzie można zoptymalizować przez pooling, grupową symulację i instancjonowane wizualizacje.

## Assety prototypu

- `Content/Main/Blueprints/BP_RocketProjectile`
- `Content/Main/SystemNiagara/NS_RocketExplosion`
- `Content/Main/SystemNiagara/NS_RocketTrail`
- `Content/Main/Audio/Weapons/SW_RocketLaunch`
- `Content/Main/Audio/Weapons/SW_RocketExplosion`
- `Content/Main/Audio/Weapons/ATT_Global`
- `Content/ArrowTrail`
- `Content/RocketThrusterExhaustFX`

Paczki VFX z Fab zostały zachowane w całości wraz z zależnościami. Systemy Niagara wymagające starszych sygnatur modułów zostały w edytorze odświeżone przez usunięcie i ponowne dodanie wadliwych wywołań.

`RocketLaunchSound` znajduje się w `RocketWeaponComponent`, ponieważ dotyczy uruchomienia całej salwy. `ExplosionSound` znajduje się w `RocketProjectile`, ponieważ wystrzelona rakieta pozostaje samodzielna również po śmierci właściciela.
