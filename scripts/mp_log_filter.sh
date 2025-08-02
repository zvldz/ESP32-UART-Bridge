#!/bin/bash

# Mission Planner Log Filter Script
# Filters MAVLink-related issues from Mission Planner logs

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if input file provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <mission_planner.log> [output.log]"
    echo "If output file not specified, will create <input>_filtered.log"
    exit 1
fi

INPUT_FILE="$1"
OUTPUT_FILE="${2:-${INPUT_FILE%.*}_filtered.log}"

# Check if input file exists
if [ ! -f "$INPUT_FILE" ]; then
    echo -e "${RED}Error: File '$INPUT_FILE' not found${NC}"
    exit 1
fi

echo -e "${BLUE}Processing Mission Planner log...${NC}"
echo "Input: $INPUT_FILE"
echo "Output: $OUTPUT_FILE"
echo ""

# Create temporary files for different categories
TMP_DIR=$(mktemp -d)
CLEAN_FILE="$TMP_DIR/clean_input.tmp"
PACKET_LOSS="$TMP_DIR/packet_loss.tmp"
CRC_ERRORS="$TMP_DIR/crc_errors.tmp"
MAVFTP_ERRORS="$TMP_DIR/mavftp_errors.tmp"
WRONG_ANSWERS="$TMP_DIR/wrong_answers.tmp"
STATS="$TMP_DIR/stats.tmp"

# Clean the input file from binary garbage
echo -e "${YELLOW}Cleaning input file from binary data...${NC}"
# Remove null bytes and non-printable characters, keep only ASCII and common UTF-8
tr -cd '\11\12\15\40-\176' < "$INPUT_FILE" > "$CLEAN_FILE"

# Check if cleaning was needed
ORIG_SIZE=$(wc -c < "$INPUT_FILE")
CLEAN_SIZE=$(wc -c < "$CLEAN_FILE")
if [ $ORIG_SIZE -ne $CLEAN_SIZE ]; then
    echo "Removed $(($ORIG_SIZE - $CLEAN_SIZE)) bytes of binary data"
fi

# Use cleaned file for analysis
ANALYSIS_FILE="$CLEAN_FILE"

# Force grep to treat file as text with -a flag (extra safety)
GREP="grep -a"

# Extract different types of errors
echo -e "${YELLOW}Extracting packet losses...${NC}"
# More strict pattern - line must start with INFO/ERROR/WARN
$GREP -E "^(INFO|ERROR|WARN) .*pkts lost [1-9][0-9]*" "$ANALYSIS_FILE" > "$PACKET_LOSS" 2>/dev/null

echo -e "${YELLOW}Extracting CRC errors...${NC}"
$GREP -E "^(INFO|ERROR|WARN) .*(crc fail|Bad Packet|Mavlink Bad Packet)" "$ANALYSIS_FILE" > "$CRC_ERRORS" 2>/dev/null

echo -e "${YELLOW}Extracting MAVFtp errors...${NC}"
$GREP -E "^(INFO|ERROR|WARN) .*MAVFtp.*(lost data|kErr|Missing Part|Failed|exception)" "$ANALYSIS_FILE" > "$MAVFTP_ERRORS" 2>/dev/null

echo -e "${YELLOW}Extracting wrong answers...${NC}"
$GREP -E "^(ERROR) .*Wrong Answer" "$ANALYSIS_FILE" > "$WRONG_ANSWERS" 2>/dev/null

# Create summary statistics
echo -e "${YELLOW}Generating statistics...${NC}"
{
    echo "=== MISSION PLANNER LOG ANALYSIS ==="
    echo "File: $INPUT_FILE"
    echo "Date: $(date)"
    echo ""
    echo "=== SUMMARY ==="
    echo "Packet losses: $(wc -l < "$PACKET_LOSS" 2>/dev/null || echo 0) occurrences"
    echo "CRC errors: $(wc -l < "$CRC_ERRORS" 2>/dev/null || echo 0) occurrences"
    echo "MAVFtp errors: $(wc -l < "$MAVFTP_ERRORS" 2>/dev/null || echo 0) occurrences"
    echo "Wrong answers: $(wc -l < "$WRONG_ANSWERS" 2>/dev/null || echo 0) occurrences"
    echo ""

    # Extract loss statistics - last 20 instead of 10 for better view
    echo "=== LOSS STATISTICS (last 20 entries) ==="
    $GREP "bps" "$ANALYSIS_FILE" | $GREP -E "loss [1-9]" | tail -20 | sed 's/^/  /'
    echo ""

    # Show max packet loss
    echo "=== MAX PACKET LOSSES ==="
    $GREP -E "^(INFO|ERROR|WARN) .*pkts lost [1-9][0-9]*" "$ANALYSIS_FILE" | \
        sed -E 's/.*pkts lost ([0-9]+).*/\1/' | \
        grep -E '^[0-9]+$' | \
        sort -nr | head -10 | while read loss; do
        if [ -n "$loss" ]; then
            echo "  Lost $loss packets"
        fi
    done
    echo ""
} > "$STATS"

