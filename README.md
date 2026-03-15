# uEVSE or dumb little charger
The minimally viable Electric Vehicle Supply Equipment. Mostly out of curiocity but also as a reaction to the relatively high prices of the cheapest available "Granny charger" products that might be your first step when charging your first BEV.  

In my exploration, the highest unavoidable component costs are the power cable and special connectors. You won't have much choice for the connectors and you really shouldn't compromise on the power cable either. When comparing prices, be sure they are the length you need with a safe amount of slack.  

I've seen quite a few Arduino based DIY EVSE projects but that still has an entry-level threshold of complexity that might put some people off. And I was confident I could make something better - in terms of minimalism.  

Staring with the excellent work by Julian Ilett (JI) at https://www.youtube.com/watch?v=wdK0bfTGi6M&t=7s and other videos, I wanted to address the two biggest issues standing in the way of further simplification.  

The CP (Control Pilot) signal needs to be +/-12V or 24V peak-to-peak.  
In the above example this is acheived with a pair of small 12V batteries which you won't find in your local supermarket. 
Some others have used a dual output PSU with +12V & +5V for the electronics, with a DC-DC converter to generate -12V. 
Another example used 3 separate PSUs.  

I have used a single 12V PSU with the output swing acheived by an H-bridge configuration. 
This takes advantage of the fact that the PE (Protective Earth) of the car & the Earth from the mains, are not related to the 0V power rail of the electonics. This does mean than a fully isolated PSU is an ABSOLUTE requirement but it should be anyway. 
In practice, I bought the 3 cheapest different designs of 12V PSU modules from AliExpress and all were good enough. Two were under GBP 2.00  

The CP signal also needs to follow a multi-stage protocol sequence to make the car happy enough to start drawing current.  
As JL noted, his car was pretty tolerant of the sequence and a definitive specification for the timing of the sequence stages cannot be found online anyway. This made me wonder how much more of the protocol sequence could be skipped. 
I have since succeded without a microcontroller at all, using just a 555 timer & an op-amp with a few resistors & capacitors. 

## Design decisions  
I quickly decided I was going to follow two different paths - the pure minimal and the feature-ritch minimal which will be a separate project. The goals for this version are:  
Drop the option for vented charging such as AGM batteries in golf buggies.  
Cheapest possible entry level that delivers the minimum viable function.  
Fixed output at a modest rate no more than 10A.  
Must connect to car before mains and disconnect mains before car.  
An upgrade path that doesn't waste earlier purchases.  
Not an end product but a kit to educate, and for those willing, form the base of their own functioning DIY EVSE.  

## Experimental evolution  
Following the spec. at  https://en.wikipedia.org/wiki/SAE_J1772  my first thought regarding the 1kHz CP signal was that "you could probably do that with a 555" as it can run directly from 12V. 
I calculated that the maximum current needed from the CP signal is less than 20mA, if we drop the vented charging option. This is within the capability of an NE555P timer chip. But that only has one output when I need a complimentary pair of outputs to get a 24V swing from the 12V supply. A single buffer transistor wouldn't do as the output needs to both sink and source 20mA. I settled on using a cheap op-amp that suits.  

The H-bridge output worked fine on my oscilloscope so I moved on to the next issue of duty cycle. I've never explored the duty-cycle quirks of the 555 so I did some research and found a circuit which allows extreme adjustment from 5 to 95% while also solving the issue of maintaining the frequency while adjusting the duty-cycle. I selected passive components and trim-pots for frequence & duty.  

The first experiments with my car took a bit of manual fidelity but worked in both getting the car to draw some current and learning about the car's protocol expectations. Specifically:  
It did pay attention to the PP (Proximity Pilot) resistor being there (as JI found).  
It didn't seem to care if the mains was presented too early (as JI found).  
It didn't seem to like the 1kHz CP signal being present from the start.  
It does respond to the duty cycle as expected.  
I would like to find the minimum duty cycle the car will work at but I haven't yet.  
I did find that if the duty cycle is accidentally inverted, the 87% presented will cause the car to draw more than enough to trip my home battery before blowing the 13A fuse in the mains plug.  If you're in a country where the mains plugs don't include a fuse, take appropriate precautions.  

While waiting for the special conectors to arrive from China, I bought the cheapest EVSE product I could find which included a timer function to make use of my over-night tariff. Disappointingly it can set a delay or a duration but not both. I later discovered that after a power-loss, it will return to the last used charge current and carry on. So I tried it via a Smart-switch rated at 16A and found it works fine with a schedule configred from the mobile app. I could have bought an EVSE without the built-in timer function. In fact this is the key to the cost-efficient upgrate path.  

