# Phase 4 - Dungeon Visual Upgrade (Asset Requirements)

This file defines the required pixel-art assets and layer order for a dark dungeon look.
No generated art is included here.

## Folder structure

```text
assets/
  dungeon/
    tiles/
      floor_stone_base_16.png
      floor_stone_cracked_16.png
      floor_moss_patch_16.png
      floor_rubble_16.png
      floor_water_edge_16.png
    walls/
      wall_stone_block_16.png
      wall_stone_cracked_16.png
      wall_corner_inner_16.png
      wall_corner_outer_16.png
      wall_pillar_16.png
      door_arch_closed_16.png
      door_arch_open_16.png
    props/
      torch_wall_off_16.png
      torch_wall_on_16.png
      brazier_off_16.png
      brazier_on_16.png
      barrel_16.png
      crate_16.png
      bones_pile_16.png
      banner_torn_16.png
      chain_hanging_16.png
      treasure_pedestal_16.png
      pressure_plate_16.png
    lighting/
      light_torch_glow_16.png
      light_brazier_glow_16.png
      vignette_soft_256.png
      fog_patch_soft_64.png
```

## Per-file content requirements

### `tiles/`
- `floor_stone_base_16.png`: clean dark stone floor tile; neutral base used most frequently.
- `floor_stone_cracked_16.png`: same stone palette with visible cracks for age.
- `floor_moss_patch_16.png`: stone floor with green moss in corners/edges.
- `floor_rubble_16.png`: loose pebbles/debris to break floor repetition.
- `floor_water_edge_16.png`: damp edge tile for puddles or sewer transitions.

### `walls/`
- `wall_stone_block_16.png`: primary solid wall blocks; heavy contrast outline.
- `wall_stone_cracked_16.png`: damaged wall variant with chips and fractures.
- `wall_corner_inner_16.png`: inside-corner transition for room interiors.
- `wall_corner_outer_16.png`: outside-corner transition for corridor turns.
- `wall_pillar_16.png`: narrow pillar tile for larger chambers.
- `door_arch_closed_16.png`: sealed stone doorway variant.
- `door_arch_open_16.png`: open doorway variant for unlocked/triggered state.

### `props/`
- `torch_wall_off_16.png`: unlit wall torch frame.
- `torch_wall_on_16.png`: lit wall torch with bright flame pixels.
- `brazier_off_16.png`: unlit ground brazier frame.
- `brazier_on_16.png`: lit brazier frame with stronger flame silhouette.
- `barrel_16.png`: old storage barrel for room dressing.
- `crate_16.png`: broken/aged wooden crate tile.
- `bones_pile_16.png`: bones/skull clutter tile.
- `banner_torn_16.png`: damaged hanging banner for faction identity.
- `chain_hanging_16.png`: hanging chain detail for vertical accent.
- `treasure_pedestal_16.png`: elevated treasure stand marker.
- `pressure_plate_16.png`: floor button sprite for trigger gameplay readability.

### `lighting/`
- `light_torch_glow_16.png`: additive circular warm glow centered on torch.
- `light_brazier_glow_16.png`: larger additive glow for braziers.
- `vignette_soft_256.png`: soft dark border overlay for mood and focus.
- `fog_patch_soft_64.png`: subtle low-opacity smoke/fog tile for depth.

## Tilemap layering order

Use exactly this render order (bottom to top):
1. `floor`: base walkable tiles (`tiles/*`).
2. `walls`: structural blockers (`walls/*`, door states).
3. `decorations`: non-blocking and blocking props (`props/*`).
4. `lighting`: additive glows + vignette/fog (`lighting/*`).

## Minimal usage rules

- Keep all gameplay-collision logic in code/grid, not image alpha.
- Use 16x16 tile source art for world tiles and props.
- Reserve lighting assets for overlays; do not bake glow directly into base walls.
- Keep pressure plate visuals high-contrast so triggers are readable.
