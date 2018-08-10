#ifndef LOCALE_H
#define LOCALE_H

enum script {
	SCRIPT_ADLAM,
	SCRIPT_CAUCASIAN_ALBANIAN,
	SCRIPT_AHOM,
	SCRIPT_ARABIC,
	SCRIPT_IMPERIAL_ARAMAIC,
	SCRIPT_ARMENIAN,
	SCRIPT_AVESTAN,
	SCRIPT_BALINESE,
	SCRIPT_BAMUM,
	SCRIPT_BASSA_VAH,
	SCRIPT_BATAK,
	SCRIPT_BENGALI,
	SCRIPT_BHAIKSUKI,
	SCRIPT_BOPOMOFO,
	SCRIPT_BRAHMI,
	SCRIPT_BRAILLE,
	SCRIPT_BUGINESE,
	SCRIPT_BUHID,
	SCRIPT_CHAKMA,
	SCRIPT_CANADIAN_ABORIGINAL,
	SCRIPT_CARIAN,
	SCRIPT_CHAM,
	SCRIPT_CHEROKEE,
	SCRIPT_COPTIC,
	SCRIPT_CYPRIOT,
	SCRIPT_CYRILLIC,
	SCRIPT_DEVANAGARI,
	SCRIPT_DESERET,
	SCRIPT_DUPLOYAN,
	SCRIPT_EGYPTIAN_HIEROGLYPHS,
	SCRIPT_ELBASAN,
	SCRIPT_ETHIOPIC,
	SCRIPT_GEORGIAN,
	SCRIPT_GLAGOLITIC,
	SCRIPT_MASARAM_GONDI,
	SCRIPT_GOTHIC,
	SCRIPT_GRANTHA,
	SCRIPT_GREEK,
	SCRIPT_GUJARATI,
	SCRIPT_GURMUKHI,
	SCRIPT_HANGUL,
	SCRIPT_HAN,
	SCRIPT_HANUNOO,
	SCRIPT_HATRAN,
	SCRIPT_HEBREW,
	SCRIPT_HIRAGANA,
	SCRIPT_ANATOLIAN_HIEROGLYPHS,
	SCRIPT_PAHAWH_HMONG,
	SCRIPT_KATAKANA_OR_HIRAGANA,
	SCRIPT_OLD_HUNGARIAN,
	SCRIPT_OLD_ITALIC,
	SCRIPT_JAVANESE,
	SCRIPT_KAYAH_LI,
	SCRIPT_KATAKANA,
	SCRIPT_KHAROSHTHI,
	SCRIPT_KHMER,
	SCRIPT_KHOJKI,
	SCRIPT_KANNADA,
	SCRIPT_KAITHI,
	SCRIPT_TAI_THAM,
	SCRIPT_LAO,
	SCRIPT_LATIN,
	SCRIPT_LEPCHA,
	SCRIPT_LIMBU,
	SCRIPT_LINEAR_A,
	SCRIPT_LINEAR_B,
	SCRIPT_LISU,
	SCRIPT_LYCIAN,
	SCRIPT_LYDIAN,
	SCRIPT_MAHAJANI,
	SCRIPT_MANDAIC,
	SCRIPT_MANICHAEAN,
	SCRIPT_MARCHEN,
	SCRIPT_MENDE_KIKAKUI,
	SCRIPT_MEROITIC_CURSIVE,
	SCRIPT_MEROITIC_HIEROGLYPHS,
	SCRIPT_MALAYALAM,
	SCRIPT_MODI,
	SCRIPT_MONGOLIAN,
	SCRIPT_MRO,
	SCRIPT_MEETEI_MAYEK,
	SCRIPT_MULTANI,
	SCRIPT_MYANMAR,
	SCRIPT_OLD_NORTH_ARABIAN,
	SCRIPT_NABATAEAN,
	SCRIPT_NEWA,
	SCRIPT_NKO,
	SCRIPT_NUSHU,
	SCRIPT_OGHAM,
	SCRIPT_OL_CHIKI,
	SCRIPT_OLD_TURKIC,
	SCRIPT_ORIYA,
	SCRIPT_OSAGE,
	SCRIPT_OSMANYA,
	SCRIPT_PALMYRENE,
	SCRIPT_PAU_CIN_HAU,
	SCRIPT_OLD_PERMIC,
	SCRIPT_PHAGS_PA,
	SCRIPT_INSCRIPTIONAL_PAHLAVI,
	SCRIPT_PSALTER_PAHLAVI,
	SCRIPT_PHOENICIAN,
	SCRIPT_MIAO,
	SCRIPT_INSCRIPTIONAL_PARTHIAN,
	SCRIPT_REJANG,
	SCRIPT_RUNIC,
	SCRIPT_SAMARITAN,
	SCRIPT_OLD_SOUTH_ARABIAN,
	SCRIPT_SAURASHTRA,
	SCRIPT_SIGNWRITING,
	SCRIPT_SHAVIAN,
	SCRIPT_SHARADA,
	SCRIPT_SIDDHAM,
	SCRIPT_KHUDAWADI,
	SCRIPT_SINHALA,
	SCRIPT_SORA_SOMPENG,
	SCRIPT_SOYOMBO,
	SCRIPT_SUNDANESE,
	SCRIPT_SYLOTI_NAGRI,
	SCRIPT_SYRIAC,
	SCRIPT_TAGBANWA,
	SCRIPT_TAKRI,
	SCRIPT_TAI_LE,
	SCRIPT_NEW_TAI_LUE,
	SCRIPT_TAMIL,
	SCRIPT_TANGUT,
	SCRIPT_TAI_VIET,
	SCRIPT_TELUGU,
	SCRIPT_TIFINAGH,
	SCRIPT_TAGALOG,
	SCRIPT_THAANA,
	SCRIPT_THAI,
	SCRIPT_TIBETAN,
	SCRIPT_TIRHUTA,
	SCRIPT_UGARITIC,
	SCRIPT_VAI,
	SCRIPT_WARANG_CITI,
	SCRIPT_OLD_PERSIAN,
	SCRIPT_CUNEIFORM,
	SCRIPT_YI,
	SCRIPT_ZANABAZAR_SQUARE,
	SCRIPT_INHERITED,
	SCRIPT_COMMON,
	SCRIPT_UNKNOWN,
};

