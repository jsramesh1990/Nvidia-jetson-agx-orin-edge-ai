#!/usr/bin/env python3
import time
import cv2
import numpy as np
import json
import sys
import psutil
import GPUtil

def benchmark_system():
    """Run system benchmarks"""
    print("🏃 Running system benchmarks...")
    print("====================================")
    
    results = {
        "timestamp": time.time(),
        "system": {
            "cpu": {},
            "memory": {},
            "gpu": {}
        },
        "performance": {
            "camera_fps": 0,
            "detection_ms": 0,
            "recognition_ms": 0
        }
    }
    
    # CPU info
    cpu_info = psutil.cpu_times()
    results["system"]["cpu"] = {
        "cores": psutil.cpu_count(),
        "frequency": psutil.cpu_freq().current if psutil.cpu_freq() else 0,
        "usage": psutil.cpu_percent(interval=1)
    }
    
    # Memory info
    mem = psutil.virtual_memory()
    results["system"]["memory"] = {
        "total_gb": mem.total / (1024**3),
        "available_gb": mem.available / (1024**3),
        "used_percent": mem.percent
    }
    
    # GPU info
    try:
        gpus = GPUtil.getGPUs()
        if gpus:
            gpu = gpus[0]
            results["system"]["gpu"] = {
                "name": gpu.name,
                "memory_total_gb": gpu.memoryTotal / 1024,
                "memory_used_gb": gpu.memoryUsed / 1024,
                "load": gpu.load
            }
    except:
        print("⚠️  GPU information not available")
    
    print(json.dumps(results, indent=2))
    
    # Save results
    with open("benchmark_results.json", "w") as f:
        json.dump(results, f, indent=2)
    
    print(f"\n✅ Results saved to benchmark_results.json")
    return results

if __name__ == "__main__":
    benchmark_system()
