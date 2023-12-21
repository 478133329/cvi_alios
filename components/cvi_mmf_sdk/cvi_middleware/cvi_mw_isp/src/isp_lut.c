/*
 * Copyright (C) Cvitek Co., Ltd. 2019-2021. All rights reserved.
 *
 * File Name: isp_lut.c
 * Description:
 *
 */

#include "isp_lut.h"

const CVI_U16 gau16ReverseSRGBLut[GAMMA_LUT_EXPAND_COUNT] = {
	    0,    0,    0,    0,    0,    0,    0,    1,    1,    1,
	    1,    1,    1,    1,    1,    1,    1,    1,    1,    1,
	    2,    2,    2,    2,    2,    2,    2,    2,    2,    2,
	    2,    2,    2,    3,    3,    3,    3,    3,    3,    3,
	    3,    3,    3,    3,    3,    3,    4,    4,    4,    4,
	    4,    4,    4,    4,    4,    4,    4,    4,    4,    5,
	    5,    5,    5,    5,    5,    5,    5,    5,    5,    5,
	    5,    5,    6,    6,    6,    6,    6,    6,    6,    6,
	    6,    6,    6,    6,    7,    7,    7,    7,    7,    7,
	    7,    7,    7,    7,    7,    7,    7,    8,    8,    8,
	    8,    8,    8,    8,    8,    8,    8,    8,    8,    8,
	    9,    9,    9,    9,    9,    9,    9,    9,    9,    9,
	    9,    9,    9,   10,   10,   10,   10,   10,   10,   10,
	   10,   10,   10,   10,   10,   10,   11,   11,   11,   11,
	   11,   11,   11,   11,   11,   11,   11,   11,   11,   12,
	   12,   12,   12,   12,   12,   12,   12,   12,   12,   12,
	   12,   12,   13,   13,   13,   13,   13,   13,   13,   13,
	   13,   13,   13,   13,   13,   14,   14,   14,   14,   14,
	   14,   14,   14,   14,   14,   14,   14,   15,   15,   15,
	   15,   15,   15,   15,   15,   15,   15,   15,   16,   16,
	   16,   16,   16,   16,   16,   16,   16,   16,   16,   17,
	   17,   17,   17,   17,   17,   17,   17,   17,   17,   17,
	   18,   18,   18,   18,   18,   18,   18,   18,   18,   18,
	   18,   19,   19,   19,   19,   19,   19,   19,   19,   19,
	   19,   20,   20,   20,   20,   20,   20,   20,   20,   20,
	   20,   21,   21,   21,   21,   21,   21,   21,   21,   21,
	   22,   22,   22,   22,   22,   22,   22,   22,   22,   23,
	   23,   23,   23,   23,   23,   23,   23,   23,   24,   24,
	   24,   24,   24,   24,   24,   24,   24,   25,   25,   25,
	   25,   25,   25,   25,   25,   25,   26,   26,   26,   26,
	   26,   26,   26,   26,   27,   27,   27,   27,   27,   27,
	   27,   27,   28,   28,   28,   28,   28,   28,   28,   28,
	   28,   29,   29,   29,   29,   29,   29,   29,   30,   30,
	   30,   30,   30,   30,   30,   30,   31,   31,   31,   31,
	   31,   31,   31,   31,   32,   32,   32,   32,   32,   32,
	   32,   33,   33,   33,   33,   33,   33,   33,   34,   34,
	   34,   34,   34,   34,   34,   34,   35,   35,   35,   35,
	   35,   35,   35,   36,   36,   36,   36,   36,   36,   36,
	   37,   37,   37,   37,   37,   37,   37,   38,   38,   38,
	   38,   38,   38,   39,   39,   39,   39,   39,   39,   39,
	   40,   40,   40,   40,   40,   40,   41,   41,   41,   41,
	   41,   41,   41,   42,   42,   42,   42,   42,   42,   43,
	   43,   43,   43,   43,   43,   43,   44,   44,   44,   44,
	   44,   44,   45,   45,   45,   45,   45,   45,   46,   46,
	   46,   46,   46,   46,   47,   47,   47,   47,   47,   47,
	   48,   48,   48,   48,   48,   48,   49,   49,   49,   49,
	   49,   49,   50,   50,   50,   50,   50,   51,   51,   51,
	   51,   51,   51,   52,   52,   52,   52,   52,   52,   53,
	   53,   53,   53,   53,   54,   54,   54,   54,   54,   54,
	   55,   55,   55,   55,   55,   56,   56,   56,   56,   56,
	   57,   57,   57,   57,   57,   57,   58,   58,   58,   58,
	   58,   59,   59,   59,   59,   59,   60,   60,   60,   60,
	   60,   61,   61,   61,   61,   61,   62,   62,   62,   62,
	   62,   62,   63,   63,   63,   63,   63,   64,   64,   64,
	   64,   64,   65,   65,   65,   65,   66,   66,   66,   66,
	   66,   67,   67,   67,   67,   67,   68,   68,   68,   68,
	   68,   69,   69,   69,   69,   69,   70,   70,   70,   70,
	   71,   71,   71,   71,   71,   72,   72,   72,   72,   72,
	   73,   73,   73,   73,   74,   74,   74,   74,   74,   75,
	   75,   75,   75,   75,   76,   76,   76,   76,   77,   77,
	   77,   77,   78,   78,   78,   78,   78,   79,   79,   79,
	   79,   80,   80,   80,   80,   80,   81,   81,   81,   81,
	   82,   82,   82,   82,   83,   83,   83,   83,   83,   84,
	   84,   84,   84,   85,   85,   85,   85,   86,   86,   86,
	   86,   87,   87,   87,   87,   88,   88,   88,   88,   88,
	   89,   89,   89,   89,   90,   90,   90,   90,   91,   91,
	   91,   91,   92,   92,   92,   92,   93,   93,   93,   93,
	   94,   94,   94,   94,   95,   95,   95,   95,   96,   96,
	   96,   96,   97,   97,   97,   97,   98,   98,   98,   99,
	   99,   99,   99,  100,  100,  100,  100,  101,  101,  101,
	  101,  102,  102,  102,  102,  103,  103,  103,  104,  104,
	  104,  104,  105,  105,  105,  105,  106,  106,  106,  106,
	  107,  107,  107,  108,  108,  108,  108,  109,  109,  109,
	  109,  110,  110,  110,  111,  111,  111,  111,  112,  112,
	  112,  113,  113,  113,  113,  114,  114,  114,  114,  115,
	  115,  115,  116,  116,  116,  116,  117,  117,  117,  118,
	  118,  118,  118,  119,  119,  119,  120,  120,  120,  121,
	  121,  121,  121,  122,  122,  122,  123,  123,  123,  123,
	  124,  124,  124,  125,  125,  125,  126,  126,  126,  126,
	  127,  127,  127,  128,  128,  128,  129,  129,  129,  129,
	  130,  130,  130,  131,  131,  131,  132,  132,  132,  132,
	  133,  133,  133,  134,  134,  134,  135,  135,  135,  136,
	  136,  136,  137,  137,  137,  137,  138,  138,  138,  139,
	  139,  139,  140,  140,  140,  141,  141,  141,  142,  142,
	  142,  143,  143,  143,  143,  144,  144,  144,  145,  145,
	  145,  146,  146,  146,  147,  147,  147,  148,  148,  148,
	  149,  149,  149,  150,  150,  150,  151,  151,  151,  152,
	  152,  152,  153,  153,  153,  154,  154,  154,  155,  155,
	  155,  156,  156,  156,  157,  157,  157,  158,  158,  158,
	  159,  159,  159,  160,  160,  160,  161,  161,  161,  162,
	  162,  163,  163,  163,  164,  164,  164,  165,  165,  165,
	  166,  166,  166,  167,  167,  167,  168,  168,  168,  169,
	  169,  170,  170,  170,  171,  171,  171,  172,  172,  172,
	  173,  173,  173,  174,  174,  175,  175,  175,  176,  176,
	  176,  177,  177,  177,  178,  178,  179,  179,  179,  180,
	  180,  180,  181,  181,  181,  182,  182,  183,  183,  183,
	  184,  184,  184,  185,  185,  186,  186,  186,  187,  187,
	  187,  188,  188,  189,  189,  189,  190,  190,  190,  191,
	  191,  192,  192,  192,  193,  193,  194,  194,  194,  195,
	  195,  195,  196,  196,  197,  197,  197,  198,  198,  199,
	  199,  199,  200,  200,  201,  201,  201,  202,  202,  202,
	  203,  203,  204,  204,  204,  205,  205,  206,  206,  206,
	  207,  207,  208,  208,  208,  209,  209,  210,  210,  210,
	  211,  211,  212,  212,  212,  213,  213,  214,  214,  214,
	  215,  215,  216,  216,  217,  217,  217,  218,  218,  219,
	  219,  219,  220,  220,  221,  221,  221,  222,  222,  223,
	  223,  224,  224,  224,  225,  225,  226,  226,  226,  227,
	  227,  228,  228,  229,  229,  229,  230,  230,  231,  231,
	  232,  232,  232,  233,  233,  234,  234,  235,  235,  235,
	  236,  236,  237,  237,  238,  238,  238,  239,  239,  240,
	  240,  241,  241,  241,  242,  242,  243,  243,  244,  244,
	  245,  245,  245,  246,  246,  247,  247,  248,  248,  249,
	  249,  249,  250,  250,  251,  251,  252,  252,  253,  253,
	  253,  254,  254,  255,  255,  256,  256,  257,  257,  257,
	  258,  258,  259,  259,  260,  260,  261,  261,  262,  262,
	  263,  263,  263,  264,  264,  265,  265,  266,  266,  267,
	  267,  268,  268,  268,  269,  269,  270,  270,  271,  271,
	  272,  272,  273,  273,  274,  274,  275,  275,  276,  276,
	  276,  277,  277,  278,  278,  279,  279,  280,  280,  281,
	  281,  282,  282,  283,  283,  284,  284,  285,  285,  286,
	  286,  286,  287,  287,  288,  288,  289,  289,  290,  290,
	  291,  291,  292,  292,  293,  293,  294,  294,  295,  295,
	  296,  296,  297,  297,  298,  298,  299,  299,  300,  300,
	  301,  301,  302,  302,  303,  303,  304,  304,  305,  305,
	  306,  306,  307,  307,  308,  308,  309,  309,  310,  310,
	  311,  311,  312,  312,  313,  313,  314,  314,  315,  315,
	  316,  316,  317,  317,  318,  318,  319,  319,  320,  320,
	  321,  321,  322,  322,  323,  323,  324,  324,  325,  326,
	  326,  327,  327,  328,  328,  329,  329,  330,  330,  331,
	  331,  332,  332,  333,  333,  334,  334,  335,  335,  336,
	  337,  337,  338,  338,  339,  339,  340,  340,  341,  341,
	  342,  342,  343,  343,  344,  345,  345,  346,  346,  347,
	  347,  348,  348,  349,  349,  350,  350,  351,  352,  352,
	  353,  353,  354,  354,  355,  355,  356,  356,  357,  358,
	  358,  359,  359,  360,  360,  361,  361,  362,  363,  363,
	  364,  364,  365,  365,  366,  366,  367,  368,  368,  369,
	  369,  370,  370,  371,  371,  372,  373,  373,  374,  374,
	  375,  375,  376,  377,  377,  378,  378,  379,  379,  380,
	  380,  381,  382,  382,  383,  383,  384,  384,  385,  386,
	  386,  387,  387,  388,  388,  389,  390,  390,  391,  391,
	  392,  393,  393,  394,  394,  395,  395,  396,  397,  397,
	  398,  398,  399,  400,  400,  401,  401,  402,  402,  403,
	  404,  404,  405,  405,  406,  407,  407,  408,  408,  409,
	  410,  410,  411,  411,  412,  413,  413,  414,  414,  415,
	  416,  416,  417,  417,  418,  419,  419,  420,  420,  421,
	  422,  422,  423,  423,  424,  425,  425,  426,  426,  427,
	  428,  428,  429,  429,  430,  431,  431,  432,  432,  433,
	  434,  434,  435,  436,  436,  437,  437,  438,  439,  439,
	  440,  440,  441,  442,  442,  443,  444,  444,  445,  445,
	  446,  447,  447,  448,  449,  449,  450,  450,  451,  452,
	  452,  453,  454,  454,  455,  455,  456,  457,  457,  458,
	  459,  459,  460,  461,  461,  462,  462,  463,  464,  464,
	  465,  466,  466,  467,  468,  468,  469,  470,  470,  471,
	  471,  472,  473,  473,  474,  475,  475,  476,  477,  477,
	  478,  479,  479,  480,  481,  481,  482,  482,  483,  484,
	  484,  485,  486,  486,  487,  488,  488,  489,  490,  490,
	  491,  492,  492,  493,  494,  494,  495,  496,  496,  497,
	  498,  498,  499,  500,  500,  501,  502,  502,  503,  504,
	  504,  505,  506,  506,  507,  508,  508,  509,  510,  510,
	  511,  512,  512,  513,  514,  514,  515,  516,  516,  517,
	  518,  519,  519,  520,  521,  521,  522,  523,  523,  524,
	  525,  525,  526,  527,  527,  528,  529,  529,  530,  531,
	  532,  532,  533,  534,  534,  535,  536,  536,  537,  538,
	  539,  539,  540,  541,  541,  542,  543,  543,  544,  545,
	  545,  546,  547,  548,  548,  549,  550,  550,  551,  552,
	  553,  553,  554,  555,  555,  556,  557,  558,  558,  559,
	  560,  560,  561,  562,  562,  563,  564,  565,  565,  566,
	  567,  568,  568,  569,  570,  570,  571,  572,  573,  573,
	  574,  575,  575,  576,  577,  578,  578,  579,  580,  581,
	  581,  582,  583,  583,  584,  585,  586,  586,  587,  588,
	  589,  589,  590,  591,  592,  592,  593,  594,  594,  595,
	  596,  597,  597,  598,  599,  600,  600,  601,  602,  603,
	  603,  604,  605,  606,  606,  607,  608,  609,  609,  610,
	  611,  612,  612,  613,  614,  615,  615,  616,  617,  618,
	  618,  619,  620,  621,  621,  622,  623,  624,  624,  625,
	  626,  627,  627,  628,  629,  630,  630,  631,  632,  633,
	  634,  634,  635,  636,  637,  637,  638,  639,  640,  640,
	  641,  642,  643,  644,  644,  645,  646,  647,  647,  648,
	  649,  650,  651,  651,  652,  653,  654,  654,  655,  656,
	  657,  658,  658,  659,  660,  661,  661,  662,  663,  664,
	  665,  665,  666,  667,  668,  669,  669,  670,  671,  672,
	  673,  673,  674,  675,  676,  676,  677,  678,  679,  680,
	  680,  681,  682,  683,  684,  684,  685,  686,  687,  688,
	  688,  689,  690,  691,  692,  693,  693,  694,  695,  696,
	  697,  697,  698,  699,  700,  701,  701,  702,  703,  704,
	  705,  706,  706,  707,  708,  709,  710,  710,  711,  712,
	  713,  714,  715,  715,  716,  717,  718,  719,  719,  720,
	  721,  722,  723,  724,  724,  725,  726,  727,  728,  729,
	  729,  730,  731,  732,  733,  734,  734,  735,  736,  737,
	  738,  739,  739,  740,  741,  742,  743,  744,  744,  745,
	  746,  747,  748,  749,  749,  750,  751,  752,  753,  754,
	  755,  755,  756,  757,  758,  759,  760,  761,  761,  762,
	  763,  764,  765,  766,  767,  767,  768,  769,  770,  771,
	  772,  773,  773,  774,  775,  776,  777,  778,  779,  779,
	  780,  781,  782,  783,  784,  785,  785,  786,  787,  788,
	  789,  790,  791,  792,  792,  793,  794,  795,  796,  797,
	  798,  799,  799,  800,  801,  802,  803,  804,  805,  806,
	  806,  807,  808,  809,  810,  811,  812,  813,  814,  814,
	  815,  816,  817,  818,  819,  820,  821,  822,  822,  823,
	  824,  825,  826,  827,  828,  829,  830,  830,  831,  832,
	  833,  834,  835,  836,  837,  838,  839,  839,  840,  841,
	  842,  843,  844,  845,  846,  847,  848,  849,  849,  850,
	  851,  852,  853,  854,  855,  856,  857,  858,  859,  859,
	  860,  861,  862,  863,  864,  865,  866,  867,  868,  869,
	  870,  870,  871,  872,  873,  874,  875,  876,  877,  878,
	  879,  880,  881,  882,  883,  883,  884,  885,  886,  887,
	  888,  889,  890,  891,  892,  893,  894,  895,  896,  897,
	  897,  898,  899,  900,  901,  902,  903,  904,  905,  906,
	  907,  908,  909,  910,  911,  912,  913,  914,  914,  915,
	  916,  917,  918,  919,  920,  921,  922,  923,  924,  925,
	  926,  927,  928,  929,  930,  931,  932,  933,  934,  935,
	  935,  936,  937,  938,  939,  940,  941,  942,  943,  944,
	  945,  946,  947,  948,  949,  950,  951,  952,  953,  954,
	  955,  956,  957,  958,  959,  960,  961,  962,  963,  964,
	  965,  966,  967,  968,  968,  969,  970,  971,  972,  973,
	  974,  975,  976,  977,  978,  979,  980,  981,  982,  983,
	  984,  985,  986,  987,  988,  989,  990,  991,  992,  993,
	  994,  995,  996,  997,  998,  999, 1000, 1001, 1002, 1003,
	 1004, 1005, 1006, 1007, 1008, 1009, 1010, 1011, 1012, 1013,
	 1014, 1015, 1016, 1017, 1018, 1019, 1020, 1021, 1022, 1023,
	 1024, 1025, 1026, 1027, 1028, 1029, 1030, 1031, 1032, 1033,
	 1035, 1036, 1037, 1038, 1039, 1040, 1041, 1042, 1043, 1044,
	 1045, 1046, 1047, 1048, 1049, 1050, 1051, 1052, 1053, 1054,
	 1055, 1056, 1057, 1058, 1059, 1060, 1061, 1062, 1063, 1064,
	 1065, 1066, 1067, 1068, 1070, 1071, 1072, 1073, 1074, 1075,
	 1076, 1077, 1078, 1079, 1080, 1081, 1082, 1083, 1084, 1085,
	 1086, 1087, 1088, 1089, 1090, 1091, 1093, 1094, 1095, 1096,
	 1097, 1098, 1099, 1100, 1101, 1102, 1103, 1104, 1105, 1106,
	 1107, 1108, 1109, 1110, 1112, 1113, 1114, 1115, 1116, 1117,
	 1118, 1119, 1120, 1121, 1122, 1123, 1124, 1125, 1127, 1128,
	 1129, 1130, 1131, 1132, 1133, 1134, 1135, 1136, 1137, 1138,
	 1139, 1140, 1142, 1143, 1144, 1145, 1146, 1147, 1148, 1149,
	 1150, 1151, 1152, 1153, 1155, 1156, 1157, 1158, 1159, 1160,
	 1161, 1162, 1163, 1164, 1165, 1167, 1168, 1169, 1170, 1171,
	 1172, 1173, 1174, 1175, 1176, 1178, 1179, 1180, 1181, 1182,
	 1183, 1184, 1185, 1186, 1187, 1189, 1190, 1191, 1192, 1193,
	 1194, 1195, 1196, 1197, 1199, 1200, 1201, 1202, 1203, 1204,
	 1205, 1206, 1207, 1209, 1210, 1211, 1212, 1213, 1214, 1215,
	 1216, 1217, 1219, 1220, 1221, 1222, 1223, 1224, 1225, 1226,
	 1228, 1229, 1230, 1231, 1232, 1233, 1234, 1236, 1237, 1238,
	 1239, 1240, 1241, 1242, 1243, 1245, 1246, 1247, 1248, 1249,
	 1250, 1251, 1253, 1254, 1255, 1256, 1257, 1258, 1259, 1261,
	 1262, 1263, 1264, 1265, 1266, 1267, 1269, 1270, 1271, 1272,
	 1273, 1274, 1275, 1277, 1278, 1279, 1280, 1281, 1282, 1284,
	 1285, 1286, 1287, 1288, 1289, 1290, 1292, 1293, 1294, 1295,
	 1296, 1297, 1299, 1300, 1301, 1302, 1303, 1304, 1306, 1307,
	 1308, 1309, 1310, 1311, 1313, 1314, 1315, 1316, 1317, 1318,
	 1320, 1321, 1322, 1323, 1324, 1326, 1327, 1328, 1329, 1330,
	 1331, 1333, 1334, 1335, 1336, 1337, 1339, 1340, 1341, 1342,
	 1343, 1344, 1346, 1347, 1348, 1349, 1350, 1352, 1353, 1354,
	 1355, 1356, 1358, 1359, 1360, 1361, 1362, 1364, 1365, 1366,
	 1367, 1368, 1370, 1371, 1372, 1373, 1374, 1376, 1377, 1378,
	 1379, 1380, 1382, 1383, 1384, 1385, 1386, 1388, 1389, 1390,
	 1391, 1392, 1394, 1395, 1396, 1397, 1399, 1400, 1401, 1402,
	 1403, 1405, 1406, 1407, 1408, 1410, 1411, 1412, 1413, 1414,
	 1416, 1417, 1418, 1419, 1421, 1422, 1423, 1424, 1425, 1427,
	 1428, 1429, 1430, 1432, 1433, 1434, 1435, 1437, 1438, 1439,
	 1440, 1441, 1443, 1444, 1445, 1446, 1448, 1449, 1450, 1451,
	 1453, 1454, 1455, 1456, 1458, 1459, 1460, 1461, 1463, 1464,
	 1465, 1466, 1468, 1469, 1470, 1471, 1473, 1474, 1475, 1476,
	 1478, 1479, 1480, 1481, 1483, 1484, 1485, 1486, 1488, 1489,
	 1490, 1491, 1493, 1494, 1495, 1497, 1498, 1499, 1500, 1502,
	 1503, 1504, 1505, 1507, 1508, 1509, 1510, 1512, 1513, 1514,
	 1516, 1517, 1518, 1519, 1521, 1522, 1523, 1525, 1526, 1527,
	 1528, 1530, 1531, 1532, 1533, 1535, 1536, 1537, 1539, 1540,
	 1541, 1542, 1544, 1545, 1546, 1548, 1549, 1550, 1551, 1553,
	 1554, 1555, 1557, 1558, 1559, 1561, 1562, 1563, 1564, 1566,
	 1567, 1568, 1570, 1571, 1572, 1574, 1575, 1576, 1577, 1579,
	 1580, 1581, 1583, 1584, 1585, 1587, 1588, 1589, 1591, 1592,
	 1593, 1594, 1596, 1597, 1598, 1600, 1601, 1602, 1604, 1605,
	 1606, 1608, 1609, 1610, 1612, 1613, 1614, 1616, 1617, 1618,
	 1619, 1621, 1622, 1623, 1625, 1626, 1627, 1629, 1630, 1631,
	 1633, 1634, 1635, 1637, 1638, 1639, 1641, 1642, 1643, 1645,
	 1646, 1647, 1649, 1650, 1651, 1653, 1654, 1655, 1657, 1658,
	 1659, 1661, 1662, 1664, 1665, 1666, 1668, 1669, 1670, 1672,
	 1673, 1674, 1676, 1677, 1678, 1680, 1681, 1682, 1684, 1685,
	 1686, 1688, 1689, 1691, 1692, 1693, 1695, 1696, 1697, 1699,
	 1700, 1701, 1703, 1704, 1706, 1707, 1708, 1710, 1711, 1712,
	 1714, 1715, 1716, 1718, 1719, 1721, 1722, 1723, 1725, 1726,
	 1727, 1729, 1730, 1732, 1733, 1734, 1736, 1737, 1738, 1740,
	 1741, 1743, 1744, 1745, 1747, 1748, 1750, 1751, 1752, 1754,
	 1755, 1756, 1758, 1759, 1761, 1762, 1763, 1765, 1766, 1768,
	 1769, 1770, 1772, 1773, 1775, 1776, 1777, 1779, 1780, 1782,
	 1783, 1784, 1786, 1787, 1789, 1790, 1791, 1793, 1794, 1796,
	 1797, 1798, 1800, 1801, 1803, 1804, 1805, 1807, 1808, 1810,
	 1811, 1813, 1814, 1815, 1817, 1818, 1820, 1821, 1822, 1824,
	 1825, 1827, 1828, 1830, 1831, 1832, 1834, 1835, 1837, 1838,
	 1840, 1841, 1842, 1844, 1845, 1847, 1848, 1850, 1851, 1852,
	 1854, 1855, 1857, 1858, 1860, 1861, 1862, 1864, 1865, 1867,
	 1868, 1870, 1871, 1872, 1874, 1875, 1877, 1878, 1880, 1881,
	 1883, 1884, 1885, 1887, 1888, 1890, 1891, 1893, 1894, 1896,
	 1897, 1899, 1900, 1901, 1903, 1904, 1906, 1907, 1909, 1910,
	 1912, 1913, 1915, 1916, 1917, 1919, 1920, 1922, 1923, 1925,
	 1926, 1928, 1929, 1931, 1932, 1934, 1935, 1937, 1938, 1939,
	 1941, 1942, 1944, 1945, 1947, 1948, 1950, 1951, 1953, 1954,
	 1956, 1957, 1959, 1960, 1962, 1963, 1965, 1966, 1968, 1969,
	 1970, 1972, 1973, 1975, 1976, 1978, 1979, 1981, 1982, 1984,
	 1985, 1987, 1988, 1990, 1991, 1993, 1994, 1996, 1997, 1999,
	 2000, 2002, 2003, 2005, 2006, 2008, 2009, 2011, 2012, 2014,
	 2015, 2017, 2018, 2020, 2021, 2023, 2024, 2026, 2027, 2029,
	 2030, 2032, 2033, 2035, 2036, 2038, 2039, 2041, 2043, 2044,
	 2046, 2047, 2049, 2050, 2052, 2053, 2055, 2056, 2058, 2059,
	 2061, 2062, 2064, 2065, 2067, 2068, 2070, 2071, 2073, 2074,
	 2076, 2078, 2079, 2081, 2082, 2084, 2085, 2087, 2088, 2090,
	 2091, 2093, 2094, 2096, 2098, 2099, 2101, 2102, 2104, 2105,
	 2107, 2108, 2110, 2111, 2113, 2114, 2116, 2118, 2119, 2121,
	 2122, 2124, 2125, 2127, 2128, 2130, 2132, 2133, 2135, 2136,
	 2138, 2139, 2141, 2142, 2144, 2146, 2147, 2149, 2150, 2152,
	 2153, 2155, 2157, 2158, 2160, 2161, 2163, 2164, 2166, 2167,
	 2169, 2171, 2172, 2174, 2175, 2177, 2178, 2180, 2182, 2183,
	 2185, 2186, 2188, 2190, 2191, 2193, 2194, 2196, 2197, 2199,
	 2201, 2202, 2204, 2205, 2207, 2209, 2210, 2212, 2213, 2215,
	 2216, 2218, 2220, 2221, 2223, 2224, 2226, 2228, 2229, 2231,
	 2232, 2234, 2236, 2237, 2239, 2240, 2242, 2244, 2245, 2247,
	 2248, 2250, 2252, 2253, 2255, 2256, 2258, 2260, 2261, 2263,
	 2264, 2266, 2268, 2269, 2271, 2273, 2274, 2276, 2277, 2279,
	 2281, 2282, 2284, 2285, 2287, 2289, 2290, 2292, 2294, 2295,
	 2297, 2298, 2300, 2302, 2303, 2305, 2307, 2308, 2310, 2311,
	 2313, 2315, 2316, 2318, 2320, 2321, 2323, 2324, 2326, 2328,
	 2329, 2331, 2333, 2334, 2336, 2338, 2339, 2341, 2343, 2344,
	 2346, 2347, 2349, 2351, 2352, 2354, 2356, 2357, 2359, 2361,
	 2362, 2364, 2366, 2367, 2369, 2371, 2372, 2374, 2376, 2377,
	 2379, 2380, 2382, 2384, 2385, 2387, 2389, 2390, 2392, 2394,
	 2395, 2397, 2399, 2400, 2402, 2404, 2405, 2407, 2409, 2410,
	 2412, 2414, 2415, 2417, 2419, 2420, 2422, 2424, 2425, 2427,
	 2429, 2431, 2432, 2434, 2436, 2437, 2439, 2441, 2442, 2444,
	 2446, 2447, 2449, 2451, 2452, 2454, 2456, 2457, 2459, 2461,
	 2463, 2464, 2466, 2468, 2469, 2471, 2473, 2474, 2476, 2478,
	 2479, 2481, 2483, 2485, 2486, 2488, 2490, 2491, 2493, 2495,
	 2496, 2498, 2500, 2502, 2503, 2505, 2507, 2508, 2510, 2512,
	 2514, 2515, 2517, 2519, 2520, 2522, 2524, 2526, 2527, 2529,
	 2531, 2532, 2534, 2536, 2538, 2539, 2541, 2543, 2544, 2546,
	 2548, 2550, 2551, 2553, 2555, 2557, 2558, 2560, 2562, 2563,
	 2565, 2567, 2569, 2570, 2572, 2574, 2576, 2577, 2579, 2581,
	 2583, 2584, 2586, 2588, 2590, 2591, 2593, 2595, 2596, 2598,
	 2600, 2602, 2603, 2605, 2607, 2609, 2610, 2612, 2614, 2616,
	 2617, 2619, 2621, 2623, 2624, 2626, 2628, 2630, 2631, 2633,
	 2635, 2637, 2639, 2640, 2642, 2644, 2646, 2647, 2649, 2651,
	 2653, 2654, 2656, 2658, 2660, 2661, 2663, 2665, 2667, 2669,
	 2670, 2672, 2674, 2676, 2677, 2679, 2681, 2683, 2685, 2686,
	 2688, 2690, 2692, 2693, 2695, 2697, 2699, 2701, 2702, 2704,
	 2706, 2708, 2709, 2711, 2713, 2715, 2717, 2718, 2720, 2722,
	 2724, 2726, 2727, 2729, 2731, 2733, 2735, 2736, 2738, 2740,
	 2742, 2744, 2745, 2747, 2749, 2751, 2753, 2754, 2756, 2758,
	 2760, 2762, 2763, 2765, 2767, 2769, 2771, 2772, 2774, 2776,
	 2778, 2780, 2782, 2783, 2785, 2787, 2789, 2791, 2792, 2794,
	 2796, 2798, 2800, 2802, 2803, 2805, 2807, 2809, 2811, 2813,
	 2814, 2816, 2818, 2820, 2822, 2823, 2825, 2827, 2829, 2831,
	 2833, 2834, 2836, 2838, 2840, 2842, 2844, 2846, 2847, 2849,
	 2851, 2853, 2855, 2857, 2858, 2860, 2862, 2864, 2866, 2868,
	 2869, 2871, 2873, 2875, 2877, 2879, 2881, 2882, 2884, 2886,
	 2888, 2890, 2892, 2894, 2895, 2897, 2899, 2901, 2903, 2905,
	 2907, 2908, 2910, 2912, 2914, 2916, 2918, 2920, 2922, 2923,
	 2925, 2927, 2929, 2931, 2933, 2935, 2936, 2938, 2940, 2942,
	 2944, 2946, 2948, 2950, 2952, 2953, 2955, 2957, 2959, 2961,
	 2963, 2965, 2967, 2968, 2970, 2972, 2974, 2976, 2978, 2980,
	 2982, 2984, 2985, 2987, 2989, 2991, 2993, 2995, 2997, 2999,
	 3001, 3003, 3004, 3006, 3008, 3010, 3012, 3014, 3016, 3018,
	 3020, 3022, 3023, 3025, 3027, 3029, 3031, 3033, 3035, 3037,
	 3039, 3041, 3043, 3044, 3046, 3048, 3050, 3052, 3054, 3056,
	 3058, 3060, 3062, 3064, 3066, 3067, 3069, 3071, 3073, 3075,
	 3077, 3079, 3081, 3083, 3085, 3087, 3089, 3091, 3092, 3094,
	 3096, 3098, 3100, 3102, 3104, 3106, 3108, 3110, 3112, 3114,
	 3116, 3118, 3120, 3122, 3123, 3125, 3127, 3129, 3131, 3133,
	 3135, 3137, 3139, 3141, 3143, 3145, 3147, 3149, 3151, 3153,
	 3155, 3157, 3159, 3161, 3162, 3164, 3166, 3168, 3170, 3172,
	 3174, 3176, 3178, 3180, 3182, 3184, 3186, 3188, 3190, 3192,
	 3194, 3196, 3198, 3200, 3202, 3204, 3206, 3208, 3210, 3212,
	 3214, 3216, 3218, 3220, 3221, 3223, 3225, 3227, 3229, 3231,
	 3233, 3235, 3237, 3239, 3241, 3243, 3245, 3247, 3249, 3251,
	 3253, 3255, 3257, 3259, 3261, 3263, 3265, 3267, 3269, 3271,
	 3273, 3275, 3277, 3279, 3281, 3283, 3285, 3287, 3289, 3291,
	 3293, 3295, 3297, 3299, 3301, 3303, 3305, 3307, 3309, 3311,
	 3313, 3315, 3317, 3319, 3321, 3323, 3325, 3327, 3329, 3331,
	 3333, 3335, 3337, 3339, 3341, 3343, 3345, 3347, 3350, 3352,
	 3354, 3356, 3358, 3360, 3362, 3364, 3366, 3368, 3370, 3372,
	 3374, 3376, 3378, 3380, 3382, 3384, 3386, 3388, 3390, 3392,
	 3394, 3396, 3398, 3400, 3402, 3404, 3406, 3408, 3411, 3413,
	 3415, 3417, 3419, 3421, 3423, 3425, 3427, 3429, 3431, 3433,
	 3435, 3437, 3439, 3441, 3443, 3445, 3447, 3450, 3452, 3454,
	 3456, 3458, 3460, 3462, 3464, 3466, 3468, 3470, 3472, 3474,
	 3476, 3478, 3480, 3483, 3485, 3487, 3489, 3491, 3493, 3495,
	 3497, 3499, 3501, 3503, 3505, 3507, 3510, 3512, 3514, 3516,
	 3518, 3520, 3522, 3524, 3526, 3528, 3530, 3532, 3535, 3537,
	 3539, 3541, 3543, 3545, 3547, 3549, 3551, 3553, 3555, 3558,
	 3560, 3562, 3564, 3566, 3568, 3570, 3572, 3574, 3576, 3579,
	 3581, 3583, 3585, 3587, 3589, 3591, 3593, 3595, 3597, 3600,
	 3602, 3604, 3606, 3608, 3610, 3612, 3614, 3616, 3619, 3621,
	 3623, 3625, 3627, 3629, 3631, 3633, 3636, 3638, 3640, 3642,
	 3644, 3646, 3648, 3650, 3653, 3655, 3657, 3659, 3661, 3663,
	 3665, 3667, 3670, 3672, 3674, 3676, 3678, 3680, 3682, 3685,
	 3687, 3689, 3691, 3693, 3695, 3697, 3700, 3702, 3704, 3706,
	 3708, 3710, 3712, 3715, 3717, 3719, 3721, 3723, 3725, 3727,
	 3730, 3732, 3734, 3736, 3738, 3740, 3743, 3745, 3747, 3749,
	 3751, 3753, 3756, 3758, 3760, 3762, 3764, 3766, 3769, 3771,
	 3773, 3775, 3777, 3779, 3782, 3784, 3786, 3788, 3790, 3792,
	 3795, 3797, 3799, 3801, 3803, 3805, 3808, 3810, 3812, 3814,
	 3816, 3819, 3821, 3823, 3825, 3827, 3829, 3832, 3834, 3836,
	 3838, 3840, 3843, 3845, 3847, 3849, 3851, 3854, 3856, 3858,
	 3860, 3862, 3865, 3867, 3869, 3871, 3873, 3876, 3878, 3880,
	 3882, 3884, 3887, 3889, 3891, 3893, 3895, 3898, 3900, 3902,
	 3904, 3907, 3909, 3911, 3913, 3915, 3918, 3920, 3922, 3924,
	 3926, 3929, 3931, 3933, 3935, 3938, 3940, 3942, 3944, 3946,
	 3949, 3951, 3953, 3955, 3958, 3960, 3962, 3964, 3967, 3969,
	 3971, 3973, 3975, 3978, 3980, 3982, 3984, 3987, 3989, 3991,
	 3993, 3996, 3998, 4000, 4002, 4005, 4007, 4009, 4011, 4014,
	 4016, 4018, 4020, 4023, 4025, 4027, 4029, 4032, 4034, 4036,
	 4038, 4041, 4043, 4045, 4047, 4050, 4052, 4054, 4056, 4059,
	 4061, 4063, 4065, 4068, 4070, 4072, 4075, 4077, 4079, 4081,
	 4084, 4086, 4088, 4090, 4093, 4095
};

