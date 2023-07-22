/*
	Copyright 1998-2003 Interplay Entertainment Corp.  All rights reserved.
*/

#ifndef _ITEMPID_H_DEFINED
#define _ITEMPID_H_DEFINED

/******************************************************************
   File Name: ItemPid.H

   Note: This file is arranged differently than all the other
         header files. Rather than having all of the items in the
         order in which they appear in the item list, they will
         be sorted, but the Pid number defines will remain as they
         are in the item list. Please do not alter numbers, unless
         it is to fix errors found through mapper.

         SKIPPING PIDs 60-70   BECAUSE THEY ARE DESKS AND BOOKCASES
         SKIPPING PIDs 145-158 BECAUSE THEY ARE DESKS AND BOOKCASES
         SKIPPING PIDs 164-179 BECAUSE THEY ARE FALLOUT 1 SPECIFIC
         SKIPPING PIDs 181-204 BECAUSE THEY ARE FALLOUT 1 SPECIFIC
         SKIPPING PIDs 213-217 BECAUSE THEY ARE FALLOUT 1 SPECIFIC
         SKIPPING PIDs 230-231 BECAUSE THEY ARE FALLOUT 1 SPECIFIC
         SKIPPING PID  238     BECAUSE THEY ARE FALLOUT 1 SPECIFIC

   Purpose: This file will contain defines for all the item
            prototypes for the game. These prototypes can be
            found in mapper using <F1> or selecting items. All
            defines in here will need to be prepended with PID_
            for the ease of everyone. Additionally, Please do not
            make duplicate names for the same item. Most should
            have the correct name.

   Structure: The following is the structure of this file.
                I.    Armor
                II.   Weapons
                III.  Ammo
                IV.   Medical
                V.    Containers
                VI.   Books
                VII.  Tools
                VIII. Misc.
                IX.   Active_Items
                X.    Flying Weapons

   Created: October 02, 1997
******************************************************************/


/******************************************************************
***************       Armor                         ***************
******************************************************************/

#define PID_LEATHER_ARMOR                   (1)
#define PID_METAL_ARMOR                     (2)
#define PID_POWERED_ARMOR                   (3)
#define PID_COMBAT_ARMOR                    (17)
#define PID_LEATHER_JACKET                  (74)
#define PID_PURPLE_ROBE                     (113)
#define PID_HARDENED_POWER_ARMOR            (232)
#define PID_BROTHERHOOD_COMBAT_ARMOR        (239)
#define PID_TESLA_ARMOR                     (240)
#define PID_CURED_LEATHER_ARMOR             (265)
#define PID_ADVANCED_POWER_ARMOR            (348)
#define PID_ADVANCED_POWER_ARMOR_MK2        (349)
#define PID_LEATHER_ARMOR_MK_II             (379)
#define PID_METAL_ARMOR_MK_II               (380)
#define PID_COMBAT_ARMOR_MK_II              (381)
#define PID_BRIDGEKEEPERS_ROBE              (524)

/******************************************************************
***************       Weapons                       ***************
******************************************************************/

