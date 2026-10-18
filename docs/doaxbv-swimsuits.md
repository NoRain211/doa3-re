# DOAXBV swimsuits

Every woman in DOA3 (Kasumi, Ayane, Christie, Tina, Hitomi, Helena and Lei
Fang) can wear any of her swimsuits from _Dead or Alive Xtreme Beach
Volleyball_, switched live during a fight. Each fighter keeps her own DOA3
head, hair, face and animation; from the neck down she wears her DOAXBV body
in the chosen suit. Lisa has no DOA3 fighter.

The swimsuits are built on the fly from **your own DOAXBV disc**. No DOAXBV
data ships: the suit recipes (`doaxbv-suits.pack.gz`, a release download) hold
only numbers and locations on the disc (see [How it works](#how-it-works)).

## Setting it up

1. Put `doaxbv-suits.pack.gz` from the release page in `private/`.
2. Have your DOAXBV disc as an image (`.iso`, trimmed or full) or as an
   extracted folder (the one holding `datahdd.afs` and `datadvd.afs`, as
   doaxbv-re's setup imports it).
3. Point the game at it, either way:
   - **macOS and Windows:** in the launcher, **Choose DOAXBV disc...**.
   - **Any platform:** put the `.iso` in `private/`, or its path (image or
     folder) on the first line of `private/doaxbv-path.txt`, or set
     `RECOMP_DOAXBV_DISC`.
4. Leave **DOAXBV swimsuits** ticked in the launcher (it is on by default).

Without the recipes or a disc the game is unchanged.

## In a fight

Everyone starts in her DOA3 costume. Nothing changes until a switch is used.

| Input | Effect |
|---|---|
| Hold **Back**, then **RB** / **LB** | Next / previous swimsuit for your own fighter (pad 1 plays P1, pad 2 P2) |
| Hold **Back**, then **RT** / **LT** | Next / previous swimsuit for your opponent, so one player can dress both |
| Keyboard **1** / **2** (**Shift** goes back) | P1 / P2 |
| Hold **Back**, then **Y** | Mark or unmark your fighter's swimsuit as a favorite |
| Hold **Back**, then **X** | Browse only your fighter's favorites, or all her swimsuits again |
| Keyboard **F** / **G** | Mark a favorite / browse favorites only, for P1 (with **Shift**, P2) |
| Hold **Back**, then d-pad **Left** / **Right** | Chest bounce weaker / stronger (Off to 1000%), for both fighters |
| Keyboard **5** | Stronger bounce (**Shift**: weaker) |
| Hold **Back**, then **L3** / **R3** (click a stick) | Bigger / smaller chest (60% to 130%, 5% steps), for both fighters |
| Keyboard **6** | Bigger chest (**Shift**: smaller) |

Holding a switch scrolls: it repeats after a moment and speeds up the longer
it is held. A box under the life bars shows each player's swimsuit number and
name while switching, with `*` after a favorite and `fav` after the player
while browsing favorites only. The list for each fighter runs from her DOA3 costume
through every DOAXBV swimsuit she can wear. A new fighter in a slot starts in
her DOA3 costume.

Favorites are kept per girl in `private/suit-favorites.txt` (one
`<girl> <suit number>` per line) and last between sessions. Tick **Start in a
favorite** in the launcher to have each fighter start in her first favorite
instead of her DOA3 costume.

**Bounce** in the launcher sets the strength of the swimsuits' chest physics,
from **Off** to **1000%** (100% is the default); in a fight, Back + d-pad
left / right changes it, shown in the swimsuit box. **Size** in the launcher
sets the chest size from **60%** to **130%** in 5% steps (100% is normal; Back +
L3 / R3 in a fight). The suit grows and shrinks with the body; beyond that
range it would pull apart under the breast, so the setting stops there.

## How it works

Both games build a character from rigid per-bone pieces and joint pieces, and
both keep models in AFS archives of XPR0 bundles. DOAXBV keeps every suit as
a separate file fitted to each girl's body; its body and suit pieces, their
textures and their vertex weights are all on the disc.

What the disc does not say is how the game puts them together: which bone
each piece hangs on, which body pieces a suit hides, and how the game bends
its CPU-skinned pieces (joints, the chest, tops) each frame. Those were taken
once, offline, from DOAXBV running under doaxbv-re, and reduced to a recipe
per suit: bone bindings and offsets, materials, the chest's bounce mask,
weld lists for the joints, and for every piece where its bytes are on the
disc and the steps that turn them into the suit's (a transform, welds onto a
neighbouring piece, trimming to the vertices used, mirroring, re-binding to
the chest). The pack stores identical pieces once; all seven girls' 2,149
suits take about 4 MB compressed.

At run time the costume injector (`recomp-runtime/costume_inject.c`) loads a
suit when it is chosen: `suit_recipe.c` reads the ranges the recipe names from
the disc image or folder and replays its steps, which takes a few
milliseconds, then the injector draws it in place of the fighter's body
(the most recent twelve suits stay loaded). The suit's skeleton is retargeted
onto the DOA3 fighter's: DOA3's rotations, DOAXBV's bone lengths. Suit mode
then joins the two heads and bodies:

- **Head offset.** The two games put the head joint at different points of
  the same face. A per-girl offset, fitted by matching the two faces (to
  under 2 mm), seats the DOAXBV neck exactly where the DOA3 face starts.
- **Neck length.** Where DOA3's torso is longer than the DOAXBV one, the
  upper body rises along the spine instead of stretching the neck.
- **Skin.** DOAXBV skin takes its colour from the fighter's own face material
  each frame, so the seam matches under any stage's lights.
- **Neckwear.** DOA3 chokers and collars on the head bone are hidden.
- **Chest.** Tops are re-bound to the chest so they move with the breast; one
  bounce field covers skin and cloth and fades to nothing at the shoulders.

Fighters are told apart by skeleton (bone lengths); Kasumi and Ayane share
one, so the hair colour of the head draws decides.

## Limits

- Skirts and other cloth DOAXBV simulates are rigid in their rest shape.
- Fingers keep DOAXBV's relaxed rest pose.
- The swimsuit overlay draws only on macOS and Linux for now.
- Cloth highlights are a little strong under some stages' lights.

## Runtime options

| Variable | Effect |
|---|---|
| `RECOMP_DOAXBV_DISC` | The DOAXBV disc: an `.iso` or a folder with `datahdd.afs` and `datadvd.afs`. |
| `RECOMP_COSTUME_PACK` | The unpacked recipe pack. `run_game.sh` and `Play.cmd` unpack `doaxbv-suits.pack.gz` from `private/` next to it. |
| `RECOMP_COSTUME_FAVORITES` | The favorites file. Default: `suit-favorites.txt` beside the recipe pack. |
| `RECOMP_COSTUME_START_FAVORITE=1` | Each fighter starts in her first favorite (the launcher's **Start in a favorite**). |
| `RECOMP_COSTUME_JIGGLE` | Starting bounce strength, default 0.7; 0 turns it off. The launcher's **Bounce** sets it (100% = 0.7). |
| `RECOMP_COSTUME_SIZE` | Starting chest size in percent, 60-130 (default 100). The launcher's **Size** sets it. |
| `RECOMP_COSTUME_SKIN_MATCH=0` | Keeps DOAXBV's own skin colour. |
| `RECOMP_COSTUME_TORSO_FIX=0` | Lets the neck stretch instead of raising the upper body. |
| `RECOMP_COSTUME_HEAD_UP`, `RECOMP_COSTUME_HEAD_FORWARD` | Override the girl's head offset, in cm. |
