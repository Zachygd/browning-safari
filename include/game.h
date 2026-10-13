/*
 * game.h - game-side globals and prototypes shared between modules
 */
#ifndef GAME_H
#define GAME_H

/* ------------------------------------------------------------- screens -- */
enum {
    SCR_MAIN = 0, SCR_NEW, SCR_LOAD, SCR_DELETE, SCR_SOUND, SCR_MUSEUM,
    SCR_TRAVEL, SCR_MAP, SCR_LODGE, SCR_LOUNGE, SCR_VILLAGE, SCR_11,
    SCR_SHOP, SCR_SCOPES, SCR_SUITCASE, SCR_SUPPLIES, SCR_LOCATION,
    SCR_HUNT, SCR_GAMEBAG, SCR_HANDBOOK, SCR_TROPHY, SCR_GAMEOVER
};

/* ------------------------------------------------------------- globals -- */
/* main.c */
extern HOT_SPOT *screen_hotspots;
extern int       oldGame_Screen, game_screen;
extern PLAYER   *player;
extern int       num_screen_gfx;
extern int       done;
extern int       palette_steps;

/* init.c */
extern int    current_player;
extern PLAYER players[8];

/* mouse.c */
extern int mouseX, mouseY;
extern int lastInput, lastKey, lastCode;
extern int bFlipMouse;
extern int debugLog;

/* character.c */
extern char guide_said[64];

/* hunt.c */
extern SPRITE       sprites[10];
extern int          sp_sort[5];
extern int          number_of_species;
extern int          game_offset;
extern int          bow_sight_x, bow_sight_y;
extern SPECIES_TYPE species[7];
extern int          land[7];
extern int          area_pop[49];
extern int          num_permits[7];
extern int          animals_overall;
extern HUNTER       hunter;
extern GAMEBAG      huntbag[70];
extern MGLDC       *sceneDC;
extern int          pending_comment;

/* intrface.c */
extern int time_hour, time_total, time_day, time_min;
extern int temperature;
extern int update_temp, update_sprite, update_score, update_lifebar, update_wind, update_clock;
extern int mouse_shape, hunt_pointer, gunSight, lastMouseX, lastMouseY;
extern void (*DrawPointer)(void), (*CopyPointer)(void);
extern void (*DrawWeapon)(void), (*CopyWeapon)(void);
extern HOT_SPOT hunt_hotspots[];
extern int weapon_array[3];

/* ------------------------------------------------------------ prototypes -- */
/* main.c */
void DoSaveGame(int unused);
void StartOver(void);
void DoExit(int unused);
void DrawMainScreen(void);
void DumpScreen(void);
void DumpScreen2(void);
void DoScreen(void);
void DoScreen2(void);
HOT_SPOT *CheckHotSpots(HOT_SPOT *hs);
void CoverHotSpots(HOT_SPOT *hs);
void CopyHotSpots(HOT_SPOT *hs);
void ExitScreen(int param);
void SwitchScreen(int which);
void TempSwitchScreen(int which);
int  GameInit(void);
void GameExit(void);
void GameLoop(void);
void EmergencyExit(void);
void SetPalette(void);
void FadeStep(char step);
void BlackPalette(void);
void FadeToBlack(void);
void FadeFromBlack(void);
void FadeToRed(void);
void FadeToNight(void);
void GetPaletteFromGraphicsFile(void);

/* init.c */
int  CheckRegistry(void);
void SetGameOptions(void);
void DeleteGame(int which);
void SaveGame(int which);
int  LoadPlayer(int which);
void GraphicsExit(void);
int  GetGameOptions(void);
int  CheckCD(void);
int  GraphicsInit(void);
int  SafariMain(void);

/* front.c */
void ShowTitleScreen(void);
void ShowCredits(void);
void DrawNewScreen(void);
void DrawLoadScreen(void);
void DrawDeleteScreen(void);
int  GetName(char *szName, int x, int y);
void EditField(EDITFIELDINFO *e);

/* mouse.c */
int  InitMouse(void);
void InitMouseSurface(int num);
void ExitMouse(void);
void SetMouseGraphic(int which);
int  GetMouseGraphic(void);
void VerifyMouse(void);
void WaitMouse(void);
void WaitMouseUp(void);
void GetMouse(int *mx, int *my, int *button);
void InitMouseSave(void);
void MoveMouse(int x, int y);
void TrackMouse(void);
void BlitMouse(int x, int y);
void DrawMouse(void);
void CopyMouse(void);
void DrawBinocs(void);
void PrepareScopeLayer(void);
void ScaleElk(int num, int xpos, int ypos, int zpos, int percent, int flip);
void DrawScope(void);
void InitBowPeepSave(void);
void DrawBowPeep(void);
void CopyBowPeep(void);
void CheckEvents(void);
void MouseHourGlass(void);
void MouseRestore(void);
void MouseNormal(void);
void ShowMouse(void);
void HideMouse(void);
void HideMouseOnPage(MGLDC *page);
void ShowMouseOnPage(MGLDC *page);

