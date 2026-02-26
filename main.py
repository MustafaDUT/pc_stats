import uvicorn
import asyncio
import socket
import json
from contextlib import asynccontextmanager
from fastapi import FastAPI, Request, HTTPException
from zeroconf import Zeroconf, ServiceBrowser, ServiceListener
from app.stats import get_pc_stats

# UDP and mDNS Configuration
SERVICE_TYPE = "_getPcStats._udp.local."
UDP_PORT = 8266
target_ip = "255.255.255.255" # Default to broadcast until discovered

class ESP32DiscoveryListener(ServiceListener):
    def update_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        self.add_service(zc, type_, name)

    def remove_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        global target_ip
        print(f"Service {name} removed, falling back to broadcast.")
        target_ip = "255.255.255.255"

    def add_service(self, zc: Zeroconf, type_: str, name: str) -> None:
        global target_ip
        info = zc.get_service_info(type_, name)
        if info:
            addresses = [socket.inet_ntoa(addr) for addr in info.addresses]
            if addresses:
                target_ip = addresses[0]
                print(f"Found/Updated ESP32 at {target_ip}")

import datetime

async def broadcast_stats():
    """Background task to send PC stats over UDP."""
    global target_ip
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    
    while True:
        try:
            stats = get_pc_stats()
            payload = {
                "cpu": stats["normalized"]["cpu"],
                "mem": stats["normalized"]["memory"],
                "gpu": stats["normalized"]["gpu"],
                "time": datetime.datetime.now().strftime("%H:%M:%S")
            }
            message = json.dumps(payload).encode()
            # Send to specifically discovered IP or broadcast
            sock.sendto(message, (target_ip, UDP_PORT))
        except Exception as e:
            print(f"Broadcast error: {e}")
            pass
        await asyncio.sleep(0.5)

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Start mDNS discovery with a more active browser
    zeroconf = Zeroconf()
    listener = ESP32DiscoveryListener()
    # ServiceBrowser will automatically call add_service for existing services
    browser = ServiceBrowser(zeroconf, SERVICE_TYPE, listener)
    
    # Start the broadcast task
    task = asyncio.create_task(broadcast_stats())
    
    yield
    
    # Cleanup
    task.cancel()
    browser.cancel()
    zeroconf.close()

# Security: Disable standard docs/redoc to minimize footprint
app = FastAPI(
    title="PC Stats Backend", 
    lifespan=lifespan,
    docs_url=None, 
    redoc_url=None,
    openapi_url=None
)

# Security middleware: Explicitly reject any non-GET requests (Read-Only)
@app.middleware("http")
async def read_only_middleware(request: Request, call_next):
    if request.method != "GET":
        from fastapi.responses import JSONResponse
        return JSONResponse(
            status_code=405, 
            content={"detail": "Read-Only API: Method Not Allowed"}
        )
    return await call_next(request)

@app.get("/")
async def root():
    return {"status": "online", "mode": "read-only"}

@app.get("/stats")
async def stats():
    return get_pc_stats()

if __name__ == "__main__":
    # Security: Log level INFO, only read-only methods allowed via middleware
    uvicorn.run(app, host="0.0.0.0", port=58008, log_level="info")
