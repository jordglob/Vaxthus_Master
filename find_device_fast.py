import requests
import socket
import concurrent.futures

def check_ip(ip):
    url = f"http://{ip}"
    try:
        response = requests.get(url, timeout=0.5)
        if response.status_code == 200:
            if "Växthus" in response.text or "Vaxthus" in response.text:
                return ip
    except:
        pass
    return None

print("Scanning 192.168.38.0/24 for 'Växthus'...")

found_ip = None
with concurrent.futures.ThreadPoolExecutor(max_workers=50) as executor:
    ips = [f"192.168.38.{i}" for i in range(1, 255)]
    results = executor.map(check_ip, ips)
    
    for result in results:
        if result:
            found_ip = result
            print(f"!!! FOUND DEVICE AT: {found_ip} !!!")
            break

if not found_ip:
    print("Could not find device. Is it powered on and connected to WiFi?")
