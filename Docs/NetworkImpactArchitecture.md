# Network Impact Architecture

## Cel

Wizualny tracer, serwerowe trafienie, obrażenia i Niagara muszą opisywać ten
sam kontakt mimo opóźnienia sieciowego. Surowy punkt świata nie wystarcza dla
ruchomego celu, ponieważ po odebraniu potwierdzenia cel jest już w innym miejscu.

## Semantyczny impact

`FCombatImpactEvent` przechowuje:

- sekwencję strzału i indeks pocisku/pelletu,
- potwierdzony cel oraz nazwę trafionego komponentu,
- awaryjny punkt i normalną w świecie,
- punkt i normalną w lokalnej przestrzeni trafionego komponentu.

Statyczne powierzchnie korzystają z punktu świata. Dla ruchomego celu klient
odtwarza lokalny punkt na aktualnym transformie komponentu prezentacyjnego.
Spóźniony efekt pozostaje więc na kadłubie lub skrzydle zamiast wisieć za celem.

## Rifle i Shotgun

Każdy lokalny strzał ma własną komendę i numer sekwencji:

1. właściciel natychmiast wykonuje lokalny trace, tracer, muzzle flash, dźwięk
   i przewidywany impact,
2. serwer waliduje wyposażenie, stan lotu i maksymalną szybkostrzelność,
3. serwer cofa zarejestrowane hitboxy do zsynchronizowanego czasu strzału,
4. serwer wykonuje trace, przywraca hitboxy i nalicza obrażenia,
5. potwierdzenie z tym samym numerem sekwencji nie odtwarza drugi raz zgodnego
   efektu; rozbieżny serwerowy kontakt może dodać brakujący impact.

Komendy szybkostrzelnej broni są `Unreliable`, aby strata pakietów nie tworzyła
rosnącej kolejki starych strzałów. Serwer pozostaje jedynym źródłem obrażeń.

## Historia hitboxów

`UHitRewindComponent` zapisuje na serwerze 350 ms historii wyłącznie
komponentów oznaczonych `DamageHitbox`. Historia nie jest replikowana. Gracz,
latający NPC i naziemny NPC używają tej samej implementacji.

Klient podaje estymowany czas serwera z `GameState`; serwer ogranicza żądany
rewind do 300 ms. W wersji produkcyjnej czas należy dodatkowo powiązać z
potwierdzoną klatką wejścia Network Prediction, aby klient nie mógł dowolnie
wybierać chwili w dozwolonym oknie.

## Rakiety i zderzenia

Rakieta replikuje ten sam semantyczny impact. Eksplozja ruchomego celu jest
rekonstruowana przy jego aktualnym komponencie, a eksplozja zasięgowa lub na
terenie używa punktu świata.

Kolizja właściciela odtwarza lokalnie przewidywany dźwięk, Niagara i camera
shake w klatce odbicia. Serwer nadal zatwierdza obrażenia i rozsyła kontakt
innym klientom. Ostatni zgodny lokalny kontakt tłumi powtórne potwierdzenie.

## Dalsze utwardzenie

- powiązanie czasu strzału bezpośrednio z numerem klatki PlaneFlight,
- redundancja komend szybkostrzelnej broni przy stratach pakietów,
- test obciążenia historii dla 64/128 graczy oraz dużej liczby NPC,
- osobna polityka fizycznego kontaktu dwóch samolotów; rewind hitscanu nie
  rozwiązuje symetrycznej synchronizacji twardego odbicia dwóch właścicieli.
