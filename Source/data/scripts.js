var timerId;
var timeout=1000;
var active;
var address = -1;
window.onfocus = function() { active = true; clearTimeout(timerId); timeout=500; GetStatus(); };
window.onblur = function() { active = false; clearTimeout(timerId); };

const tzs=[-1000,"",-660,"UTC-11:00",-630,"UTC-10:30",-600,"UTC-10:00",-570,"UTC-9:30",-540,"UTC-9:00",-510,"UTC-8:30",-480,"UTC-8:00",-450,"UTC-7:30",
	-420,"UTC-7:00",-390,"UTC-6:30",-360,"UTC-6:00",-330,"UTC-5:30",-300,"UTC-5:00",-270,"UTC-4:30",-240,"UTC-4:00",-210,"UTC-3:30",-180,"UTC-3:00",
	-150,"UTC-2:30",-120,"UTC-2:00",-90,"UTC-1:30",-60,"UTC-1:00",-30,"UTC-0:30",0,"UTC+0:00",30,"UTC+0:30",60,"UTC+1:00",90,"UTC+1:30",120,"UTC+2:00",
	150,"UTC+2:30",180,"UTC+3:00",210,"UTC+3:30",240,"UTC+4:00",270,"UTC+4:30",300,"UTC+5:00",330,"UTC+5:30",360,"UTC+6:00",390,"UTC+6:30",
	420,"UTC+7:00",450,"UTC+7:30",480,"UTC+8:00",510,"UTC+8:30",540,"UTC+9:00",570,"UTC+9:30",600,"UTC+10:00",630,"UTC+10:30",660,"UTC+11:00",
	690,"UTC+11:30",720,"UTC+12:00",750,"UTC+12:30",780,"UTC+13:00",810,"UTC+13:30",840,"UTC+14:00"];

