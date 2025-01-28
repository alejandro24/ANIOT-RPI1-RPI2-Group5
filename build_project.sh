#!/usr/bin/env bash
docstring='This script builds and flashes an ESP32 project.

 Usage:
   ./build_project.sh -p <port> -c <chip>

 Options:
   -p <port>   Specify the serial port to use for flashing (e.g., /dev/ttyUSB0)
   -c <chip>   Specify the chip type (e.g., esp32)

 Description:
   This script performs the following steps:
     1. Parses command-line options for the serial port and chip type.
     2. Builds the project using idf.py.
     3. Generates the NVS partition using the nvs_partition_gen.py script.
     4. Flashes the project to the specified chip using esptool.py.

 Example:
   ./build_project.sh -p /dev/ttyUSB0 -c esp32
'
while getopts ":p:c:f:" opt; do
    case ${opt} in
    p)
        port=${OPTARG}
        ;;
    c)
        chip=${OPTARG}
        ;;
    :)
        echo $docstring
        exit 1
        ;;
    esac
done
# Check if all required parameters are provided
if [ -z "${port}" ] || [ -z "${chip}" ]; then
    echo $docstring
    exit 1
fi

echo "Building the project"
idf.py build

echo "Run NVS partition generator"
python ${IDF_PATH}/components/nvs_flash/nvs_partition_generator/nvs_partition_gen.py generate nvs.csv build/certs.bin 24576

echo "Flashing the project"
esptool.py \
  -p ${port} \
  -b 460800 \
  --before default_reset \
  --after hard_reset \
  --chip ${chip} \
  write_flash --flash_mode dio \
  --flash_freq 40m \
  --flash_size detect \
  0x10000 build/ProjectoFinalGrupo5.bin \
  0x1000 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x9000 build/certs.bin


