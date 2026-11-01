# Interface autonome Cardputer ADV

Interface utilisateur du firmware MeshCore autonome pour le M5Stack Cardputer ADV + Cap LoRa.

```
core/            UI portable (identique sur le Cardputer et dans le simulateur)
  Services.h     seule frontière avec le reste du firmware (mesh, système, GPS...)
  Keys.h         événements clavier logiques
  App.*          pile d'écrans, barre d'état, ligne d'aide, routage clavier
  Theme.h        dimensions, couleurs, polices (Latin-1, accents français)
  screens/       un fichier par écran
sim/             simulateur PC sans écran, piloté par scénarios, captures PNG
```

## Lancer le simulateur

Sous Linux (ou WSL sous Windows), avec `g++`, `make` et `git` :

```bash
make -C examples/cardputer_ui/sim -j8 run
```

Les captures sont écrites dans `examples/cardputer_ui/sim/build/shots/`.
À chaque push, la CI GitHub les publie aussi dans l'artefact `simulator-screenshots`.

## Écrire un scénario

Un fichier texte dans `sim/scenarios/`, une commande par ligne :

```
name Raphaël-CPADV
time 1793541600
rx Bob On se retrouve à 18h ?
key m
expect-screen Messages
shot messages
```

La liste complète des commandes est en tête de `sim/main_sim.cpp`.
