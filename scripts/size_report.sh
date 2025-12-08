#!/bin/bash
# Size report for ATtiny85 firmware builds
# Usage: size_report.sh <elf_file> [object_dir]
#
# Dependencies: avr-size, bc, find
# Optional: avr-nm (for symbol analysis)
#
# Exit codes:
#   0 - Success
#   1 - Memory limit exceeded
#   2 - Missing required dependencies

set -e

ELF_FILE="${1:-gatekeeper}"
OBJ_DIR="${2:-CMakeFiles/gatekeeper.dir/src}"

# Check required dependencies
check_deps() {
    local missing=""
    for cmd in avr-size bc; do
        if ! command -v "$cmd" >/dev/null 2>&1; then
            missing="$missing $cmd"
        fi
    done
    if [ -n "$missing" ]; then
        echo "ERROR: Missing required tools:$missing" >&2
        echo "Install with: sudo apt install binutils-avr bc" >&2
        exit 2
    fi
}

check_deps

# ATtiny85 limits
FLASH_LIMIT=8192
RAM_LIMIT=512

# Colors (disabled if not a terminal)
if [ -t 1 ]; then
    RED='\033[0;31m'
    YELLOW='\033[0;33m'
    GREEN='\033[0;32m'
    CYAN='\033[0;36m'
    BOLD='\033[1m'
    NC='\033[0m'
else
    RED='' YELLOW='' GREEN='' CYAN='' BOLD='' NC=''
fi

echo ""
echo -e "${BOLD}=== ATtiny85 Memory Report ===${NC}"
echo ""

# Get sizes using avr-size
SIZE_OUTPUT=$(avr-size --format=avr --mcu=attiny85 "$ELF_FILE" 2>/dev/null)

# Parse the output
FLASH_USED=$(echo "$SIZE_OUTPUT" | grep "Program:" | awk '{print $2}')
FLASH_PCT=$(echo "$SIZE_OUTPUT" | grep "Program:" | awk '{print $4}' | tr -d '(%')
RAM_USED=$(echo "$SIZE_OUTPUT" | grep "Data:" | awk '{print $2}')
RAM_PCT=$(echo "$SIZE_OUTPUT" | grep "Data:" | awk '{print $4}' | tr -d '(%')

# Determine color based on usage
flash_color() {
    local pct=$1
    if (( $(echo "$pct > 90" | bc -l) )); then echo -e "${RED}"
    elif (( $(echo "$pct > 75" | bc -l) )); then echo -e "${YELLOW}"
    else echo -e "${GREEN}"
    fi
}

ram_color() {
    local pct=$1
    if (( $(echo "$pct > 80" | bc -l) )); then echo -e "${RED}"
    elif (( $(echo "$pct > 60" | bc -l) )); then echo -e "${YELLOW}"
    else echo -e "${GREEN}"
    fi
}

FLASH_COLOR=$(flash_color "$FLASH_PCT")
RAM_COLOR=$(ram_color "$RAM_PCT")

# Print size summary
printf "  ${CYAN}Flash:${NC}  %s%5d${NC} / %d bytes  (%s%5.1f%%${NC})\n" \
    "$FLASH_COLOR" "$FLASH_USED" "$FLASH_LIMIT" "$FLASH_COLOR" "$FLASH_PCT"
printf "  ${CYAN}RAM:${NC}    %s%5d${NC} / %d bytes  (%s%5.1f%%${NC})\n" \
    "$RAM_COLOR" "$RAM_USED" "$RAM_LIMIT" "$RAM_COLOR" "$RAM_PCT"

# Calculate remaining
FLASH_FREE=$((FLASH_LIMIT - FLASH_USED))
RAM_FREE=$((RAM_LIMIT - RAM_USED))

echo ""
printf "  ${CYAN}Free:${NC}   Flash: %d bytes, RAM: %d bytes\n" "$FLASH_FREE" "$RAM_FREE"

# Top symbols by size (from object files)
echo ""
echo -e "${BOLD}=== Largest Symbols ===${NC}"
echo ""

if ! command -v avr-nm >/dev/null 2>&1; then
    echo "  (avr-nm not found, skipping - install binutils-avr for symbol analysis)"
elif [ ! -d "$OBJ_DIR" ]; then
    echo "  (object directory not found, skipping)"
else
    # Find all .o files and analyze
    # avr-nm --size-sort outputs: <hex_size> <type> <name>
    find "$OBJ_DIR" -name "*.o" -exec avr-nm --size-sort -C {} + 2>/dev/null | \
        grep -E "^[0-9a-f]+ [TtDdBb]" | \
        while read hex_size type name; do
            # Convert hex to decimal using shell
            size=$((16#$hex_size))
            case $type in
                T|t) section="text" ;;
                D|d) section="data" ;;
                B|b) section="bss " ;;
                *) section="    " ;;
            esac
            # Use space padding to avoid leading zeros (octal interpretation)
            printf "%d %s %s\n" "$size" "$section" "$name"
        done | \
        sort -rn | \
        head -10 | \
        while read size section name; do
            # Strip leading zeros for printf by forcing decimal with $((10#...))
            printf "  %5d bytes  [%s]  %s\n" "$((10#$size))" "$section" "$name"
        done
fi

echo ""

# Threshold warnings
WARN=0
if (( $(echo "$FLASH_PCT > 90" | bc -l) )); then
    echo -e "${RED}WARNING: Flash usage above 90%!${NC}"
    WARN=1
fi
if (( $(echo "$RAM_PCT > 80" | bc -l) )); then
    echo -e "${RED}WARNING: RAM usage above 80%!${NC}"
    WARN=1
fi

# Exit with error if over limits (for CI)
if [ "$FLASH_USED" -gt "$FLASH_LIMIT" ] || [ "$RAM_USED" -gt "$RAM_LIMIT" ]; then
    echo -e "${RED}ERROR: Memory limit exceeded!${NC}"
    exit 1
fi

exit 0
