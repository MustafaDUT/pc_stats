import socket
import json
import time
from zeroconf import ServiceInfo, Zeroconf

# Configuration
UDP_IP = "0.0.0.0"
UDP_PORT = 8266
SERVICE_TYPE = "_getPcStats._udp.local."
SERVICE_NAME = "TestReceiver._getPcStats._udp.local."

def start_receiver():
    # Setup mDNS advertisement
    desc = {'version': '0.1'}
    info = ServiceInfo(
        SERVICE_TYPE,
        SERVICE_NAME,
        addresses=[socket.inet_aton("127.0.0.1")],
        port=UDP_PORT,
        properties=desc,
        server="testreceiver.local.",
    )
    
    zeroconf = Zeroconf()
    print(f"Registering service {SERVICE_NAME}...")
    zeroconf.register_service(info)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    
    print(f"Listening for UDP packets on port {UDP_PORT}...")
    
    try:
        while True:
            data, addr = sock.recvfrom(1024)
            try:
                payload = json.loads(data.decode())
                print(f"Received from {addr}: {payload}")
            except Exception as e:
                print(f"Error decoding packet: {e}")
    except KeyboardInterrupt:
        pass
    finally:
        print("Unregistering...")
        zeroconf.unregister_service(info)
        zeroconf.close()

if __name__ == "__main__":
    start_receiver()
