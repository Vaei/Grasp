# Grasp <img align="right" width=128, height=128 src="https://github.com/Vaei/Grasp/blob/main/Resources/Icon128.png">

> [!IMPORTANT]
> **Gameplay Interaction System**
> <br>Full Net Prediction
> <br>Pairs with the [Vigil](https://github.com/Vaei/Vigil) Focusing System
> <br>Pairs with [Doors](https://github.com/Vaei/Doors) - Coming Soon
> <br>And its **FREE!**

> [!WARNING]
> [Optionally download the pre-compiled binaries here](https://github.com/Vaei/Grasp/wiki/How-to-Use)
> <br>Install this as a project plugin, not an engine plugin

> [!TIP]
> Suitable for both singleplayer and multiplayer games
> <br>Supports UE5.4+

> [!CAUTION]
> Grasp is currently in beta
> <br>There are many non-standard setups that have not been fully tested
> <br>Grasp has not been tested at scale in production
> <br>Feedback is wanted on Grasp's workflow

## Watch Me

> [!TIP]
> [Showcase Video](https://youtu.be/irJWn86mR_k)

## How to Use
> [!IMPORTANT]
> [Read the Wiki to Learn How to use Grasp](https://github.com/Vaei/Grasp/wiki/How-to-Use)

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

### 1.3.0
* Support multiple grasp data per component
	* This allows multiple interaction options (e.g. Door: Open, Knock, Break)
	* `UGraspStatics` many functions now take `GraspDataIndex`; provide this from your activating ability, UI, etc.
	* `UGraspStatics::GetNumGraspData()` added
	* `UGraspStatics::GetGraspDataEntries()` added
	* `UGraspStatics::GetGraspDataIndex()` added

### 1.2.7
* Add fallback for finding ASC on non-authority for testing ability activations

### 1.2.6
* Add UI helper function `UGraspStatics::GetNormalizedDistanceBetweenInteractAndHighlight()`

### 1.2.5
* Fixed graspable static and skeletal meshes defaulting to hidden in game

### 1.2.4
* Add optional InputTag to pass along with ability spec on activation
	* Resolves Lyra tag-based input systems and "wait for input" tasks

### 1.2.3
* Change Failsafe Timer to Weak Lambda because OnDestroy not being called at correct point in lifecycle after UEngine::Browse (`open map`)

### 1.2.2
* Fix `UGraspScanTask::GetOwnerNetMode()` checks

### 1.2.1
* Replace native ptr with TObjectPtr

### 1.2.0
* Due to Engine bug where Targeting Subsystem loses all requests when another client joins, a failsafe has been added
* Debug logging now identifies which client is logging

### 1.1.1
* Fix struct not initializing properties in ctor

### 1.1.0
* Added delegates for ability pre/post clear/grant and other events that were previously available only as virtual functions
* Added ability locking system - locked abilities do not get automatically cleared until unlocked, see `UGraspStatics::` `AddGraspAbilityLock()` and `RemoveGraspAbilityLock()`
* Can manually clear abilities, see `ClearGrantedGameplayAbility()`, `ClearGrantedGameplayAbilityForComponent()`
* AbilitySpec is now cached to `FGraspAbilityData`
* Improved user setup error reporting if you forgot to call `InitializeGrasp()`

### 1.0.3
_Do not remain on 1.0.2 -- UHT causing issues with graspable components, the changes were band-aiding this unnecessarily and a lot have been rolled back_

* Fix uht weirdness causing ctor to overwrite post-ctor changes
* Removed notifications, somewhat redundant now that ctor is acting appropriately, and triggering notifications from ctor does not behave as intended
* Improve `CanGraspActivateAbility` and `TryActivateGraspAbility`
	* Should not fail silently in cases of user error
	* Add parameter tooltips to make input requirements clear

### 1.0.2
* Fixed bug with `UGraspFilter_Graspable::ShouldFilterTarget` using `CastChecked` instead of `Cast` causing crash with null interface (this was a copy/paste accident)
* Fixed bug with ctor overwriting settings for graspable components
	* Related to using `UGraspStatics::SetupGraspableComponentCollision`, has now been dumped in each header
	* When changing collision profile settings post-component creation, it will no longer get overwritten
* Added `UGraspEditorSettings` (`UDeveloperSettings`) for per-user config
	* Added `bNotifyOnCollisionChanged` that spawns notification window when auto changing collision properties
		* Silently changing settings was not ideal
		* This is enabled by default, but you can now disable the notification

### 1.0.1
* Stopped passing in source object -- breaking prediction
	* Use EventData if you require the component we're interacting with (vs getting it from focus targeting)
	* In the future the ability to grant multiple unique abilities with source objects may be added

### 1.0.0
* Initial Release