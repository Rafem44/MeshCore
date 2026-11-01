# Installer le firmware sur le Cardputer ADV

## 1. Choisir le bon fichier

| Fichier | Pour qui | Module LoRa |
| --- | --- | --- |
| `cardputer_adv_demo` | **Essayer l'interface** : contacts et GPS simulés | pas besoin |
| `cardputer_adv_standalone` | Messagerie autonome, sans téléphone | Cap LoRa-1262 |
| `cardputer_adv_standalone_map` | Pareil + carte hors ligne sur la carte SD | Cap LoRa-1262 |
| `cardputer_adv_companion_ble` | Avec l'appli MeshCore du téléphone (Bluetooth) | Cap LoRa-1262 |

**Avec le Cap LoRa : visser l'antenne avant d'allumer.** Émettre sans antenne abîme la radio.

## 2. Télécharger

- **Versions publiées** : page *Releases* du dépôt GitHub (`Rafem44/MeshCore`), dernière version
  `cardputer-v...`. Un seul fichier `.bin` par firmware, à flasher tel quel.
- **Dernière compilation** (il faut être connecté à GitHub) : onglet *Actions* → dernière exécution
  « Cardputer ADV » verte → *Artifacts* → `firmware-<nom>` → prendre le fichier `...-merged.bin`.

## 3. Flasher (le plus simple : dans le navigateur)

1. Ouvrir https://thelastoutpostworkshop.github.io/ESPConnect/ avec **Chrome ou Edge** sur un ordinateur.
2. Brancher le Cardputer en USB-C. S'il n'est pas reconnu : l'éteindre, maintenir le bouton **G0**
   (sur le côté) en branchant le câble, puis relâcher.
3. *Connect* → choisir le port série du Cardputer.
4. Choisir le fichier `.bin`, adresse **0x0**, lancer l'écriture.
5. Débrancher / rebrancher (ou appuyer sur reset).

En ligne de commande, c'est équivalent :

```bash
esptool.py --chip esp32s3 write_flash 0x0 cardputer_adv_demo-0.6.0.bin
```

## 4. Voir les messages de diagnostic (pour les tests)

Moniteur série à **115200 bauds** (ESPConnect a un onglet *Serial*, ou l'outil de l'IDE Arduino).
Les lignes qui commencent par `[CPADV]` décrivent ce que fait le firmware : les copier en entier
pour signaler un problème.

## 5. Garder sa configuration

Avant de changer de firmware : *Réglages → Sauvegarder sur SD*. Après le flash :
*Réglages → Restaurer depuis SD* (deux fois Entrée). Le fichier `cardputer_backup.bin` contient ta
**clé privée MeshCore** : ne prête pas la carte SD.

## Touches utiles

| Touche | Action |
| --- | --- |
| `M` `C` `G` `K` `S` | Messages, Contacts, GPS, Carte, Réglages (depuis l'accueil) |
| `;` `.` `,` `/` | haut, bas, gauche, droite |
| `` ` `` | retour |
| Entrée | ouvrir / envoyer |
| Fn + `;` `.` | défiler dans une discussion |
| Fn + `` ` `` | retour depuis un écran de saisie |
| Alt + `e` (`a`, `i`, `o`, `u`, `c`) | lettres accentuées : é, puis è ê ë en rappuyant |