enum language {
	LANG_AAR, /* Afar */
	LANG_ABK, /* Abkhazian */
	LANG_ACE, /* Achinese */
	LANG_ACH, /* Acoli */
	LANG_ADA, /* Adangme */
	LANG_ADY, /* Adyghe; Adygei */
	LANG_AFA, /* Afro-Asiatic languages */
	LANG_AFH, /* Afrihili */
	LANG_AFR, /* Afrikaans */
	LANG_AIN, /* Ainu */
	LANG_AKA, /* Akan */
	LANG_AKK, /* Akkadian */
	LANG_ALB, /* Albanian */
	LANG_ALE, /* Aleut */
	LANG_ALG, /* Algonquian languages */
	LANG_ALT, /* Southern Altai */
	LANG_AMH, /* Amharic */
	LANG_ANG, /* English, Old (ca.450-1100) */
	LANG_ANP, /* Angika */
	LANG_APA, /* Apache languages */
	LANG_ARA, /* Arabic */
	LANG_ARC, /* Official Aramaic (700-300 BCE); Imperial Aramaic (700-300 BCE) */
	LANG_ARG, /* Aragonese */
	LANG_ARM, /* Armenian */
	LANG_ARN, /* Mapudungun; Mapuche */
	LANG_ARP, /* Arapaho */
	LANG_ART, /* Artificial languages */
	LANG_ARW, /* Arawak */
	LANG_ASM, /* Assamese */
	LANG_AST, /* Asturian; Bable; Leonese; Asturleonese */
	LANG_ATH, /* Athapascan languages */
	LANG_AUS, /* Australian languages */
	LANG_AVA, /* Avaric */
	LANG_AVE, /* Avestan */
	LANG_AWA, /* Awadhi */
	LANG_AYM, /* Aymara */
	LANG_AZE, /* Azerbaijani */
	LANG_BAD, /* Banda languages */
	LANG_BAI, /* Bamileke languages */
	LANG_BAK, /* Bashkir */
	LANG_BAL, /* Baluchi */
	LANG_BAM, /* Bambara */
	LANG_BAN, /* Balinese */
	LANG_BAQ, /* Basque */
	LANG_BAS, /* Basa */
	LANG_BAT, /* Baltic languages */
	LANG_BEJ, /* Beja; Bedawiyet */
	LANG_BEL, /* Belarusian */
	LANG_BEM, /* Bemba */
	LANG_BEN, /* Bengali */
	LANG_BER, /* Berber languages */
	LANG_BHO, /* Bhojpuri */
	LANG_BIH, /* Bihari languages */
	LANG_BIK, /* Bikol */
	LANG_BIN, /* Bini; Edo */
	LANG_BIS, /* Bislama */
	LANG_BLA, /* Siksika */
	LANG_BNT, /* Bantu (Other) */
	LANG_BOS, /* Bosnian */
	LANG_BRA, /* Braj */
	LANG_BRE, /* Breton */
	LANG_BTK, /* Batak languages */
	LANG_BUA, /* Buriat */
	LANG_BUG, /* Buginese */
	LANG_BUL, /* Bulgarian */
	LANG_BUR, /* Burmese */
	LANG_BYN, /* Blin; Bilin */
	LANG_CAD, /* Caddo */
	LANG_CAI, /* Central American Indian languages */
	LANG_CAR, /* Galibi Carib */
	LANG_CAT, /* Catalan; Valencian */
	LANG_CAU, /* Caucasian languages */
	LANG_CEB, /* Cebuano */
	LANG_CEL, /* Celtic languages */
	LANG_CHA, /* Chamorro */
	LANG_CHB, /* Chibcha */
	LANG_CHE, /* Chechen */
	LANG_CHG, /* Chagatai */
	LANG_CHI, /* Chinese */
	LANG_CHK, /* Chuukese */
	LANG_CHM, /* Mari */
	LANG_CHN, /* Chinook jargon */
	LANG_CHO, /* Choctaw */
	LANG_CHP, /* Chipewyan; Dene Suline */
	LANG_CHR, /* Cherokee */
	LANG_CHU, /* Church Slavic; Old Slavonic; Church Slavonic; Old Bulgarian; Old Church Slavonic */
	LANG_CHV, /* Chuvash */
	LANG_CHY, /* Cheyenne */
	LANG_CMC, /* Chamic languages */
	LANG_CNR, /* Montenegrin */
	LANG_COP, /* Coptic */
	LANG_COR, /* Cornish */
	LANG_COS, /* Corsican */
	LANG_CPE, /* Creoles and pidgins, English based */
	LANG_CPF, /* Creoles and pidgins, French-based  */
	LANG_CPP, /* Creoles and pidgins, Portuguese-based  */
	LANG_CRE, /* Cree */
	LANG_CRH, /* Crimean Tatar; Crimean Turkish */
	LANG_CRP, /* Creoles and pidgins  */
	LANG_CSB, /* Kashubian */
	LANG_CUS, /* Cushitic languages */
	LANG_CZE, /* Czech */
	LANG_DAK, /* Dakota */
	LANG_DAN, /* Danish */
	LANG_DAR, /* Dargwa */
	LANG_DAY, /* Land Dayak languages */
	LANG_DEL, /* Delaware */
	LANG_DEN, /* Slave (Athapascan) */
	LANG_DGR, /* Dogrib */
	LANG_DIN, /* Dinka */
	LANG_DIV, /* Divehi; Dhivehi; Maldivian */
	LANG_DOI, /* Dogri */
	LANG_DRA, /* Dravidian languages */
	LANG_DSB, /* Lower Sorbian */
	LANG_DUA, /* Duala */
	LANG_DUM, /* Dutch, Middle (ca.1050-1350) */
	LANG_DUT, /* Dutch; Flemish */
	LANG_DYU, /* Dyula */
	LANG_DZO, /* Dzongkha */
	LANG_EFI, /* Efik */
	LANG_EGY, /* Egyptian (Ancient) */
	LANG_EKA, /* Ekajuk */
	LANG_ELX, /* Elamite */
	LANG_ENG, /* English */
	LANG_ENM, /* English, Middle (1100-1500) */
	LANG_EPO, /* Esperanto */
	LANG_EST, /* Estonian */
	LANG_EWE, /* Ewe */
	LANG_EWO, /* Ewondo */
	LANG_FAN, /* Fang */
	LANG_FAO, /* Faroese */
	LANG_FAT, /* Fanti */
	LANG_FIJ, /* Fijian */
	LANG_FIL, /* Filipino; Pilipino */
	LANG_FIN, /* Finnish */
	LANG_FIU, /* Finno-Ugrian languages */
	LANG_FON, /* Fon */
	LANG_FRE, /* French */
	LANG_FRM, /* French, Middle (ca.1400-1600) */
	LANG_FRO, /* French, Old (842-ca.1400) */
	LANG_FRR, /* Northern Frisian */
	LANG_FRS, /* Eastern Frisian */
	LANG_FRY, /* Western Frisian */
	LANG_FUL, /* Fulah */
	LANG_FUR, /* Friulian */
	LANG_GAA, /* Ga */
	LANG_GAY, /* Gayo */
	LANG_GBA, /* Gbaya */
	LANG_GEM, /* Germanic languages */
	LANG_GEO, /* Georgian */
	LANG_GER, /* German */
	LANG_GEZ, /* Geez */
	LANG_GIL, /* Gilbertese */
	LANG_GLA, /* Gaelic; Scottish Gaelic */
	LANG_GLE, /* Irish */
	LANG_GLG, /* Galician */
	LANG_GLV, /* Manx */
	LANG_GMH, /* German, Middle High (ca.1050-1500) */
	LANG_GOH, /* German, Old High (ca.750-1050) */
	LANG_GON, /* Gondi */
	LANG_GOR, /* Gorontalo */
	LANG_GOT, /* Gothic */
	LANG_GRB, /* Grebo */
	LANG_GRC, /* Greek, Ancient (to 1453) */
	LANG_GRE, /* Greek, Modern (1453-) */
	LANG_GRN, /* Guarani */
	LANG_GSW, /* Swiss German; Alemannic; Alsatian */
	LANG_GUJ, /* Gujarati */
	LANG_GWI, /* Gwich'in */
	LANG_HAI, /* Haida */
	LANG_HAT, /* Haitian; Haitian Creole */
	LANG_HAU, /* Hausa */
	LANG_HAW, /* Hawaiian */
	LANG_HEB, /* Hebrew */
	LANG_HER, /* Herero */
	LANG_HIL, /* Hiligaynon */
	LANG_HIM, /* Himachali languages; Western Pahari languages */
	LANG_HIN, /* Hindi */
	LANG_HIT, /* Hittite */
	LANG_HMN, /* Hmong; Mong */
	LANG_HMO, /* Hiri Motu */
	LANG_HRV, /* Croatian */
	LANG_HSB, /* Upper Sorbian */
	LANG_HUN, /* Hungarian */
	LANG_HUP, /* Hupa */
	LANG_IBA, /* Iban */
	LANG_IBO, /* Igbo */
	LANG_ICE, /* Icelandic */
	LANG_IDO, /* Ido */
	LANG_III, /* Sichuan Yi; Nuosu */
	LANG_IJO, /* Ijo languages */
	LANG_IKU, /* Inuktitut */
	LANG_ILE, /* Interlingue; Occidental */
	LANG_ILO, /* Iloko */
	LANG_INA, /* Interlingua (International Auxiliary Language Association) */
	LANG_INC, /* Indic languages */
	LANG_IND, /* Indonesian */
	LANG_INE, /* Indo-European languages */
	LANG_INH, /* Ingush */
	LANG_IPK, /* Inupiaq */
	LANG_IRA, /* Iranian languages */
	LANG_IRO, /* Iroquoian languages */
	LANG_ITA, /* Italian */
	LANG_JAV, /* Javanese */
	LANG_JBO, /* Lojban */
	LANG_JPN, /* Japanese */
	LANG_JPR, /* Judeo-Persian */
	LANG_JRB, /* Judeo-Arabic */
	LANG_KAA, /* Kara-Kalpak */
	LANG_KAB, /* Kabyle */
	LANG_KAC, /* Kachin; Jingpho */
	LANG_KAL, /* Kalaallisut; Greenlandic */
	LANG_KAM, /* Kamba */
	LANG_KAN, /* Kannada */
	LANG_KAR, /* Karen languages */
	LANG_KAS, /* Kashmiri */
	LANG_KAU, /* Kanuri */
	LANG_KAW, /* Kawi */
	LANG_KAZ, /* Kazakh */
	LANG_KBD, /* Kabardian */
	LANG_KHA, /* Khasi */
	LANG_KHI, /* Khoisan languages */
	LANG_KHM, /* Central Khmer */
	LANG_KHO, /* Khotanese; Sakan */
	LANG_KIK, /* Kikuyu; Gikuyu */
	LANG_KIN, /* Kinyarwanda */
	LANG_KIR, /* Kirghiz; Kyrgyz */
	LANG_KMB, /* Kimbundu */
	LANG_KOK, /* Konkani */
	LANG_KOM, /* Komi */
	LANG_KON, /* Kongo */
	LANG_KOR, /* Korean */
	LANG_KOS, /* Kosraean */
	LANG_KPE, /* Kpelle */
	LANG_KRC, /* Karachay-Balkar */
	LANG_KRL, /* Karelian */
	LANG_KRO, /* Kru languages */
	LANG_KRU, /* Kurukh */
	LANG_KUA, /* Kuanyama; Kwanyama */
	LANG_KUM, /* Kumyk */
	LANG_KUR, /* Kurdish */
	LANG_KUT, /* Kutenai */
	LANG_LAD, /* Ladino */
	LANG_LAH, /* Lahnda */
	LANG_LAM, /* Lamba */
	LANG_LAO, /* Lao */
	LANG_LAT, /* Latin */
	LANG_LAV, /* Latvian */
	LANG_LEZ, /* Lezghian */
	LANG_LIM, /* Limburgan; Limburger; Limburgish */
	LANG_LIN, /* Lingala */
	LANG_LIT, /* Lithuanian */
	LANG_LOL, /* Mongo */
	LANG_LOZ, /* Lozi */
	LANG_LTZ, /* Luxembourgish; Letzeburgesch */
	LANG_LUA, /* Luba-Lulua */
	LANG_LUB, /* Luba-Katanga */
	LANG_LUG, /* Ganda */
	LANG_LUI, /* Luiseno */
	LANG_LUN, /* Lunda */
	LANG_LUO, /* Luo (Kenya and Tanzania) */
	LANG_LUS, /* Lushai */
	LANG_MAC, /* Macedonian */
	LANG_MAD, /* Madurese */
	LANG_MAG, /* Magahi */
	LANG_MAH, /* Marshallese */
	LANG_MAI, /* Maithili */
	LANG_MAK, /* Makasar */
	LANG_MAL, /* Malayalam */
	LANG_MAN, /* Mandingo */
	LANG_MAO, /* Maori */
	LANG_MAP, /* Austronesian languages */
	LANG_MAR, /* Marathi */
	LANG_MAS, /* Masai */
	LANG_MAY, /* Malay */
	LANG_MDF, /* Moksha */
	LANG_MDR, /* Mandar */
	LANG_MEN, /* Mende */
	LANG_MGA, /* Irish, Middle (900-1200) */
	LANG_MIC, /* Mi'kmaq; Micmac */
	LANG_MIN, /* Minangkabau */
	LANG_MIS, /* Uncoded languages */
	LANG_MKH, /* Mon-Khmer languages */
	LANG_MLG, /* Malagasy */
	LANG_MLT, /* Maltese */
	LANG_MNC, /* Manchu */
	LANG_MNI, /* Manipuri */
	LANG_MNO, /* Manobo languages */
	LANG_MOH, /* Mohawk */
	LANG_MON, /* Mongolian */
	LANG_MOS, /* Mossi */
	LANG_MUL, /* Multiple languages */
	LANG_MUN, /* Munda languages */
	LANG_MUS, /* Creek */
	LANG_MWL, /* Mirandese */
	LANG_MWR, /* Marwari */
	LANG_MYN, /* Mayan languages */
	LANG_MYV, /* Erzya */
	LANG_NAH, /* Nahuatl languages */
	LANG_NAI, /* North American Indian languages */
	LANG_NAP, /* Neapolitan */
	LANG_NAU, /* Nauru */
	LANG_NAV, /* Navajo; Navaho */
	LANG_NBL, /* Ndebele, South; South Ndebele */
	LANG_NDE, /* Ndebele, North; North Ndebele */
	LANG_NDO, /* Ndonga */
	LANG_NDS, /* Low German; Low Saxon; German, Low; Saxon, Low */
	LANG_NEP, /* Nepali */
	LANG_NEW, /* Nepal Bhasa; Newari */
	LANG_NIA, /* Nias */
	LANG_NIC, /* Niger-Kordofanian languages */
	LANG_NIU, /* Niuean */
	LANG_NNO, /* Norwegian Nynorsk; Nynorsk, Norwegian */
	LANG_NOB, /* Bokmål, Norwegian; Norwegian Bokmål */
	LANG_NOG, /* Nogai */
	LANG_NON, /* Norse, Old */
	LANG_NOR, /* Norwegian */
	LANG_NQO, /* N'Ko */
	LANG_NSO, /* Pedi; Sepedi; Northern Sotho */
	LANG_NUB, /* Nubian languages */
	LANG_NWC, /* Classical Newari; Old Newari; Classical Nepal Bhasa */
	LANG_NYA, /* Chichewa; Chewa; Nyanja */
	LANG_NYM, /* Nyamwezi */
	LANG_NYN, /* Nyankole */
	LANG_NYO, /* Nyoro */
	LANG_NZI, /* Nzima */
	LANG_OCI, /* Occitan (post 1500); Provençal */
	LANG_OJI, /* Ojibwa */
	LANG_ORI, /* Oriya */
	LANG_ORM, /* Oromo */
	LANG_OSA, /* Osage */
	LANG_OSS, /* Ossetian; Ossetic */
	LANG_OTA, /* Turkish, Ottoman (1500-1928) */
	LANG_OTO, /* Otomian languages */
	LANG_PAA, /* Papuan languages */
	LANG_PAG, /* Pangasinan */
	LANG_PAL, /* Pahlavi */
	LANG_PAM, /* Pampanga; Kapampangan */
	LANG_PAN, /* Panjabi; Punjabi */
	LANG_PAP, /* Papiamento */
	LANG_PAU, /* Palauan */
	LANG_PEO, /* Persian, Old (ca.600-400 B.C.) */
	LANG_PER, /* Persian */
	LANG_PHI, /* Philippine languages */
	LANG_PHN, /* Phoenician */
	LANG_PLI, /* Pali */
	LANG_POL, /* Polish */
	LANG_PON, /* Pohnpeian */
	LANG_POR, /* Portuguese */
	LANG_PRA, /* Prakrit languages */
	LANG_PRO, /* Provençal, Old (to 1500) */
	LANG_PUS, /* Pushto; Pashto */
	LANG_QAA, /* Reserved for local use */
	LANG_QUE, /* Quechua */
	LANG_RAJ, /* Rajasthani */
	LANG_RAP, /* Rapanui */
	LANG_RAR, /* Rarotongan; Cook Islands Maori */
	LANG_ROA, /* Romance languages */
	LANG_ROH, /* Romansh */
	LANG_ROM, /* Romany */
	LANG_RUM, /* Romanian; Moldavian; Moldovan */
	LANG_RUN, /* Rundi */
	LANG_RUP, /* Aromanian; Arumanian; Macedo-Romanian */
	LANG_RUS, /* Russian */
	LANG_SAD, /* Sandawe */
	LANG_SAG, /* Sango */
	LANG_SAH, /* Yakut */
	LANG_SAI, /* South American Indian (Other) */
	LANG_SAL, /* Salishan languages */
	LANG_SAM, /* Samaritan Aramaic */
	LANG_SAN, /* Sanskrit */
	LANG_SAS, /* Sasak */
	LANG_SAT, /* Santali */
	LANG_SCN, /* Sicilian */
	LANG_SCO, /* Scots */
	LANG_SEL, /* Selkup */
	LANG_SEM, /* Semitic languages */
	LANG_SGA, /* Irish, Old (to 900) */
	LANG_SGN, /* Sign Languages */
	LANG_SHN, /* Shan */
	LANG_SID, /* Sidamo */
	LANG_SIN, /* Sinhala; Sinhalese */
	LANG_SIO, /* Siouan languages */
	LANG_SIT, /* Sino-Tibetan languages */
	LANG_SLA, /* Slavic languages */
	LANG_SLO, /* Slovak */
	LANG_SLV, /* Slovenian */
	LANG_SMA, /* Southern Sami */
	LANG_SME, /* Northern Sami */
	LANG_SMI, /* Sami languages */
	LANG_SMJ, /* Lule Sami */
	LANG_SMN, /* Inari Sami */
	LANG_SMO, /* Samoan */
	LANG_SMS, /* Skolt Sami */
	LANG_SNA, /* Shona */
	LANG_SND, /* Sindhi */
	LANG_SNK, /* Soninke */
	LANG_SOG, /* Sogdian */
	LANG_SOM, /* Somali */
	LANG_SON, /* Songhai languages */
	LANG_SOT, /* Sotho, Southern */
	LANG_SPA, /* Spanish; Castilian */
	LANG_SRD, /* Sardinian */
	LANG_SRN, /* Sranan Tongo */
	LANG_SRP, /* Serbian */
	LANG_SRR, /* Serer */
	LANG_SSA, /* Nilo-Saharan languages */
	LANG_SSW, /* Swati */
	LANG_SUK, /* Sukuma */
	LANG_SUN, /* Sundanese */
	LANG_SUS, /* Susu */
	LANG_SUX, /* Sumerian */
	LANG_SWA, /* Swahili */
	LANG_SWE, /* Swedish */
	LANG_SYC, /* Classical Syriac */
	LANG_SYR, /* Syriac */
	LANG_TAH, /* Tahitian */
	LANG_TAI, /* Tai languages */
	LANG_TAM, /* Tamil */
	LANG_TAT, /* Tatar */
	LANG_TEL, /* Telugu */
	LANG_TEM, /* Timne */
	LANG_TER, /* Tereno */
	LANG_TET, /* Tetum */
	LANG_TGK, /* Tajik */
	LANG_TGL, /* Tagalog */
	LANG_THA, /* Thai */
	LANG_TIB, /* Tibetan */
	LANG_TIG, /* Tigre */
	LANG_TIR, /* Tigrinya */
	LANG_TIV, /* Tiv */
	LANG_TKL, /* Tokelau */
	LANG_TLH, /* Klingon; tlhIngan-Hol */
	LANG_TLI, /* Tlingit */
	LANG_TMH, /* Tamashek */
	LANG_TOG, /* Tonga (Nyasa) */
	LANG_TON, /* Tonga (Tonga Islands) */
	LANG_TPI, /* Tok Pisin */
	LANG_TSI, /* Tsimshian */
	LANG_TSN, /* Tswana */
	LANG_TSO, /* Tsonga */
	LANG_TUK, /* Turkmen */
	LANG_TUM, /* Tumbuka */
	LANG_TUP, /* Tupi languages */
	LANG_TUR, /* Turkish */
	LANG_TUT, /* Altaic languages */
	LANG_TVL, /* Tuvalu */
	LANG_TWI, /* Twi */
	LANG_TYV, /* Tuvinian */
	LANG_UDM, /* Udmurt */
	LANG_UGA, /* Ugaritic */
	LANG_UIG, /* Uighur; Uyghur */
	LANG_UKR, /* Ukrainian */
	LANG_UMB, /* Umbundu */
	LANG_UND, /* Undetermined */
	LANG_URD, /* Urdu */
	LANG_UZB, /* Uzbek */
	LANG_VAI, /* Vai */
	LANG_VEN, /* Venda */
	LANG_VIE, /* Vietnamese */
	LANG_VOL, /* Volapük */
	LANG_VOT, /* Votic */
	LANG_WAK, /* Wakashan languages */
	LANG_WAL, /* Walamo */
	LANG_WAR, /* Waray */
	LANG_WAS, /* Washo */
	LANG_WEL, /* Welsh */
	LANG_WEN, /* Sorbian languages */
	LANG_WLN, /* Walloon */
	LANG_WOL, /* Wolof */
	LANG_XAL, /* Kalmyk; Oirat */
	LANG_XHO, /* Xhosa */
	LANG_YAO, /* Yao */
	LANG_YAP, /* Yapese */
	LANG_YID, /* Yiddish */
	LANG_YOR, /* Yoruba */
	LANG_YPK, /* Yupik languages */
	LANG_ZAP, /* Zapotec */
	LANG_ZBL, /* Blissymbols; Blissymbolics; Bliss */
	LANG_ZEN, /* Zenaga */
	LANG_ZGH, /* Standard Moroccan Tamazight */
	LANG_ZHA, /* Zhuang; Chuang */
	LANG_ZND, /* Zande languages */
	LANG_ZUL, /* Zulu */
	LANG_ZUN, /* Zuni */
	LANG_ZXX, /* No linguistic content; Not applicable */
	LANG_ZZA, /* Zaza; Dimili; Dimli; Kirdki; Kirmanjki; Zazaki */
};

