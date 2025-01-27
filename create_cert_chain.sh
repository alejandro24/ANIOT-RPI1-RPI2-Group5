#!/usr/bin/env bash
docstring='
This script generates a certificate chain with a root certificate, an intermediate certificate, and a device certificate.

Usage:
  ./script.sh -d <device_subdomain> -i <intermediate_subdomain> -r <root_domain>

Options:
  -d   Device subdomain (e.g., device1)
  -i   Intermediate subdomain (e.g., intermediate)
  -r   Root domain (e.g., example.com)
  -f   Folder to save the certificates (default: ./certs)
  -s   Key size (default: 2048)

Examples:
  ./script.sh -d device1 -i intermediate -r example.com

This will create a certificate chain with:
  - Root Certificate: example.com
  - Intermediate Certificate: intermediate.example.com
  - Device Certificate: device1.intermediate.example.com
'
certs_dir=./certs
size=2048
while getopts ":d:i:r:f:s:" opt; do
    case ${opt} in
    d)
        device_subdomain=${OPTARG}
        ;;
    i)
        intermediate_subdomain=${OPTARG}
        ;;
    r)
        root_domain=${OPTARG}
        ;;
    f)
        certs_dir=${OPTARG}
        ;;
    s)
        size=${OPTARG}
        ;;
    :)
        echo $docstring
        exit 1
        ;;
    esac
done

# Check if all required parameters are provided
if [ -z "${device_subdomain}" ] || [ -z "${intermediate_subdomain}" ] || [ -z "${root_domain}" ]; then
    echo $docstring
    exit 1
fi

intermediate_domain="${intermediate_subdomain}.${root_domain}"
device_domain="${device_subdomain}.${intermediate_subdomain}.${root_domain}"

subject="/C=ES/ST=Madrid/L=Madrid/O=MIoT/OU=Grupo4/CN=%s"
num_days=365

printf -v root_cn "$subject" "$root_domain"
printf -v intermediate_cn "$subject" "$intermediate_domain"
printf -v device_cn "$subject" "$device_domain"

# Make sure the certs directory exists
mkdir -p ${certs_dir}
# Create the root key
openssl req -x509 -newkey rsa:${size}\
    -keyout ${certs_dir}/rootKey.pem\
    -out ${certs_dir}/rootCert.pem\
    -sha256 -nodes -subj "${root_cn}" >/dev/null 2>&1

# Create the intermediate key
openssl req -new -newkey rsa:${size}\
    -keyout ${certs_dir}/intermediateKey.pem\
    -out ${certs_dir}/intermediate.csr\
    -sha256 -nodes -subj "${intermediate_cn}" >/dev/null 2>&1

# Sign the intermediate key with the root key
openssl x509 -req -in ${certs_dir}/intermediate.csr \
    -out ${certs_dir}/intermediateCert.pem \
    -CA ${certs_dir}/rootCert.pem \
    -CAkey ${certs_dir}/rootKey.pem \
    -days $num_days \
    -sha256 -CAcreateserial >/dev/null 2>&1

# Create the device key
openssl req -new -newkey rsa:${size} \
    -keyout ${certs_dir}/deviceKey.pem \
    -out ${certs_dir}/device.csr \
    -sha256 -nodes -subj "${device_cn}" >/dev/null 2>&1

# Sign the device key with the intermediate key
openssl x509 -req -in ${certs_dir}/device.csr \
    -out ${certs_dir}/deviceCert.pem \
    -CA ${certs_dir}/intermediateCert.pem \
    -CAkey ${certs_dir}/intermediateKey.pem \
    -days $num_days \
    -sha256 -CAcreateserial >/dev/null 2>&1

# Create chain certificate
cat ${certs_dir}/deviceCert.pem ${certs_dir}/intermediateCert.pem ${certs_dir}/rootCert.pem >${certs_dir}/chainCert.pem

# Check that certificates have been created
if [ -f ${certs_dir}/rootCert.pem ] && [ -f ${certs_dir}/intermediateCert.pem ] && [ -f ${certs_dir}/deviceCert.pem ]; then
    echo "Certificates have been created successfully."
else
    echo "Error creating certificates."
fi
