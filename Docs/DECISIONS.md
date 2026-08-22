# Decyzje architektoniczne

Ten dokument zapisuje decyzje, których nie należy przypadkowo odwracać podczas dalszych iteracji.

## Server authority z client prediction

Ruch gracza jest serwerowo autorytatywny, ale lokalny input jest przewidywany natychmiast przez `NetworkPrediction`. Opóźnienie sterowania do czasu odpowiedzi serwera jest niedopuszczalne. Serwer wykonuje te same komendy, potwierdza wynik i koryguje klienta przez rewind/resymulację.

Powód: responsywne sterowanie arcade przy zachowaniu spójności i odporności na oszustwa w PvP dla 64+ graczy.

## Jedna symulacja gameplayowa lotu

Lot, boost, hover, granice mapy i ruchy specjalne należą do wspólnego modelu `PlaneFlight`. Starsze komponenty umiejętności mogą pozostać fasadami wejścia, HUD-u, kamery, audio i VFX, ale nie mogą prowadzić równoległych zegarów ani samodzielnie przesuwać Actora. `ReplicateMovement` pozostaje wyłączone dla samolotu.

Powód: deterministyczny rewind, brak dwóch konkurujących transformów i jedna historia do korekt sieciowych.

## Rozdzielenie symulacji od prezentacji

Stan symulacji i hitbox lokalnego właściciela nie są celowo opóźniane. Obce samoloty mogą być interpolowane lub krótko ekstrapolowane. Wygładzony transform jest wyłącznie prezentacją i nie wraca jako wejście kolejnego kroku symulacji.

Powód: lokalna responsywność bez drgań proxy i bez zanieczyszczania autorytatywnej symulacji danymi renderowymi.

## Wspólna oś czasu walki

Serwer jest jedynym źródłem obrażeń. RifleGun i Shotgun używają historii uproszczonych hitboxów oraz ograniczonego rewindu; klient może natychmiast przewidzieć tracer, audio i impact. `FCombatImpactEvent` opisuje kontakt semantycznie, również w lokalnej przestrzeni trafionego komponentu. Rakiety używają serwerowej logiki gameplayowej i lokalnej prezentacji.

Powód: efekt ma pozostać na ruchomym celu, a wysoki ping nie może powodować ani podwójnej kosmetyki, ani rozbieżności między widocznym i zatwierdzonym trafieniem.

## Skalowanie sieciowe

60 Hz jest krokiem symulacji, a nie wymogiem wysyłania pełnego stanu każdego aktora 60 razy na sekundę. Docelowo należy użyć priorytetów/LOD sieciowego: właściciel i bezpośrednia walka, pobliskie samoloty, odległe kontakty radarowe. Dane radaru powinny być oddzielone od pełnej replikacji ruchu.

Powód: budżet CPU i pasma dla co najmniej 64 graczy oraz wielu NPC i pocisków.

## Zachowanie zaakceptowanego feelingu lotu

Arcade'owy kierunek, bank i offset wizualny samolotu zostały zaakceptowane po wielu iteracjach. Zmiany powinny być małe, mierzalne i poprzedzielane checkpointami.

Powód: pozornie lokalna korekta transformu lub hierarchii mesha może zmienić sterowanie, kamerę, kolizję i prezentację sieciową jednocześnie.

## Oddzielna kolizja ruchu i trafień

`MovementCollision`, hitboxy obrażeń i visual mesh mają osobne role. Visual mesh nie powinien odpowiadać za ruch ani generować zbędnej kolizji, nawigacji czy kosztów fizycznych.

Powód: ograniczenie zaczepiania o otoczenie, stabilniejsze trafienia/rewind i możliwość niezależnej optymalizacji grafiki.

## Modułowe komponenty gameplayowe

Flight, stats/health, boost, weapon system, konkretne bronie, radar/targeting, HUD, AI i movement skills pozostają wyspecjalizowanymi komponentami. Bronie dziedziczą po wspólnej bazie i są wybierane przez sloty zamiast specjalnych warunków w Pawnie.

Powód: rozszerzalność wyposażenia, niezależne testowanie i ograniczenie rozrostu klasy gracza.

## Profil graficzny dla starszego sprzętu

Bazą jest DX11 bez Nanite, ray tracingu, Lumena, VSM i motion blur; domyślne AA to FXAA. Ustawienia dostępne graczowi należą do `SamolotyGameUserSettings`, a nie do twardych CVAR-ów projektowych. Fallback meshe Nanite powinny być generowane dla assetów, które ich potrzebują.

Powód: szeroka kompatybilność, przewidywalny cook i działające menu ustawień.

## Optymalizacja masowych rakiet

Logika rakiet pozostaje serwerowa, ale docelowa prezentacja setek rakiet powinna korzystać ze wspólnego Niagara GPU Mesh Renderer lub instancingu. Visual mesh rakiety ma wyłączone cienie, decals, ray tracing visibility, navigation i kolizję oraz używa LOD. Traile rakiet: GPU Compute Sim, Fixed Bounds, krótki lifetime/spawn rate, prosty materiał Unlit, bez lights/collision/events/distortion. Dwie smugi skrzydeł mogą pozostać CPU Ribbon.

Powód: przy około 2 tys. trójkątów geometria pojedynczej rakiety jest lekka, lecz setki Actorów i mesh componentów generują nadmierne draw calle i narzut CPU. Fixed Bounds steruje cullingiem, a nie fizycznym przycinaniem cząstek.

## Referencyjna zawartość legacy

Źródła starej gry są wyłącznie materiałem referencyjnym: poza kontrolą wersji, poza produkcyjnym cookiem i bez używania chronionych nazw. Jednocześnie nie wolno wykluczać całego `F16Control`, dopóki istnieje zależność `BP_PlayerPlane -> AB_Jet -> BP_Pilot`.

Powód: bezpieczeństwo prawne i rozmiar/build projektu, bez uszkadzania istniejącego łańcucha zależności assetów.