enum region {
	REG_AF, /* Afghanistan */
	REG_AX, /* Åland Islands */
	REG_AL, /* Albania */
	REG_DZ, /* Algeria */
	REG_AS, /* American Samoa */
	REG_AD, /* Andorra */
	REG_AO, /* Angola */
	REG_AI, /* Anguilla */
	REG_AQ, /* Antarctica */
	REG_AG, /* Antigua and Barbuda */
	REG_AR, /* Argentina */
	REG_AM, /* Armenia */
	REG_AW, /* Aruba */
	REG_AU, /* Australia */
	REG_AT, /* Austria */
	REG_AZ, /* Azerbaijan */
	REG_BS, /* Bahamas (the) */
	REG_BH, /* Bahrain */
	REG_BD, /* Bangladesh */
	REG_BB, /* Barbados */
	REG_BY, /* Belarus */
	REG_BE, /* Belgium */
	REG_BZ, /* Belize */
	REG_BJ, /* Benin */
	REG_BM, /* Bermuda */
	REG_BT, /* Bhutan */
	REG_BO, /* Bolivia (Plurinational State of) */
	REG_BQ, /* Bonaire, Sint Eustatius and Saba */
	REG_BA, /* Bosnia and Herzegovina */
	REG_BW, /* Botswana */
	REG_BV, /* Bouvet Island */
	REG_BR, /* Brazil */
	REG_IO, /* British Indian Ocean Territory (the) */
	REG_BN, /* Brunei Darussalam */
	REG_BG, /* Bulgaria */
	REG_BF, /* Burkina Faso */
	REG_BI, /* Burundi */
	REG_CV, /* Cabo Verde */
	REG_KH, /* Cambodia */
	REG_CM, /* Cameroon */
	REG_CA, /* Canada */
	REG_KY, /* Cayman Islands (the) */
	REG_CF, /* Central African Republic (the) */
	REG_TD, /* Chad */
	REG_CL, /* Chile */
	REG_CN, /* China */
	REG_CX, /* Christmas Island */
	REG_CC, /* Cocos (Keeling) Islands (the) */
	REG_CO, /* Colombia */
	REG_KM, /* Comoros (the) */
	REG_CD, /* Congo (the Democratic Republic of the) */
	REG_CG, /* Congo (the) */
	REG_CK, /* Cook Islands (the) */
	REG_CR, /* Costa Rica */
	REG_CI, /* Côte d'Ivoire */
	REG_HR, /* Croatia */
	REG_CU, /* Cuba */
	REG_CW, /* Curaçao */
	REG_CY, /* Cyprus */
	REG_CZ, /* Czechia */
	REG_DK, /* Denmark */
	REG_DJ, /* Djibouti */
	REG_DM, /* Dominica */
	REG_DO, /* Dominican Republic (the) */
	REG_EC, /* Ecuador */
	REG_EG, /* Egypt */
	REG_SV, /* El Salvador */
	REG_GQ, /* Equatorial Guinea */
	REG_ER, /* Eritrea */
	REG_EE, /* Estonia */
	REG_SZ, /* Eswatini */
	REG_ET, /* Ethiopia */
	REG_FK, /* Falkland Islands (the) [Malvinas] */
	REG_FO, /* Faroe Islands (the) */
	REG_FJ, /* Fiji */
	REG_FI, /* Finland */
	REG_FR, /* France */
	REG_GF, /* French Guiana */
	REG_PF, /* French Polynesia */
	REG_TF, /* French Southern Territories (the) */
	REG_GA, /* Gabon */
	REG_GM, /* Gambia (the) */
	REG_GE, /* Georgia */
	REG_DE, /* Germany */
	REG_GH, /* Ghana */
	REG_GI, /* Gibraltar */
	REG_GR, /* Greece */
	REG_GL, /* Greenland */
	REG_GD, /* Grenada */
	REG_GP, /* Guadeloupe */
	REG_GU, /* Guam */
	REG_GT, /* Guatemala */
	REG_GG, /* Guernsey */
	REG_GN, /* Guinea */
	REG_GW, /* Guinea-Bissau */
	REG_GY, /* Guyana */
	REG_HT, /* Haiti */
	REG_HM, /* Heard Island and McDonald Islands */
	REG_VA, /* Holy See (the) */
	REG_HN, /* Honduras */
	REG_HK, /* Hong Kong */
	REG_HU, /* Hungary */
	REG_IS, /* Iceland */
	REG_IN, /* India */
	REG_ID, /* Indonesia */
	REG_IR, /* Iran (Islamic Republic of) */
	REG_IQ, /* Iraq */
	REG_IE, /* Ireland */
	REG_IM, /* Isle of Man */
	REG_IL, /* Israel */
	REG_IT, /* Italy */
	REG_JM, /* Jamaica */
	REG_JP, /* Japan */
	REG_JE, /* Jersey */
	REG_JO, /* Jordan */
	REG_KZ, /* Kazakhstan */
	REG_KE, /* Kenya */
	REG_KI, /* Kiribati */
	REG_KP, /* Korea (the Democratic People's Republic of) */
	REG_KR, /* Korea (the Republic of) */
	REG_KW, /* Kuwait */
	REG_KG, /* Kyrgyzstan */
	REG_LA, /* Lao People's Democratic Republic (the) */
	REG_LV, /* Latvia */
	REG_LB, /* Lebanon */
	REG_LS, /* Lesotho */
	REG_LR, /* Liberia */
	REG_LY, /* Libya */
	REG_LI, /* Liechtenstein */
	REG_LT, /* Lithuania */
	REG_LU, /* Luxembourg */
	REG_MO, /* Macao */
	REG_MK, /* Macedonia (the former Yugoslav Republic of) */
	REG_MG, /* Madagascar */
	REG_MW, /* Malawi */
	REG_MY, /* Malaysia */
	REG_MV, /* Maldives */
	REG_ML, /* Mali */
	REG_MT, /* Malta */
	REG_MH, /* Marshall Islands (the) */
	REG_MQ, /* Martinique */
	REG_MR, /* Mauritania */
	REG_MU, /* Mauritius */
	REG_YT, /* Mayotte */
	REG_MX, /* Mexico */
	REG_FM, /* Micronesia (Federated States of) */
	REG_MD, /* Moldova (the Republic of) */
	REG_MC, /* Monaco */
	REG_MN, /* Mongolia */
	REG_ME, /* Montenegro */
	REG_MS, /* Montserrat */
	REG_MA, /* Morocco */
	REG_MZ, /* Mozambique */
	REG_MM, /* Myanmar */
	REG_NA, /* Namibia */
	REG_NR, /* Nauru */
	REG_NP, /* Nepal */
	REG_NL, /* Netherlands (the) */
	REG_NC, /* New Caledonia */
	REG_NZ, /* New Zealand */
	REG_NI, /* Nicaragua */
	REG_NE, /* Niger (the) */
	REG_NG, /* Nigeria */
	REG_NU, /* Niue */
	REG_NF, /* Norfolk Island */
	REG_MP, /* Northern Mariana Islands (the) */
	REG_NO, /* Norway */
	REG_OM, /* Oman */
	REG_PK, /* Pakistan */
	REG_PW, /* Palau */
	REG_PS, /* Palestine, State of */
	REG_PA, /* Panama */
	REG_PG, /* Papua New Guinea */
	REG_PY, /* Paraguay */
	REG_PE, /* Peru */
	REG_PH, /* Philippines (the) */
	REG_PN, /* Pitcairn */
	REG_PL, /* Poland */
	REG_PT, /* Portugal */
	REG_PR, /* Puerto Rico */
	REG_QA, /* Qatar */
	REG_RE, /* Réunion */
	REG_RO, /* Romania */
	REG_RU, /* Russian Federation (the) */
	REG_RW, /* Rwanda */
	REG_BL, /* Saint Barthélemy */
	REG_SH, /* Saint Helena, Ascension and Tristan da Cunha */
	REG_KN, /* Saint Kitts and Nevis */
	REG_LC, /* Saint Lucia */
	REG_MF, /* Saint Martin (French part) */
	REG_PM, /* Saint Pierre and Miquelon */
	REG_VC, /* Saint Vincent and the Grenadines */
	REG_WS, /* Samoa */
	REG_SM, /* San Marino */
	REG_ST, /* Sao Tome and Principe */
	REG_SA, /* Saudi Arabia */
	REG_SN, /* Senegal */
	REG_RS, /* Serbia */
	REG_SC, /* Seychelles */
	REG_SL, /* Sierra Leone */
	REG_SG, /* Singapore */
	REG_SX, /* Sint Maarten (Dutch part) */
	REG_SK, /* Slovakia */
	REG_SI, /* Slovenia */
	REG_SB, /* Solomon Islands */
	REG_SO, /* Somalia */
	REG_ZA, /* South Africa */
	REG_GS, /* South Georgia and the South Sandwich Islands */
	REG_SS, /* South Sudan */
	REG_ES, /* Spain */
	REG_LK, /* Sri Lanka */
	REG_SD, /* Sudan (the) */
	REG_SR, /* Suriname */
	REG_SJ, /* Svalbard and Jan Mayen */
	REG_SE, /* Sweden */
	REG_CH, /* Switzerland */
	REG_SY, /* Syrian Arab Republic */
	REG_TW, /* Taiwan (Province of China) */
	REG_TJ, /* Tajikistan */
	REG_TZ, /* Tanzania, United Republic of */
	REG_TH, /* Thailand */
	REG_TL, /* Timor-Leste */
	REG_TG, /* Togo */
	REG_TK, /* Tokelau */
	REG_TO, /* Tonga */
	REG_TT, /* Trinidad and Tobago */
	REG_TN, /* Tunisia */
	REG_TR, /* Turkey */
	REG_TM, /* Turkmenistan */
	REG_TC, /* Turks and Caicos Islands (the) */
	REG_TV, /* Tuvalu */
	REG_UG, /* Uganda */
	REG_UA, /* Ukraine */
	REG_AE, /* United Arab Emirates (the) */
	REG_GB, /* United Kingdom of Great Britain and Northern Ireland (the) */
	REG_UM, /* United States Minor Outlying Islands (the) */
	REG_US, /* United States of America (the) */
	REG_UY, /* Uruguay */
	REG_UZ, /* Uzbekistan */
	REG_VU, /* Vanuatu */
	REG_VE, /* Venezuela (Bolivarian Republic of) */
	REG_VN, /* Viet Nam */
	REG_VG, /* Virgin Islands (British) */
	REG_VI, /* Virgin Islands (U.S.) */
	REG_WF, /* Wallis and Futuna */
	REG_EH, /* Western Sahara* */
	REG_YE, /* Yemen */
	REG_ZM, /* Zambia */
	REG_ZW, /* Zimbabwe */
};

struct locale {
       enum language lang;
       enum script script;
       enum region reg;
};

struct locale parse_locale(const char *locale);

#endif
