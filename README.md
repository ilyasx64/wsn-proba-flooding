# WSN — Probabilistic Flooding v1 (P = 0,5)

Mini-projet M2 ISRI — *Convergence et terminaux mobiles*.
Étude expérimentale d'une stratégie de diffusion probabiliste dans un réseau de
capteurs sans fil (WSN), simulée avec **Contiki-NG / Cooja** sur des nœuds **Sky**.

## Principe

- Tous les nœuds sont des **sources** : chacun génère périodiquement une donnée
  numérotée, identifiée par le couple `(origin, seq)`.
- Le **nœud 1 est le puits** (destination unique) : il collecte les données.
- À la première réception d'un paquet, un nœud le retransmet avec une
  **probabilité fixe P = 0,5**, sinon il se tait.
- Suppression des doublons via la table des `(origin, seq)` déjà vus.
- **TTL = 15 sauts** (~2× le diamètre du réseau) : anti-boucle.

## Contenu

| Fichier | Rôle |
|---|---|
| `proba-flooding.c` | Application Contiki-NG (stratégie + logs de mesure) |
| `project-conf.h` | Active Energest (mesure d'énergie) |
| `Makefile` | Build NullNet / CSMA |
| `proba-flooding-10.csc` | Simulation Cooja — 10 nœuds (grille 5×2) |
| `proba-flooding-20.csc` | Simulation Cooja — 20 nœuds (grille 5×4) |
| `analyse.py` | Calcule les métriques à partir d'un log Cooja |
| `results/` | Logs bruts des campagnes (300 s, graine 123456) |

## Utilisation

```bash
# compiler pour Sky
make TARGET=sky proba-flooding.sky

# ouvrir une simulation dans Cooja
cd <contiki-ng>/tools/cooja
./gradlew run --args="--contiki=<contiki-ng> <ce-depot>/proba-flooding-10.csc"

# analyser un log sauvegardé depuis "Mote output"
python3 analyse.py log.txt --nodes 10
```

Paramètres modifiables en tête de `proba-flooding.c` :
`FORWARD_PERCENT` (50 = P = 0,5 ; 100 = flooding classique), `SEND_INTERVAL`,
`SINK_ID`, `MAX_HOPS`.

## Résultats (300 s par configuration)

| Configuration | Livraison au puits | Latence moyenne | TX / message |
|---|---|---|---|
| 10 nœuds, P = 0,5 | 55,0 % | 0,062 s | 3,49 |
| 10 nœuds, P = 100 % | 95,9 % | 0,072 s | 8,76 |
| 20 nœuds, P = 0,5 | 44,1 % | 0,083 s | 5,98 |
| 20 nœuds, P = 100 % | 96,3 % | 0,090 s | 18,38 |

**Lecture :** P = 0,5 divise le coût en transmissions par ~2,5 à 3, mais la
fiabilité chute avec la taille du réseau (extinction de la diffusion quand tous
les voisins « tirent face »), alors que le flooding classique reste à ~96 % au
prix d'un trafic bien plus élevé (broadcast storm).
