import psutil
import subprocess
import plistlib
import platform
import os

def get_gpu_stats():
    """Retrieves GPU stats based on the operating system."""
    system = platform.system()
    
    if system == "Darwin":  # macOS
        try:
            # Run ioreg to get AGXAccelerator stats in XML format
            result = subprocess.run(
                ["ioreg", "-r", "-c", "AGXAccelerator", "-a"],
                capture_output=True, text=True, timeout=2
            )
            if result.returncode != 0:
                return None
            
            # Parse the plist output
            plist = plistlib.loads(result.stdout.encode())
            
            # The output is usually a list of dictionaries. 
            # We look for 'PerformanceStatistics' in the items.
            for item in plist:
                if "PerformanceStatistics" in item:
                    stats = item["PerformanceStatistics"]
                    return {
                        "model": item.get("model", "Apple Silicon GPU"),
                        "utilization": stats.get("Device Utilization %", 0),
                        "cores": item.get("gpu-core-count", 0),
                        "renderer_utilization": stats.get("Renderer Utilization %", 0),
                        "tiler_utilization": stats.get("Tiler Utilization %", 0)
                    }
        except Exception:
            pass
            
    elif system == "Linux" or system == "Windows":
        # First try NVIDIA (works on both Linux and Windows if drivers/toolkit installed)
        try:
            result = subprocess.run(
                ["nvidia-smi", "--query-gpu=name,utilization.gpu,memory.used,memory.total", "--format=csv,noheader,nounits"],
                capture_output=True, text=True, timeout=2
            )
            if result.returncode == 0:
                parts = result.stdout.strip().split(',')
                if len(parts) >= 2:
                    return {
                        "model": parts[0].strip(),
                        "utilization": float(parts[1].strip()),
                        "memory_used": float(parts[2].strip()) if len(parts) > 2 else 0,
                        "memory_total": float(parts[3].strip()) if len(parts) > 3 else 0
                    }
        except Exception:
            pass

        # Windows-specific fallback for non-NVIDIA GPUs
        if system == "Windows":
            try:
                # Get GPU name using wmic
                result = subprocess.run(
                    ["wmic", "path", "win32_VideoController", "get", "name"],
                    capture_output=True, text=True, timeout=2
                )
                if result.returncode == 0:
                    lines = [l.strip() for l in result.stdout.splitlines() if l.strip() and "Name" not in l]
                    if lines:
                        return {
                            "model": lines[0],
                            "utilization": 0,  # Generic fallback doesn't easily provide utilization
                            "note": "Generic Windows GPU detection"
                        }
            except Exception:
                pass
            
    return None

def get_pc_stats():
    """
    Retrieves various PC statistics including CPU, Memory, Disk, and Network info.
    Includes normalized values for easy display.
    Guaranteed not to raise exceptions.
    """
    stats = {
        "cpu": {"percent": 0, "count": 0, "freq": None},
        "memory": {"total": 0, "available": 0, "used": 0, "percent": 0},
        "gpu": None,
        "disk": {"usage": None, "io": None},
        "network": {"io": None},
        "normalized": {"cpu": 0, "memory": 0, "gpu": 0}
    }

    try:
        # CPU Stats
        try:
            cpu_percent = psutil.cpu_percent(interval=0.1)
            stats["cpu"]["percent"] = cpu_percent
            stats["cpu"]["count"] = psutil.cpu_count(logical=True)
            freq = psutil.cpu_freq()
            if freq:
                stats["cpu"]["freq"] = freq._asdict()
            stats["normalized"]["cpu"] = cpu_percent
        except Exception:
            pass

        # Memory Stats
        try:
            vm = psutil.virtual_memory()
            stats["memory"] = {
                "total": vm.total,
                "available": vm.available,
                "used": vm.used,
                "percent": vm.percent,
            }
            stats["normalized"]["memory"] = vm.percent
        except Exception:
            pass

        # GPU Stats
        try:
            gpu = get_gpu_stats()
            stats["gpu"] = gpu
            if gpu:
                stats["normalized"]["gpu"] = gpu.get("utilization", 0)
        except Exception:
            pass

        # Disk Stats
        try:
            disk = psutil.disk_usage('/')
            stats["disk"]["usage"] = disk._asdict()
            io = psutil.disk_io_counters()
            if io:
                stats["disk"]["io"] = io._asdict()
        except Exception:
            pass

        # Network Stats
        try:
            net_io = psutil.net_io_counters()
            if net_io:
                stats["network"]["io"] = net_io._asdict()
        except Exception:
            pass
            
    except Exception:
        # Final catch-all to ensure it never ever raises an exception to the caller
        pass

    return stats
