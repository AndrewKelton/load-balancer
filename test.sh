# !/bin/bash

URL="http://localhost:8080"
ARGS=""

# Generate the command arguments
for i in {1..10}; do
    ARGS="$ARGS $URL"
done

# Execute all of them simultaneously
curl --parallel $ARGS -s -o /dev/null -w " HTTP: %{http_code} | Time: %{time_total}\n"