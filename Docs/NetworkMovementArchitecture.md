# Network Movement Architecture

## Cel

System ruchu jest projektowany dla serwerowo autorytatywnej rozgrywki PvP z
minimum 64 graczami. Lokalny samolot nie może czekać na odpowiedź serwera, ale
klient nie może samodzielnie zatwierdzać pozycji, boosta, kolizji ani trafień.

Implementacja korzysta z rdzenia `NetworkPrediction` z Unreal Engine 5.8.
Kod z `NetworkPredictionExtras` jest wyłącznie materiałem referencyjnym i nie
jest zależnością gry.

## Model symulacji

`UArcadeFlightComponent` jest sterownikiem modelu `PlaneFlight`:

```text
FPlaneFlightInputCmd
    Steering, Strafe, Brake, BoostRequested, HoverRequested,
    AbilitySequence, AbilityType, AbilityDirection
                 |
                 v
FPlaneFlightNetworkSimulation (fixed tick 60 Hz)
                 |
                 v
FPlaneFlightSyncState
    transform, velocity, smoothed input, speed, visual bank,
    boost energy/alpha/regen state, hover state/alpha,
    mobility/roll/cooldowns/ability queue, boundary return, movement epoch
                 |
                 v
server confirmation -> reconcile -> rewind -> replay pending inputs
```

`FPlaneFlightAuxState` przechowuje rzadko zmieniane statystyki lotu. Serwer
aktualizuje ten stan po zmianie statystyk, dzięki czemu przedmioty i perki nie
będą wymagały osobnego protokołu ruchu.

### Zasady

- Symulacja wykonuje stałe kroki 60 Hz niezależnie od FPS renderowania.
- Klient właściciela przewiduje każdą komendę natychmiast.
- Serwer wykonuje te same komendy i jest jedynym autorytetem.
- Rozbieżność przekraczająca tolerancję uruchamia rewind oraz resymulację.
- Pozycja renderowana może być wygładzona, ale stan symulacji i hitbox nie są
  celowo opóźniane dla lokalnego gracza.
- Teleport zwiększa `MovementEpoch`; stanów po dwóch stronach teleportu nie
  wolno interpolować.
- Actor ma wyłączone standardowe `ReplicateMovement`, aby dwa systemy nie
  zmieniały jednocześnie tego samego transformu.

### Wygładzanie prezentacji

Włączona usługa `FixedTickSmoothing` interpoluje dwa ostatnie stany symulacji
60 Hz do częstotliwości renderowania. `FinalizeSmoothingFrame` przekazuje ten
stan do samolotu w każdej klatce obrazu. Wartości prezentacyjne nigdy nie są
używane jako wejście następnego kroku symulacji, więc wygładzenie nie dodaje
opóźnienia do sterowania ani nie zmienia autorytatywnego wyniku serwera.

## Skalowanie do 64+ graczy

60 Hz jest częstotliwością symulacji, a nie obowiązkową częstotliwością
wysyłania pełnych stanów. Aktualny Pawn ma limit replikacji 30 Hz. Następne
etapy wprowadzą sieciowe poziomy szczegółowości:

- wysoki priorytet: właściciel, selected target, attacker i bezpośrednia walka,
- średni priorytet: pobliskie samoloty mogące wejść do walki,
- niski priorytet: odległe kontakty wymagane tylko przez radar,
- brak replikacji kosmetyki na serwer dedykowany.

Pozycje radarowe muszą być oddzielone od pełnego strumienia ruchu. Rakiety
docelowo replikują przede wszystkim spawn, sporadyczną korektę i eksplozję,
zamiast pełnego transformu każdego pocisku w każdej klatce.

## Stan migracji

Gotowe w pierwszym etapie:

- fixed-step normalnego lotu,
- przewidywanie właściciela i historia frameworka,
- korekta przez rewind/resimulation,
- skompresowane wejście i podstawowa kwantyzacja stanu,
- przewidywany boost wraz z energią i regeneracją,
- przewidywany hover wraz z wejściem, zatrzymaniem i wyjściem,
- przewidywany automatyczny powrót do granic mapy,
- przewidywane evasive roll, backward dash, forward dash, quick reversal i blink,
- jedna deterministyczna kolejka ruchów specjalnych we wspólnej historii,
- cooldowny, ochrona rakietowa rolla i blokada broni podczas zawrotu odczytywane
  z potwierdzonego stanu lotu,
- synchronizowane statystyki lotu,
- ciągłość teleportów przez `MovementEpoch`,
- prezentacja obcych samolotów przez Network Prediction zamiast starego,
  równoległego bufora snapshotów,
- przekazywanie renderowanego stanu przez `FinalizeSmoothingFrame`.

Stare komponenty ruchów specjalnych nie przesuwają już Actor samodzielnie i
nie prowadzą równoległych zegarów sieciowych. Pozostają fasadami dla wejścia,
HUD-u, kamery, audio i efektów. Cały wynik gameplayowy powstaje w modelu
`PlaneFlight`, dlatego może zostać cofnięty i ponownie zasymulowany.

## Walka na tej samej osi czasu

- Rifle i Shotgun: serwerowa historia uproszczonych hitboxów, ograniczony
  rewind do czasu oddania strzału oraz semantyczny impact rekonstruowany na
  aktualnie prezentowanym celu.
- Rakiety: serwerowa symulacja gameplayu i lokalna predykcja prezentacji.
- Roll: przedział nietykalności zapisany w klatkach symulacji.
- Blink: przerwanie historii przez nową epokę ruchu.
- Kolizje z otoczeniem: lokalny, przewidywany dźwięk i Niagara w klatce
  odbicia; serwer zatwierdza obrażenia, a identyfikator kontaktu zapobiega
  ponownemu odtworzeniu efektu po replikacji.

Nie wolno cofać całego świata dla jednego strzału. Serwer przechowuje stały
ring buffer tylko dla danych niezbędnych do rozstrzygnięcia trafienia.

## Diagnostyka i testy

Framework można obserwować przez Network Prediction Insights. Przydatne
uruchomienie:

```text
-trace=NetworkPrediction,Net,Frame -NetTrace=1
```

Menu diagnostyczne frameworka otwiera komenda konsoli:

```text
np2.DevMenu
```

Minimalna macierz testowa dla każdej zmiany ruchu:

| RTT | Loss | Oczekiwany wynik |
|---:|---:|---|
| 0 ms | 0% | brak korekt odczuwalnych przez gracza |
| 50 ms | 0% | natychmiastowy input, płynne proxy |
| 100 ms | 0% | natychmiastowy input, bez regularnych snapów |
| 150 ms | 2% | możliwe drobne korekty, bez utraty kontroli |
| 100 ms | 5% | brak trwałej desynchronizacji po ustaniu strat |

Po zachowaniu jakości należy wykonać test obciążenia 64, a następnie 128
połączeń/botów i zmierzyć czas serwera, pasmo, liczbę korekt i koszt pocisków.
