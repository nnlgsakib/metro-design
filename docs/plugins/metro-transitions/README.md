# metro-transitions

Video transition effects — dissolve, directional wipes, radial wipe, cross zoom, and circle open/close.

## Parameters

| Parameter      | Type   | Default      | Description                   |
|----------------|--------|--------------|-------------------------------|
| TransitionType | enum   | Dissolve     | Transition style              |
| Progress       | float  | 0.5          | Transition progress (0-1)     |
| EdgeSoftness   | float  | 0.1          | Edge feathering               |
| EdgeColor      | color  | 0,0,0        | Transition edge color         |
| Mix            | float  | 1.0          | Effect blend amount           |

## Transition types

- Dissolve — crossfade between clips
- WipeLeft / WipeRight / WipeUp / WipeDown — directional wipes
- RadialWipe — circular wipe from center
- CrossZoom — zoom-in/zoom-out transition
- CircleOpen / CircleClose — expanding/contracting circle
