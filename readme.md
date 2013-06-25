#Hackerbrause-Zapfanlage 3000 Version 1.0
istmagard & istlulued (23.6.2013)



##1. Einleitung
Die Zapfanlage ermöglicht das Mikrocontrollergesteuerte Abfüllen von einer definierten Menge Flüssigkeit (bevorzugt Club-Mate aka Hackerbrause).

##2. Bedienung
Zum Abfüllen der Flüssigkeit muss ein Plastikbecher auf der Waage stehen. Dies dient dazu, zu verhindern, dass ungewollt Flüssigkeit auf die empfindliche Elektronik fällt.

Durch einen Druck auf den Taster 1 wird das Kalibrieren der Waage ausgelöst. War diese Kalibrierung erfolgreich, kann mit dem Taster 2 der Abfüllvorgang gestartet werden. Entsprechende Meldungen sollten über die serielle Schnittstelle auf dem Terminal ausgegeben werden.

##3. Konfiguration
Die Zapfanlage kann über einige Parameter konfiguriert werden:
in der Datei Termin6.c:

 * Waagenkonstanten C1 & C2. Diese sind notwendig für eine korrekte Gewichtsberechnung
 * MaxGewicht. Die Zapfmenge in Gramm. Gewöhnlich entspricht 1g = 1ml
 * becher_minimum und _maximum. Schwellwerte, mit denen das Vorhandensein eines Bechers überprüft wird (in Gramm)
 * zapftolanz. Menge, die Schwankungen und sonstige Ungenauigkeiten abfangen soll (in Gramm)

in der Datei seriell.c:

 * DEFAULT_BAUD. Baud-Rate für die serielle Konsole
 * CLOCK_SPEED. Prozessortakt in Hz