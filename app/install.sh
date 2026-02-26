#!/bin/bash

# PC Stats Backend - Unified Installer (macOS/Linux)
# This script installs 'uv', sets up the environment, and runs the app in the background.

set -e

echo "--- PC Stats Backend Installer ---"

# 1. Detect OS
OS_TYPE=$(uname -s)
echo "Platform: $OS_TYPE"

# 2. Check for 'uv'
if ! command -v uv &> /dev/null; then
    echo "'uv' not found. Installing now..."
    curl -LsSf https://astral.sh/uv/install.sh | sh
    source $HOME/.cargo/env
else
    echo "'uv' is already installed."
fi

# 3. Environment Setup
echo "Syncing dependencies..."
uv sync

# 4. Service Setup (Background Execution)
case "$OS_TYPE" in
    Darwin)
        echo "Configuring macOS LaunchAgent..."
        PLIST_PATH="$HOME/Library/LaunchAgents/com.pcstats.backend.plist"
        APP_PATH=$(pwd)
        PYTHON_PATH="$APP_PATH/.venv/bin/python3"
        
        cat <<EOF > "$PLIST_PATH"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.pcstats.backend</string>
    <key>ProgramArguments</key>
    <array>
        <string>$PYTHON_PATH</string>
        <string>$APP_PATH/main.py</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>WorkingDirectory</key>
    <string>$APP_PATH</string>
    <key>StandardOutPath</key>
    <string>$APP_PATH/server.log</string>
    <key>StandardErrorPath</key>
    <string>$APP_PATH/server.log</string>
</dict>
</plist>
EOF
        launchctl unload "$PLIST_PATH" 2>/dev/null || true
        # Wait a bit for the previous process to clear
        sleep 2
        launchctl load "$PLIST_PATH"
        echo "Service configured and started (macOS). Log: $APP_PATH/server.log"
        ;;
    Linux)
        echo "Starting as a background process (Linux)..."
        nohup uv run python main.py > server.log 2>&1 &
        echo "Service started (Linux). Log: server.log"
        ;;
    *)
        echo "Unsupported OS for automatic service setup. Please run: uv run python main.py"
        ;;
esac

echo "--- Installation Complete ---"
echo "API is running on http://0.0.0.0:58008"
echo "--- Security Note: The API is READ-ONLY ---"
