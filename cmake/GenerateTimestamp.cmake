# Generate BuildTimestamp.h with current date/time
# This script is run at build time, not configure time

string(TIMESTAMP BUILD_TIMESTAMP "%b %d %Y %H:%M:%S")

file(WRITE "${OUTPUT_FILE}"
"// Auto-generated at build time - do not edit
#pragma once
#define BUILD_TIMESTAMP \"${BUILD_TIMESTAMP}\"
")