const citylist=[
[0,"",""],
[1,"Africa/Abidjan","GMT0"],
[2,"Africa/Accra","GMT0"],
[3,"Africa/Addis_Ababa","EAT-3"],
[4,"Africa/Algiers","CET-1"],
[5,"Africa/Asmara","EAT-3"],
[6,"Africa/Bamako","GMT0"],
[7,"Africa/Bangui","WAT-1"],
[8,"Africa/Banjul","GMT0"],
[9,"Africa/Bissau","GMT0"],
[10,"Africa/Blantyre","CAT-2"],
[11,"Africa/Brazzaville","WAT-1"],
[12,"Africa/Bujumbura","CAT-2"],
[13,"Africa/Cairo","EET-2EEST,M4.5.5/0,M10.5.4/24"],
[14,"Africa/Casablanca","<+01>-1"],
[15,"Africa/Ceuta","CET-1CEST,M3.5.0,M10.5.0/3"],
[16,"Africa/Conakry","GMT0"],
[17,"Africa/Dakar","GMT0"],
[18,"Africa/Dar_es_Salaam","EAT-3"],
[19,"Africa/Djibouti","EAT-3"],
[20,"Africa/Douala","WAT-1"],
[21,"Africa/El_Aaiun","<+01>-1"],
[22,"Africa/Freetown","GMT0"],
[23,"Africa/Gaborone","CAT-2"],
[24,"Africa/Harare","CAT-2"],
[25,"Africa/Johannesburg","SAST-2"],
[26,"Africa/Juba","CAT-2"],
[27,"Africa/Kampala","EAT-3"],
[28,"Africa/Khartoum","CAT-2"],
[29,"Africa/Kigali","CAT-2"],
[30,"Africa/Kinshasa","WAT-1"],
[31,"Africa/Lagos","WAT-1"],
[32,"Africa/Libreville","WAT-1"],
[33,"Africa/Lome","GMT0"],
[34,"Africa/Luanda","WAT-1"],
[35,"Africa/Lubumbashi","CAT-2"],
[36,"Africa/Lusaka","CAT-2"],
[37,"Africa/Malabo","WAT-1"],
[38,"Africa/Maputo","CAT-2"],
[39,"Africa/Maseru","SAST-2"],
[40,"Africa/Mbabane","SAST-2"],
[41,"Africa/Mogadishu","EAT-3"],
[42,"Africa/Monrovia","GMT0"],
[43,"Africa/Nairobi","EAT-3"],
[44,"Africa/Ndjamena","WAT-1"],
[45,"Africa/Niamey","WAT-1"],
[46,"Africa/Nouakchott","GMT0"],
[47,"Africa/Ouagadougou","GMT0"],
[48,"Africa/Porto-Novo","WAT-1"],
[49,"Africa/Sao_Tome","GMT0"],
[50,"Africa/Tripoli","EET-2"],
[51,"Africa/Tunis","CET-1"],
[52,"Africa/Windhoek","CAT-2"],
[53,"America/Adak","HST10HDT,M3.2.0,M11.1.0"],
[54,"America/Anchorage","AKST9AKDT,M3.2.0,M11.1.0"],
[55,"America/Anguilla","AST4"],
[56,"America/Antigua","AST4"],
[57,"America/Araguaina","<-03>3"],
[58,"America/Argentina/Buenos_Aires","<-03>3"],
[59,"America/Argentina/Catamarca","<-03>3"],
[60,"America/Argentina/Cordoba","<-03>3"],
[61,"America/Argentina/Jujuy","<-03>3"],
[62,"America/Argentina/La_Rioja","<-03>3"],
[63,"America/Argentina/Mendoza","<-03>3"],
[64,"America/Argentina/Rio_Gallegos","<-03>3"],
[65,"America/Argentina/Salta","<-03>3"],
[66,"America/Argentina/San_Juan","<-03>3"],
[67,"America/Argentina/San_Luis","<-03>3"],
[68,"America/Argentina/Tucuman","<-03>3"],
[69,"America/Argentina/Ushuaia","<-03>3"],
[70,"America/Aruba","AST4"],
[71,"America/Asuncion","<-03>3"],
[72,"America/Atikokan","EST5"],
[73,"America/Bahia","<-03>3"],
[74,"America/Bahia_Banderas","CST6"],
[75,"America/Barbados","AST4"],
[76,"America/Belem","<-03>3"],
[77,"America/Belize","CST6"],
[78,"America/Blanc-Sablon","AST4"],
[79,"America/Boa_Vista","<-04>4"],
[80,"America/Bogota","<-05>5"],
[81,"America/Boise","MST7MDT,M3.2.0,M11.1.0"],
[82,"America/Cambridge_Bay","MST7MDT,M3.2.0,M11.1.0"],
[83,"America/Campo_Grande","<-04>4"],
[84,"America/Cancun","EST5"],
[85,"America/Caracas","<-04>4"],
[86,"America/Cayenne","<-03>3"],
[87,"America/Cayman","EST5"],
[88,"America/Chicago","CST6CDT,M3.2.0,M11.1.0"],
[89,"America/Chihuahua","CST6"],
[90,"America/Costa_Rica","CST6"],
[91,"America/Creston","MST7"],
[92,"America/Cuiaba","<-04>4"],
[93,"America/Curacao","AST4"],
[94,"America/Danmarkshavn","GMT0"],
[95,"America/Dawson","MST7"],
[96,"America/Dawson_Creek","MST7"],
[97,"America/Denver","MST7MDT,M3.2.0,M11.1.0"],
[98,"America/Detroit","EST5EDT,M3.2.0,M11.1.0"],
[99,"America/Dominica","AST4"],
[100,"America/Edmonton","MST7MDT,M3.2.0,M11.1.0"],
[101,"America/Eirunepe","<-05>5"],
[102,"America/El_Salvador","CST6"],
[103,"America/Fortaleza","<-03>3"],
[104,"America/Fort_Nelson","MST7"],
[105,"America/Glace_Bay","AST4ADT,M3.2.0,M11.1.0"],
[106,"America/Godthab","<-02>2<-01>,M3.5.0/-1,M10.5.0/0"],
[107,"America/Goose_Bay","AST4ADT,M3.2.0,M11.1.0"],
[108,"America/Grand_Turk","EST5EDT,M3.2.0,M11.1.0"],
[109,"America/Grenada","AST4"],
[110,"America/Guadeloupe","AST4"],
[111,"America/Guatemala","CST6"],
[112,"America/Guayaquil","<-05>5"],
[113,"America/Guyana","<-04>4"],
[114,"America/Halifax","AST4ADT,M3.2.0,M11.1.0"],
[115,"America/Havana","CST5CDT,M3.2.0/0,M11.1.0/1"],
[116,"America/Hermosillo","MST7"],
[117,"America/Indiana/Indianapolis","EST5EDT,M3.2.0,M11.1.0"],
[118,"America/Indiana/Knox","CST6CDT,M3.2.0,M11.1.0"],
[119,"America/Indiana/Marengo","EST5EDT,M3.2.0,M11.1.0"],
[120,"America/Indiana/Petersburg","EST5EDT,M3.2.0,M11.1.0"],
[121,"America/Indiana/Tell_City","CST6CDT,M3.2.0,M11.1.0"],
[122,"America/Indiana/Vevay","EST5EDT,M3.2.0,M11.1.0"],
[123,"America/Indiana/Vincennes","EST5EDT,M3.2.0,M11.1.0"],
[124,"America/Indiana/Winamac","EST5EDT,M3.2.0,M11.1.0"],
[125,"America/Inuvik","MST7MDT,M3.2.0,M11.1.0"],
[126,"America/Iqaluit","EST5EDT,M3.2.0,M11.1.0"],
[127,"America/Jamaica","EST5"],
[128,"America/Juneau","AKST9AKDT,M3.2.0,M11.1.0"],
[129,"America/Kentucky/Louisville","EST5EDT,M3.2.0,M11.1.0"],
[130,"America/Kentucky/Monticello","EST5EDT,M3.2.0,M11.1.0"],
[131,"America/Kralendijk","AST4"],
[132,"America/La_Paz","<-04>4"],
[133,"America/Lima","<-05>5"],
[134,"America/Los_Angeles","PST8PDT,M3.2.0,M11.1.0"],
[135,"America/Lower_Princes","AST4"],
[136,"America/Maceio","<-03>3"],
[137,"America/Managua","CST6"],
[138,"America/Manaus","<-04>4"],
[139,"America/Marigot","AST4"],
[140,"America/Martinique","AST4"],
[141,"America/Matamoros","CST6CDT,M3.2.0,M11.1.0"],
[142,"America/Mazatlan","MST7"],
[143,"America/Menominee","CST6CDT,M3.2.0,M11.1.0"],
[144,"America/Merida","CST6"],
[145,"America/Metlakatla","AKST9AKDT,M3.2.0,M11.1.0"],
[146,"America/Mexico_City","CST6"],
[147,"America/Miquelon","<-03>3<-02>,M3.2.0,M11.1.0"],
[148,"America/Moncton","AST4ADT,M3.2.0,M11.1.0"],
[149,"America/Monterrey","CST6"],
[150,"America/Montevideo","<-03>3"],
[151,"America/Montreal","EST5EDT,M3.2.0,M11.1.0"],
[152,"America/Montserrat","AST4"],
[153,"America/Nassau","EST5EDT,M3.2.0,M11.1.0"],
[154,"America/New_York","EST5EDT,M3.2.0,M11.1.0"],
[155,"America/Nipigon","EST5EDT,M3.2.0,M11.1.0"],
[156,"America/Nome","AKST9AKDT,M3.2.0,M11.1.0"],
[157,"America/Noronha","<-02>2"],
[158,"America/North_Dakota/Beulah","CST6CDT,M3.2.0,M11.1.0"],
[159,"America/North_Dakota/Center","CST6CDT,M3.2.0,M11.1.0"],
[160,"America/North_Dakota/New_Salem","CST6CDT,M3.2.0,M11.1.0"],
[161,"America/Nuuk","<-02>2<-01>,M3.5.0/-1,M10.5.0/0"],
[162,"America/Ojinaga","CST6CDT,M3.2.0,M11.1.0"],
[163,"America/Panama","EST5"],
[164,"America/Pangnirtung","EST5EDT,M3.2.0,M11.1.0"],
[165,"America/Paramaribo","<-03>3"],
[166,"America/Phoenix","MST7"],
[167,"America/Port-au-Prince","EST5EDT,M3.2.0,M11.1.0"],
[168,"America/Port_of_Spain","AST4"],
[169,"America/Porto_Velho","<-04>4"],
[170,"America/Puerto_Rico","AST4"],
[171,"America/Punta_Arenas","<-03>3"],
[172,"America/Rainy_River","CST6CDT,M3.2.0,M11.1.0"],
[173,"America/Rankin_Inlet","CST6CDT,M3.2.0,M11.1.0"],
[174,"America/Recife","<-03>3"],
[175,"America/Regina","CST6"],
[176,"America/Resolute","CST6CDT,M3.2.0,M11.1.0"],
[177,"America/Rio_Branco","<-05>5"],
[178,"America/Santarem","<-03>3"],
[179,"America/Santiago","<-04>4<-03>,M9.1.6/24,M4.1.6/24"],
[180,"America/Santo_Domingo","AST4"],
[181,"America/Sao_Paulo","<-03>3"],
[182,"America/Scoresbysund","<-02>2<-01>,M3.5.0/-1,M10.5.0/0"],
[183,"America/Sitka","AKST9AKDT,M3.2.0,M11.1.0"],
[184,"America/St_Barthelemy","AST4"],
[185,"America/St_Johns","NST3:30NDT,M3.2.0,M11.1.0"],
[186,"America/St_Kitts","AST4"],
[187,"America/St_Lucia","AST4"],
[188,"America/St_Thomas","AST4"],
[189,"America/St_Vincent","AST4"],
[190,"America/Swift_Current","CST6"],
[191,"America/Tegucigalpa","CST6"],
[192,"America/Thule","AST4ADT,M3.2.0,M11.1.0"],
[193,"America/Thunder_Bay","EST5EDT,M3.2.0,M11.1.0"],
[194,"America/Tijuana","PST8PDT,M3.2.0,M11.1.0"],
[195,"America/Toronto","EST5EDT,M3.2.0,M11.1.0"],
[196,"America/Tortola","AST4"],
[197,"America/Vancouver","PST8PDT,M3.2.0,M11.1.0"],
[198,"America/Whitehorse","MST7"],
[199,"America/Winnipeg","CST6CDT,M3.2.0,M11.1.0"],
[200,"America/Yakutat","AKST9AKDT,M3.2.0,M11.1.0"],
[201,"America/Yellowknife","MST7MDT,M3.2.0,M11.1.0"],
[202,"Antarctica/Casey","<+08>-8"],
[203,"Antarctica/Davis","<+07>-7"],
[204,"Antarctica/DumontDUrville","<+10>-10"],
[205,"Antarctica/Macquarie","AEST-10AEDT,M10.1.0,M4.1.0/3"],
[206,"Antarctica/Mawson","<+05>-5"],
[207,"Antarctica/McMurdo","NZST-12NZDT,M9.5.0,M4.1.0/3"],
[208,"Antarctica/Palmer","<-03>3"],
[209,"Antarctica/Rothera","<-03>3"],
[210,"Antarctica/Syowa","<+03>-3"],
[211,"Antarctica/Troll","<+00>0<+02>-2,M3.5.0/1,M10.5.0/3"],
[212,"Antarctica/Vostok","<+05>-5"],
[213,"Arctic/Longyearbyen","CET-1CEST,M3.5.0,M10.5.0/3"],
[214,"Asia/Aden","<+03>-3"],
[215,"Asia/Almaty","<+05>-5"],
[216,"Asia/Amman","<+03>-3"],
[217,"Asia/Anadyr","<+12>-12"],
[218,"Asia/Aqtau","<+05>-5"],
[219,"Asia/Aqtobe","<+05>-5"],
[220,"Asia/Ashgabat","<+05>-5"],
[221,"Asia/Atyrau","<+05>-5"],
[222,"Asia/Baghdad","<+03>-3"],
[223,"Asia/Bahrain","<+03>-3"],
[224,"Asia/Baku","<+04>-4"],
[225,"Asia/Bangkok","<+07>-7"],
[226,"Asia/Barnaul","<+07>-7"],
[227,"Asia/Beirut","EET-2EEST,M3.5.0/0,M10.5.0/0"],
[228,"Asia/Bishkek","<+06>-6"],
[229,"Asia/Brunei","<+08>-8"],
[230,"Asia/Chita","<+09>-9"],
[231,"Asia/Choibalsan","<+08>-8"],
[232,"Asia/Colombo","<+0530>-5:30"],
[233,"Asia/Damascus","<+03>-3"],
[234,"Asia/Dhaka","<+06>-6"],
[235,"Asia/Dili","<+09>-9"],
[236,"Asia/Dubai","<+04>-4"],
[237,"Asia/Dushanbe","<+05>-5"],
[238,"Asia/Famagusta","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[239,"Asia/Gaza","EET-2EEST,M3.4.4/50,M10.4.4/50"],
[240,"Asia/Hebron","EET-2EEST,M3.4.4/50,M10.4.4/50"],
[241,"Asia/Ho_Chi_Minh","<+07>-7"],
[242,"Asia/Hong_Kong","HKT-8"],
[243,"Asia/Hovd","<+07>-7"],
[244,"Asia/Irkutsk","<+08>-8"],
[245,"Asia/Jakarta","WIB-7"],
[246,"Asia/Jayapura","WIT-9"],
[247,"Asia/Jerusalem","IST-2IDT,M3.4.4/26,M10.5.0"],
[248,"Asia/Kabul","<+0430>-4:30"],
[249,"Asia/Kamchatka","<+12>-12"],
[250,"Asia/Karachi","PKT-5"],
[251,"Asia/Kathmandu","<+0545>-5:45"],
[252,"Asia/Khandyga","<+09>-9"],
[253,"Asia/Kolkata","IST-5:30"],
[254,"Asia/Krasnoyarsk","<+07>-7"],
[255,"Asia/Kuala_Lumpur","<+08>-8"],
[256,"Asia/Kuching","<+08>-8"],
[257,"Asia/Kuwait","<+03>-3"],
[258,"Asia/Macau","CST-8"],
[259,"Asia/Magadan","<+11>-11"],
[260,"Asia/Makassar","WITA-8"],
[261,"Asia/Manila","PST-8"],
[262,"Asia/Muscat","<+04>-4"],
[263,"Asia/Nicosia","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[264,"Asia/Novokuznetsk","<+07>-7"],
[265,"Asia/Novosibirsk","<+07>-7"],
[266,"Asia/Omsk","<+06>-6"],
[267,"Asia/Oral","<+05>-5"],
[268,"Asia/Phnom_Penh","<+07>-7"],
[269,"Asia/Pontianak","WIB-7"],
[270,"Asia/Pyongyang","KST-9"],
[271,"Asia/Qatar","<+03>-3"],
[272,"Asia/Qyzylorda","<+05>-5"],
[273,"Asia/Riyadh","<+03>-3"],
[274,"Asia/Sakhalin","<+11>-11"],
[275,"Asia/Samarkand","<+05>-5"],
[276,"Asia/Seoul","KST-9"],
[277,"Asia/Shanghai","CST-8"],
[278,"Asia/Singapore","<+08>-8"],
[279,"Asia/Srednekolymsk","<+11>-11"],
[280,"Asia/Taipei","CST-8"],
[281,"Asia/Tashkent","<+05>-5"],
[282,"Asia/Tbilisi","<+04>-4"],
[283,"Asia/Tehran","<+0330>-3:30"],
[284,"Asia/Thimphu","<+06>-6"],
[285,"Asia/Tokyo","JST-9"],
[286,"Asia/Tomsk","<+07>-7"],
[287,"Asia/Ulaanbaatar","<+08>-8"],
[288,"Asia/Urumqi","<+06>-6"],
[289,"Asia/Ust-Nera","<+10>-10"],
[290,"Asia/Vientiane","<+07>-7"],
[291,"Asia/Vladivostok","<+10>-10"],
[292,"Asia/Yakutsk","<+09>-9"],
[293,"Asia/Yangon","<+0630>-6:30"],
[294,"Asia/Yekaterinburg","<+05>-5"],
[295,"Asia/Yerevan","<+04>-4"],
[296,"Atlantic/Azores","<-01>1<+00>,M3.5.0/0,M10.5.0/1"],
[297,"Atlantic/Bermuda","AST4ADT,M3.2.0,M11.1.0"],
[298,"Atlantic/Canary","WET0WEST,M3.5.0/1,M10.5.0"],
[299,"Atlantic/Cape_Verde","<-01>1"],
[300,"Atlantic/Faroe","WET0WEST,M3.5.0/1,M10.5.0"],
[301,"Atlantic/Madeira","WET0WEST,M3.5.0/1,M10.5.0"],
[302,"Atlantic/Reykjavik","GMT0"],
[303,"Atlantic/South_Georgia","<-02>2"],
[304,"Atlantic/Stanley","<-03>3"],
[305,"Atlantic/St_Helena","GMT0"],
[306,"Australia/Adelaide","ACST-9:30ACDT,M10.1.0,M4.1.0/3"],
[307,"Australia/Brisbane","AEST-10"],
[308,"Australia/Broken_Hill","ACST-9:30ACDT,M10.1.0,M4.1.0/3"],
[309,"Australia/Currie","AEST-10AEDT,M10.1.0,M4.1.0/3"],
[310,"Australia/Darwin","ACST-9:30"],
[311,"Australia/Eucla","<+0845>-8:45"],
[312,"Australia/Hobart","AEST-10AEDT,M10.1.0,M4.1.0/3"],
[313,"Australia/Lindeman","AEST-10"],
[314,"Australia/Lord_Howe","<+1030>-10:30<+11>-11,M10.1.0,M4.1.0"],
[315,"Australia/Melbourne","AEST-10AEDT,M10.1.0,M4.1.0/3"],
[316,"Australia/Perth","AWST-8"],
[317,"Australia/Sydney","AEST-10AEDT,M10.1.0,M4.1.0/3"],
[318,"Europe/Amsterdam","CET-1CEST,M3.5.0,M10.5.0/3"],
[319,"Europe/Andorra","CET-1CEST,M3.5.0,M10.5.0/3"],
[320,"Europe/Astrakhan","<+04>-4"],
[321,"Europe/Athens","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[322,"Europe/Belgrade","CET-1CEST,M3.5.0,M10.5.0/3"],
[323,"Europe/Berlin","CET-1CEST,M3.5.0,M10.5.0/3"],
[324,"Europe/Bratislava","CET-1CEST,M3.5.0,M10.5.0/3"],
[325,"Europe/Brussels","CET-1CEST,M3.5.0,M10.5.0/3"],
[326,"Europe/Bucharest","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[327,"Europe/Budapest","CET-1CEST,M3.5.0,M10.5.0/3"],
[328,"Europe/Busingen","CET-1CEST,M3.5.0,M10.5.0/3"],
[329,"Europe/Chisinau","EET-2EEST,M3.5.0,M10.5.0/3"],
[330,"Europe/Copenhagen","CET-1CEST,M3.5.0,M10.5.0/3"],
[331,"Europe/Dublin","IST-1GMT0,M10.5.0,M3.5.0/1"],
[332,"Europe/Gibraltar","CET-1CEST,M3.5.0,M10.5.0/3"],
[333,"Europe/Guernsey","GMT0BST,M3.5.0/1,M10.5.0"],
[334,"Europe/Helsinki","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[335,"Europe/Isle_of_Man","GMT0BST,M3.5.0/1,M10.5.0"],
[336,"Europe/Istanbul","<+03>-3"],
[337,"Europe/Jersey","GMT0BST,M3.5.0/1,M10.5.0"],
[338,"Europe/Kaliningrad","EET-2"],
[339,"Europe/Kiev","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[340,"Europe/Kirov","MSK-3"],
[341,"Europe/Lisbon","WET0WEST,M3.5.0/1,M10.5.0"],
[342,"Europe/Ljubljana","CET-1CEST,M3.5.0,M10.5.0/3"],
[343,"Europe/London","GMT0BST,M3.5.0/1,M10.5.0"],
[344,"Europe/Luxembourg","CET-1CEST,M3.5.0,M10.5.0/3"],
[345,"Europe/Madrid","CET-1CEST,M3.5.0,M10.5.0/3"],
[346,"Europe/Malta","CET-1CEST,M3.5.0,M10.5.0/3"],
[347,"Europe/Mariehamn","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[348,"Europe/Minsk","<+03>-3"],
[349,"Europe/Monaco","CET-1CEST,M3.5.0,M10.5.0/3"],
[350,"Europe/Moscow","MSK-3"],
[351,"Europe/Oslo","CET-1CEST,M3.5.0,M10.5.0/3"],
[352,"Europe/Paris","CET-1CEST,M3.5.0,M10.5.0/3"],
[353,"Europe/Podgorica","CET-1CEST,M3.5.0,M10.5.0/3"],
[354,"Europe/Prague","CET-1CEST,M3.5.0,M10.5.0/3"],
[355,"Europe/Riga","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[356,"Europe/Rome","CET-1CEST,M3.5.0,M10.5.0/3"],
[357,"Europe/Samara","<+04>-4"],
[358,"Europe/San_Marino","CET-1CEST,M3.5.0,M10.5.0/3"],
[359,"Europe/Sarajevo","CET-1CEST,M3.5.0,M10.5.0/3"],
[360,"Europe/Saratov","<+04>-4"],
[361,"Europe/Simferopol","MSK-3"],
[362,"Europe/Skopje","CET-1CEST,M3.5.0,M10.5.0/3"],
[363,"Europe/Sofia","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[364,"Europe/Stockholm","CET-1CEST,M3.5.0,M10.5.0/3"],
[365,"Europe/Tallinn","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[366,"Europe/Tirane","CET-1CEST,M3.5.0,M10.5.0/3"],
[367,"Europe/Ulyanovsk","<+04>-4"],
[368,"Europe/Uzhgorod","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[369,"Europe/Vaduz","CET-1CEST,M3.5.0,M10.5.0/3"],
[370,"Europe/Vatican","CET-1CEST,M3.5.0,M10.5.0/3"],
[371,"Europe/Vienna","CET-1CEST,M3.5.0,M10.5.0/3"],
[372,"Europe/Vilnius","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[373,"Europe/Volgograd","MSK-3"],
[374,"Europe/Warsaw","CET-1CEST,M3.5.0,M10.5.0/3"],
[375,"Europe/Zagreb","CET-1CEST,M3.5.0,M10.5.0/3"],
[376,"Europe/Zaporozhye","EET-2EEST,M3.5.0/3,M10.5.0/4"],
[377,"Europe/Zurich","CET-1CEST,M3.5.0,M10.5.0/3"],
[378,"Indian/Antananarivo","EAT-3"],
[379,"Indian/Chagos","<+06>-6"],
[380,"Indian/Christmas","<+07>-7"],
[381,"Indian/Cocos","<+0630>-6:30"],
[382,"Indian/Comoro","EAT-3"],
[383,"Indian/Kerguelen","<+05>-5"],
[384,"Indian/Mahe","<+04>-4"],
[385,"Indian/Maldives","<+05>-5"],
[386,"Indian/Mauritius","<+04>-4"],
[387,"Indian/Mayotte","EAT-3"],
[388,"Indian/Reunion","<+04>-4"],
[389,"Pacific/Apia","<+13>-13"],
[390,"Pacific/Auckland","NZST-12NZDT,M9.5.0,M4.1.0/3"],
[391,"Pacific/Bougainville","<+11>-11"],
[392,"Pacific/Chatham","<+1245>-12:45<+1345>,M9.5.0/2:45,M4.1.0/3:45"],
[393,"Pacific/Chuuk","<+10>-10"],
[394,"Pacific/Easter","<-06>6<-05>,M9.1.6/22,M4.1.6/22"],
[395,"Pacific/Efate","<+11>-11"],
[396,"Pacific/Enderbury","<+13>-13"],
[397,"Pacific/Fakaofo","<+13>-13"],
[398,"Pacific/Fiji","<+12>-12"],
[399,"Pacific/Funafuti","<+12>-12"],
[400,"Pacific/Galapagos","<-06>6"],
[401,"Pacific/Gambier","<-09>9"],
[402,"Pacific/Guadalcanal","<+11>-11"],
[403,"Pacific/Guam","ChST-10"],
[404,"Pacific/Honolulu","HST10"],
[405,"Pacific/Kiritimati","<+14>-14"],
[406,"Pacific/Kosrae","<+11>-11"],
[407,"Pacific/Kwajalein","<+12>-12"],
[408,"Pacific/Majuro","<+12>-12"],
[409,"Pacific/Marquesas","<-0930>9:30"],
[410,"Pacific/Midway","SST11"],
[411,"Pacific/Nauru","<+12>-12"],
[412,"Pacific/Niue","<-11>11"],
[413,"Pacific/Norfolk","<+11>-11<+12>,M10.1.0,M4.1.0/3"],
[414,"Pacific/Noumea","<+11>-11"],
[415,"Pacific/Pago_Pago","SST11"],
[416,"Pacific/Palau","<+09>-9"],
[417,"Pacific/Pitcairn","<-08>8"],
[418,"Pacific/Pohnpei","<+11>-11"],
[419,"Pacific/Port_Moresby","<+10>-10"],
[420,"Pacific/Rarotonga","<-10>10"],
[421,"Pacific/Saipan","ChST-10"],
[422,"Pacific/Tahiti","<-10>10"],
[423,"Pacific/Tarawa","<+12>-12"],
[424,"Pacific/Tongatapu","<+13>-13"],
[425,"Pacific/Wake","<+12>-12"],
[426,"Pacific/Wallis","<+12>-12"],
[427,"Etc/GMT","GMT0"],
[428,"Etc/GMT-0","GMT0"],
[429,"Etc/GMT-1","<+01>-1"],
[430,"Etc/GMT-2","<+02>-2"],
[431,"Etc/GMT-3","<+03>-3"],
[432,"Etc/GMT-4","<+04>-4"],
[433,"Etc/GMT-5","<+05>-5"],
[434,"Etc/GMT-6","<+06>-6"],
[435,"Etc/GMT-7","<+07>-7"],
[436,"Etc/GMT-8","<+08>-8"],
[437,"Etc/GMT-9","<+09>-9"],
[438,"Etc/GMT-10","<+10>-10"],
[439,"Etc/GMT-11","<+11>-11"],
[440,"Etc/GMT-12","<+12>-12"],
[441,"Etc/GMT-13","<+13>-13"],
[442,"Etc/GMT-14","<+14>-14"],
[443,"Etc/GMT0","GMT0"],
[444,"Etc/GMT+0","GMT0"],
[445,"Etc/GMT+1","<-01>1"],
[446,"Etc/GMT+2","<-02>2"],
[447,"Etc/GMT+3","<-03>3"],
[448,"Etc/GMT+4","<-04>4"],
[449,"Etc/GMT+5","<-05>5"],
[450,"Etc/GMT+6","<-06>6"],
[451,"Etc/GMT+7","<-07>7"],
[452,"Etc/GMT+8","<-08>8"],
[453,"Etc/GMT+9","<-09>9"],
[454,"Etc/GMT+10","<-10>10"],
[455,"Etc/GMT+11","<-11>11"],
[456,"Etc/GMT+12","<-12>12"],
[457,"Etc/UCT","UTC0"],
[458,"Etc/UTC","UTC0"],
[459,"Etc/Greenwich","GMT0"],
[460,"Etc/Universal","UTC0"],
[461,"Etc/Zulu","UTC0"]
];

function HideS(s)
{
	var t=document.getElementById(s);
	if (t) t.className=s+" hide";
}

function ShowInfo()
{
	document.getElementById("info").className="info show";
	HideS("settings");
	HideS("alarms");
	HideS("main");
	HideS("log");
	return false;
}

function ShowSettings()
{
	e=document.getElementById("settings");
	if (e)
	{
		e.className="settings show";
		HideS("info");
		return false;
	} else
		return true;
}

function ShowAlarms()
{
	e=document.getElementById("alarms");
	if (e)
	{
		e.className="alarms show";
		HideS("info");
		return false;
	} else
		return true;
}

function ShowLog()
{
	e=document.getElementById("log");
	if (e)
	{
		e.className="log show";
		HideS("info");
		return false;
	} else
		return true;
}

function ShowMain()
{
	e=document.getElementById("main");
	if (e)
	{
		e.className="main show";
		HideS("info");
		return false;
	} else
		location.href='/';
}

function UpdateLog()
{
	var request = new XMLHttpRequest();
	request.onreadystatechange = function()
	{
		if (this.readyState == 4 && this.status == 200)
		{
			var rows = document.getElementById('log_table').getElementsByTagName('tr'), index;

			for (index = rows.length - 1; index >= 0; index--)
			{
				if (rows[index].className != 'sect_name')
					rows[index].parentNode.removeChild(rows[index]);
			}
			rows[0].insertAdjacentHTML("afterend", this.responseText);
		}
	}
	// send HTTP GET request
	request.open("GET", "log?table=1"); // Need =1 while not fixed https://github.com/espressif/arduino-esp32/issues/6759
	request.send(null);
}

function st(t, id, tag)
{
	var f=t.responseXML.getElementsByTagName(tag)[0];
	var h = document.getElementById(id);
	if(f && h) h.innerHTML = f.childNodes[0].nodeValue;
}

var last_slaves = "";

function slaves(t)
{
	var f=t.responseXML.getElementsByTagName("Slaves")[0];
	var h = document.getElementById("ip_tx_slaves");
	if (!f || !h) return;
	if (f.childNodes[0].nodeValue == last_slaves) return;
	last_slaves = f.childNodes[0].nodeValue;
	var ips = last_slaves.split(',');
	if (ips.length == 0) return;

	while (h.firstChild) h.removeChild(h.lastChild);

	for (i=0; i<ips.length; i++)
	{
		var ip = subnet + ips[i];
		var a = document.createElement('a');
		a.appendChild(document.createTextNode(ip));
		a.href = "http://" + ip + "/";
		a.target = "_blank";
		h.appendChild(a);
		h.appendChild(document.createElement('br'));
	}
}

function GetStatus()
{
	if (active)
	{
		nocache = "&nocache=" + Math.random() * 1000000;
		var request = new XMLHttpRequest();
		request.onreadystatechange = function()
		{
			if (this.readyState == 4)
			{
				if (this.status == 200)
				{
					if (this.responseXML != null)
					{
						st(this, "date", 'Date');
						st(this, "time", 'Time');
						st(this, "uptime", 'UpTime');
						st(this, "RSSI", 'RSSI');
						st(this, "pos", 'Now');
						st(this, "dest", 'Dest');
						st(this, "end1", 'End1');
						st(this, "end2", 'End2');
						st(this, "mqtt", 'MQTT');
						st(this, "voltage", 'Voltage');
						st(this, "temp", 'Temp');
						st(this, "led_mode", 'Mode');
						st(this, "led_level", 'Level');
						st(this, "lastcode", 'LastCode');
						slaves(this);
						lc=document.getElementById("log_count").innerHTML;
						st(this, "log_count", 'Log');
						if (document.getElementById("log_count").innerHTML != lc &&
							document.getElementById("log")) UpdateLog();
						if (document.getElementById("pos").innerHTML != document.getElementById("dest").innerHTML)
						timeout=500;
						else
						timeout=1000;
					}
				}
				timerId = setTimeout('GetStatus()', timeout);
			}
		}
		// send HTTP GET request
		request.open("GET", "xml");
		request.send(null);
	}
}

function Call(url)
{
	clearTimeout(timerId);
	var xhttp = new XMLHttpRequest();
	xhttp.open("GET", url);
	xhttp.send();
	timeout=500;
	GetStatus();
}

function Open() { Call("set?pos=0"); return false; }
function OpenA() { Call("set?pos=0" + addr_str()); return false; }
function Close() { Call("set?pos=100"); return false; }
function CloseA() { Call("set?pos=100" + addr_str()); return false; }
function Steps(s) { Call("set?steps="+s); return false; }
function StepsOvr(s) { Call("set?stepsovr="+s); return false; }
function Stop() { Call("stop"); return false; }
function StopA() { Call("stop?d" + addr_str()); return false; }
function TestPreset(p) { var s=document.getElementById(p).value;var m=document.getElementById("length").value;if (Number(s)>Number(m)) { s=m; document.getElementById(p).value=s; } StepsOvr(s); return false; }
function SetPreset(p) { document.getElementById(p).value = document.getElementById("pos").innerHTML; }

function Test(dir, speed)
{
	if (speed == 2) speed = '2'; else speed = '';
	document.getElementById("btn_up1").disabled=true;
	document.getElementById("btn_dn1").disabled=true;
	document.getElementById("btn_up2").disabled=true;
	document.getElementById("btn_dn2").disabled=true;
	pinout=document.getElementById("pinout").value;
	reversed=document.getElementById("reversed").value;
	delay=document.getElementById("delay" + speed).value;
	pwm=document.getElementById("pwm" + speed).value;
	var xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function()
	{
		if (this.readyState == 4)
		{
			if (this.status == 200)
			{
				document.getElementById("pos").innerHTML=this.responseText;
				document.getElementById("dest").innerHTML=this.responseText;
			}
			document.getElementById("btn_up1").disabled=false;
			document.getElementById("btn_dn1").disabled=false;
			document.getElementById("btn_up2").disabled=false;
			document.getElementById("btn_dn2").disabled=false;
		}
	};
	url="test?pinout="+pinout+"&reversed="+reversed+"&delay="+delay+"&pwm="+pwm;
	if (dir==1) url=url+"&up=1"; else url=url+"&down=1";
	xhttp.open("GET", url, true);
	xhttp.send();
}

function TestUp(n) { Test(1, n); }
function TestDown(n) { Test(0, n); }

function CheckPin(p, id)
{
	var pin = document.getElementById(id);
	if (!pin) return;
	if (p[pin.selectedIndex]) pin.selectedIndex = 0; // current pin busy, disable
	p[pin.selectedIndex] = 1; // mark busy
}

function SetDisabled(p, id)
{
	var pin = document.getElementById(id);
	if (!pin) return;
	var op = pin.getElementsByTagName("option");
	for (var i = 1; i < op.length; i++) op[i].disabled = p[i] && (pin.selectedIndex != i);
}

function SetChecked(id, ch)
{
	var cb = document.getElementById(id);
	if (!cb) return;
	cb.checked = ch;
}

function PinChange()
{
	var p = [0, 0, 0, 0, 0]; // all available pin options: none, 0, 2, 3, 15. 0 - free to use, 1 - busy
	const RX = 3; // index of RX pin in p array

	var slave = document.getElementById('slave');
	if (slave && slave.selectedIndex > 1) p[RX] = 1; // slave mode takes GPIO3 (RX)

	CheckPin(p, 'end2_pin');
	CheckPin(p, 'btn1_pin');
	CheckPin(p, 'btn2_pin');
	CheckPin(p, 'aux_pin');
	CheckPin(p, 'rf_pin');
	SetDisabled(p, 'end2_pin');
	SetDisabled(p, 'btn1_pin');
	SetDisabled(p, 'btn2_pin');
	SetDisabled(p, 'aux_pin');
	SetDisabled(p, 'rf_pin');
	var dis = p[RX];
	if (slave) // if RX pin busy, no slave mode allowed
	{
		if (slave.selectedIndex > 1) dis = false;
		var op = document.getElementById('slave').getElementsByTagName("option");
		for (var i = 2; i < op.length; i++) op[i].disabled = dis;
	}
}

function SetMSEnabled(td_id, enabled)
{
	var td = document.getElementById(td_id);
	if (td)
	{
		if (enabled) td.classList.remove("disabled"); else td.classList.add("disabled");
		var op = td.getElementsByTagName("input");
		for (var i = 0; i < op.length; i++) op[i].disabled = !enabled;
	}
}

function SetActionsEnabled(sel_id, chb_id, master)
{
	var sel = document.getElementById(sel_id);
	var chb = document.getElementById(chb_id);
	if (sel && chb)
	{
		sel.disabled = !chb.checked && master;
	}
}

function AddEventHandler(id)
{
	var chb = document.getElementById(id);
	if (chb && !chb.onchange) chb.onchange = MSChange;
}

function MSChange()
{
	var uart_master = false;
	var standalone = false;
	var slave = document.getElementById('slave');
	var ip_master = IPSlaves.length > 0;
	if (slave && slave.selectedIndex == 1) uart_master = true;
	if (slave && slave.selectedIndex == 0) standalone = true;
	var master = uart_master || (ip_master && standalone);

	SetMSEnabled('b1c', master);
	SetMSEnabled('b1l', master);
	SetMSEnabled('b2c', master);
	SetMSEnabled('b2l', master);
	SetActionsEnabled('btn1_click', 'b1c_0', master);
	SetActionsEnabled('btn1_long', 'b1l_0', master);
	SetActionsEnabled('btn2_click', 'b2c_0', master);
	SetActionsEnabled('btn2_long', 'b2l_0', master);
	AddEventHandler('b1c_0');
	AddEventHandler('b1l_0');
	AddEventHandler('b2c_0');
	AddEventHandler('b2l_0');
	if (uart_master || standalone)
		ShowEl('tr_ips');
	else
		HideEl('tr_ips');
	if (uart_master)
		ShowEl('tr_sl_ips');
	else
		HideEl('tr_sl_ips');
}

function AddOption(sel_id, opts, selected)
{
	select = document.getElementById(sel_id);
	for (var i = 0; i<opts.length/2; i++)
	{
		var opt = document.createElement('option');
		opt.value = opts[i*2];
		opt.innerHTML = opts[i*2+1];
			opt.selected = opt.value == selected;
		select.appendChild(opt);
	};
}

function EnableEl(id)
{
	h = document.getElementById(id);
	if(h) h.disabled = false;
}

function DisableEl(id)
{
	h = document.getElementById(id);
	if(h) h.disabled = true;
}

function ShowEl(id)
{
	h = document.getElementById(id);
	if(h) h.style.display = '';
}

function HideEl(id)
{
	h = document.getElementById(id);
	if(h) h.style.display = 'none';
}

function SetInnerHTML(id, val)
{
	h = document.getElementById(id);
	if(h) h.innerHTML = val;
}

function stt(t, id, tag)
{
	f=t.responseXML.getElementsByTagName(tag)[0];
	h = document.getElementById(id);
	if(f && h) h.value = f.childNodes[0].nodeValue;
}

function SetVal(id, val)
{
	h = document.getElementById(id);
	if(h) h.value = val;
}

function GetRFKey(cmd, msg)
{
	var request = new XMLHttpRequest(); DisableEl('btn'+cmd); SetVal('rfc'+cmd, msg);
	request.onreadystatechange = function()
	{
		if (this.readyState == 4)
		{
			SetVal('rfc'+cmd, 0);
			if (this.responseXML != null && this.status == 200)
			{
				stt(this, 'rfc'+cmd, 'LastCode');
			} EnableEl('btn'+cmd);
		}
	}
	request.open("GET", "xml?rf&get");
	request.send(null);
}

function RFCancel()
{
	location.href='/settings';
}

function Save()
{
	f=document.getElementById('lat');
	if (f) f.value = Math.round(f.value * 2048 * 1024);
	f=document.getElementById('lng');
	if (f) f.value = Math.round(f.value * 2048 * 1024);
	return true;
}

function SetCoord(lat, lng)
{
	f=document.getElementById('lat');
	if (f) f.value = Math.round(lat / 2048 / 1024 * 1000000) / 1000000;
	f=document.getElementById('lng');
	if (f) f.value = Math.round(lng / 2048 / 1024 * 1000000) / 1000000;
}

function AddWeek(sel_id, opts, selected)
{
	td = document.getElementById(sel_id);
	if (!td) return;
	for (var i = 0; i<7; i++)
	{
		var chb = document.createElement('input');
		chb.type = 'checkbox';
		chb.id = sel_id+'_'+i;
		chb.name = chb.id;
		chb.defaultChecked = selected & (1 << i);
		var lbl = document.createElement('label');
		lbl.appendChild(chb);
		lbl.htmlFor = chb.id;
		lbl.innerHTML += opts[i];

		td.appendChild(lbl);
		td.innerHTML += " ";
	};
}

function AddMasterSlave(sel_id, opts, selected)
{
	td = document.getElementById(sel_id);
	if (!td) return;
	for (var i = 0; i<=5; i++)
	{
		var chb = document.createElement('input');
		chb.type = 'checkbox';
		chb.id = sel_id+'_'+i;
		chb.name = chb.id;
		chb.defaultChecked = selected & (1 << i);
		var lbl = document.createElement('label');
		lbl.appendChild(chb);
		lbl.htmlFor = chb.id;
		lbl.innerHTML += opts[i];

		td.appendChild(lbl);
		td.innerHTML += " ";
	};
}

function a_ch(n)
{
	r=document.getElementById('a_time'+n);
	if (!r) return;
	f=document.getElementById('a_st'+n);
	if (f) f.hidden = !r.checked;
	f=document.getElementById('a_ss'+n);
	if (f) f.hidden = r.checked;
	f=document.getElementById('a_sf'+n);
	if (f) f.hidden = r.checked;
}

function AddRadio(id, val, label, n, ch)
{
	td=document.getElementById('a_sel'+n);
	if (!td) return;
	var inp = document.createElement('input');
	inp.type = 'radio';
	inp.id = id+n;
	inp.name = 'a_src'+n;
	inp.value = val;
	inp.defaultChecked = ch;
	inp.setAttribute( "onchange", "a_ch("+n+");");
	td.appendChild(inp);
	var lbl = document.createElement('label');
	lbl.htmlFor = inp.id;
	lbl.innerHTML = label;
	td.appendChild(lbl);
}

function AlarmRadio(n, l, v)
{
	AddRadio('a_time', '0', l[0], n, v==0)
	AddRadio('a_sunrise', '1', l[1], n, v==1)
	AddRadio('a_sunset', '2', l[2], n, v==2)
	a_ch(n);
}

function SetAlarm(n, v1, v2, v3, v4, v5, ms, en, time, tmin)
{
	AddOption('dest'+n, sh_a, v1);
	AddWeek('d'+n, dow, v2);
	AlarmRadio(n, a_srs, v3);
	AddOption('sunh'+n, a_shs, v4);
	AddOption('spd'+n, a_spd, v5);
	AddMasterSlave("ms"+n, m_s, ms);
	SetChecked('en'+n, en);
	SetVal('time'+n, time);
	SetVal('tmin'+n, tmin);
}

function AddAlarms(ALARMS, en, time, tmin, height, action, speed, repeat, master)
{
	for (n=0; n<ALARMS; n++)
	{
		document.write('<tr><td colspan="3"><hr/></td></tr>\n<tr><td class="en"><label for="en' + n + '">\n');
		document.write('<input type="checkbox" id="en' + n + '" name="en' + n + '"/>' + en + '</label></td>\n');
		//out += ((ini.alarms[a].flags & ALARM_FLAG_ENABLED) ? " checked" : "");
		document.write('<td colspan="2" id="a_sel' + n + '" class="a_radio"></td></tr>\n<tr><td></td><td>\n');
		document.write('<span id="a_st' + n + '"><label for="time' + n + '">' + time + '</label> <br/><input type="time" id="time' + n + '" name="time' + n + '" value="00:00" required></span>\n');
		document.write('<span id="a_ss' + n + '"><label for="sunh' + n + '">' + height + '</label> <br/><select id="sunh' + n + '" name="sunh' + n + '"></select></span>\n</td><td>\n');
		document.write('<span id="a_sf' + n + '"><label for="tmin' + n + '">' + tmin + '</label> <br/><input type="time" id="tmin' + n + '" name="tmin' + n + '" value="00:00" required></span>\n</td>');

		document.write('</tr><tr><td></td><td><label for="dest' + n + '"> ' + action + '</label> <br/><select id="dest' + n + '" name="dest' + n + '">\n</select>\n</td>');
		document.write('<td><label for="spd' + n + '"> ' + speed + '</label> <br/><select id="spd' + n + '" name="spd' + n + '">\n</select>\n</td></tr>');
		document.write('<tr><td>' + repeat + '</td><td colspan="2" class="days" id="d' + n + '">\n</td></tr>\n');

		if (master)
			document.write('<tr><td></td><td colspan="2" class="m_s" id="ms' + n + '"></td></tr>\n');
	}
}

function EnableClass(id, classname, enabled)
{
	var td = document.getElementById(id);
	if (td)
	{
		if (!enabled) td.classList.remove(classname); else td.classList.add(classname);
	}
}

function BtnMS(n)
{
	address = n;
	EnableClass('ms_all', 'sel', n==-1)
	EnableClass('ms_0', 'sel', n==0)
	EnableClass('ms_1', 'sel', n==1)
	EnableClass('ms_2', 'sel', n==2)
	EnableClass('ms_3', 'sel', n==3)
	EnableClass('ms_4', 'sel', n==4)
	EnableClass('ms_5', 'sel', n==5)
	return false;
}

function addr_str()
{
	var addr = "";
	if (address >= 0) addr = "&addr=" + address;
	return addr;
}
function Preset(n)
{
	Call("set?preset=" + n + addr_str());
	return false;
}

function PinoutChange()
{
	var pinout = document.getElementById('pinout');
	if (!pinout) return;

	var i = pinout.selectedIndex;
	if (i<4) ShowEl('po_step'); else HideEl('po_step');
	if (i==4 || i==5) ShowEl('po_ms'); else HideEl('po_ms');
	if (i>=4 && i<=7)
	{
		ShowEl('po_pwm');
		ShowEl('po_pwm2');
	} else
	{
		HideEl('po_pwm');
		HideEl('po_pwm2');
	}
	var st_time = document.getElementById('delay');
	if (st_time)
	{
		st_time = st_time.parentElement.parentElement;
		if (i<4)
			st_time.style.display = '';
		else
			st_time.style.display = 'none';
	}
	st_time = document.getElementById('delay2');
	if (st_time)
	{
		st_time = st_time.parentElement.parentElement;
		if (i<4)
			st_time.style.display = '';
		else
			st_time.style.display = 'none';
	}
}

function PwmChangeN(n)
{
	var pwm = document.getElementById('pwm'+n);
	if (!pwm) return;
	var pwm_num = document.getElementById('pwm_num'+n);
	if (!pwm_num) return;

	var i = pwm.value;
	if (i < 10) i = 0;
	if (i > 90) i = 100;
	pwm.value = i;
	pwm_num.innerText = '  ' + i + '%';
}

function PwmChange()
{
	PwmChangeN('');
	PwmChangeN('2');
}

function OnPageLoad()
{
	active=true;
	GetStatus();
	PinChange();
	MSChange();
	BtnMS(-1);
	PinoutChange();
	PwmChange();
}

function ip4_addr_get_byte(ip, b)
{
	return (ip >> (b*8)) & 0xFF;
}

function edtIP(header, id, inifield)
{
	var i;
	document.write('<tr><td class="idip">');
	document.write(header);
	document.write(":</td><td class=\"val_ip\">");
	for (i=0; i<4; i++)
	{
		document.write("<input type=\"text\" name=\"");
		document.write(id);
		document.write(i+1);
		document.write("\" value=\"");
		document.write(ip4_addr_get_byte(inifield, i));
		document.write("\" maxlength=3\"");
		document.write("\"/>");
		if (i<3) document.write(" . ");
	}
	document.write("</td></tr>\n");
}

function edtStr(header, id, inistr, len)
{
	document.write("<tr><td class=\"idname\">");
	document.write(header);
	document.write("</td><td class=\"val\"><input type=\"text\" name=\"");
	document.write(id);
	document.write("\" id=\"");
	document.write(id);
	document.write("\" value=\"");
	document.write(inistr);
	document.write("\" maxlength=\"");
	document.write(len);
	document.write("\"/></td></tr>\n");
}

function edtSteps(lbl, id, val, name, test, here)
{
	document.write("<tr><td class=\"idname\">");
	document.write(lbl);
	document.write("</td><td class=\"val_p\"><input type=\"text\" name=\"");
	document.write(name);
	document.write("\" id=\"");
	document.write(id);
	document.write("\" value=\"");
	document.write(val);
	document.write("\" maxlength=\"6\"/>\n<input type=\"button\" value=\"");
	document.write(test);
	document.write("\" onclick=\"TestPreset('");
	document.write(name);
	document.write("')\">\n<input type=\"button\" value=\"");
	document.write(here);
	document.write("\" onclick=\"SetPreset('");
	document.write(name);
	document.write("')\">\n</td></tr>\n");
}

// ip slaves

var subnet;
var IPSlaves = [];
var MaxIPSlaves = 10;
function SetIPSlaves(ip_slaves)
{
	var ip = self_ip.match(/^(\d+)\.(\d+)\.(\d+)\.(\d+)$/);
	if(!ip) return;
	subnet = ip[1] + '.' + ip[2] + '.' + ip[3] + '.';

	for (var i = 0; i<ip_slaves.length/2; i++)
	{
		if (ip_slaves[i*2] == 0) continue;
		IPSlaves.push({ ip: ip_slaves[i*2], num: ip_slaves[i*2+1], hostname: ''});
	}
	var id = document.getElementById('ip_slave_add');
	if (!id) return;
	var btn = document.createElement('button');
	btn.type = 'button';
	btn.innerHTML = 'Scan';
	btn.id = 'btn_scan';
	btn.setAttribute("onClick", 'SearchIPSlaves();');
	id.appendChild(btn);

	// manual add IP
	id.appendChild(document.createElement('br'));
	id.appendChild(document.createTextNode(subnet));
	var edt = document.createElement('input');
	edt.type = 'edit';
	edt.innerHTML = '0';
	edt.id = 'edt_ip';
	edt.maxLength = 3;
	id.appendChild(edt);
	id.appendChild(document.createTextNode(' '));
	var btn2 = document.createElement('button');
	btn2.type = 'button';
	btn2.innerHTML = '+';
	btn2.id = 'btn_add_ip';
	btn2.setAttribute("onClick", 'AddIP();');
	id.appendChild(btn2);
}

function ShowIPSlaves()
{
	var id = document.getElementById('ip_slaves');
	if (!id) return;
	while (id.firstChild) id.removeChild(id.lastChild);
	var table = document.createElement('table');
	table.classList.add("slave_ips");
	id.appendChild(table);
	for (var i = 0; i<IPSlaves.length; i++)
	{
		var tr = document.createElement('tr');
		table.appendChild(tr);
		var sel = document.createElement('select');
		sel.id = "ip_sl" + i;
		sel.name = 'snm' + i;
		sel.setAttribute("onChange", 'SetIPSlaveGr(' + i +');');
		var td = document.createElement('td');
		tr.appendChild(td);
		td.appendChild(sel);
		AddOption(sel.id, [0,'1',1,'2',2,'3',3,'4',4,'5'], IPSlaves[i].num);

		var ip = subnet + IPSlaves[i].ip;
		var a = document.createElement('a');
		a.appendChild(document.createTextNode(ip));
		//a.title = "my title text";
		a.href = "http://" + ip + "/";
		a.target = "_blank";
		if (i >= MaxIPSlaves) a.classList.add("limit");
		td = document.createElement('td');
		tr.appendChild(td);
		td.appendChild(a);

		var btn = document.createElement('button');
		btn.innerHTML = '−';
		btn.setAttribute("onClick", 'DelIPSlave(' + i + ');');
		td = document.createElement('td');
		tr.appendChild(td);
		td.appendChild(btn);

		var hn = document.createElement('span');
		hn.innerHTML = ' ' + IPSlaves[i].hostname;
		td = document.createElement('td');
		tr.appendChild(td);
		td.appendChild(hn);

		hn.innerHTML = ' ' + IPSlaves[i].hostname;
		td = document.createElement('td');
		tr.appendChild(td);
		td.appendChild(hn);

	}
	for (var i = 0; i<MaxIPSlaves; i++)
	{
		var he = document.createElement('input');
		he.type = 'hidden';
		he.name = 'sip' + i;
		if (i < IPSlaves.length) he.value = IPSlaves[i].ip; else he.value = 0;
		id.appendChild(he);
	}
	if (IPSlaves.length > MaxIPSlaves)
	{
		var mx = document.createElement('span');
		mx.innerHTML = 'Max ' + MaxIPSlaves + ' IPs';
		mx.classList.add("maxIPsWarn");
		id.appendChild(mx);
	}
	MSChange();
}

function DelIPSlave(n)
{
	IPSlaves.splice(n, 1);
	ShowIPSlaves();
	return false;
}

function SetIPSlaveGr(n)
{
	var sel = document.getElementById('ip_sl'+n);
	if (!sel) return;
	IPSlaves[n].num = sel.selectedIndex;
}

var thr, ip;
function SearchIPSlaves()
{
	DisableEl('btn_scan');
	// scan over a range of IP addresses
	thr = 20; // threads
	ip = 1;
	ScanNext();
	return false;
}

function ScanNext()
{
	if (thr > 0 && ip < 255)
	{
		request = new XMLHttpRequest();
		request.onerror = function(e) { };
		request.timeout = 1000;
		request.onreadystatechange = function()
		{
			if (this.readyState == 4)
			{
				if (this.status == 200)
				{
					var xml = this.responseXML;
					if (xml != null)
					{
						var h=xml.getElementsByTagName("IP")[0];
						var n=xml.getElementsByTagName("Name")[0];
						if (!n || !n.childNodes[0] || n.childNodes[0].nodeValue == '')
							n=xml.getElementsByTagName("Hostname")[0];
						if(h && n) AddNewIPSlave(h.childNodes[0].nodeValue, n.childNodes[0].nodeValue);
					}
				}
				thr++;
			}
		}
		// send HTTP GET request
			//document.getElementById("ip").innerText = subnet + ip;
			SetInnerHTML('btn_scan', subnet + ip);
			request.open("GET", "http://" + subnet + ip + "/xml");
			ip++;
			request.send(null);
			thr--;
	};
	if (ip<255) setTimeout(ScanNext, 60);
	if (ip == 255)
	{
		EnableEl('btn_scan');
		SetInnerHTML('btn_scan', 'Scan');
	}
}

function AddNewIPSlave(ip, hostname)
{
	var ips = ip.match(/^(\d+)\.(\d+)\.(\d+)\.(\d+)$/);
	if(!ips) return;
	if (ip == self_ip) return;

	for (var i = 0; i<IPSlaves.length; i++)
		if (IPSlaves[i].ip == ips[4])
		{
			IPSlaves[i].hostname = hostname;
			ShowIPSlaves();
			return; // already exists
		}
	IPSlaves.push({ ip: ips[4], num: 0, hostname: hostname});
	ShowIPSlaves();
}

function AddIP()
{
	var sel = document.getElementById('edt_ip');
	if (!sel) return;
	ip = Number(sel.value);
	if (isNaN(ip)) return;
	if (ip <= 0 || ip >= 255) return;
	if (subnet + ip == self_ip) return;
	AddNewIPSlave(subnet + ip, '');
}

function WakeUp(n)
{
	if (n)
		Call("set?wake&addr=" + n);
	else
		Call("set?wake");
}

function AddWakeUp()
{
	var id = document.getElementById('wake_btns');
	if (!id) return;
	for (var i=0; i<=5; i++)
	{
		var btn = document.createElement('button');
		btn.type = 'button';
		btn.innerHTML = (i ? i : '*');
		btn.id = 'btn_wake_' + i;
		btn.setAttribute("onClick", 'WakeUp(' + i + ');');
		id.appendChild(btn);
	}
}

function AddCities(selected)
{
	select = document.getElementById("city");
	for (var i = 0; i < citylist.length; i++)
	{
		var opt = document.createElement('option');
		opt.value = citylist[i][0];
		opt.innerHTML = citylist[i][1];
		opt.selected = opt.value == selected;
		select.appendChild(opt);
	};
	document.getElementById('timezone').children[0].hidden = true;
	document.getElementById('city').children[0].hidden = true;
}

function CitySelected()
{
	var c = document.getElementById('city');
	if (!c) return;
	var tz = citylist[c.value][2];
	document.getElementById('tz').value = tz;
	if (tz != "")
		document.getElementById('timezone').selectedIndex = 0;
	else
		document.getElementById('timezone').selectedIndex = 23; // UTC+0
}

function UTCSelected()
{
	var c = document.getElementById('timezone');
	if (!c) return;
	var tz = c.value;
	document.getElementById('city').selectedIndex = 0;
	document.getElementById('tz').value = "";
}

