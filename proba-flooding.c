/*
 * Mini-projet WSN - M2 ISRI - Sujet 2
 * Strategie : PROBABILISTIC FLOODING v1 (P = 0.5)
 *
 * MODELE (version demandee par l'encadrante) :
 *   - TOUS les noeuds sont des SOURCES : chaque capteur genere ses propres
 *     donnees, numerotees par une sequence PROPRE a lui (origin, seq).
 *     Ex : noeud 8 -> seq 0,1,2...   noeud 2 -> seq 0,1,2...
 *   - Un SEUL noeud est le PUITS (SINK_ID) = la destination qui collecte.
 *   - Diffusion par flooding probabiliste : a la 1ere reception d'un (origin,seq)
 *     nouveau, un noeud retransmet avec proba P = 0.5 ; sinon il se tait.
 *     Les (origin,seq) deja vus sont ignores (suppression des doublons).
 *
 * Metriques (extraites des logs) :
 *   - messages generes : lignes "SOURCE origin=.. seq=.."
 *   - livraison au puits : lignes "SINK_RECV origin=.. seq=.."
 *   - LATENCE : t(SINK_RECV) - t(SOURCE) pour un meme (origin,seq)
 *   - nb transmissions : lignes "TX"
 *   - energie : lignes "ENERGEST"
 */

#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "sys/energest.h"
#include "sys/node-id.h"
#include "lib/random.h"
#include <string.h>
#include <stdio.h>

#include "sys/log.h"
#define LOG_MODULE "Flood"
#define LOG_LEVEL LOG_LEVEL_INFO

/* ==================== PARAMETRES ==================== */
#define SINK_ID          1                    /* le PUITS = destination unique */
#define SEND_INTERVAL    (10 * CLOCK_SECOND)  /* frequence d'emission de CHAQUE source */
#define FORWARD_PERCENT  50                   /* strategie */
#define MAX_HOPS         15                   /* TTL (anti-boucle) */
#define SEEN_MAX         256                  /* memoire des (origin,seq) deja vus */
#define ENERGEST_PERIOD  (30 * CLOCK_SECOND)
/* =================================================== */

typedef struct {
  uint16_t origin;
  uint16_t seq;
  uint8_t  hops;
} flood_msg_t;

static struct { uint16_t origin; uint16_t seq; } seen[SEEN_MAX];
static uint16_t seen_head = 0, seen_count = 0;
static uint32_t nb_tx = 0;
static flood_msg_t tx_buf;

/*---------------------------------------------------------------------------*/
static int is_seen(uint16_t o, uint16_t s) {
  uint16_t i;
  for(i = 0; i < seen_count; i++)
    if(seen[i].origin == o && seen[i].seq == s) return 1;
  return 0;
}
static void add_seen(uint16_t o, uint16_t s) {
  seen[seen_head].origin = o; seen[seen_head].seq = s;
  seen_head = (seen_head + 1) % SEEN_MAX;
  if(seen_count < SEEN_MAX) seen_count++;
}
static int should_forward(void) {
  return (random_rand() % 100) < FORWARD_PERCENT;
}
static void broadcast_msg(flood_msg_t *m) {
  memcpy(&tx_buf, m, sizeof(tx_buf));
  nullnet_buf = (uint8_t *)&tx_buf;
  nullnet_len = sizeof(tx_buf);
  NETSTACK_NETWORK.output(NULL);
  nb_tx++;
  LOG_INFO("TX origin=%u seq=%u hops=%u total_tx=%lu\n",
           m->origin, m->seq, m->hops, (unsigned long)nb_tx);
}
/*---------------------------------------------------------------------------*/
static void input_callback(const void *data, uint16_t len,
                           const linkaddr_t *src, const linkaddr_t *dest) {
  flood_msg_t m;
  if(len != sizeof(m)) return;
  memcpy(&m, data, sizeof(m));

  if(is_seen(m.origin, m.seq)) return;        /* doublon -> ignore */
  add_seen(m.origin, m.seq);

  if(node_id == SINK_ID) {                     /* arrive a destination ! */
    LOG_INFO("SINK_RECV origin=%u seq=%u hops=%u\n", m.origin, m.seq, m.hops);
    return;                                    /* le puits ne retransmet pas */
  }

  if(m.hops >= MAX_HOPS) return;
  if(should_forward()) {                       /* proba P = 0.5 */
    m.hops++;
    broadcast_msg(&m);
  } else {
    LOG_INFO("DROP origin=%u seq=%u (proba)\n", m.origin, m.seq);
  }
}
/*---------------------------------------------------------------------------*/
static void print_energest(void) {
  energest_flush();
  LOG_INFO("ENERGEST cpu=%lu lpm=%lu tx=%lu rx=%lu\n",
    (unsigned long)energest_type_time(ENERGEST_TYPE_CPU),
    (unsigned long)energest_type_time(ENERGEST_TYPE_LPM),
    (unsigned long)energest_type_time(ENERGEST_TYPE_TRANSMIT),
    (unsigned long)energest_type_time(ENERGEST_TYPE_LISTEN));
}
/*---------------------------------------------------------------------------*/
PROCESS(flooding_process, "Probabilistic Flooding v1 - multi-sources");
AUTOSTART_PROCESSES(&flooding_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(flooding_process, ev, data) {
  static struct etimer send_timer, energest_timer;
  static uint16_t my_seq = 0;

  PROCESS_BEGIN();
  nullnet_set_input_callback(input_callback);
  etimer_set(&energest_timer, ENERGEST_PERIOD);

  if(node_id == SINK_ID) {
    LOG_INFO("Role=PUITS id=%u (destination unique)\n", node_id);
  } else {
    LOG_INFO("Role=SOURCE id=%u P=%u%%\n", node_id, FORWARD_PERCENT);
    /* depart aleatoire pour desynchroniser les sources (evite collisions) */
    etimer_set(&send_timer, CLOCK_SECOND + (random_rand() % SEND_INTERVAL));
  }

  while(1) {
    PROCESS_WAIT_EVENT();

    /* chaque SOURCE genere sa propre donnee (sa propre sequence) */
    if(node_id != SINK_ID && etimer_expired(&send_timer)) {
      flood_msg_t m;
      m.origin = node_id;
      m.seq = my_seq++;
      m.hops = 0;
      add_seen(m.origin, m.seq);               /* on ne retraite pas sa propre donnee */
      LOG_INFO("SOURCE origin=%u seq=%u\n", m.origin, m.seq);
      broadcast_msg(&m);
      /* periode + petit jitter pour rester desynchronise */
      etimer_set(&send_timer, SEND_INTERVAL - (SEND_INTERVAL/8) + (random_rand() % (SEND_INTERVAL/4 + 1)));
    }

    if(etimer_expired(&energest_timer)) {
      print_energest();
      etimer_reset(&energest_timer);
    }
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
