import serial, sys, time
port = sys.argv[1] if len(sys.argv) > 1 else "COM5"
duration = float(sys.argv[2]) if len(sys.argv) > 2 else 90
logpath = sys.argv[3] if len(sys.argv) > 3 else r"C:\Users\JC\Downloads\BW16\BW16\s3-integration\_serial_log.txt"
try:
    sp = serial.Serial(port, 115200, timeout=0.5)
except Exception as e:
    print("OPEN_FAIL", e); sys.exit(1)
print("LISTENING", port, "for", duration, "s ->", logpath)
t0 = time.time()
buf = ""
with open(logpath, "w", encoding="utf-8") as f:
    while time.time() - t0 < duration:
        try:
            n = sp.in_waiting
            if n:
                data = sp.read(n).decode("utf-8", errors="replace")
                buf += data
                while "\n" in buf:
                    line, buf = buf.split("\n", 1)
                    line = line.rstrip("\r")
                    if line.strip():
                        f.write(time.strftime("%H:%M:%S") + " " + line + "\n")
                        f.flush()
                        print(line)
        except Exception as e:
            print("READ_ERR", e); time.sleep(0.2)
sp.close()
print("DONE")
