# MURMULATOR devboard Example / SDK
To get it working you should have an Murmulator (development) board with VGA output. Schematics available here at https://github.com/AlexEkb4ever/MURMULATOR_classical_scheme

Or connect your Raspberry Pi Pico to VGA using 8 resistors:
```
GP6 --> R1K  --> VGA #3 (Blue)
GP7 --> R330 --> VGA #3 (Blue)

GP8 --> R1K  --> VGA #2 (Green)
GP9 --> R330 --> VGA #2 (Green)

GP10 --> R1K  --> VGA #1 (Red)
GP11 --> R330 --> VGA #1 (Red)

GP12 --> R100  --> VGA #13 (Horizontal Sync)
GP13 --> R100 --> VGA #14 (Vertical Sync)
GND --> VGA #5,6,7,8,10
```

