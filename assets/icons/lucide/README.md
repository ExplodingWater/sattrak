# Playback icons

Official Lucide SVGs downloaded October 24, 2026:

- https://github.com/lucide-icons/lucide/blob/main/icons/play.svg
- https://github.com/lucide-icons/lucide/blob/main/icons/pause.svg
- https://github.com/lucide-icons/lucide/blob/main/icons/skip-back.svg
- https://github.com/lucide-icons/lucide/blob/main/icons/skip-forward.svg

The native player reproduces these SVG paths in `include/gui/music_icons.hpp`, including their rounded corners and stroke caps. Paths are rendered directly by ImGui at the current interface scale and text color; no network connection is needed at runtime.

The original SVGs and complete upstream LICENSE are included here. See https://lucide.dev/license.
