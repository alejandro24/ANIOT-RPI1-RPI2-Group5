This README describes the system developed to measure the air quality level at Computer Science building. Please, refers to the the final project description https://docs.google.com/document/d/1FNXO_7Orssy4XhAqD0iTwK2otf0ZnthtwTQHBp-P2XU/edit?tab=t.0#heading=h.rd7qtrq0uhbq

It has been developed by the following IOT Master students:
- Pablo Cayo Alcalde
- Alejandro de Celis
- Jaime Garzón
- Diego Pellicer




# _Sample project_

(See the README.md file in the upper level 'examples' directory for more information about examples.)

This is the simplest buildable example. The example is used by command `idf.py create-project`
that copies the project to user specified path and set it's name. For more information follow the [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project)

## Prerequisites
hardware:
ESP32 development board. the firmware ESP32-WROOM-32 has been used for developing the system.
Air quality sensor. We have used the SGP30 sensor to measure CO2 levels (please refer to datasheet ![image](https://github.com/user-attachments/assets/487a4db2-32e9-4cf4-b931-e7d86eef890f)).
USB cable for flashing firmware.
Software:
ESP-IDF v5.x or later.
Python 3.8+
Visual Studio Code software.

Tools:
Text editor or IDE with ESP-IDF support 
Serial monitor

Project Management:
A GitHub repository has been created to follow a collaborative working model. Additionally, a Project in GitHub has been defined to help team members to track, prioritise and identify potential delays/stoppers to address accordingly. Please, refers to the Github site: https://github.com/alejandro24/ANIOT-RPI1-RPI2-Group5

# _Design_
Once objectives and mandatory and optinal requirements have been analysed by the team, an initial system definition was performed. It includes workflow definition, gaps 

## Workflow diagram
We have defined the following initial workflow diagram for our Air quality evaluation system.

Turning our system on has been considered the starting point for our workflow diagram programme to develop.

We considered four cases to consider in our main procedure:
1. First running case. We have just started with our programme.
2. Waking up from "deep sleep" mode.
3. Firmware update through OTA just completed. 
4. Error detected. An unexpected behaviour was detected here. As an initial software version, error details will be registered in an error log file and the programme execution will finish.

As we can see in the diagram, we will have common starting points for cases 1 to 3: WiFi provisioning. This is to provide the required WiFi network credentials and ThingsBoard URL address. It doesn’t mean that WiFi connection has to be established to move forward, but our programme shouldn’t proceed without this minimal step completed.

Once the provisioning phase has been completed by our programme, we evaluate if NVS "is empty" or if specific information (WiFi credentials and ThingsBoard URL address) has already been written. Why are we checking it? To cover cases 1, 2 and 3 as NVS write will be needed for 1 and 3 (double-check it) and two.

Provisioning and NVS checking done, what’s next? Time to connect to the WiFi network:

### Successful Connection
------------------
Our system is connected to the provided WiFi network. Then, we get the related time by connecting to the SNTP server and move to connect to the ThingsBoard dashboard. Once the connection has been established correctly, we get data saved previously in our NVS and send it to the ThingsBoard dashboard. We take a new air quality measurement and if it has been done correctly and it is not 10 pm yet, we check if WiFi is still connected. If so, we take the air quality measurement again. If not, we will be in the failed connection case and go to disconnected mode, described below.

Once 10 pm is reached out, we will enter in "deep sleep" mode until next day at 8 am.

### Failed connection
------------------
Entering disconnected mode status. Our programme will run and get air quality measures in offline mode until the connection is established/re-established. We have considered air measures every 10 minutes as an initial approach. If the air quality measurement was correct, we will store the value in a temporary buffer called MINUTES buffer. It will be used to save measurements done every 10 minutes. Additionally, we will have a second buffer called HOURS buffer to save the average value of each hour and will be used later as we will describe below. The MINUTES buffer will be reset every 60 minutes to avoid wrong values. The first measurement taken will be saved simultaneously in MINUTES and HOURS buffers as the WiFi connection could reconnect in less than one complete hour.

Once one measurement has been completed, we will check if WiFi is connected. If yes, we will calculate the average value of the MINUTES buffer and follow the "Successful Connection" case from Connection to ThingsBoard status. If no, we will then check what time it is and come back to disconnected mode status if it’s earlier than 10 pm and repeat the process just explained or calculate the average value of the HOURS buffer, save it in NVS and enter "deep sleep" mode, which will keep until the next day at 8 am. Then, our system will wake up and enter case 2: waking up from "deep sleep" mode.


![ProyectoFinal-Group5-Workflow_Diagram](https://github.com/user-attachments/assets/e4f287a2-2356-40ed-9cbb-9bcfc61d35d1)

# _Deployement_

# _Testing_and Troubleshooting_
_
## QUICK START
git clone
Configure WiFi credentials and ThingsBoard settings
idf.py build
idf.py flash
idf.py monitor
## Features
Real-time SNTP synchronization for accurate timestamps.
Reliable WiFi provisioning with fallback to offline mode.
Data buffering using MINUTES and HOURS buffers for robustness.
OTA firmware update support.
Low power consumption with deep sleep functionality.
Integration with ThingsBoard for dashboard analytics.
## How to use example
We encourage the users to use the example as a template for the new projects.
A recommended way is to follow the instructions on a [docs page](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html#start-a-new-project).
## Hardware Configuration
I2C Configuration:
SCL (clock line): GPIO 22
SDA (data line): GPIO 21
## Key Dependencies
esp_wifi: For Wi-Fi management.
esp_event: Event loop and event handling.
nvs_flash: Non-volatile storage for saving Wi-Fi credentials and other settings.
sntp_sync: Synchronizing system time with an SNTP server.
freertos: Real-time multitasking.
## Key Functional Highlights
Wi-Fi Provisioning:

SoftAP provisioning for initial setup.
Event-driven Wi-Fi connection handling with exponential backoff on failures.
Time Synchronization:

Uses SNTP for system time alignment.
Attempts up to 10 retries with a 2-second delay for synchronization.
SGP30 Sensor Events:

Monitors for new air quality measurements (eCO2 and TVOC).
Logs baseline values with timestamps.
Offline Buffering:

Data is stored in memory when offline and synced to ThingsBoard upon reconnection.


## Example folder contents

The project **sample_project** contains one source file in C language [main.c](main/main.c). The file is located in folder [main](main).

ESP-IDF projects are built using CMake. The project build configuration is contained in `CMakeLists.txt`
files that provide set of directives and instructions describing the project's source files and targets
(executable, library, or both). 

Below is short explanation of remaining files in the project folder.

```
├── CMakeLists.txt
├── main
│   ├── CMakeLists.txt
│   └── main.c
└── README.md                  This is the file you are currently reading
```
Additionally, the sample project contains Makefile and component.mk files, used for the legacy Make based build system. 
They are not used or needed when building with CMake and idf.py.





# X.509 Certificate Chain

We will use a certificate chain approach for authenticating with the server.

On first instance, the center domain authority, namely UCM or the Facultad de Informática has to generate a Root certificate, this root certificate and key have to be secured heavily since all authentication is based on it.
```bash
openssl req -x509 -newkey rsa:2048 -keyout rootKey.pem -out rootCert.pem -sha256 -nodes
```
We will then share this certificate with everyone as the public key, any actor interested in creating a certificate authored by us can send us a certificate signing request created by the following command.
```bash
openssl req -new -newkey rsa:2048 -keyout intermediateKey.pem -out intermediate.csr -sha256 -nodes
```
We can approve this requests by signing its public key with the following command for `num_days` days.
```bash
openssl x509 -req -in    device.csr           \
                  -out   deviceCert.pem       \
                  -CA    intermediateCert.pem \
                  -CAkey intermediateKey.pem  \
                  -days  <num_days>           \
                  -sha256 -CAcreateserial
```
We can repeat this process to the depth desired to cover our organization structure and store the chain of public certificates that assures our identity

[TODO]
This line:

-subj "/C=PE/ST=Lima/L=Lima/O=Acme Inc. /OU=IT Department/CN=acme.com"
Description:

Country Name (2 letter code) [AU]:PE
State or Province Name (full name) [Some-State]:Lima
Locality Name (eg, city) []:Lima
Organization Name (eg, company) [Internet Widgits Pty Ltd]:Acme Inc.
Organizational Unit Name (eg, section) []:IT Department
Common Name (e.g. server FQDN or YOUR name) []:acme.com
Use "/" like separator.

