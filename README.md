This is a 1 Msps digital oscilloscope using the TM4C129 Microcontroller and BOOSTXL-EDUMKII shield. 

We created a circuit to generate frequencies, that connected to our microcontroller’s ADC. The ADC sampled at the high-rate
of 1,000,000 samples per second using a circular buffer. To achieve such a high sample rate, we read the ADC values directly
through register calls to avoid function time overhead, along with a FIFO to store button presses from an ISR. The 
user is able to select a button and switch whether the trigger slope of the signal is a rising edge or a falling edge. 
Additionally, the voltage scale was calibrated so that 0 Volts always corresponds to the center of the screen, as well as 
being able to be adjusted to scales of 100 mV, 200 mV, 500 mV, and 1V through the joystick on the booster board. We utilized 
a timer that polls until timeout, incrementing a counter for each poll. Every time the timer is interrupted, it will count 
fewer iterations, allowing us to calculate CPU load. Towards the bottom of the display, the calculated CPU load is displayed 
as a percentage. 

![alt text]https://i.imgur.com/qWKVWV4.jpg
