# Projet : MeshCore autonome pour M5Stack Cardputer ADV + Cap LoRa

Fork de meshcore-dev/MeshCore. Branche de travail : `cardputer-adv`.
But : firmware utilisable sans téléphone (messagerie au clavier, GPS, cartes hors ligne, réglages),
avec un mode BLE companion optionnel. Le code réseau de MeshCore n'est jamais modifié.

## Règles
- Ne jamais modifier src/ hors de src/helpers/ui et de nouveaux fichiers ;
  le spécifique va dans variants/m5stack_cardputer_adv/ et examples/cardputer_ui/.
- L'UI (examples/cardputer_ui/core/) ne parle au reste du firmware que via core/Services.h.
  Elle n'inclut jamais Arduino.h, RadioLib ni les en-têtes MeshCore : c'est ce qui permet le simulateur.
- Toujours compiler avant de pousser :
  - simulateur : `make -C examples/cardputer_ui/sim -j8 run` (doit finir avec 0 failure)
  - firmware : `pio run -e <env>` (voir la note réseau ci-dessous)
- Rapporter RAM statique et taille flash après chaque build ; alerte si RAM > 70 %.
- Pas de PSRAM : aucun double buffer, aucun malloc > 32 Ko. Un seul framebuffer 240x135 RGB565 (64,8 Ko).
- Le SPI est partagé radio/SD : tout accès SD passe par un mutex.
- Ajouter des logs série préfixés [CPADV] pour tout ce qui touche au matériel.
- L'utilisateur teste lui-même sur le matériel ; fournir une checklist de test à la fin de chaque phase.
- L'utilisateur n'est pas développeur : lui parler en français simple, sans jargon.

## Simulateur (examples/cardputer_ui/sim/)
- Compile l'UI portable pour Linux avec g++ + LovyanGFX (sans SDL, sans PlatformIO).
- Piloté par des scénarios texte (scenarios/*.txt) : touches, messages reçus, GPS, batterie,
  captures d'écran PNG (x3) dans sim/build/shots/, assertions `expect-*`. Syntaxe en tête de main_sim.cpp.
- Après un changement d'UI : lancer les scénarios et REGARDER les PNG produits (outil Read) avant de pousser.
- Chaque nouvel écran ajoute au moins un scénario avec captures et assertions.
- Ce que le simulateur ne couvre pas : radio SX1262, PI4IOE, codes réels du TCA8418, SPI partagé,
  audio ES8311, GPS réel, batterie, RAM réelle. Ça se valide sur le matériel.

## Build firmware et réseau
- La CI GitHub Actions (.github/workflows/cardputer.yml) compile les envs de la matrice et publie
  firmware.bin et firmware-merged.bin (flash complet à 0x0) en artefacts, plus les captures du simulateur.
- Dans l'environnement cloud Claude Code, le registre PlatformIO (api.registry.platformio.org,
  dl.registry.platformio.org, collector.platformio.org) peut être bloqué par la politique réseau :
  dans ce cas `pio run` échoue avec HTTPClientError. Se fier à la CI, ou demander à l'utilisateur
  d'autoriser ces domaines dans les réglages réseau de l'environnement.
- Lire l'état de la CI : `curl -s https://api.github.com/repos/Rafem44/MeshCore/actions/runs?branch=cardputer-adv`.

## Brochage
SX1262 : NSS 5, DIO1 4, RST 3, BUSY 6, SCK 40, MOSI 14, MISO 39,
DIO2 = RF switch, TCXO 3.0 V. Cap 1262 : PI4IOE I2C 0x43, broche 0 à activer.
GPS UART : ESP TX 13, ESP RX 15. Clavier TCA8418 : SDA 8, SCL 9, INT 11.
SD CS 12. Batterie ADC 10. Écran ST7789V2 240x135.

## Envs PlatformIO
- cardputer_adv_companion_ble : firmware companion officiel (appli téléphone), écran companion zoomé.
- cardputer_adv_standalone : firmware autonome, UI examples/cardputer_ui (core/ + device/), sans carte.
- cardputer_adv_standalone_map : idem + carte hors ligne sur SD (option voulue par l'utilisateur).
- cardputer_adv_demo : Cardputer seul, sans Cap : réseau et GPS simulés (DemoMesh / DemoGps), carte incluse.
- Écran, clavier (I2C interne 8/9), son, batterie via M5Unified/M5GFX ; Wire = port Grove (2/1).
- Firmware autonome : le GPS (Serial1) est lu par device/DeviceGps, EnvironmentSensorManager n'est pas démarré.

## Clavier (convention UI)
Écrans sans saisie : `;` `.` `,` `/` = flèches haut/bas/gauche/droite, `` ` `` = Échap.
Écrans de saisie : caractères bruts, flèches et Échap via Fn. Voir core/Keys.h (toNavigation).

## Feuille de route
0. Socle, CI, simulateur — fait.
1. Bring-up matériel (variante, radio, PI4IOE, écran, companion BLE) — compile, à valider sur matériel.
2. Clavier TCA8418 + squelette d'UI (accueil, barre d'état, réglages) — compile + simulateur OK,
   à valider sur matériel. Checklist : examples/cardputer_ui/TESTS.md.
3. Messagerie autonome + persistance LittleFS + son — compile + simulateur OK, à valider sur matériel.
   core/MessageStore (historique, partagé sim/appareil), device/CardputerMesh (BaseChatMesh, ACK, 3 essais).
4. GPS et boussole — compile + simulateur OK, à valider sur matériel.
   core/Nmea (parseur partagé), device/DeviceGps (UART, détection baud, horloge), écrans GPS + boussole.
5. Cartes hors ligne (tuiles RGB565 sur SD) — EN OPTION : env cardputer_adv_standalone_map (CPADV_MAP=1).
   tools/tiles/make_tiles.py (url autorisée / mbtiles / synthetic), core/screens/MapScreen, device/SdTiles.
6. Finitions — fait côté code : démo sans LoRa (env cardputer_adv_demo, core/DemoMesh + DemoGps), sauvegarde /
   restauration SD (device/DeviceBackup), luminosité, CPU 80 MHz écran éteint, alerte batterie, Releases sur tag
   cardputer-v*. Notice : examples/cardputer_ui/FLASH.md. Upstream à jour au 2026-11-02 (rien à intégrer).

## Suivre MeshCore officiel
- `git remote add upstream https://github.com/meshcore-dev/MeshCore.git && git fetch upstream main`
- `git merge upstream/main` dans cardputer-adv (pas de rebase : pas de force-push sur la branche partagée),
  puis compiler les 5 envs en CI et relancer le simulateur. Le code spécifique ne touche pas src/.

## Références
Portages existants à étudier sans copier aveuglément :
sosprz/meshcore-cardputer-adv, hdcasey/meshcore-cardputer-adv.
Variante la plus proche dans ce dépôt (ESP32-S3 + SX1262 + clavier + ST7789) : variants/lilygo_tdeck/.
