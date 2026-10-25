import sys, statistics

path = sys.argv[1]
skip = float(sys.argv[2]) if len(sys.argv) > 2 else 20.0   # ignore start-up seconds

rows = []
for line in open(path, encoding="utf-8"):
    t, _, text = line.rstrip("\n").partition("\t")
    try:
        rows.append((float(t), text))
    except ValueError:
        pass

marks = {text.split(None, 1)[1]: t for t, text in rows
         if text.startswith("MARK") and len(text.split(None, 1)) > 1}
t_start = marks.get("outage_start", float("inf"))
t_end   = marks.get("outage_end", float("inf"))
phases = {"baseline": (skip, t_start),
          "outage":   (t_start, t_end),
          "recovery": (t_end, float("inf"))}

tags = {"DHT": "[DHT]", "GPS": "[GPS]", "RTLS": "[RTLS]"}
sense = [t for t, text in rows if any(k in text for k in tags.values())]

def gaps(times, lo, hi):
    return [(b, b - a) for a, b in zip(times, times[1:]) if lo <= b < hi]

print(f"{'phase':9} {'signal':6} {'n':>4} {'median s':>9} {'max s':>7} {'at t=':>8}")
for pname, (lo, hi) in phases.items():
    if lo == float("inf"):
        continue
    series = {name: [t for t, text in rows if tag in text] for name, tag in tags.items()}
    series["ANY"] = sense
    for name, times in series.items():
        g = gaps(times, lo, hi)
        if g:
            at, mx = max(g, key=lambda x: x[1])
            print(f"{pname:9} {name:6} {len(g):>4} {statistics.median(x[1] for x in g):>9.2f} {mx:>7.2f} {at:>8.1f}")
    dropped = sum(1 for t, text in rows if "dropping telemetry" in text and lo <= t < hi)
    recon   = sum(1 for t, text in rows if "[MQTT] Reconnecting" in text and lo <= t < hi)
    print(f"{pname:9} dropped telemetry={dropped}, reconnect attempts={recon}\n")