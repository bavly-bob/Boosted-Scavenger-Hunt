# Phase 5 - Lightweight Hint System

## Goal
Provide readable AI-generated guidance without changing core gameplay logic.

## Message contract
- AI-originated hint text must begin with the label `AI Hint:`.
- Non-AI status messages keep existing behavior.

## Visual identity
- Label: `AI Hint`
- Frame: rounded rectangle in cool blue tones to separate it from normal amber status text.
- Icon: `[*]` prefix beside the label.

## Minimal implementation approach
1. `Game` marks AI-rephrased clues as `AI Hint: <text>` before emitting `clueRevealed`.
2. `GameWindow` detects this prefix (`startsWith("AI Hint:")`) and stores a boolean hint flag.
3. During HUD render:
   - If hint flag is true, draw framed hint bar with icon + label + hint body.
   - Otherwise, render the existing plain status line.
4. Keep all hint behavior presentation-only; no changes to level logic, scoring, or progression.
