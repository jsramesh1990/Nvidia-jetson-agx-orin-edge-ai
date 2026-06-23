#!/bin/bash
set -e

echo "🚀 Edge AI System Docker Container"
echo "===================================="

# Create necessary directories
mkdir -p /var/edge_ai/data
mkdir -p /var/edge_ai/logs
mkdir -p /tmp/edge_ai

# Start the application
exec "$@"
