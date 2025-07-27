#!/bin/bash

# TickShaper System Monitor
# Monitors system resources while TickShaper is running

echo "TickShaper System Monitor"
echo "========================="

# Function to monitor system resources
monitor_resources() {
    while true; do
        clear
        echo "TickShaper System Monitor - $(date)"
        echo "=================================="
        
        # CPU usage
        echo "CPU Usage:"
        top -bn1 | grep "Cpu(s)" | awk '{print $2}' | sed 's/%us,//'
        
        # Memory usage
        echo ""
        echo "Memory Usage:"
        free -h
        
        # TickShaper process info
        echo ""
        echo "TickShaper Processes:"
        ps aux | grep tickshaper | grep -v grep || echo "No TickShaper processes found"
        
        # Network connections
        echo ""
        echo "ZeroMQ Connections (port 5555):"
        netstat -an | grep :5555 || echo "No connections on port 5555"
        
        # Shared memory
        echo ""
        echo "Shared Memory:"
        ipcs -m | grep tickshaper || echo "No TickShaper shared memory segments"
        
        echo ""
        echo "Press Ctrl+C to exit monitor"
        sleep 2
    done
}

# Check if TickShaper is running
if pgrep -f tickshaper > /dev/null; then
    echo "TickShaper is running. Starting monitor..."
    monitor_resources
else
    echo "TickShaper is not running."
    echo "Start TickShaper first with: ./run.sh"
fi