#define PID_KNIFE                           (4)
#define PID_CLUB                            (5)
#define PID_SLEDGEHAMMER                    (6)
#define PID_SPEAR                           (7)
#define PID_10MM_PISTOL                     (8)
#define PID_10MM_SMG                        (9)
#define PID_HUNTING_RIFLE                   (10)
#define PID_FLAMER                          (11)
#define PID_MINIGUN                         (12)
#define PID_ROCKET_LAUNCHER                 (13)
#define PID_PLASMA_RIFLE                    (15)
#define PID_LASER_PISTOL                    (16)
#define PID_DESERT_EAGLE                    (18)
#define PID_ROCK                            (19)
#define PID_CROWBAR                         (20)
#define PID_BRASS_KNUCKLES                  (21)
#define PID_14MM_PISTOL                     (22)
#define PID_ASSAULT_RIFLE                   (23)
#define PID_PLASMA_PISTOL                   (24)
#define PID_FRAG_GRENADE                    (25)
#define PID_PLASMA_GRENADE                  (26)
#define PID_PULSE_GRENADE                   (27)
#define PID_GATLING_LASER                   (28)
#define PID_THROWING_KNIFE                  (45)
#define PID_SHOTGUN                         (94)
#define PID_SUPER_SLEDGE                    (115)
#define PID_RIPPER                          (116)
#define PID_LASER_RIFLE                     (118)
#define PID_ALIEN_LASER_PISTOL              (120)
#define PID_9MM_MAUSER                      (122)
#define PID_SNIPER_RIFLE                    (143)
#define PID_MOLOTOV_COCKTAIL                (159)
#define PID_CATTLE_PROD                     (160)
#define PID_RED_RYDER_BB_GUN                (161)
#define PID_RED_RYDER_LE_BB_GUN             (162)
#define PID_TURBO_PLASMA_RIFLE              (233)
#define PID_SPIKED_KNUCKLES                 (234)
#define PID_POWER_FIST                      (235)
#define PID_COMBAT_KNIFE                    (236)
#define PID_223_PISTOL                      (241)
#define PID_COMBAT_SHOTGUN                  (242)
#define PID_JONNY_BB_GUN                    (261)
#define PID_HK_CAWS                         (268)
#define PID_ROBO_ROCKET_LAUNCHER            (270)
#define PID_SHARP_SPEAR                     (280)
#define PID_SCOPED_HUNTING_RIFLE            (287)
#define PID_EYEBALL_FIST_1                  (290)
#define PID_EYEBALL_FIST_2                  (291)
#define PID_BOXING_GLOVES                   (292)
#define PID_PLATED_BOXING_GLOVES            (293)
#define PID_HK_P90C                         (296)
#define PID_SPRINGER_RIFLE                  (299)
#define PID_ZIP_GUN                         (300)
#define PID_44_MAGNUM_REVOLVER              (313)
#define PID_SWITCHBLADE                     (319)
#define PID_SHARPENED_POLE                  (320)
#define PID_PLANT_SPIKE                     (365)
#define PID_DEATHCLAW_CLAW_1                (371)
#define PID_DEATHCLAW_CLAW_2                (372)
#define PID_TOMMY_GUN                       (283)
#define PID_GREASE_GUN                      (332)
#define PID_BOZAR                           (350)
#define PID_LIGHT_SUPPORT_WEAPON            (355)
#define PID_FN_FAL                          (351)
#define PID_HK_G11                          (352)
#define PID_INDEPENDENT                     (353)
#define PID_PANCOR_JACKHAMMER               (354)
#define PID_SHIV                            (383)
#define PID_WRENCH                          (384)
#define PID_LOUISVILLE_SLUGGER              (386)
#define PID_SOLAR_SCORCHER                  (390)  // No ammo
#define PID_SAWED_OFF_SHOTGUN               (385)  // 12 ga.
#define PID_M60                             (387)  // 7.62
#define PID_NEEDLER_PISTOL                  (388)  // HN Needler
#define PID_AVENGER_MINIGUN                 (389)  // 5mm JHP
#define PID_HK_G11E                         (391)  // 4.7mm Caseless
#define PID_M72_GAUSS_RIFLE                 (392)  // 2mm EC
#define PID_PHAZER                          (393)  // Small Energy
#define PID_PK12_GAUSS_PISTOL               (394)  // 2mm EC
#define PID_VINDICATOR_MINIGUN              (395)  // 4.7mm Caseless
#define PID_YK32_PULSE_PISTOL               (396)  // Small Energy
#define PID_YK42B_PULSE_RIFLE               (397)  // Micro Fusion
#define PID_44_MAGNUM_SPEEDLOADER           (398)
#define PID_SUPER_CATTLE_PROD               (399)
#define PID_IMPROVED_FLAMETHROWER           (400)
#define PID_LASER_RIFLE_EXT_CAP             (401)
#define PID_MAGNETO_LASER_PISTOL            (402)
#define PID_FN_FAL_NIGHT_SCOPE              (403)
#define PID_DESERT_EAGLE_EXT_MAG            (404)
#define PID_ASSAULT_RIFLE_EXT_MAG           (405)
#define PID_PLASMA_PISTOL_EXT_CART          (406)
#define PID_MEGA_POWER_FIST                 (407)
#define PID_HOLY_HAND_GRENADE               (421)  // Special don't use this
#define PID_GOLD_NUGGET                     (423)
#define PID_URANIUM_ORE                     (426)
#define PID_FLAME_BREATH                    (427)
#define PID_REFINED_ORE                     (486)
#define PID_SPECIAL_BOXING_GLOVES           (496)  // DO NOT USE, SPECIAL, VERY VERY SPECIAL, GOT IT?
#define PID_SPECIAL_PLATED_BOXING_GLOVES    (497)  // NO YOU CANNOT USE THIS, NO I SAID
#define PID_GUN_TURRET_WEAPON               (498)
#define PID_FN_FAL_HPFA                     (500)
#define PID_LIL_JESUS_WEAPON                (517)
#define PID_DUAL_MINIGUN                    (518)
#define PID_HEAVY_DUAL_MINIGUN              (520)
#define PID_WAKIZASHI_BLADE                 (522)
#define PID_PLANT_SPIKE_EPA                 (583)

