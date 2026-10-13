# Accessibility: first implementation pass

Sattrak's native interface uses Dear ImGui. This work improves keyboard and visual
access; it is not a claim of WCAG conformance or complete screen-reader support.

## Keyboard use

- Press **F10** to enter the Accessibility & help panel. Press F10 again or activate
  **Return to flight controls** to give the camera keyboard focus without moving
  the mouse. Open popups and the command console retain their own input handling.
- **Tab / Shift+Tab** move through controls; **arrow keys** navigate or adjust values;
  **Enter / Space** activate; **Escape** backs out of a menu or edit.
- **Ctrl+Tab** switches ImGui panels. The help panel includes buttons to switch
  studios and open Settings, even when the usual HUD is hidden.
- In flight focus, **WASD** moves, **Q/E or C/Space** moves vertically, **arrows** look
  around, and **Page Up / Page Down** zooms the single main view. In embedded Multi
  View, explicit keyboard flight focus targets View 1.
- Camera and action shortcuts are suspended while navigating widgets or typing.
  Existing editable bindings remain under Settings → Controls; unmodified F10
  and Page Up / Page Down are reserved for navigation.

## Text alternatives

- Music controls have visible Previous, Play/Pause and Next labels alongside icons.
- Ground-station cards include real text for the name and coordinates. The photograph
  is illustrative; the textual target and coordinates convey its relevant purpose.
- The ground-track and CFD views include textual descriptions and sample values;
  visibility is stated in words rather than only through red/green lines.
- Help contains a stationary, selectable snapshot of the active studio, with Refresh
  and Copy buttons. Paste it into your preferred accessible text reader. This is a
  user-triggered snapshot, not an automatically updating screen-reader view.
- Slider descriptions appear on keyboard focus as well as hover. Exact-value fields
  have focused descriptions identifying the corresponding quantity.

## Contrast

The default theme uses opaque panel/control surfaces, brighter secondary text,
visible control borders and a yellow keyboard-focus outline. Automated checks
cover normal/secondary text against key default surfaces (4.5:1) and control/focus
indicators against their surfaces (3:1). These checks do not cover every custom
plot, image, rendered scene, transparency override or disabled state.

Design references: [W3C contrast guidance](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum),
[non-text contrast](https://www.w3.org/WAI/WCAG22/Understanding/non-text-contrast),
[keyboard access](https://www.w3.org/WAI/WCAG22/Understanding/keyboard), and
[text alternatives](https://www.w3.org/WAI/WCAG22/Understanding/non-text-content).

## Remaining gaps

- There is no native UI Automation/accessibility tree for the ImGui controls yet.
  Labels, internal IDs and tooltips are not equivalent to OS screen-reader names.
- Plots and all individual data points still need a fuller tabular alternative.
- Separate desktop view windows, file dialogs, high-DPI layouts, and the complete
  startup-to-exit workflow need broader assistive-technology testing.
- No reduced-motion setting is implemented by this pass. Pause is available, but
  animated warnings and other motion require a separate review.

The draft free-release [Terms of Use](../TERMS.md) are also available in Help.