/* screens.c */
void DoMusic(int param);
void DoVolume(int param);
void RestoreSoundScreen(void);
void DrawSoundScreen(void);
void DrawVillageScreen(void);
void PickGuide(int which);
void DrawTravelScreen(void);
void DoTravelScreen(void);
void DrawMapScreen(void);
void DoMapScreen(void);
void ShowDay(void);
void ShowTime(void);
void DoBed(int param);
void DoShop(int unused);
void DrawLodgeScreen(void);
void DoLodgeScreen(void);
void DoGuide(int param);
void DrawLoungeScreen(void);
void DoMap(int which);
void DrawLocationScreen(void);
void LocationKey(int which);

/* character.c */
void DoCharacterComment(int bkg, int character, int comment, int xpos, int ypos);
void DoCharacterVoice(int bkg, int character, int comment);
void DoGuideComment(int guide, int comment);
void DoGuideVoice(int guide, int comment);
void DoWardenComment(int comment);
void DoWardenJailComment(int comment, int xpos, int ypos);
void InitSnidleyComment(void);
void ExitSnidleyComment(void);
void DoSnidleyComment(int comment);

/* store.c (museum, shop, suitcase, supplies, scopes, handbook) */
void DrawMuseumScreen(void);
void RestoreMuseumScreen(void);
void DumpMuseumScreen(void);
void DoMuseumScreen(void);
void MoveMuseum(int direction);
void DrawShopScreen(void);
void RestoreShopScreen(void);
void DumpShopScreen(void);
void DoShopScreen(void);
void MoveShop(int direction);
void BuyItem(int item);
void ResetShopItems(void);
int  LoadCanvas(const char *file, MGLDC *page);
int  CheckBitmapData(int which, int xpos, int ypos);
void DrawSuitcaseScreen(void);
void DumpSuitcaseScreen(void);
void DoSuitcaseScreen(void);
void DrawSuppliesScreen(void);
void DumpSuppliesScreen(void);
void DoSuppliesScreen(void);
void DrawScopesScreen(void);
void DumpScopesScreen(void);
void DoScopesScreen(void);
void PickRifle(int which);
void DrawHandbookScreen(void);
void RestoreHandbookScreen(void);
void DumpHandbookScreen(void);
void DoHandbookScreen(void);
void NextPage(int dir);
void ExitBook(int param);

/* hunt.c */
void DivideAnimals(void);
void MoveAnimals(void);
void ScareAnimals(void);
void SpeciesRatio(void);
int  PickSpecies(int s);
int  GetSpriteYPosition(int s);
void ScaleGraphic2(int num, int xpos, int ypos, int zpos, int percent, int flip);
void RestoreHuntScreen(void);
void MoveScreen(int direction);
int  LoadGameScene(void);
void FreeGameScene(void);
void DrawHuntScreen(void);
void InitSprites(void);
void SpriteWait(int s);
void SpriteGone(int s);
void InitGone(int s);
void InitWalk(int s);
void AssignDelta(int s, int direction);
void SpriteWalk(int s);
void InitStill(int s);
void InitCharge(int s);
void InitShot(int s);
void StartRun(int s);
void SpriteStill(int s);
void InitFeed(int s);
void InitLook(int s);
void InitLookAway(int s);
void StartWalk(int s);
void SpriteFeed(int s);
void SpriteLook(int s);
void SpriteCharge(int s);
void InitKill(int s);
void SpriteKill(int s);
void SpriteShot(int s);
void DumpHuntScreen(void);
void SwitchWeapon(int weapon);
void AlignGunSight(void);
void AlignGunSight2(void);
void DoHuntScreen(void);
void PlayGuideSound(int sndnum);
void AlignBowSight(void);
void AlignBowSight2(void);
void ShootGun(void);
void CommentAfterShot(void);
void CommentAtEnd(void);
void ExitHunt(void);

/* intrface.c */
void ToggleBinocs(void);
void LoadWeapon(void);
void PutAmmoBackInBox(void);
void AdvanceClock(void);
void DrawLifebar(void);
void BlitLifebar(void);
void DrawCompass(void);
void BlitCompass(void);
void DrawWind(void);
void BlitWind(void);
void DrawClock(void);
void BlitClock(void);
void DrawTemperature(void);
void BlitTemperature(void);
void DrawScore(void);
void BlitScore(void);
void InitInterfaceIcons(void);
void ExitInterfaceIcons(void);

/* sprites.c */
int  CheckKillZone(int which, int xpos, int ypos, int flip, int scale);
int  CheckSpriteData(int which, int xpos, int ypos, int flip, int scale);
int  CheckLayer(int which, int xpos, int ypos);
void SortSprites(int num);
int  GetSpriteScale(int xpos, int ypos);
int  CheckSpritePath(int xpos, int ypos, int unused);

/* gamebag.c */
void EmptyHuntGameBag(void);
void EmptyPlayerGameBag(void);
void RestoreGamebagScreen(void);
void DrawGamebagScreen(void);
void DrawTrophyScreen(void);
void DumpGamebagScreen(void);
void DumpTrophyScreen(void);
void DoGamebagScreen(void);
void DoTrophyScreen(void);

/* gameover.c */
void DrawGameOverScreen(void);
void DumpGameOverScreen(void);
void DoGameOverScreen(void);

#endif