// NOTE: any new weapons that need ammo of any sort need to be placed in the arm_obj macro in COMMANDS.H
//       if you're not gonna do it, please put the pid definition after the comments so that we know to add it
//       thank you for your support

//Below not YET added to arm_obj macro

/******************************************************************
***************       Ammo                          ***************
******************************************************************/

#define PID_EXPLOSIVE_ROCKET                (14)
#define PID_10MM_JHP                        (29)
#define PID_10MM_AP                         (30)
#define PID_44_MAGNUM_JHP                   (31)
#define PID_FLAMETHROWER_FUEL               (32)
#define PID_14MM_AP                         (33)
#define PID_223_FMJ                         (34)
#define PID_5MM_JHP                         (35)
#define PID_5MM_AP                          (36)
#define PID_ROCKET_AP                       (37)
#define PID_SMALL_ENERGY_CELL               (38)
#define PID_MICRO_FUSION_CELL               (39)
#define PID_SHOTGUN_SHELLS                  (95)
#define PID_44_FMJ_MAGNUM                   (111)
#define PID_9MM_BALL                        (121)
#define PID_BBS                             (163)
#define PID_ROBO_ROCKET_AMMO                (274)
#define PID_45_CALIBER_AMMO                 (357)
#define PID_2MM_EC_AMMO                     (358)
#define PID_4_7MM_CASELESS                  (359)
#define PID_9MM_AMMO                        (360)
#define PID_HN_NEEDLER_CARTRIDGE            (361)
#define PID_HN_AP_NEEDLER_CARTRIDGE         (362)
#define PID_7_62MM_AMMO                     (363)
#define PID_FLAMETHROWER_FUEL_MK_II         (382)


/******************************************************************
***************       Medical                       ***************
******************************************************************/

#define PID_STIMPAK                         (40)
#define PID_FIRST_AID_KIT                   (47)
#define PID_RADAWAY                         (48)
#define PID_ANTIDOTE                        (49)
#define PID_MENTATS                         (53)
#define PID_MUTATED_FRUIT                   (71)
#define PID_BUFFOUT                         (87)
#define PID_DOCTORS_BAG                     (91)
#define PID_RAD_X                           (109)
#define PID_PSYCHO                          (110)
#define PID_SUPER_STIMPAK                   (144)

#define PID_JET                             (259)
#define PID_JET_ANTIDOTE                    (260)
#define PID_BROC_FLOWER                     (271)
#define PID_XANDER_ROOT                     (272)
#define PID_HEALING_POWDER                  (273)
#define PID_MEAT_JERKY                      (284)
#define PID_HYPODERMIC_NEEDLE               (318)
#define PID_MUTAGENIC_SYRUM                 (329)
#define PID_HEART_PILLS                     (333)
#define PID_HYPO_POISON                     (334)
#define PID_FIELD_MEDIC_KIT                 (408)
#define PID_PARAMEDICS_BAG                  (409)
#define PID_MONUMENT_CHUNCK                 (424)
#define PID_MEDICAL_SUPPLIES                (428)


/******************************************************************
***************       Container                     ***************
******************************************************************/

