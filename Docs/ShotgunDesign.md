# Prowizoryczny Shotgun

Shotgun jest drugim typem uzbrojenia w slocie `Gun`. Służy do sprawdzenia
przełączania broni oraz walki na małym dystansie.

## Zachowanie

- strzał jest hitscanem wykonywanym przez serwer,
- każdy strzał tworzy kilka niezależnych śrutów w stożku wokół kierunku kamery,
- cały strzał wychodzi naprzemiennie z lewego, a następnie prawego działa,
- obrażenia śrutów trafiających ten sam cel są sumowane i przekazywane jako jedno
  zdarzenie obrażeń,
- klient właściciela natychmiast odtwarza przewidywaną kosmetykę,
- pozostali gracze otrzymują końcowe tory śrutów przez multicast,
- broń ma nieskończoną amunicję i może strzelać ponownie podczas trzymania LPM,
- w pionowym zawisie broń jest zablokowana tak samo jak RifleGun.

## Domyślne parametry

Parametry znajdują się w `JetStats -> Base Shotgun Stats`:

- 12 śrutów,
- 75 obrażeń za śrut,
- 1,25 strzału na sekundę,
- 500 m zasięgu,
- 4 stopnie rozrzutu,
- wizualny śrut o długości 600 cm i szerokości 60 cm.

Żółte linie oznaczają śruty bez trafienia, a czerwone śruty, które trafiły
kolizję. Debug można wyłączyć na komponencie `ShotgunGun` przez
`Draw Pellet Debug`.

## Prezentacja

Komponent udostępnia osobne pola na klasę tracera, Niagara muzzle flash, Niagara
impact oraz dźwięki wystrzału i trafienia. Nie są wymagane do pierwszego testu.
