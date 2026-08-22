# Roadmap

Kolejność jest orientacyjna. Przed zmianami binarnych assetów należy wykonać świadomy przegląd working tree i mały checkpoint.

## Najbliższe zadania

1. **Uporządkować ustawienia antyaliasingu.** Usunąć twarde `r.AntiAliasingMethod=1` z `DefaultEngine.ini` oraz mobilny odpowiednik, jeżeli nie jest potrzebny. Domyślne FXAA pozostawić w `SamolotyGameUserSettings`, aby menu mogło zmieniać ustawienie bez warningu `SetByProjectSetting`.
2. **Przejrzeć obecne lokalne zmiany.** Zweryfikować zmodyfikowane pliki konfiguracyjne i assety, trzy usunięte Blueprinty legacy oraz nowe mapy/modele. Nie przywracać ani nie dodawać ich zbiorczo bez zrozumienia pochodzenia.
3. **Naprawić zależność legacy input.** Usunąć referencję `AB_Jet -> BP_Pilot` albo oczyścić `BP_Pilot` z przestarzałych `InputAxis`/`InputAction`. Do tego czasu nie wykluczać całego `F16Control` z cooka.
4. **Naprawić HLOD mapy.** Usunąć lub ponownie wygenerować `DefaultMap_HLOD0_Instancing`, które odwołuje się do niedostępnej klasy `WorldPartitionHLODUtilities`.
5. **Zweryfikować packaging.** Sprawdzić potrzebę `/NNEDenoiser` i DirectML w `AlwaysCook`, zachować `bCookAll=False` i nie dołączać PDB klienta do paczek dla testerów.

## Testy sieciowe i utwardzenie

- Zachować natychmiastowy input lokalnego samolotu w każdej zmianie ruchu.
- Przetestować ruch, roll, zawrót, dash, blink, hover i kolizje przy RTT 0/50/100/150 ms oraz loss 0/2/5%.
- Powiązać czas strzału rewind bezpośrednio z potwierdzoną klatką `PlaneFlight`, zamiast ufać dowolnemu czasowi klienta w dozwolonym oknie.
- Rozważyć redundancję komend szybkostrzelnej broni przy utracie pakietów.
- Zdefiniować osobną politykę kontaktu dwóch samolotów; rewind hitscanu nie rozwiązuje symetrycznego twardego zderzenia dwóch właścicieli.
- Wykonać test obciążenia 64, a następnie 128 graczy/botów. Mierzyć czas serwera, pasmo, liczbę korekt, historię hitboxów, AI i koszt pocisków.
- Wprowadzić LOD/prioritety replikacji i osobny lekki strumień kontaktów radarowych.

## Wydajność grafiki i assetów

- Zwykły żółw mający około 408 tys. trójkątów wymaga retopologii lub redukcji, nie retargetingu. Cele: LOD0 30–60k, kolejne 15–25k, 5–10k i 1–3k.
- Dopilnować fallback meshów dla assetów z danymi Nanite używanych na DX11 (`bGenerateNaniteFallbackMeshes=True`).
- Wyłączyć na visual meshach rakiet cienie, decals, ray tracing visibility, navigation i kolizję; dodać LOD.
- Zastąpić osobne komponenty wizualne setek rakiet wspólnym Niagara GPU Mesh Rendererem lub instancingiem, oddzielonym od serwerowej logiki rakiet.
- Uprościć traile rakiet zgodnie z decyzjami w [DECISIONS.md](DECISIONS.md); pozostawić dwie smugi skrzydeł jako CPU Ribbon, dopóki pomiary nie wykażą problemu.

## Dalsze plany gameplayowe

- Dalsze archetypy broni, wyposażenie, przedmioty, perki i drzewko pasywne.
- Drużyny i blokada friendly fire.
- Dalsze dopracowanie AI latającego/naziemnego, spawnera i respawnu.
- Czytelność HUD-u, targetingu, attacker indicators i nameplates przy dużej liczbie graczy.
- Efekt śmierci, informacja o zabójcy i pełny przebieg meczu.

## Znane, nieblokujące ostrzeżenia

- `EditorDataStorageUI` — ostrzeżenia wyłącznie edytorowe; nie traktować jako błąd runtime/packaging bez dodatkowych symptomów.
- Legacy input w `BP_Pilot` — znany dług techniczny, ale folder pozostaje obecnie częścią twardego łańcucha zależności.

## Zasady realizacji

- Nie wracać do ruchu oczekującego na serwer.
- Nie wykonywać dużego refaktoru równocześnie z tuningiem feelingu lotu.
- Nie modyfikować ani usuwać binarnych assetów bez wyraźnej potrzeby i osobnego przeglądu.
- Nie umieszczać referencyjnego źródła legacy w repozytorium ani cooku.
- Po zmianach C++ budować target `SamolotyEditor`; po zmianach sieciowych wykonywać macierz testów opisaną w [NetworkMovementArchitecture.md](NetworkMovementArchitecture.md).