#define PID_FRIDGE                          (42)
#define PID_ICE_CHEST_LEFT                  (43)
#define PID_ICE_CHEST_RIGHT                 (44)
#define PID_BAG                             (46)
#define PID_BACKPACK                        (90)
#define PID_BROWN_BAG                       (93)
#define PID_FOOTLOCKER_CLEAN_LEFT           (128)
#define PID_FOOTLOCKER_RUSTY_LEFT           (129)
#define PID_FOOTLOCKER_CLEAN_RIGHT          (130)
#define PID_FOOTLOCKER_RUSTY_RIGHT          (131)
#define PID_LOCKER_CLEAN_LEFT               (132)
#define PID_LOCKER_RUSTY_LEFT               (133)
#define PID_LOCKER_CLEAN_RIGHT              (134)
#define PID_LOCKER_RUSTY_RIGHT              (135)
#define PID_WALL_LOCKER_CLEAN_LEFT          (136)
#define PID_WALL_LOCKER_CLEAN_RIGHT         (137)
#define PID_WALL_LOCKER_RUSTY_LEFT          (138)
#define PID_WALL_LOCKER_RUSTY_RIGHT         (139)
#define PID_CONTAINER_WOOD_CRATE            (180)
#define PID_VAULT_DWELLER_BONES             (211)
#define PID_SMALL_POT                       (243)
#define PID_TALL_POT                        (244)
#define PID_CHEST                           (245)
#define PID_LEFT_ARROYO_BOOKCASE            (246)
#define PID_RIGHT_ARROYO_BOOKCASE           (247)
#define PID_OLIVE_POT                       (248)
#define PID_FLOWER_POT                      (249)
#define PID_HUMAN_BONES                     (250)
#define PID_ANNA_BONES                      (251)
#define PID_CRASHED_VERTI_BIRD              (330)
#define PID_GRAVESITE_1                     (344)
#define PID_GRAVESITE_2                     (345)
#define PID_GRAVESITE_3                     (346)
#define PID_GRAVESITE_4                     (347)
#define PID_LG_LT_AMMO_CRATE                (367)
#define PID_SM_LT_AMMO_CRATE                (368)
#define PID_LG_RT_AMMO_CRATE                (369)
#define PID_SM_RT_AMMO_CRATE                (370)
#define PID_LF_GRAVESITE_1                  (374)
#define PID_LF_GRAVESITE_2                  (375)
#define PID_LF_GRAVESITE_3                  (376)
#define PID_STONE_HEAD                      (425)
#define PID_WAGON_RED                       (434)
#define PID_WAGON_GREY                      (435)
#define PID_CAR_TRUNK                       (455)
#define PID_JESSE_CONTAINER                 (467)  // RESERVED
#define PID_WALL_SAFE                       (501)
#define PID_FLOOR_SAFE                      (502)
#define PID_POOL_TABLE_1                    (510)
#define PID_POOL_TABLE_2                    (511)
#define PID_POOL_TABLE_3                    (512)
#define PID_POOL_TABLE_4                    (513)
#define PID_POOL_TABLE_5                    (514)
#define PID_POOL_TABLE_6                    (515)
#define PID_POOR_BOX                        (521)

/******************************************************************
***************       Books                         ***************
******************************************************************/

#define PID_BIG_BOOK_OF_SCIENCE             (73)
#define PID_DEANS_ELECTRONICS               (76)
#define PID_FIRST_AID_BOOK                  (80)
#define PID_SCOUT_HANDBOOK                  (86)
#define PID_GUNS_AND_BULLETS                (102)
#define PID_CATS_PAW                        (225)
#define PID_TECHNICAL_MANUAL                (228)
#define PID_CHEMISTRY_MANUAL                (237)

#define PID_SLAG_MESSAGE                    (263)
#define PID_CATS_PAW_ISSUE_5                (331)


/******************************************************************
***************       Tools                         ***************
******************************************************************/

#define PID_DYNAMITE                        (51)
#define PID_GEIGER_COUNTER                  (52)
#define PID_STEALTH_BOY                     (54)
#define PID_MOTION_SENSOR                   (59)
#define PID_MULTI_TOOL                      (75)
#define PID_ELECTRONIC_LOCKPICKS            (77)
#define PID_LOCKPICKS                       (84)
#define PID_PLASTIC_EXPLOSIVES              (85)
#define PID_ROPE                            (127)

