# footsynth
Prototype of multi-user feet controlled polyphonic synthesizer & sequencer based on Arduino.

Synthesizer, which uses Direct digital synthesis to generate sound, consists of four separate voices. Wave type of individual voice is selectable: sine, square, saw and triangle. The aim was to be able to even generate sound of one voice combining multiple of these waves, but it's unfinished. Wave parameters - frequency and envelope (attack, decline, sustain, release) are adjustable.

Sequencer has eight steps and individual synthesizer voice has separate sequencer. 

Hardware consists of central synthesizer unit which generates sound and outputs it to line out. It also has seven segment display, rotary encoder and buttons to save and load patterns as songs. It has four seven pin DIN connectors to connect up to four controllers - so the pattern played can be controlled by multiple musicians in real time.

It kind of works but have no time to finish or improve it - so giving it to public in case someone gets interested.

https://hackaday.io/project/163506-footsynth

Synth part inspiration: 
https://groovesizer.com/tb2-resources/
http://groovuino.blogspot.com/
