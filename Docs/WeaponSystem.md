# System uzbrojenia samolotu

System rozdziela uzbrojenie na dwa niezależne sloty:

- `Gun` - broń strzelecka uruchamiana lewym przyciskiem myszy,
- `Missile` - broń rakietowa uruchamiana prawym przyciskiem myszy.

`UWeaponSystemComponent` odnajduje komponenty broni na samolocie, wybiera po jednej
aktywnej broni w każdym slocie i przekazuje do niej input, punkty wystrzału oraz
dane celowania. Wybrany indeks jest autorytatywny na serwerze i replikowany do
klientów.

## Sterowanie testowe

- `1` - następna broń w slocie `Gun`,
- `2` - następna broń w slocie `Missile`,
- LPM - użycie aktywnej broni `Gun`,
- PPM - użycie aktywnej broni `Missile` (w zawisie nadal steruje kamerą).

Na ekranie debugowym widoczna jest nazwa aktywnej broni i liczba broni wykrytych
w danym slocie. Dopóki w slocie istnieje tylko jeden komponent, naciśnięcie `1`
lub `2` wybierze ponownie tę samą broń.

## Dodawanie kolejnego typu broni

1. Utworzyć komponent dziedziczący po `UPlaneWeaponComponent`.
2. W konstruktorze ustawić `WeaponSlot` i czytelną `WeaponDisplayName`.
3. Zaimplementować `SetFireHeld`.
4. Jeżeli broń korzysta z punktów montażowych, zaimplementować `SetFirePoints`.
5. Jeżeli celuje z kamery, zaimplementować `SetAimContext`.
6. Dodać komponent do klasy samolotu lub jego Blueprinta.

Menedżer automatycznie wykryje nową broń przy uruchomieniu aktora. RifleGun i
RocketWeapon pozostają osobnymi komponentami odpowiedzialnymi za własną logikę,
cooldown, efekty i komunikację serwerową. System uzbrojenia jedynie wybiera broń
i kieruje do niej wspólne polecenia.

Przyszły ekwipunek będzie ustalał listę zamontowanych komponentów lub definicji
broni. Obecne przełączanie jest fundamentem do testów i nie implementuje jeszcze
przedmiotów, perków ani zapisu wyposażenia.