#define PID_SUPER_TOOL_KIT                  (308)
#define PID_EXP_LOCKPICK_SET                (410)
#define PID_ELEC_LOCKPICK_MKII              (411)


/******************************************************************
***************       Misc.                         ***************
******************************************************************/

#define PID_BOTTLE_CAPS                     (41)
#define RESERVED_ITEM00                     (50)  // Reserved item! Don't use! Broken!
#define PID_WATER_CHIP                      (55)
#define PID_DOG_TAGS                        (56)
#define PID_ELECTRONIC_BUG                  (57)
#define PID_HOLODISK                        (58)
#define PID_BRIEFCASE                       (72)
#define PID_FUZZY_PAINTING                  (78)
#define PID_FLARE                           (79)
#define PID_IGUANA_ON_A_STICK               (81)
#define PID_KEY                             (82)
#define PID_KEYS                            (83)
#define PID_WATCH                           (88)
#define PID_MOTOR                           (89)
#define PID_SCORPION_TAIL                   (92)
#define PID_RED_PASS_KEY                    (96)
#define PID_BLUE_PASS_KEY                   (97)
#define PID_PUMP_PARTS                      (98)
#define PID_GOLD_LOCKET                     (99)
#define PID_RADIO                           (100)
#define PID_LIGHTER                         (101)
#define PID_MEAT_ON_A_STICK                 (103)
#define PID_TAPE_RECORDER                   (104)
#define PID_NUKE_KEY                        (105)
#define PID_NUKA_COLA                       (106)
#define PID_ALIEN_SIDE                      (107)
#define PID_ALIEN_FORWARD                   (108)
#define PID_URN                             (112)
#define PID_TANGLERS_HAND                   (114)
#define PID_FLOWER                          (117)
#define PID_NECKLACE                        (119)
#define PID_PSYCHIC_NULLIFIER               (123)
#define PID_BEER                            (124)
#define PID_BOOZE                           (125)
#define PID_WATER_FLASK                     (126)
#define PID_ACCESS_CARD                     (140)
#define PID_BLACK_COC_BADGE                 (141)
#define PID_RED_COC_BADGE                   (142)
#define PID_BARTER_TANDI                    (212)
#define PID_BARTER_LIGHT_HEALING            (218)
#define PID_BARTER_MEDIUM_HEALING           (219)
#define PID_BARTER_HEAVY_HEALING            (220)
#define PID_SECURITY_CARD                   (221)
#define PID_TOGGLE_SWITCH                   (222)
#define PID_YELLOW_PASS_KEY                 (223)
#define PID_SMALL_STATUETTE                 (224)
#define PID_BOX_OF_NOODLES                  (226)
#define PID_FROZEN_DINNER                   (227)
#define PID_MOTIVATOR                       (229)

