# Developer Console Architecture

## Purpose

`tickline-dev` is Tickline's local developer and verification console.

It is responsible for orchestrating project checks, reporting their results, exposing machine-readable output, and providing an optional interactive terminal interface.

It is not part of the simulation runtime or future production services.

## Command surface

The initial command structure is:

```text
tickline-dev
├── check
├── version
└── help
