docstring='
This script creates a device configuration for AQDevice using jq to build JSON objects for various configurations.
Variables:
  - name: The name of the device (default: AQDevice).
  - description: A description of the device (default: "Mox sensor for a room").
  - image: URL to an image of the device (default: "https://eu.robotshop.com/cdn/shop/files/elecrow-gas-sensor-sgp30-air-quality-sensor-breakout-voc-eco2-img1.webp?v=1720492889&width=500").
  - certificateRegExPattern: Regex pattern for the device certificate (default: "(.*).MIoT.pablicdomain.com").
  - certificate_file: Path to the certificate file.
The script constructs the following configurations:
  - transport_payload_type_configuration: JSON payload type configuration.
  - sparkplug_attributes_metric_names: List of Sparkplug attribute metric names.
  - transport_configuration: MQTT transport configuration.
  - configuration: Default configuration.
  - provision_configuration: X509 certificate chain provision configuration.
  - profile_data: Combined profile data.

The final output is a JSON object containing all the above configurations.

Usage:
  ./create_device.sh -n <name> -d <description> -i <image> -r <certificateRegExPattern> -c <certificate_file>

Example:
  ./create_device.sh -n "NewDevice" -d "New device description" -i "https://example.com/image.jpg" -r "(.*).example.com" -c /path/to/certificate.pem
'
name=AQDevice
description="Mox sensor for a room"
image="https://eu.robotshop.com/cdn/shop/files/elecrow-gas-sensor-sgp30-air-quality-sensor-breakout-voc-eco2-img1.webp?v=1720492889&width=500"
certificateRegExPattern="(.*).MIoT.pablicdomain.com"
while getopts ":n:d:i:r:c:" opt; do
    case ${opt} in
    n)
        name=${OPTARG}
        ;;
    d)
        description=${OPTARG}
        ;;
    i)
        image=${OPTARG}
        ;;
    r) 
        certificateRegExPattern=${OPTARG}
        ;;
    c)
        certificate_file=${OPTARG}
        ;;
    :)
        echo $docstring
        ;;
    esac
done

# Check all required parameters are provided
if [ -z name ] || [ -z description ] || [ -z image ] || [ -z certificateRegExPattern ] || [ -z certificate_file ]; then
    echo $docstring
    exit 1
fi
# Form the device json
transport_payload_type_configuration=$(jq -n \
    --arg transportPayloadType "JSON" \
    '$ARGS.named')
sparkplug_attributes_metric_names=$(jq -n \
    '. += ["Properties/*", "Device Control/*", "Node Control/*"]')
transport_configuration=$(jq -n \
      --arg type "MQTT" \
      --arg deviceTelemetryTopic "v1/devices/me/telemetry" \
      --arg deviceAttributesTopic "v1/devices/me/attributes" \
      --arg deviceAttributesSubscribeTopic "v1/devices/me/attributes" \
      --argjson transportPayloadTypeConfiguration "${transport_payload_type_configuration}" \
      --argjson sparkplug false \
      --argjson sparkplugAttributesMetricNames "${sparkplug_attributes_metric_names}" \
      --argjson sendAckOnValidationException false \
      '$ARGS.named')
intermediate_key=$(awk 'NR > 1 && !/-----/{ printf "%s", $0 }' $1)
configuration=$(jq -n --arg type "DEFAULT" '$ARGS.named')
provision_configuration=$(jq -n \
    --arg type "X509_CERTIFICATE_CHAIN" \
    --arg provisionDeviceSecret "${intermediate_key}" \
    --arg certificateRegExPattern "${certificateRegExPattern}" \
    --argjson allowCreateNewDevicesByX509Certificate true \
    '$ARGS.named')
profile_data=$(jq -n \
    --argjson configuration "${configuration}" \
    --argjson transportConfiguration "${transport_configuration}" \
    --argjson provisionConfiguration "${provision_configuration}" \
    '$ARGS.named')
jq -n \
    --arg name "${name}" \
    --arg description "${description}" \
    --arg image "${image}" \
    --arg type "DEFAULT" \
    --arg transportType "MQTT" \
    --arg provisionType "X509_CERTIFICATE_CHAIN" \
    --argjson defaultRuleChainId null \
    --argjson defaultDashboardId null \
    --argjson defaultQueueName null \
    --argjson provisionDeviceKey null \
    --argjson firmwareId null \
    --argjson softwareId null \
    --argjson defaultEdgeRuleChainId null \
    --argjson default false \
    --argjson defaultQueueName null \
    --argjson profileData "${profile_data}"  \
    '$ARGS.named'