# Combine all filtered content
{
    cat "$STATS"

    if [ -s "$PACKET_LOSS" ]; then
        echo "=== PACKET LOSSES (first 30) ==="
        # Additional cleanup - remove lines with garbage
        grep -E "^(INFO|ERROR|WARN) MissionPlanner" "$PACKET_LOSS" | head -30
        TOTAL_LOSSES=$(wc -l < "$PACKET_LOSS")
        if [ $TOTAL_LOSSES -gt 30 ]; then
            echo "... and $(($TOTAL_LOSSES - 30)) more"
        fi
        echo ""
    fi

    if [ -s "$CRC_ERRORS" ]; then
        echo "=== CRC ERRORS (first 30) ==="
        # Additional cleanup
        grep -E "^(INFO|ERROR|WARN) MissionPlanner" "$CRC_ERRORS" | head -30
        TOTAL_CRC=$(wc -l < "$CRC_ERRORS")
        if [ $TOTAL_CRC -gt 30 ]; then
            echo "... and $(($TOTAL_CRC - 30)) more"
        fi
        echo ""
    fi

    if [ -s "$MAVFTP_ERRORS" ]; then
        echo "=== MAVFTP ERRORS ==="
        cat "$MAVFTP_ERRORS"
        echo ""
    fi

    if [ -s "$WRONG_ANSWERS" ]; then
        echo "=== WRONG ANSWERS (first 30) ==="
        head -30 "$WRONG_ANSWERS"
        TOTAL_WRONG=$(wc -l < "$WRONG_ANSWERS")
        if [ $TOTAL_WRONG -gt 30 ]; then
            echo "... and $(($TOTAL_WRONG - 30)) more"
        fi
        echo ""
    fi

    # Show sequence number analysis
    echo "=== SEQUENCE NUMBER ANALYSIS ==="
    $GREP -E "^(INFO|ERROR|WARN) .*seqno [0-9]+ exp [0-9]+" "$ANALYSIS_FILE" | head -20
    echo ""

    # Connection statistics
    echo "=== CONNECTION STATISTICS ==="
    echo "Parameter download attempts:"
    $GREP -c "getParamList" "$ANALYSIS_FILE" || echo 0
    echo ""
    echo "MAVFtp file transfer attempts:"
    $GREP -c "MAVFtp.*GetFile" "$ANALYSIS_FILE" || echo 0
    echo ""
    echo "Connection resets/retries:"
    $GREP -c "Retry" "$ANALYSIS_FILE" || echo 0
    echo ""

} > "$OUTPUT_FILE"

# Cleanup
rm -rf "$TMP_DIR"

# Display results
echo ""
echo -e "${GREEN}Analysis complete!${NC}"
echo ""
echo "Summary:"
echo "  - Packet losses: $($GREP -c "pkts lost [1-9]" "$ANALYSIS_FILE" 2>/dev/null || echo 0)"
echo "  - CRC errors: $($GREP -c "crc fail" "$ANALYSIS_FILE" 2>/dev/null || echo 0)"
echo "  - MAVFtp failures: $($GREP -c "MAVFtp.*lost data" "$ANALYSIS_FILE" 2>/dev/null || echo 0)"
echo "  - Wrong answers: $($GREP -c "Wrong Answer" "$ANALYSIS_FILE" 2>/dev/null || echo 0)"
echo ""
echo "Output saved to: $OUTPUT_FILE"

# Show first few lines of output
echo ""
echo "Preview of filtered log:"
echo "------------------------"
head -50 "$OUTPUT_FILE"
echo "..."
echo ""
echo "Use 'less $OUTPUT_FILE' to view full filtered log"
