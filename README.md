2026-02-23
NE555 starts at 975 Hz and quickly drifts up to 996 Hz at which point the car starts pulling current. 
When freq reaches 1100 Hz, the car stops pulling current so presumably has tolerance of -1 to +10 %

Q: Is this another case where the ESP-01 is better at simple timing tasks that the 555?



# uEVSE or dumb little charger
The minimally viable Electric Vehicle Supply Equipment. Mostly out of curiocity but also as a reeaction to the relatively high prices of the cheapest available "Granny charger" products that might be your first step when charging your first BEV.  

In my exploration, the highest unavoidable component costs are the power cable and special connectors. You won't have much choice for the connectors and you really shouldn't compromise on the power cable either. When comparing prices, be sure they are the length you need with a save amount of slack.  

I've seen quite a few Arduino based projects to test a DIY EVSE but that still has an entry-level threshold of complexity that might put some people off. And I was confident I could make something better - in terms of minimalism.  

Staring with the excellent work by Julian Llett at https://www.youtube.com/watch?v=wdK0bfTGi6M&t=7s and other videos, I wanted to address the two biggest issues standing in the way of further simplification.  

The CP (Control Pilot) signal needs to be +/-12V or 24V peak-to-peak.  
In the above example this is acheived with a pair of 12V batteries which you won't find in your local supermarket. 
Some others have used a dual output PSU with +12 & +5V for the electronics, with a DC-DC convreter to generate -12V. 
Another example used 3 separate PSUs.  

I have used a single 12V PSU with the output swing acheived by an H-bridge configuration. 
This takes advantage of the fact that the PE (Protective Earth) of the car & the Earth from the mains, are not related to the 0V power rail of the electonics. This does mean than a fully isolated PSU is an ABSOLUTE requirement but it should be anyway. 
In practice, I bought 3 different designs of the cheapest 12V PSU modules from AliExpress and all were good enough. 

The CP signal also needs to follow a multi-stage protocol sequence to make the car happy enough to start drawing current.  
As Julian noted, his car was pretty tolerant of the sequence and a definitive specification for the timing of the sequence stages cannot be found online. This made me wonder how much more of the protocol sequence could be skipped. 
I have succeded without a microcontroller at all, using just a 555 timer & an op-amp with a few resistors & capacitors. 

Design decisions  
I quickly decided I was going to follow two different paths - the pure minimal and the feature-ritch minimal which will be a separate project. The goals for this version are: 
Drop the option for vented charging such as AGM batteries in golf buggies. 
Cheapest possible entry level that delivers the minimum viable function. 
An upgrade path that doesn't waste earlier purchases. 
Not an end product but a kit to educate, and for those willing, form the base of their own functioning DIY EVSE. 

Experimental evolution  
Following the spec. my first thought regarding the 1kHz CP signal was that ""you could probably do that with a 555"" as it can run directly from 12V. 
I calculated that the maximum current needed from the CP signal is less than 20mA, if we drop the vented charging option. This is within the capability of an NE555P timer chip. But that only has one output when I need a complimentary pair of outputs to get a 24V swing from the 12V supply. A single buffer transistor wouldn't do as the output needs to sink and source 20mA. I settled on using a cheap op-amp that suits.  

The H-bridge output worked fine on my oscilloscope so I moved on to the next issue of duty cycle. I've never explored the duty-cycle quirks of the 555 so I did some research and found a circuit which allows extreme adjustment from 5 to 95% while also solving the issue of adjusting the duty-cycle without changing the frequency. I selected passive components and trim-pots for frequence & duty.  

The first experiments with my car took a bit of manual fidelity but worked in both getting the car to draw some current and learning about the car's protocol expectations. Specifically:
It did pay attention to the PP (Presence Pilot) resistor being there (as JL found).
It didn't seem to care if the mains was presented too early (as JL found).
It didn't seem to like the 1kHz CP signal being present from the start.
It does respond to the duty cycle as expected.
I would like to find the minimum duty cycle it will work at but I haven't yet.
I did find that if the duty cycle is inverted, the 93% presented will cause the car to draw more than enough to trip my home battery before blowing the 13A fuse in the mains plug.  

While waiting for the special conectors to arrive from China, I had bought the cheapest EVSE product I could find which included a timer function to make use of my over-night tariff. Dissapointingly it can set a delay or duration but not both. I later discovered that after a power-loss, it will return to the last used charge current and carry on. So I tried it via a Smart-switch rated at 16A and found it works fine with a schedule configred from the mobile app. I could have bought an EVSE without the built-in timer function.  

By looking at the signals from the bought EVSE on my oscilloscope, I thought mine probably needed the CP to be held at 12V+ for an initiual period before startinng the 1kHz square wave. As I used a dual op-amp chip, I could use the spare to hold the 555 in reset until a capacitor charged through a resistor, giving a couple of seconds delay. This breadboard prototype sucesfully got the car to draw a controlled amount of current shortly after boot.  

I also tried it with a (42xx?) driver IC to see if that would help square-off the rather imperfect signal edges. It didn't make much difference but the driver got rather hot. This was a dual complimemntary pair package where I was initially driving both inputs from the 555 so they had an brief cross-over period. I later tried a dual pair package where one was driven from the 555 and the other from the output of the op-amp inverter whose propogation delay was enough to avoud the over-lap heating.  






