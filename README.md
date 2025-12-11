# jk_read_bms
Arduino sketch for reading BMS data into ESP32

## Wiring (B2A8S20P)

```
ESP32    JK BMS
----     --------
    |   |
RX2 |-->| TX
TX2 |-->| RX
GND |-->| GND
    |    --------
    |
    |    MAX7219
    |    --------
Vin |-->| VCC
GND |-->| GND
D23 |-->| DIN
D18 |-->| CLK
D5  |-->| CS/LOAD
----     --------
```

Attention! Do not connect 4th pin in JK BMS GPS slot, there is battery voltage on it and you can burn your ESP32!
