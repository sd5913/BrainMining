# Mobile vertical slice

This slice validates the mobile path before the full journey is migrated.

## Code path

The mobile runtime settings live in Config/DefaultGame.ini. Android automatically enables the mobile runtime policy; desktop testing can use the -MobileVerticalSlice command-line argument. The policy increases the mining interval, caps dynamic rock triangles, and throttles repeated collision rebuilds.

The existing MobileControlsWidgetClass remains the touch UI entry point. Bind its mining button press and release events to the player's UMiningComponent:

- BeginMiningInput
- EndMiningInput

The controller already spawns the widget when touch controls are active. The default touch interface is set to UI_TouchSimple so the existing virtual joystick can be tested on a device.

## Editor setup

1. Duplicate the current gameplay map as Content/Mobile/Maps/Lvl_MobileVerticalSlice.
2. Keep one mining section, one room, one interactable device, and one ADiggableRock.
3. Assign the existing mobile controls widget to the active BrainMiningPlayerController defaults.
4. Set the mobile map's GameMode to the existing gameplay mode and keep the current player Blueprint assignments.
5. Remove unrelated rooms, showcase actors, test actors, and large crowd groups from the duplicate.
6. Set the map as the Android test map in the Project Settings packaging map list.

## Package

Run Scripts/Package_Android_MobileVerticalSlice.ps1.

Pass -MapPath /Game/Mobile/Maps/Lvl_MobileVerticalSlice after creating the map. The script produces a Shipping Android test package with Pak, IoStore, compression, and editor content excluded. It uses the default debug signing path so the APK can be installed on a test device. Pass `-Distribution` only after configuring a release keystore in the Android Project Settings. The package is delivered as an APK plus a separate OBB data file.

## Acceptance checks

- Cold launch reaches the slice without a crash.
- Touch movement and camera look work.
- The touch mining button calls the mining component and stops cleanly on release.
- Five minutes of repeated mining does not grow memory continuously.
- A rock edit does not exceed the configured mobile triangle budget.
- The test device holds the selected 30 FPS target while mining.