#define PID_ANNA_GOLD_LOCKET                (252)
#define PID_CAR_FUEL_CELL_CONTROLLER        (253)
#define PID_CAR_FUEL_INJECTION              (254)
#define PID_DAY_PASS                        (255)
#define PID_FAKE_CITIZENSHIP                (256)
#define PID_CORNELIUS_GOLD_WATCH            (257)
#define PID_HY_MAG_PART                     (258)
#define PID_RUBBER_BOOTS                    (262)
#define PID_SMITH_COOL_ITEM                 (264)
#define PID_VIC_RADIO                       (266)
#define PID_VIC_WATER_FLASK                 (267)
#define PID_ROBOT_PARTS                     (269)
#define PID_TROPHY_OF_RECOGNITION           (275)
#define PID_GECKO_PELT                      (276)
#define PID_GOLDEN_GECKO_PELT               (277)
#define PID_FLINT                           (278)
#define PID_NEURAL_INTERFACE                (279)
#define PID_DIXON_EYE                       (281)
#define PID_CLIFTON_EYE                     (282)
#define PID_RADSCORPION_PARTS               (285)
#define PID_FIREWOOD                        (286)
#define PID_CAR_FUEL_CELL                   (288)
#define PID_SHOVEL                          (289)
#define PID_HOLODISK_FAKE_V13               (294)
#define PID_CHEEZY_POOFS                    (295)
#define PID_PLANK                           (297)
#define PID_TRAPPER_TOWN_KEY                (298)
#define PID_CLIPBOARD                       (301)
#define PID_GECKO_DATA_DISK                 (302)
#define PID_REACTOR_DATA_DISK               (303)
#define PID_DECK_OF_TRAGIC_CARDS            (304)
#define PID_YELLOW_REACTOR_KEYCARD          (305)
#define PID_RED_REACTOR_KEYCARD             (306)
#define PID_PLASMA_TRANSFORMER              (307)
#define PID_TALISMAN                        (309)
#define PID_GAMMA_GULP_BEER                 (310)
#define PID_ROENTGEN_RUM                    (311)
#define PID_PART_REQUISITION_FORM           (312)
#define PID_BLUE_CONDOM                     (314)
#define PID_GREEN_CONDOM                    (315)
#define PID_RED_CONDOM                      (316)
#define PID_COSMETIC_CASE                   (317)
#define PID_CYBERNETIC_BRAIN                (321)
#define PID_HUMAN_BRAIN                     (322)
#define PID_CHIMP_BRAIN                     (323)
#define PID_ABNORMAL_BRAIN                  (324)
#define PID_DICE                            (325)
#define PID_LOADED_DICE                     (326)
#define PID_EASTER_EGG                      (327)
#define PID_MAGIC_8_BALL                    (328)
#define PID_MOORE_BAD_BRIEFCASE             (335)
#define PID_MOORE_GOOD_BRIEFCASE            (336)
#define PID_LYNETTE_HOLO                    (337)
#define PID_WESTIN_HOLO                     (338)
#define PID_SPY_HOLO                        (339)
#define PID_DR_HENRY_PAPERS                 (340)
#define PID_PRESIDENTIAL_PASS               (341)
#define PID_RANGER_PIN                      (342)
#define PID_RANGER_MAP                      (343)
#define PID_COMPUTER_VOICE_MODULE           (356)
#define PID_GECK                            (366)
#define PID_V15_KEYCARD                     (373)
#define PID_V15_COMPUTER_PART               (377)
#define PID_COOKIE                          (378)
#define PID_OIL_CAN                         (412)
#define PID_STABLES_ID_BADGE                (413)
#define PID_VAULT_13_SHACK_KEY              (414)
#define PID_SPECTACLES                      (415)  // DO NOT USE THIS IN YOUR SCRIPTS, THIS IS SPECIAL CASE
#define PID_EMPTY_JET                       (416)  // DO NOT USE THIS IN YOUR SCRIPTS, THIS IS SPECIAL CASE
#define PID_OXYGEN_TANK                     (417)  // DO NOT USE THIS IN YOUR SCRIPTS, THIS IS SPECIAL CASE
#define PID_POISON_TANK                     (418)  // DO NOT USE THIS IN YOUR SCRIPTS, THIS IS SPECIAL CASE
#define PID_MINE_PART                       (419)  // DO NOT USE THIS IN YOUR SCRIPTS, THIS IS SPECIAL CASE
#define PID_MORNING_STAR_MINE               (420)
#define PID_EXCAVATOR_CHIP                  (422)
#define PID_GOLD_TOOTH                      (429)
#define PID_HOWITZER_SHELL                  (430)
#define PID_RAMIREZ_BOX_CLOSED              (431)
#define PID_RAMIREZ_BOX_OPEN                (432)
#define PID_MIRROR_SHADES                   (433)
#define PID_DECK_OF_CARDS                   (436)
#define PID_MARKED_DECK_OF_CARDS            (437)
#define PID_TEMPLE_KEY                      (438)
#define PID_POCKET_LINT                     (439)
#define PID_BIO_GEL                         (440)
#define PID_BLONDIE_DOG_TAG                 (441)
#define PID_ANGEL_EYES_DOG_TAG              (442)
#define PID_TUCO_DOG_TAG                    (443)
#define PID_RAIDERS_MAP                     (444)
#define PID_SHERIFF_BADGE                   (445)
#define PID_VERTIBIRD_PLANS                 (446)
#define PID_BISHOPS_HOLODISK                (447)
#define PID_ACCOUNT_BOOK                    (448)
#define PID_ECON_HOLODISK                   (449)
#define PID_TORN_PAPER_1                    (450)
#define PID_TORN_PAPER_2                    (451)
#define PID_TORN_PAPER_3                    (452)
#define PID_PASSWORD_PAPER                  (453)
#define PID_EXPLOSIVE_SWITCH                (454)
#define PID_CELL_DOOR_KEY                   (456)
#define PID_ELRON_FIELD_REP                 (457)
#define PID_ENCLAVE_HOLODISK_5              (458)
#define PID_ENCLAVE_HOLODISK_1              (459)
#define PID_ENCLAVE_HOLODISK_2              (460)
#define PID_ENCLAVE_HOLODISK_3              (461)
#define PID_ENCLAVE_HOLODISK_4              (462)
#define PID_EVACUATION_HOLODISK             (463)
#define PID_EXPERIMENT_HOLODISK             (464)
#define PID_MEDICAL_HOLODISK                (465)
#define PID_PASSWORD_HOLODISK               (466)
#define PID_SMITTY_MEAL                     (468)
#define PID_ROT_GUT                         (469)
#define PID_BALL_GAG                        (470)
#define PID_BECKY_BOOK                      (471)
#define PID_ELRON_MEMBER_HOLO               (472)
#define PID_MUTATED_TOE                     (473)
#define PID_DAISIES                         (474)
#define PID_ENLIGHTENED_ONE_LETTER          (476)
#define PID_BROADCAST_HOLODISK              (477)
#define PID_SIERRA_MISSION_HOLODISK         (478)
#define PID_NAV_COMPUTER_PARTS              (479)
#define PID_KITTY_SEX_DRUG_AGILITY          (480)  // + 1 agility for 1 hr
#define PID_KITTY_SEX_DRUG_INTELLIGENCE     (481)  // + 1 iq for 1 hr
#define PID_KITTY_SEX_DRUG_STRENGTH         (482)  // + 1 strength for 1 hr
#define PID_FALLOUT_2_HINTBOOK              (483)  // no touchy
#define PID_PLAYERS_EAR                     (484)
#define PID_MASTICATORS_EAR                 (485)
#define PID_MEMO_FROM_FRANCIS               (487)
#define PID_K9_MOTIVATOR                    (488)
#define PID_SPECIAL_BOXER_WEAPON            (489)
#define PID_NCR_HISTORY_HOLODISK            (490)
#define PID_MR_NIXON_DOLL                   (491)
#define PID_TANKER_FOB                      (492)
#define PID_ELRON_TEACH_HOLO                (493)
#define PID_KOKOWEEF_MINE_SCRIP             (494)
#define PID_PRES_ACCESS_KEY                 (495)
#define PID_DERMAL_PIP_BOY_DISK             (499)  // AGAIN, VERY SPECIAL, NOT FOR YOU
#define PID_MEM_CHIP_BLUE                   (503)
#define PID_MEM_CHIP_GREEN                  (504)
#define PID_MEM_CHIP_RED                    (505)
#define PID_MEM_CHIP_YELLOW                 (506)

/******************************************************************
***************       Active Items                  ***************
******************************************************************/

#define PID_ACTIVE_FLARE                    (205)
#define PID_ACTIVE_DYNAMITE                 (206)
#define PID_ACTIVE_GEIGER_COUNTER           (207)
#define PID_ACTIVE_MOTION_SENSOR            (208)
#define PID_ACTIVE_PLASTIC_EXPLOSIVE        (209)
#define PID_ACTIVE_STEALTH_BOY              (210)


/******************************************************************
***************       Flying Weapons                ***************
******************************************************************/

#define PID_FLYING_ROCKET                   (83886081)
#define PID_FLYING_PLASMA_BALL              (83886082)
#define PID_FLYING_KNIFE                    (83886086)
#define PID_FLYING_SPEAR                    (83886087)
#define PID_FLYING_LASER_BLAST              (83886089)
#define PID_FLYING_PLASMA_BLAST             (83886090)
#define PID_FLYING_ELECTRICITY_BOLT         (83886091)


#endif  // _ITEMPID_H_DEFINED
