# dLive_MIDI_Controller
Arduino code for dLive MIDI Controller

Current code for use with Arduino Mega and mocoLUFA. 

<h4>Parts</h4>
<ul>
  <li>Encoder is quadrature encoder</li>
  <li>Select is press on encoder</li>
  <li>Mode switch is SPST ON-OFF</li>
  <li>Faders are Bourns B10K 100mm</li>
  <li>Buttons are illuminated DPST</li>
</ul>

<h4>Operation</h4>
<p>There is a SETUP and RUN mode. It always boot into RUN mode, regardless of mode switch position. When in RUN mode, all faders and mutes work as they should.
The encoder will cycle the display through each channel strip and tell you what it is assigned to. When mode switch is flipped, it changes to SETUP mode.
In SETUP mode, the faders and mutes will not be read. The LCD and encoder switch to a system that allows you to assign new channels. 
You first choose a channel strip to assign to, then a Category (Input, DCA, etc.), then the number of channel. Lastly it asks to confirm and
displays a confirm screen when chosen. Flipping mode switch again returns to RUN mode and all faders and mutes are read again.</p>
