# Anotaciones de la Practica Final

## Prácticas a tener de guía
### ANIOT
1. Práctica 4 I2C para el sensor de calidad de aire.
2. Práctica 5 modos de consumo.
3. (Opcional) Practica 6 OTA.
4. SNTP en Expressif: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/system/system_time.html
   además existe un ejemplo de sntp.

   
### RPI1
1. Práctica 2 para la pila de Wifi.
2. Práctica 3 para provisionamiento Wifi vía SoftAP.
3. Práctica 3 para los modos de bajo consumo de Wifi.
4. Para la parte de NVS, es de utilidad el siguiente enlace:
   https://docs.google.com/document/d/1FNXO_7Orssy4XhAqD0iTwK2otf0ZnthtwTQHBp-P2XU/edit?tab=t.0

### RPI2
1. Práctica 5 para MQTT.
2. Práctica 6 para Thingsboard.
3. Práctica 6 para el provisionamiento.
4. El siguiente enlace contiene información sobre LWT(Last Will Testament): 
    https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/protocols/mqtt.html

## Posibles módulos.
1. Módulo para el sensor  de calidad del aire mediante I2C. (Posible timer para el envío de muestras).
2. Módulo para provisionamiento SoftAP y gestión de la conecxión Wifi.
3. Componente para el provisionamiento con thingsboard y la comunicación con este.
4. Módulo para OTA.
5. Power Manager para los modos de consumo.
 
## Dudas a resolver
### RPI2
1. Las claves de device y secret tiene que ser parámetros de menuconfig?
2. El topic mqtt para en el provisionamiento, nos lo da también thingsboard al igual que nos da el token?
3. Para provisionar ambos Wifi y url thingsBoard componente separado?
4. Usar solo JSON o CBOR?

## Cosas importantes
- Para SNTP cuando se arranque habrá que conectarse y luego hacer uso del RTC de la placa.
- Para el arranque de cara al deep sleep, comprobar que ya se tengan las cosas necesarias en el provisionamiento softAp.

