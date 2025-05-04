# Grasp <img align="right" width=128, height=128 src="https://github.com/Vaei/Grasp/blob/main/Resources/Icon128.png">

> [!CAUTION]
> This plugin is not ready at all
> <br>You should not use it yet

> [!IMPORTANT]
> **Gameplay Interaction System**
> <br>Full Net Prediction
> <br>Pairs with the [Vigil](https://github.com/Vaei/Vigil) Focusing System
> <br>Pairs with [Doors](https://github.com/Vaei/Doors) - Coming Soon
> <br>And its **FREE!**

> [!WARNING]
> Use `git clone` instead of downloading as a zip, or you will not receive content
> <br>Install this as a project plugin, not an engine plugin

> [!TIP]
> Suitable for both singleplayer and multiplayer games
> <br>Supports UE5.4+

> [!CAUTION]
> Grasp is currently in beta
> <br>There are many non-standard setups that have not been fully tested
> <br>Grasp has not been tested at scale in production
> <br>Feedback is wanted on Grasp's workflow

## Features

### Net Prediction

Grasp is fully net predicted via Gameplay Abilities.

### Data Based

Grasp is very robust, using a data-based workflow.

### Component Based

Grasp uses components for interaction instead of actors, so you can have multiple ways to interact with the same actor.

These components don't generate overlap events and can be used to produce the physical representation, greatly reducing common performance pitfalls with interaction systems.

### Visualizers, Debugging, Logging

Grasp has some serious visualization tools and logging, so you never have to guess what your parameters are really doing, or what is going on under the hood.

### Robust Ability Handling

Highly optimized asynchronous scanning for components we can interact with. Intelligent granting and removal of abilities. Abilities are granted once per CD0 only. You can pre-grant common abilities.

### Supports Focus & UI

Built with Focus Targeting and UI in mind.

### Helper Function Library

Build your interaction abilities rapidly with useful functions to save you time.

## Changelog

### 1.0.0
* Initial Release