By looking at the signals from the bought EVSE on my oscilloscope, I thought my circuit probably needed the CP to be held at 12V+ for an initiual period before starting the 1kHz square wave. As I used an LM358 dual op-amp chip, I could use the spare to hold the 555 in reset until a capacitor charged through a resistor, giving a couple of seconds delay. This breadboard prototype sucesfully got the car to draw a controlled amount of current shortly after boot. Proof of concept!  

As an early experiment for the feature-ritch varient, I also tried it with a 4428 driver IC to see if that would help square-off the rather imperfect signal edges. It didn't make much difference but the driver got rather hot. This was a dual complimemntary pair package where I was initially driving both inputs from the 555 so they had a brief cross-over period. I later tried a 4423/4424 dual matched pair package where one was driven from the 555 and the other from the output of the op-amp inverter whose propogation delay was enough to avoid the over-lap heating.  

At this point I designed a PCB which would fit within a single-module DIN rail enclosure and ordered 5. While awaiting delivery, I did more testing and found the 555's timing to be pretty unstable. I programmed an Arduion to measure the mark and space periods from its interrupt pin and build up some statistics over an 8-hour period. It was quite alarming. I hoped this was mostly caused by the breadboard wiring as my PCB didn't have space for any ground plane.  

Upon receiving the PCBs, I populated one with decent quality pin-through-hole (PTH) passives and good sockets for the ICs. It worked better than the breadboard prototype did but hardly rock-steady. It took quite a while to trim the pots for a 13% duty cycle at 1kHz. I found it best to set the frequency pot to its centre then parallel timing capacitors to get it into the ball-park before trimming in. I found the car wouldn't stay charging for very long, even if I trimmed the 555 to start around 990Hz so that it was still below 1100Hz after settling.  

Having fitted the PCB into a 1-mod DIN rail enclosure, I can see that an hour-glass shapped PCB would have a bit more space. Switching to surface-mount-devices (SMD) should enable enough space for a ground plane while also allowing better quality components. But this would detract from the DIY-friendly aims. Meanwhile the Rev 0.2 PCB design is a mid-way improvement which allows the CP signal pair to be sent to and/or returned from a daughter board to either buffer or bypass the analogue ICs.   

I think this is another case which shows that the 555 isn't really that good at basic timing! To know if this task is really beoyond the 555's capabilities, I need to further explore the timing tolerances of my car, with some accuracy. Probably the beast way to do this would be to build the feature-ritch variant anyway!  

## Feature ritch varient  
I have used an ESP-01 in place of a 555 before so it seems a natural choice here. The pros and cons being as follows:  
 * pro Hopefully more accurate timing  
 * pro Better duty-cycle control: 0-100% without any impact on frequency  
 * pro Allows real-time control by mobile device, for duty-cycle, on/off, delay-start, duration-limit etc.  
 * pro Potential integration with Home Assistant etc.  
 * pro Easier to build the hardware  
 * pro Does not need the op-amp  
 * con Needs driver IC with 3.3V inputs and 12V outputs  
 * con Needs a 3.3V supply regulator  
 * con Very limited outputs, in terms of count and restrictions  
 * con Needs software development  
Probably about the same size and cost.  

## ESP_EVSE first test  
My mini ESP-01 prototype carrier board was ideal for this with the addition of a 3-pin 3.3V miniature DC-DC regulator and a couple of capacitors. I use the ESP-01S pin2 for the CP and pin1 for the complimentary PE after a deadband delay. It was a simple sketch with some initial debug info on the serial then little more than four delayMicroseconds() calls. This was immdiately more stable than the 555. I just added a 4424 driver IC to the patch-bay area and the 1k resistor at the output to get it working. There are still the occasional drawn-out delays and it took a while to get the frequency and duty right but I was confident enough to let it on over-night. I had the duty set for about 6A and after 6h50m on the cheap tariff it had charged 8.89kWh. A success.  

## To do  
Write a new sketch to use interrupts from the hardware timer which should improve the stability and free-up the main loop for a web-app UI.  
The pin0 button (for programming) is still usable for user interaction, possibly via magnetic hall-switch to maintain IP65.  
I will look into the LED pin usage so I can use 2 pins to drive each part of the H-bridge separately to get a good dead-zone.  
If there is stil a pin free, then I would put an addressable RGB LED on it, for user feedback.  
To be continued.  
