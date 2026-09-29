# Arithmetic Logic Unit
The Arithmetic Logic Unit is one of the the first PCBs I designed and built as part of the project. It is essentially a wrapper for the 74LS181 ICs, 4 of them handling each of the 4 nibbles, with additional circuitry to manage the IO and carry generation for each of the 4 chips.

![ALU PCB](../pictures/ALU%20PCB.png)


```mermaid
flowchart LR
	porta@{ shape: lean-r, label: "PORT A"}
	latcha["`Latch A`"]
	porta --> latcha
	ina((INA))
	ina --> latcha
	
	portb@{ shape: lean-r, label: "PORT B"}
	latchb["`Latch B`"]
	portb --> latchb
	inb((INB))
	inb --> latchb
	
	subgraph alu [Arithmetic Unit]
		aluchip1@{ shape: subproc, label: "74LS181 A" }
		aluchip2@{ shape: subproc, label: "74LS181 B" }
		aluchip3@{ shape: subproc, label: "74LS181 C" }
		aluchip4@{ shape: subproc, label: "74LS181 D" }
		
		carrygen["`Carry Generator`"]
		
		carrygen --> aluchip1
		carrygen --> aluchip2
		carrygen --> aluchip3
		carrygen --> aluchip4
	end
	
	latcha --> alu
	latchb --> alu
	
	buffer@{shape: tri, label: "Tri-State Buffer"}
	noe((nOE))
	noe --> buffer
	
	alu --> buffer --> outport
	carryout((nCo))
	alu --> carryout
	
	alus(["`S[0..3]`"]) --> alu
	m((M)) --> alu
	cin((nCin)) --> alu
	
	outport@{ shape: lean-r, label: "OUTPUT"}
```

## Carry Generation

Since each 74LS181 chip only handles 4 bits, 4 of these chips need to be used together. Instead of cascading them using the built-in carry outputs, I used full look-ahead carry generation which is more than 2x faster ([sn54s181](sn54s181.pdf)) (realistically both are fine at the speeds this works at, this is simply future/idiot proofing) But I couldn't find the carry generation chip, the 182, so I replicated the internal circuit using discrete (A)HCT gates ([sn54s182](sn54s182.pdf)).

![Circuit diagram](../pictures/Pasted%20image%2020260929200735.png)

Since I wasn't sure if this will work, I added test pads on the cascade carry outputs and inputs so they could be used as a contingency.

## Latches
The 181s need both A and B inputs driven at the same time to function correctly. Since they ultimately need to be driven by the same bus, the inputs are latched using separate signals which drive the physical inputs. I considered adding a second bus only to drive port B, but the added complexity was not worth the benefit of saving 1-2 microcode instructions and 2 control signals.

![](../pictures/Pasted%20image%2020260929201826.png)

Inputs are latched at the rising edge. The AHCT16374's ([sn74ahct16374](sn74ahct16374.pdf)) are asynchronous, so the latching input is and'ed with the clock to make them "synchronous."

## Iterations
- v1.0: The control signals on the 16374 were routed wrong due to a schematic symbol mismatch
- v1.1: The 16374 was routed wrong (again)
- v1.2: Final