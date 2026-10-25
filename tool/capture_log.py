# pip install pyserial
import sys, os, time, threading, serial

port, outfile = sys.argv[1], sys.argv[2]
os.makedirs(os.path.dirname(os.path.abspath(outfile)), exist_ok=True)
t0 = time.time()
f = open(outfile, "w", encoding="utf-8")
lock = threading.Lock()

def write(text):
    with lock:
        f.write(f"{time.time() - t0:.3f}\t{text}\n")
        f.flush()

def markers():
    while True:
        try:
            name = input().strip() or "mark"
        except EOFError:
            return
        write(f"MARK {name}")
        print(f"[marker saved: {name}]")

threading.Thread(target=markers, daemon=True).start()
print("Logging. Type a marker name and press Enter (e.g. outage_start). Ctrl+C to stop.")

with serial.Serial(port, 115200, timeout=1) as s:
    try:
        while True:
            line = s.readline().decode(errors="ignore").rstrip()
            if line:
                print(line)
                write(line)
    except KeyboardInterrupt:
        pass
f.close()
print("saved", outfile)