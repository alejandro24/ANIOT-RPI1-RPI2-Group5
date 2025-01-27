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

