# Double Buffer
The double buffer is one of the most complicated PCBs in the project. It is a RAM module that serves the important function of facilitating data transfer between the CPU and GPU.

The PCB has two access ports, each consisting of an address and I/O port and 2 control signals to interface with the single RAM chip. The SWP signal determines which port gets to access the memory. 

In the final setup, 2 of these double buffer boards are stacked on top of each other such that they share all inputs and outputs except the SWP signal. The two boards get opposite SWP lines, allowing both ports to access their designated RAM chip. If the SWP line inverts, the physical chip the ports access is exchanged. Both ports can access data written by the other. 

In deployment, one port connects to the AOS CPU while the other connects to the GPU. The CPU writes frame-buffer data and then flips the SWP using a special `swp` instruction, effectively acting as a hardware double-buffer for the video data.

![](Screenshot%202026-09-30%20at%2012.17.58%20PM.png)


```mermaid
flowchart LR
    ADDR0[\"ADDR0"\]
    IO0[\"IO0"\]
    buf1["Buffer"]
    buf2{{"Transceiver"}}
    sram["SRAM"]
    ADDR1[/"ADDR1"/]
    IO1[/"IO1"/]
    buf3["Buffer"]
    buf4{{"Transceiver"}}
    ctrl["`Control Circuit`"]
    nr0(("nR0"))
    nw0(("nW0"))
    nr1(("nR1"))
    nw1(("nW1"))
    swp(("SWP"))
    ADDR0 --> buf1
    IO0 --> buf2
    buf1 --> sram
    buf2 --> sram
    ADDR1 --> buf3
    IO1 --> buf4
    buf3 --> sram
    buf4 --> sram
    nr0 --> ctrl
    nw0 --> ctrl
    nr1 --> ctrl
    nw1 --> ctrl
    swp --> ctrl
    ctrl -->|"direction, output enable"| buf2
    ctrl -->|"direction, output enable"| buf4
    ctrl -->|"output enable"| buf1
    ctrl -->|"output enable"| buf3
    linkStyle 14 stroke:#aaaaaa,color:#888888
    %% mermaid-flow:pos ADDR0=292,326 IO0=295,462 buf1=482,329 buf2=482,462 sram=693,404 ADDR1=1056,323 IO1=1059,465 buf3=887,323 buf4=884,465 ctrl=689,899 nr0=302,1107 nw0=302,977 nr1=302,847 nw1=302,717 swp=302,587
```

 