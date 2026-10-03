# Art references

Images the user supplied as style targets. Never loaded by the game: the "no asset files" rule still holds, and every sprite stays a char array or maths. Files that are missing were shared only in chat. Save them here under the names below.

| File | What it shows | Used for |
|---|---|---|
| `amulet_serpent_ring_photo.png` | Photo of a carved dark pendant: a serpent coiled inside a ring, on a cord | Jormungandr's Coil (`items.cpp:AMULETS`) |
| `amulets_sheet.png` *(missing)* | Pixel sheet of 7 charms, necklace style: Celtic knot with waves and a sapphire, gold Mjolnir with lightning, anvil with a flame, frost snowflake, root-wrapped green stone with runes, serpent ring, winged gold helm | Amulet sprites: Njordr, Mjolnir, Brokkr, Skadi, Yggdrasil, Jormungandr. Winged helm unused (a Valkyrie amulet idea) |
| `enemies_sheet_1.png` *(missing)* | Dark pixel sheet. **Wolves** (4 poses, grey, snarling, one bloodied). **Peasants** (hooded, pitchfork, torch, club, bow, scythe). **Soldiers** (kettle helm, red heater shield with a white cross, spear, sword, bow, banner). **Knights** (great helm, plate, red tabard with a cross, sword, mace, poleaxe, mounted). **Mages** (hooded, staff with violet/fire/ice/green orbs, a necromancer with a skull staff and a green spirit) | Next change set: the enemy sprite upgrade, and the main character |
| `enemies_sheet_2.png` *(missing)* | **Underground ghosts** (teal, hooded, lanterns, a ghost archer, an antlered one). **Skeletons** (sword and shield, spear, axe, bow, a crowned skeleton lord). **Ogres & brutes** (club, spiked club, horned helm with a hammer, a tattooed pale ogre with a frost mace). **Underground dwellers** (spiny crawler, spear goblin, hooded lantern hermit, mushroom brute, pale hound). **Black Knight boss** (antlered helm, black horse, greatsword, banner). **Sea creatures** (serpents, a coral crab, a jellyfish, fish-men). **Drowned undead** (shield, trident, a crowned drowned king) | Same |
| `sprite_scale_check.png` *(missing)* | At 3x zoom: two current villagers, a detailed skeleton at villager height, and the same skeleton 1.7x taller | Target scale: detailed enemies about 1.7x villager height |
| `watchtower.png`, `hall_interior.png` *(missing)* | A wooden watchtower with a roofed archer deck, torches on the rail, a stone arched door and a lean-to; a timber hall interior with stairs, a loft rail with a skull, a hanging lantern, a fireplace, a rug and a table | Buildings (`levelgen.cpp:placeWatchtower`, `placeHouse`, the Hearthwick halls) |

**Direction from the user:** find a middle ground between these and the current sprites. Keep our rigging and animation ability. Upsizing is fine. Use the sheets to inspire the main character too.
