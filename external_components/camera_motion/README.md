# camera_motion – ESPHome External Component

Portiert die Block-Diff-Bewegungserkennung aus
kakopappa/esp32-cam-motion-detection-with-no-pir-no-microwave in eine
ESPHome External Component. Sie hängt sich per `add_image_callback()`
in die `esp32_camera`-Komponente ein, dekodiert jedes n-te JPEG-Frame
nach RGB565, vergleicht es blockweise mit dem vorherigen Frame und
setzt bei genug veränderten Blöcken einen `binary_sensor` (device_class
`motion`) – den du danach ganz normal mit `on_press:` /
`automation:` für beliebige Aktionen nutzt.

## Installation

1. Diesen `components`-Ordner unverändert neben deine ESPHome-YAML
   kopieren (oder als Git-Submodule/`external_components`-Git-Quelle
   einbinden).
2. `example.yaml` als Vorlage nehmen und an dein Board anpassen.

## Wichtige Einschränkungen – bitte vor dem Flashen lesen

- **Pinbelegung prüfen**: Die Pins in `example.yaml` sind Platzhalter.
  Für dein Freenove-ESP32-S3-Kameramodul die exakte Pinbelegung aus der
  Freenove-Dokumentation bzw. dem mitgelieferten Arduino-Beispielsketch
  übernehmen.
- **Community-Hinweis zu Freenove + ESPHome**: Es gibt Berichte, dass
  die Standard-`esp32_camera`-Komponente bei manchen Freenove-S3-Modulen
  nicht ohne Weiteres initialisiert (siehe z. B.
  `Rudd-O/esphome_freenove_camera_component` auf GitHub, ein Fork mit
  Fixes für genau dieses Board). Teste daher zuerst **nur** die normale
  `esp32_camera`-Komponente (Stream in Home Assistant sichtbar?), bevor
  du `camera_motion` hinzufügst. Falls der Stream mit der Standard-
  komponente gar nicht erst läuft, müsste der `add_image_callback`-Hook
  stattdessen in besagten Fork eingebaut werden – die Diff-Logik in
  `camera_motion.cpp` lässt sich davon unabhängig 1:1 weiterverwenden.
- **CPU-Last**: Jedes analysierte Frame wird komplett JPEG→RGB565
  dekodiert. Bei größeren Auflösungen kann das die Bildrate spürbar
  drücken. Empfehlung: niedrige Auflösung (z. B. 320x240 oder kleiner)
  und `check_every_n_frames` erhöhen, wenn es zu Rucklern kommt.
- **PSRAM erforderlich**: Für den RGB565-Zwischenbuffer (Breite × Höhe
  × 2 Byte) wird ausreichend freier Heap/PSRAM benötigt. Beim
  ESP32-S3 daher `psram: mode: octal` (oder `quad`, je nach Modul)
  nicht vergessen.
- **Empfindlichkeit tunen**: `block_size`, `block_threshold` und
  `motion_threshold` entsprechen den gleichnamigen Parametern aus dem
  Original-Repo und funktionieren nach demselben Prinzip – einfach
  anhand der Logausgabe (`ESP_LOGD`) für deine Umgebung austesten.
- **Kein Ersatz für Bewegungsfilterung**: Reine Pixel-Diff-Verfahren
  reagieren auch auf Lichtwechsel, Regen, Blätter etc. Für zuverlässigere
  Ergebnisse ggf. zusätzlich mit einem mmWave-Sensor (LD2410/LD2450)
  oder serverseitiger Erkennung (z. B. Frigate) kombinieren.

## Update: API-Anpassung für neuere ESPHome-Versionen (2026)

In aktuellen ESPHome-Entwicklungsversionen wurde die Kamera-Anbindung
umgebaut: Es gibt jetzt eine generische `camera`-Basiskomponente,
`CameraImage`/`CameraRequester`/`IDLE` liegen im Namespace
`esphome::camera`, und `add_image_callback()` existiert nicht mehr.
Stattdessen implementiert man das Interface `camera::CameraListener`
(Methode `on_camera_image(...)`) und registriert sich per
`camera_->add_listener(this)`. Ebenso heißt die JPEG→RGB565-Funktion
in der esp32-camera-Bibliothek `jpg2rgb565(src, src_len, out, scale)`,
nicht `fmt2rgb565`. Der Code in diesem Ordner ist bereits auf diese
neuere API angepasst.

Falls du eine ältere ESPHome-Version (grob vor Anfang 2026) nutzt und
der Build stattdessen "add_listener existiert nicht" meldet, ist es
umgekehrt: Dann brauchst du die ursprüngliche `add_image_callback()`-
Variante mit `fmt2rgb565`. Prüfe im Zweifel direkt die installierte
`esp32_camera.h` (liegt nach einem Build-Versuch unter
`.esphome/build/<name>/src/esphome/components/esp32_camera/esp32_camera.h`),
welche Methoden/Namespaces dort tatsächlich existieren.
