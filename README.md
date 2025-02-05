# Read the fuses on ATtiny85
```bash
avrdude -p attiny85 -c stk500v2 -P /dev/ttyACM0 -b 115200 -B 125kHz -U lfuse:r:-:h -U hfuse:r:-:h
```

# Write the fuses on ATtiny85
```bash
avrdude -p attiny85 -c stk500v2 -P /dev/ttyACM0 -b 115200 -B 125kHz -U lfuse:w:0x62:m -U hfuse:w:0xdf:m
```




