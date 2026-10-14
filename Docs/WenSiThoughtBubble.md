# WenSi thought bubbles

Both `UWenSiPuzzleWidget` (basic, extended and circuit puzzles) and
`UWenSiMirrorWidget` use `/Game/WenSi/UI/T_ThoughtBubbleRight`.
The supplied right-hand artwork is imported unchanged, including its alpha.
Its empty image margins are excluded with a brush UV region. The left brush
mirrors horizontally; the sentence never mirrors.

`WenSiThoughtBubble.h` fits a projective transform to the bubble interior.
Each ordinary Chinese character is positioned and transformed separately.
The draggable adjective uses the same projection and a local affine transform,
including its red highlight and underline. The rendered Slate geometry is also
used by hit testing, rather than retaining the old rectangular word location.
This keeps the word swap, close buttons, G/E/Q/R shortcuts, area activation,
mechanism validation and progression rules in the existing implementations.

The bubble is 440 × 328 Slate units, placed on the original screen sides.
Sentence colors are dark on the white paper; the swappable word stays red.
Each newly selected thought starts near the first-person viewport center as a
larger, tilted plane. Perspective depth recedes from 360 to 600 units; pitch,
yaw and roll foreshorten/skew the complete card while it travels to its side
over 0.42 seconds. Depth retreat and lateral motion share one immediate cubic
ease-out, removing the old 0.17-second foreground pause and slow second phase.
It then resets to an identity transform and stays anchored in screen space,
without bounce, drift, camera-follow motion or idle floating.
The artwork, perspective sentence, close
button and drag hit geometry move together. Editing or swapping adjectives
does not replay it; closing and selecting a thought again does.
No puzzle rule, object position or campaign layout is changed by the UI.
The UI asset directory is included in packaging because native widgets load
the texture by path. Source artwork lives in `Art/WenSi/UI`.
