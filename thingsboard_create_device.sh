#!/usr/bin/env bash
docstring='
This script creates a device in ThingsBoard.

Variables:
  - username: The username to login to ThingsBoard.
  - password: The password to login to ThingsBoard.
  - host: The host URL of ThingsBoard.
  - device_file: The JSON file containing the device configuration.

Usage:
  ./thingsboard_create_device.sh -u <username> -p <password> -h <host> -d <device_file>

Example:
  ./thingsboard_create_device.sh -u "user" -p "pass" -h "https://thingsboard.pablicdomain.com" -d "device.json"
'
while getopts ":u:p:h:d:" opt; do
    case ${opt} in
    u)
        username=${OPTARG}
        ;;
    p)
        password=${OPTARG}
        ;;
    h)
        host=${OPTARG}
        ;;
    d)
        device_file=${OPTARG}
        ;;
    :)
        echo $docstring
        ;;
    esac
done
# Check if all required parameters are provided
if [ -z "${username}" ] || [ -z "${password}" ] || [ -z "${host}" ]; then
    echo $docstring
    exit 1
fi

# Login in ThingsBoard
echo "Login in ThingsBoard"
token=$(curl -s -X POST -d "{\"username\":\"${username}\",\"password\":\"${password}\"}" "${host}/api/auth/login" -H "Content-Type:application/json" | jq 'if .token then .token else empty end' | tr -d \")
if [ -z "${token}" ]; then
    echo "Login failed"
    exit 1
fi

#Create device in ThingsBoard
echo "Create device in ThingsBoard"
jq . $device_file | curl -X 'POST' \
  'https://thingsboard.pablicdomain.com/api/deviceProfile' \
  -H 'accept: application/json' \
  -H 'Content-Type: application/json' \
  -H 'X-Authorization: Bearer '$token'' \
  -d @-
