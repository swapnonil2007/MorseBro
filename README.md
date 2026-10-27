# MorseBro
MorseBro is an arduino based communication device that reads morse code (dots and dashes) with a infrared sensor the presence of a human hand (anything emitting infrared ray), translates it and then creates the morse code with sound.

The detected patterns are decoded into letters and stored as a message, which is displayed on the Serial Monitor. Once the message is ready, an LED and a buzzer reproduce it using visual and audio Morse signals.

## Mechanism
The IR sensor perceives a presence of 60-399 ms as a dot and a larger presence as a das.

## Components Used
- Arduino UNO
- IR obstacle detection sensor
- LED and 220 Ω resistor
- Piezo buzzer
- Breadboard and jumper wires
- IR Remote (for simulation purpose in tinkercad)

## Future Applications

1. **Assistive communication:** Could be adapted into a simple communication aid for people who have difficulty using conventional input devices.
2. **Touch-free control systems:** Hand movements could be used to send simple commands to devices without physical buttons.
3. **Learning and training tools:** Could serve as an interactive way to learn Morse code and understand digital signal processing.
