<!-- Improved compatibility of back to top link: See: https://github.com/othneildrew/Best-README-Template/pull/73 -->

<a id="readme-top"></a>

<!--
*** Thanks for checking out the Best-README-Template. If you have a suggestion
*** that would make this better, please fork the repo and create a pull request
*** or simply open an issue with the tag "enhancement".
*** Don't forget to give the project a star!
*** Thanks again! Now go create something AMAZING! :D
-->

<!-- PROJECT SHIELDS -->
<!--
*** I'm using markdown "reference style" links for readability.
*** Reference links are enclosed in brackets [ ] instead of parentheses ( ).
*** See the bottom of this document for the declaration of the reference variables
*** for contributors-url, forks-url, etc. This is an optional, concise syntax you may use.
*** https://www.markdownguide.org/basic-syntax/#reference-style-links
-->

<!-- PROJECT LOGO -->
<br />
<div align="center">
    <a href="https://github.com/github_username/repo_name">
    <img src="pictures/sim circuit.png" alt="Logo" width="97" height="82">
  </a>
    <h1 align="center">AOS16</h1>
  <p align="center">
    A self-made 16-bit computer.
  </p>
</div>

<!-- TABLE OF CONTENTS -->
<details>
  <summary>Table of Contents</summary>
  <ol>
    <li>
      <a href="#about-the-project">About The Project</a>
    </li>
    <li><a href="#pictures">Pictures</a></li>
    <li><a href="#technical-details">Technical Details</a></li>
    <li><a href="#directory-structure">Directory Structure</a></li>
    <li><a href="#to-do">To-Do</a></li>
    <li><a href="#acknowledgments">Acknowledgments</a></li>
  </ol>
</details>

<!-- ABOUT THE PROJECT -->

## About The Project

AOS16 is a self-made 16-bit Reduced Instruction Set Computer (RISC) on PCBs, complete with a homebrew GPU for screen output. It is fully planned, designed and fabricated from scratch. A Turing-complete, Harvard-style computer with a completely custom architecture, at a blistering fast 10MHz.

The project is divided into smaller subsytems on PCBs, designed to interconnect via a central bus.

### Features
- 16-bit address and data buses
- Hardware double buffering with parallel CPU and GPU operation
- 10 MHz clock
- 6 general-purpose registers, a stack pointer and a flags register
- 65 Kb of general purpose RAM
- 131 Kb of framebuffer (double buffered, so 65Kb usable)
- 131 Kb of program memory

### Why?

This project was a learning effort for me. I started this project wanting to learn more about computer architectures and gain hands-on experience with electrical engineering. Also, building a computer, from scratch, is the coolest project I've seen around. Some inspiration from amazing engineers including Ben Eater and jdh were more than enough to convince me to try my hand at this project.

### How's it different?

Most homebrew computers on the internet are 8-bit computers built on breadboards.  AOS16 is novel in its capabilities, being a 16-bit computer built on PCBs with a much higher clock speed, with display support directly integrated. 

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Pictures

<!-- The animated GIFs will not be visible in the PDF! Please visit the repository on [Github](https://github.com/AJain9199/AOS16). -->

![Simulation of Color Palette on Screen](pictures/Color%20Palette%20Simulation.gif)

<h2 align="center">
A simulation of the computer in Digital displaying the color palette on the screen
</h2>

<br>

![A GPU Test](pictures/GPU%20test.gif)

<h2 align="center">
    GPU displaying the pillars of creation
</h2>

<br>

![ALU PCB](pictures/ALU%20PCB.png)

<h2 align="center">
    The Arithmetic Logic Unit in KiCAD
</h2>

<br>

![Logic Simulation in Digital](pictures/sim%20circuit.png)

<h2 align="center">
    The logic simulation in Digital.
</h2>

<br>

![Fabricated Reg4x16 PCB](pictures/REG4x16%20PCB.jpeg)

<h2 align="center">
    The Reg4x16 PCB 
</h2>

<br>

![Fabricated Double Buffer PCB](<pictures/DOUBLE%20BUFFER%20PCB%20(without%20chips).jpeg>)

<h2 align="center">
    The Double Buffer PCB
</h2>

<br>

<p align="right">(<a href="#readme-top">back to top</a>)</p>

## Technical Details

Details about the design decisions, design of the instruction word, etc. can be found in the [Design Doc](AOS-16%20Design.md).

## Directory Structure

- assembler: Contains the source code for the AOS16 assembler
- sim: Logic simulation files written for Digital
- microcode: Microcode implementations written in MiLa
- tests: some basic tests for ICs and the simulation
- common: basic KiCAD schematics (registers, transceivers) which are shared across subsystems

Each of the remaining folders represents a subsystem, built on a separate PCB. Each folder will contain a KiCAD schematic and PCB file.

## To-do

Main Phases of the project:

-   [x] Finalize project parameters and design goals
-   [ ] Hardware:
    -   [x] Logic Simulation and Design
    -   [ ] PCB Design
    -   [ ] PCB Fabrication & integration
-   [x] Software:
    -   [x] Microcode (with [MiLa](https://github.com/AJain9199/MiLa))
    -   [x] Assembly language design
    -   [x] Assembler
    -   [x] ROM programming software

<br>

| Component / Phase   | Logic Simulation | PCB Design | Fabrication and Integration |
| ------------------- | ---------------- | ---------- | --------------------------- |
| Double Buffer       | ✅               | ✅         | In-progress                 |
| Memory Controller   | ✅               | ✅         | In-progress                 |
| ALU                 | ✅               | ✅         | ✅                          |
| Registers           | ✅               | ✅         | ✅                          |
| Control Unit        | ✅               | ✅          | ☐                           |
| Program Counter     | ✅               | ✅         | ☐                           |
| GPU                 | ✅               | ☐          | ☐                           |
| Main Data Bus       | ✅               | ☐          | ☐                           |
| **Phase Completed** | ✅               | ☐          | ☐                           |

<p align="right">(<a href="#readme-top">back to top</a>)</p>

<!-- ACKNOWLEDGMENTS -->
## Acknowledgments

-   [Ben Eater](https://www.youtube.com/@BenEater) for his excellent series on the breadboard computer
-   [jdh](https://www.youtube.com/@jdh) for the inspiration
-   The Elements of Computing Systems and the accompanying [Nand2Tetris](https://www.nand2tetris.org/) course by Shimon Schocken and Noam Nisan
-   [r/homebrewcomputer](https://www.reddit.com/r/homebrewcomputer/) and [r/PrintedCircuitBoard](https://www.reddit.com/r/PrintedCircuitBoard/) commmunities for PCB reviews and design checks
-   [Digital](https://github.com/hneemann/Digital), the logic simulator



<p align="right">(<a href="#readme-top">back to top</a>)</p>
