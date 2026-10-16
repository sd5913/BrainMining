# Cartoon City Free — ending city

- Asset: **Cartoon City Free - Low Poly 3D Models Pack**, by **ithappy**.
- Listing: https://www.fab.com/listings/e41364e1-c811-40db-85f5-84441192591d
- Acquired free through the user's Epic Games Launcher and added to BrainMining on 2026-10-15. The user explicitly authorized acceptance of the Fab EULA during acquisition.
- Original UE content: `Content/Cartoon_City_Free` (114 files, approximately 68 MiB). Originals remain unchanged.
- Ending-only copies of four building meshes and two opaque, unlit night materials: `Content/EndingEmergence/City`.
- Used silhouettes: Eco Building Grid, Eco Building Slope, Eco Building Terrace, Regular Building Twisted Tower Large.
- Street dressing uses ending-only copies of SM_Bus_Stop_02, SM_Palm_03, SM_Bush_06, SM_traffic_light_001 and SM_Trash_Can_04, with the same atlas-based night material and instancing.
- `Scripts/apply_ending_city.py` creates the material variants and replaces the 196 old cube buildings in the existing ending map. It backs up the map before saving. `ApplyCityBuildings` also preserves these assets during an explicit map rebuild.
- Fab Standard License applies; this is not a public-domain asset pack. Keep asset redistribution within the acquired license's project/collaboration terms.
