# Asset provenance and licensing

The repository contains no license grant that covers the inherited Rogue
Assistant source. Its history also does not record the source or redistribution
terms for the three inherited artwork files. The bundled font contains useful
provenance, but the repository did not previously preserve that evidence as
distribution documentation. The build does not grant permission to distribute
the code or artwork. These issues block publication, not local builds.

## Inherited artwork and font

### Wobbuffet image

- **File:** `WobbuffetImage.png`
- **Available evidence:** The repository's initial commit added the file without
  recording its source or license.
- **Required before public release:** Identify the creator and source, and keep
  written redistribution permission. Otherwise, replace the file with a
  cleared asset.

### Wobbuffet icon

- **File:** `WobbuffetIcon.ico`
- **Available evidence:** The repository's initial commit added the file without
  recording its source or license. The history does not document its
  relationship to the PNG image.
- **Required before public release:** Establish the icon's provenance
  independently. Alternatively, regenerate it after clearing the PNG image.

### Poketch frame

- **File:** `poketch_frame.png`
- **Available evidence:** The repository's initial commit added the file without
  recording its source or license.
- **Required before public release:** Identify the creator, source, and
  redistribution terms. Otherwise, replace the file.

### Pokémon Emerald Pro font

- **File:** `pokemon-emerald-pro.ttf`
- **Available evidence:** Embedded metadata identifies “Pokemon Emerald Pro”
  version 1.0 by `crystalwalrein`, links its
  [FontStruct source](https://fontstruct.com/fontstructions/show/832818/pok_mon_emerald_pro),
  and specifies CC BY-SA 3.0. The current source page agrees.
- **Required before public release:** The package now includes attribution and
  the license URI in `THIRD_PARTY_NOTICES.md`. Confirm that the inherited file
  came from that source, and review whether separate third-party rights affect
  distribution.

`RogueAssistant_mGBA.lua` is project source rather than a third-party binary,
but it also needs a license grant before public distribution.

Before publishing an artifact, the owner must:

1. Document a license or written permission that covers the inherited source,
   then add compatible terms for the fork's new work.
2. Resolve every asset above and record the evidence here.
3. Rebuild `THIRD_PARTY_NOTICES.md` if a replacement asset introduces another
   attribution or license obligation.
4. Confirm that use of Pokémon names and imagery is appropriate for the
   intended non-commercial fan-project distribution.

This gate applies to beta and final binaries. Release automation deliberately
creates only a draft release. A draft does not complete this review. Do not
publish it while this page contains unresolved rows.
