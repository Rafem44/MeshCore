# Tests sur le Cardputer ADV

À faire quand le matériel est disponible. Garder le moniteur série ouvert (115200 bauds) pendant
chaque test et copier **tout** le log dans la session suivante : les lignes `[CPADV]` disent ce
que le firmware a détecté.

## Avant tout

- [ ] Antenne LoRa vissée sur le Cap **avant** de brancher le Cardputer (émettre sans antenne abîme la radio).
- [ ] Télécharger les fichiers depuis GitHub : onglet **Actions** du dépôt, dernière exécution
      « Cardputer ADV » réussie, section **Artifacts**.
- [ ] Flasher le fichier `...-merged.bin` à l'adresse `0x0` avec ESPConnect
      (https://thelastoutpostworkshop.github.io/ESPConnect/) ou :
      `esptool.py --chip esp32s3 write_flash 0x0 <fichier>-merged.bin`

## Phase 1 — firmware companion (`cardputer_adv_companion_ble`)

- [ ] Au démarrage, le log contient `[CPADV] board begin` puis `M5Unified board id=... (ADV=...)` avec les deux nombres identiques.
- [ ] Le log indique `Cap LoRa-1262 detected` (ou `assuming Cap LoRa868` si c'est un Cap 868).
- [ ] Si Cap 1262 : la ligne `PI4IOE dir=... -> P0 HIGH (ok)`.
- [ ] La ligne `[CPADV] SX1262 OK: 869.618 MHz ...`. Sinon, noter le code d'erreur affiché juste avant.
- [ ] L'écran affiche l'interface MeshCore (texte lisible, pas à l'envers, pas décalé).
- [ ] L'appli MeshCore sur le téléphone trouve l'appareil en Bluetooth ; le code PIN s'affiche sur l'écran.
- [ ] Un message envoyé depuis le téléphone (via le Cardputer) est reçu par un autre nœud, et inversement.
- [ ] Le niveau de batterie affiché dans l'appli est plausible (3,3 à 4,2 V).

## Phase 2 — firmware autonome (`cardputer_adv_standalone`)

- [ ] L'écran d'accueil s'affiche avec la barre d'état en haut et la ligne d'aide en bas.
- [ ] Le log indique `TCA8418 keyboard ready`.
- [ ] `S` ouvre les Réglages, `` ` `` revient à l'accueil. `M`, `C`, `G`, `K` ouvrent les écrans « Phase x ».
- [ ] Dans les Réglages : `;` et `.` montent et descendent, `,` et `/` changent la valeur.
- [ ] Renommer l'appareil : Entrée sur « Nom », effacer, taper un nom avec un accent (Alt+e donne é,
      appuyer encore pour è ê ë), Entrée. Le nouveau nom apparaît sur l'accueil.
- [ ] Fn + `` ` `` dans la saisie annule sans enregistrer.
- [ ] Chaque touche du clavier tapée dans la saisie du nom donne le bon caractère, Shift compris
      (tester toutes les lettres, chiffres et symboles ; noter celles qui sont fausses).
- [ ] Éteindre et rallumer : les réglages sont conservés (log `settings loaded`).
- [ ] Veille écran à 15 s : l'écran s'éteint, une touche le rallume (sans exécuter la touche).
- [ ] Réglages > Système : la mémoire libre et le temps « Allumé depuis » se mettent à jour.
- [ ] Laisser tourner 1 h : dans le log, la ligne `heap free ... min ...` toutes les minutes ne baisse pas régulièrement.

## Phase 3 — messagerie autonome (`cardputer_adv_standalone`)

Il faut un **second nœud MeshCore** (téléphone + appli avec un autre appareil, ou un autre Cardputer).

- [ ] Au démarrage, le log contient `mesh ready: ... contacts` puis, 3 s après, `advert sent (flood)`.
      Premier démarrage : `new identity created (saved)`.
- [ ] Le second nœud voit apparaître le Cardputer dans ses contacts (avec le nom choisi dans Réglages).
- [ ] Quand le second nœud s'annonce, un bandeau « Nouveau contact » apparaît et il est listé dans `C`.
- [ ] Message reçu du second nœud : bandeau + double bip, pastille orange sur l'accueil, compteur `*1` en haut.
- [ ] `M` → la conversation est en haut de la liste ; Entrée l'ouvre ; la pastille disparaît.
- [ ] Répondre (texte avec accents Alt+e…) : la bulle passe de « envoyé » à « reçu » (vert) en quelques secondes.
- [ ] Éteindre le second nœud et envoyer : « renvoyé » puis « échec » (rouge) au bout de ~30 s, bandeau + son grave.
- [ ] Canal Public : un message envoyé depuis l'appli sur Public arrive dans « Public » avec le nom de l'expéditeur.
- [ ] `J` puis `test` (devient `#test`) ; rejoindre le même canal `#test` dans l'appli : les messages passent
      dans les deux sens. **À vérifier : la clé des canaux #nom doit être la même que celle de l'appli.**
- [ ] Contacts : `F` met en favori (étoile, remonte en haut), `D` deux fois supprime.
- [ ] Fiche contact (Entrée) : chemin, « entendu il y a… ». Avec position du contact et GPS : distance et cap.
- [ ] 20 messages échangés dans les deux sens sans erreur ; statut « reçu » sur chacun.
- [ ] Éteindre / rallumer : conversations, messages, contacts et canaux toujours là (log `contacts loaded`).
- [ ] Écran en veille : un message reçu rallume l'écran avec le bandeau.
- [ ] Laisser tourner 1 h avec du trafic : la ligne `heap free ... min ...` du log reste stable.
