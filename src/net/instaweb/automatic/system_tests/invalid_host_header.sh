start_test Invalid HOST URL does not crash the server.
# GET with host set to "http://127.0.0.\230:8080/"
GETSTR=$(echo R0VUIC8gSFRUUC8xLjEKSG9zdDogMTI3LjAuMC7vv70KCg== | base64 -d)
HOST=$(echo $SECONDARY_HOSTNAME | cut -d \: -f 1)
PORT=$(echo $SECONDARY_HOSTNAME | cut -d \: -f 2)
OUTVAR=$(echo $GETSTR | nc -w 1 $HOST $PORT)
check $(echo $OUTVAR | grep -q "Bad Request")
