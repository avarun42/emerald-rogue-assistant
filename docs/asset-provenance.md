# Asset provenance gate

The code repository does not currently contain a project license. Its history
also does not record the source or redistribution terms for the three inherited
artwork files. The bundled font contains useful provenance, but the repository
did not previously preserve that evidence as distribution documentation. No
project license or artwork permission is inferred by the build. This is a
publication blocker, not a build blocker.

| File | Evidence currently present | Required before public release |
| --- | --- | --- |
| `WobbuffetImage.png` | Added in the repository's initial commit; no source or license recorded | Identify the creator/source and retain written redistribution permission or replace it with a cleared asset |
| `WobbuffetIcon.ico` | Added in the repository's initial commit; no source or license recorded | Establish provenance independently from the PNG because the relationship is undocumented, or regenerate after the PNG is cleared |
| `poketch_frame.png` | Added in the repository's initial commit; no source or license recorded | Identify the creator/source and redistribution terms, or replace it |
| `pokemon-emerald-pro.ttf` | Embedded metadata identifies “Pokemon Emerald Pro” version 1.0 by `crystalwalrein`, links its [FontStruct source](https://fontstruct.com/fontstructions/show/832818/pok_mon_emerald_pro), and specifies CC BY-SA 3.0; the current source page agrees | Attribution and the license URI are now shipped in `THIRD_PARTY_NOTICES.md`; before release, confirm the inherited file came from that source and review whether any separate third-party rights affect distribution |

`RogueAssistant_mGBA.lua` is project source rather than a third-party binary,
but its public redistribution terms still depend on the repository owner
choosing a project license.

Before publishing an artifact, the owner must:

1. Choose and add the repository's project license.
2. Resolve every row above and record the evidence here.
3. Rebuild `THIRD_PARTY_NOTICES.md` if a replacement asset introduces another
   attribution or license obligation.
4. Confirm that use of Pokemon names and imagery is appropriate for the
   intended non-commercial fan-project distribution.

Release automation deliberately creates a draft release only. It does not
represent completion of this provenance review and must not be used to make a
release public while this page still contains unresolved rows.
