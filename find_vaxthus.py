import requests

ips = [
    "192.168.38.1",
    "192.168.38.127",
    "192.168.38.157",
    "192.168.38.184",
    "192.168.38.202",
    "192.168.38.229",
    "192.168.38.237"
]

print("Checking for Vaxthus web server...")

for ip in ips:
    try:
        url = f"http://{ip}"
        response = requests.get(url, timeout=2)
        if response.status_code == 200:
            print(f"✅ Response from {ip}:")
            # Print first 100 chars of content to ID it
            print(response.text[:100])
            if "Växthus" in response.text or "Vaxthus" in response.text:
                print(f"!!! FOUND TARGET: {ip} !!!")
    except Exception as e:
        print(f"❌ {ip}: No response")
