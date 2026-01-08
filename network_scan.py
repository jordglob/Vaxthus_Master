import os
import socket
import threading

# Din nätverksserie (Ändra om du inte kör 192.168.1.x)
NETWORK_PREFIX = "192.168.38."

def scan_ip(ip):
    # Pinge med 100ms timeout (-w 100) för att det ska gå fort
    response = os.system(f"ping -n 1 -w 100 {ip} > nul")
    
    if response == 0:
        try:
            # Försök ta reda på namnet
            hostname = socket.gethostbyaddr(ip)[0]
        except:
            hostname = "Okänd enhet"
            
        print(f"🟢 HITTAD: {ip}  \t({hostname})")

print(f"🚀 Scannar {NETWORK_PREFIX}1 till 254...")

threads = []
for i in range(1, 255):
    ip = NETWORK_PREFIX + str(i)
    # Vi kör trådar så vi kan kolla alla samtidigt (annars tar det 5 minuter)
    t = threading.Thread(target=scan_ip, args=(ip,))
    t.start()
    threads.append(t)

# Vänta på att alla ska bli klara
for t in threads:
    t.join()

print("🏁 Scanning klar.")