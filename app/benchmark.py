import time
from app.stats import get_pc_stats

def benchmark():
    start = time.time()
    stats = get_pc_stats()
    end = time.time()
    print(f"Stats retrieval took: {(end - start) * 1000:.2f} ms")
    return stats

if __name__ == "__main__":
    benchmark()
