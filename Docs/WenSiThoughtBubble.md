# WenSi thought bubbles

MainGame's mirror museum uses keyboard-only recombination: E selects a thought,
R starts a 0.7-second exchange animation: the two red words travel on opposing
arcs to the other bubble. Their arrival applies the actual object properties;
the updated sentences remain for 0.3 seconds, then both bubbles fade out over
0.2 seconds. Q dismisses all selected thoughts and cancels an unfinished flight.
G no longer triggers recombination. Repeated R presses cannot restart or reverse
an active exchange. Resetting or leaving the room cancels pending exchanges.
Missing selections or collision-rejected exchanges preserve the current words
and bubbles, without adding explanatory text or failure messages. R does not
enter mouse editing or lock movement. MainGame routes R/Q directly through its
controller; the standalone mirror room also retains its polling input path.
Collision validation checks individual mesh shapes rather than the combined
bounds of hollow exhibits, so empty space inside a cover does not reject a swap.
The existing checks still reject solid overlap and unsafe player growth.
The drag-editing description below applies to the older non-mirror puzzle.

Both `UWenSiPuzzleWidget` (basic, extended and circuit puzzles) and
`UWenSiMirrorWidget` use `/Game/WenSi/UI/T_ThoughtBubbleRight`.
The supplied artwork is imported unchanged, including its alpha. It is used as
the left-hand bubble (tail on the lower-left); the right brush mirrors it
horizontally. Empty image margins are excluded with a brush UV region, and the
sentence never mirrors.

`WenSiThoughtBubble.h` fits a projective transform to the bubble interior.
Each ordinary Chinese character is positioned and transformed separately.
The draggable adjective uses the same projection and a local affine transform,
including its red highlight and underline. The rendered Slate geometry is also
used by hit testing, rather than retaining the old rectangular word location.
This keeps the word swap, close buttons, G/E/Q/R shortcuts, area activation,
mechanism validation and progression rules in the existing implementations.

The bubble is 440 × 369.75 Slate units, placed on the original screen sides.
Sentence colors are dark on the white paper; the swappable word stays red.
Each newly selected thought starts near the first-person viewport center as a
larger, tilted plane. Perspective depth recedes from 360 to 600 units; pitch,
yaw and roll foreshorten/skew the complete card while it travels to its side
over 0.85 seconds. The camera-facing foreground phase precedes lateral motion.
It then resets to an identity transform and stays anchored in screen space,
without bounce, drift, camera-follow motion or idle floating.
The artwork, perspective sentence, close
button and drag hit geometry move together. Editing or swapping adjectives
does not replay it; closing and selecting a thought again does.
No puzzle rule, object position or campaign layout is changed by the UI.
The UI asset directory is included in packaging because native widgets load
the texture by path. Source artwork lives in `Art/WenSi/UI`.
