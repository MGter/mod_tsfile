# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a TS (MPEG-2 Transport Stream) file modification tool for testing video player robustness. It modifies PTS/DTS timestamps and simulates various stream errors in H.264/H.265 video streams.

## Build Commands

```bash
# Compile the main executable
sh make_file.sh

# Build and run with a JSON config file
sh run.sh <config.json>

# Build and debug with gdb
sh debug.sh <config.json>
```

The compilation uses g++ with C++11 standard, pthread library, and includes rapidjson for JSON parsing.

## Generating Test Configurations

```bash
# Generate H.264 test JSON configurations
python3 makejson.py -c 264

# Generate H.265 test JSON configurations
python3 makejson.py -c 265

# Custom input/output files
python3 makejson.py -c 264 -i input.ts -o output.json --skip-ffmpeg
```

The `makejson.py` script generates JSON configuration files with various PTS modification patterns for testing.

## Architecture

### Core Components

- **main.cpp**: Entry point - parses JSON config, creates TaskParam objects, spawns ModTsFile threads
- **mod_ts_file.cpp/h**: Core TS packet processing - parses PAT/PMT tables, modifies PTS/DTS, handles packet operations
- **parse_json.cpp/h**: JSON configuration parsing - defines TaskParam and FuncPattern classes
- **common/Log.cpp/Tool.cpp**: Utility functions for logging and tools

### TS Stream Processing

The `ModTsFile` class handles:
1. Parsing TS packet headers (188-byte packets)
2. Parsing PAT (Program Association Table) to find PMT PIDs
3. Parsing PMT (Program Map Table) to identify audio/video PIDs
4. Modifying PTS/DTS timestamps based on configured patterns
5. Simulating packet loss, duplication, and null packet injection

### Supported Operations (PtsFunc enum)

| Function | Description |
|----------|-------------|
| `cut` | Time-based stream cutting (start_sec to end_sec) |
| `add` | Add constant offset to PTS |
| `minus` | Subtract constant offset from PTS |
| `sin` | Sinusoidal PTS variation |
| `pulse` | Alternating PTS offset |
| `mult` | Multiply PTS by a factor |
| `loss` | Random packet deletion |
| `repeate` | Random packet duplication |
| `null` | Fill packets with null data |
| `pat_delete` | Delete PAT packets |
| `pmt_delete` | Delete PMT packets |

### Media Types

Patterns can target specific media: `all`, `audio`, `video`

## JSON Configuration Format

```json
{
  "task_array": [
    {
      "id": 0,
      "input_file": "input.ts",
      "output_file": "output.ts",
      "pattern": [
        {
          "start_sec": 30,
          "end_sec": 90,
          "media": "video",
          "func": "add",
          "pts_base": 180000
        },
        {
          "start_sec": -1,
          "end_sec": 180,
          "media": "all",
          "func": "cut",
          "pts_base": 0
        }
      ]
    }
  ]
}
```

## Stream Type Definitions

The project defines standard MPEG-2 stream types in `mod_ts_file.h`:
- H.264/AVC (0x1b)
- H.265/HEVC (0x24)
- AAC (0x0f)
- MPEG-4 Audio (0x11)
- AVS/AVS2/AVS3 video formats

## Threading Model

Each task runs in its own thread (`std::thread`). The main thread waits for all tasks to complete before cleanup.