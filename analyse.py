#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Analyse d'un log Cooja - Sujet 2 (modele multi-sources -> 1 puits).
Chaque noeud (sauf le puits) genere ses propres donnees (origin, seq).
Metriques : messages generes, livraison au puits, LATENCE, transmissions, energie.

Usage :
  python3 analyse.py log.txt --nodes 10
  python3 analyse.py log.txt --nodes 20 --csv --label "20n_P50"
"""
import sys, re, argparse

# Courants Tmote Sky (CC2420, 3V) pour l'estimation d'energie
V=3.0; I_CPU=1.800e-3; I_LPM=0.0545e-3; I_TX=17.4e-3; I_RX=18.8e-3
ENERGEST_SECOND=32768.0

def parse_time(s):
    s=s.strip(); parts=s.split(':')
    try:
        if len(parts)==2:  return int(parts[0])*60+float(parts[1])
        if len(parts)==3:  return int(parts[0])*3600+int(parts[1])*60+float(parts[2])
        return float(s)
    except ValueError:
        return None

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('logfile')
    ap.add_argument('--nodes', type=int, default=10)
    ap.add_argument('--csv', action='store_true')
    ap.add_argument('--label', default=None)
    a=ap.parse_args()

    re_time=re.compile(r'^\s*([0-9:.]+)')
    re_src =re.compile(r'SOURCE origin=(\d+) seq=(\d+)')
    re_sink=re.compile(r'SINK_RECV origin=(\d+) seq=(\d+)')
    re_tx  =re.compile(r'\bTX\b .*seq=')
    re_drop=re.compile(r'\bDROP\b')
    re_en  =re.compile(r'ENERGEST cpu=(\d+) lpm=(\d+) tx=(\d+) rx=(\d+)')
    re_id  =re.compile(r'ID:(\d+)')

    gen={}      # (origin,seq) -> t emission
    sink={}     # (origin,seq) -> t arrivee au puits (1ere)
    n_tx=0; n_drop=0
    en_last={}  # mote -> (cpu,lpm,tx,rx)

    for line in open(a.logfile, errors='ignore'):
        tm=re_time.match(line); t=parse_time(tm.group(1)) if tm else None
        m=re_src.search(line)
        if m:
            gen[(int(m.group(1)),int(m.group(2)))]=t; continue
        m=re_sink.search(line)
        if m:
            k=(int(m.group(1)),int(m.group(2)))
            if k not in sink: sink[k]=t
            continue
        if re_tx.search(line): n_tx+=1; continue
        if re_drop.search(line): n_drop+=1; continue
        m=re_en.search(line)
        if m:
            mid=re_id.search(line)
            if mid: en_last[int(mid.group(1))]=tuple(int(x) for x in m.groups())

    n_gen=len(gen); n_deliv=len(sink)
    delivery=100.0*n_deliv/n_gen if n_gen else 0
    lats=[sink[k]-gen[k] for k in sink if k in gen and gen[k] is not None and sink[k] is not None and sink[k]>=gen[k]]
    latency=sum(lats)/len(lats) if lats else 0
    E=[]
    for mote,(cpu,lpm,tx,rx) in en_last.items():
        E.append(V*(I_CPU*cpu+I_LPM*lpm+I_TX*tx+I_RX*rx)/ENERGEST_SECOND*1000.0)
    e_avg=sum(E)/len(E) if E else 0
    tx_per=n_tx/n_gen if n_gen else 0

    if a.csv:
        lab=a.label or a.logfile
        print("%s,%d,%d,%.1f,%.3f,%d,%.2f,%.0f"%(lab,n_gen,n_deliv,delivery,latency,n_tx,tx_per,e_avg))
        return

    print("="*54)
    print(" RESULTATS  (%s)"%a.logfile)
    print("="*54)
    print(" Noeuds sources actives : %d  (puits = noeud 1)"%(a.nodes-1))
    print(" Messages generes (total): %d"%n_gen)
    print("-"*54)
    print(" Livraison au puits      : %.1f %%  (%d / %d)"%(delivery,n_deliv,n_gen))
    print(" LATENCE moyenne         : %.3f s"%latency)
    print("-"*54)
    print(" Transmissions (TX)      : %d   (%.2f / message)"%(n_tx,tx_per))
    print(" DROP (proba face)       : %d"%n_drop)
    print("-"*54)
    print(" Energie moyenne / noeud : %.0f mJ"%e_avg)
    print("="*54)

if __name__=='__main__':
    main()
