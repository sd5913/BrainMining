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
No scene, puzzle rule, object position or campaign layout is changed.
The UI asset directory is included in packaging because native widgets load
the texture by path. Source artwork lives in `Art/WenSi/UI`.
