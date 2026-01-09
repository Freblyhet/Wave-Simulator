#!/usr/bin/env python3
"""
WaveSim Screenshot Capture Utility
Enhanced version with guided sequence mode for comprehensive documentation
"""

import os
import sys
import subprocess
import time
from datetime import datetime

def get_project_root():
    """Get the project root directory (where this script's parent directory is)"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    return os.path.dirname(script_dir)

def create_screenshots_directory():
    """Create screenshots directory if it doesn't exist"""
    project_root = get_project_root()
    screenshots_dir = os.path.join(project_root, "WaveSimScreenshots")
    os.makedirs(screenshots_dir, exist_ok=True)
    return screenshots_dir

def take_screenshot(filename_prefix="wave_sim", description=None):
    """Take a screenshot using macOS screencapture tool"""
    screenshots_dir = create_screenshots_directory()
    
    # Generate timestamp for unique filename
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S_%f")[:-3]
    filename = f"{filename_prefix}_{timestamp}.png"
    filepath = os.path.join(screenshots_dir, filename)
    
    print(f"\n📸 Taking screenshot: {description or 'WaveSim'}")
    print("   Click and drag to select the area to capture, or press Space for fullscreen")
    
    try:
        # Use screencapture with interactive selection
        result = subprocess.run([
            'screencapture', 
            '-i',  # Interactive selection
            '-x',  # Do not play sounds
            filepath
        ], capture_output=True, text=True)
        
        if result.returncode == 0 and os.path.exists(filepath):
            print(f"✅ Screenshot saved: {filename}")
            return filepath
        else:
            print(f"❌ Screenshot failed or cancelled")
            return None
            
    except subprocess.CalledProcessError as e:
        print(f"❌ Screenshot failed: {e}")
        return None
    except KeyboardInterrupt:
        print(f"\n❌ Screenshot cancelled by user")
        return None

def guided_screenshot_sequence():
    """Guide user through capturing a comprehensive set of screenshots"""
    print("🎬 WaveSim Screenshot Sequence")
    print("="*50)
    print("This will guide you through capturing screenshots showing different wave simulation features.")
    print("Make sure WaveSim is running and visible on your screen.")
    print("")
    
    screenshots = []
    
    # Screenshot sequence for wave simulation
    sequence = [
        ("initial", "Initial state: Clean interface with wave simulation ready"),
        ("wave_sources", "Wave sources: Show multiple wave sources creating interference patterns"),
        ("wave_propagation", "Wave propagation: Capture waves spreading across the simulation"),
        ("interference", "Interference patterns: Show complex wave interactions and interference"),
        ("wall_interaction", "Wall interactions: Demonstrate wave reflection and barriers"),
        ("controls_panel", "Controls panel: Show the UI with wave parameters and tools"),
    ]
    
    for i, (prefix, description) in enumerate(sequence, 1):
        print(f"\n📷 Screenshot {i}/{len(sequence)}: {description}")
        input("   Press Enter when ready to capture...")
        
        filepath = take_screenshot(prefix, description)
        if filepath:
            screenshots.append((prefix, filepath, description))
        else:
            print("   Skipping this screenshot...")
        
        if i < len(sequence):
            print("   Prepare for next screenshot...")
            time.sleep(1)
    
    print(f"\n🎉 Screenshot sequence complete!")
    print(f"📁 Screenshots saved to: {create_screenshots_directory()}")
    
    if screenshots:
        print("\n📋 Captured screenshots:")
        for prefix, filepath, description in screenshots:
            filename = os.path.basename(filepath)
            print(f"  • {filename} - {description}")
    
    return screenshots

def update_readme_with_screenshots():
    """Generate markdown for README with all screenshots in the directory"""
    screenshots_dir = create_screenshots_directory()
    
    # Find all PNG files in screenshots directory
    screenshot_files = []
    if os.path.exists(screenshots_dir):
        for file in os.listdir(screenshots_dir):
            if file.lower().endswith('.png'):
                screenshot_files.append(file)
    
    screenshot_files.sort()  # Sort alphabetically
    
    if not screenshot_files:
        print("❌ No screenshots found in WaveSimScreenshots directory")
        return
    
    print(f"\n📝 Generating README markdown for {len(screenshot_files)} screenshots")
    print("="*60)
    
    # Generate markdown
    markdown = "## Screenshots\n\n"
    
    for filename in screenshot_files:
        # Generate alt text from filename
        name_part = filename.replace('.png', '').replace('_', ' ').title()
        alt_text = f"Wave Simulation - {name_part}"
        
        markdown += f"![{alt_text}](WaveSimScreenshots/{filename})\n\n"
    
    print(markdown)
    print("="*60)
    print("✅ Copy this markdown and paste it into your README.md file")

def main():
    """Main function to handle command line arguments"""
    if len(sys.argv) > 1:
        if sys.argv[1] == '--sequence':
            guided_screenshot_sequence()
        elif sys.argv[1] == '--update-readme':
            update_readme_with_screenshots()
        elif sys.argv[1] == '--help':
            print("WaveSim Screenshot Capture Utility")
            print("Usage:")
            print("  python3 screenshot_capture.py           # Take a single screenshot")
            print("  python3 screenshot_capture.py --sequence # Guided screenshot sequence")
            print("  python3 screenshot_capture.py --update-readme # Generate README markdown")
            print("  python3 screenshot_capture.py --help    # Show this help")
        else:
            print(f"Unknown option: {sys.argv[1]}")
            print("Use --help for usage information")
    else:
        # Single screenshot mode
        take_screenshot()

if __name__ == "__main__":
    main()