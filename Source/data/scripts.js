var timerId;
var timeout=1000;
var active;
var address = -1;
window.onfocus = function() { active = true; clearTimeout(timerId); timeout=500; GetStatus(); };
window.onblur = function() { active = false; clearTimeout(timerId); };

const tzs=[-660,"UTC-11:00",-630,"UTC-10:30",-600,"UTC-10:00",-570,"UTC-9:30",-540,"UTC-9:00",-510,"UTC-8:30",-480,"UTC-8:00",-450,"UTC-7:30",
	-420,"UTC-7:00",-390,"UTC-6:30",-360,"UTC-6:00",-330,"UTC-5:30",-300,"UTC-5:00",-270,"UTC-4:30",-240,"UTC-4:00",-210,"UTC-3:30",-180,"UTC-3:00",
	-150,"UTC-2:30",-120,"UTC-2:00",-90,"UTC-1:30",-60,"UTC-1:00",-30,"UTC-0:30",0,"UTC+0:00",30,"UTC+0:30",60,"UTC+1:00",90,"UTC+1:30",120,"UTC+2:00",
	150,"UTC+2:30",180,"UTC+3:00",210,"UTC+3:30",240,"UTC+4:00",270,"UTC+4:30",300,"UTC+5:00",330,"UTC+5:30",360,"UTC+6:00",390,"UTC+6:30",
	420,"UTC+7:00",450,"UTC+7:30",480,"UTC+8:00",510,"UTC+8:30",540,"UTC+9:00",570,"UTC+9:30",600,"UTC+10:00",630,"UTC+10:30",660,"UTC+11:00",
	690,"UTC+11:30",720,"UTC+12:00",750,"UTC+12:30",780,"UTC+13:00",810,"UTC+13:30",840,"UTC+14:00"];

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
	request.open("GET", "log?table");
	request.send(null);
}

function st(t, id, tag)
{
	f=t.responseXML.getElementsByTagName(tag)[0]; 
	h = document.getElementById(id);
	if(f && h) h.innerHTML = f.childNodes[0].nodeValue; 
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
						st(this, "time", 'Time');
						st(this, "uptime", 'UpTime');
						st(this, "RSSI", 'RSSI');
						st(this, "pos", 'Now');
						st(this, "dest", 'Dest');
						st(this, "end1", 'End1');
						st(this, "end2", 'End2');
						st(this, "mqtt", 'MQTT');
						st(this, "voltage", 'Voltage');
						st(this, "led_mode", 'Mode');
						st(this, "led_level", 'Level');
						st(this, "lastcode", 'LastCode');
						lc=document.getElementById("log_count").innerHTML;
						st(this, "log_count", 'Log');
						if (document.getElementById("log_count").innerHTML != lc &&
							document.getElementById("log")) UpdateLog();
						if (document.getElementById("pos").innerHTML != document.getElementById("dest").innerHTML)
						timeout=500;
						else
						timeout=5000;
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
	var master = false;
	var slave = document.getElementById('slave');
	if (slave && slave.selectedIndex == 1) master = true;

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

function SetAlarm(n, v1, v2, v3, v4, v5, ms, en, time)
{
	AddOption('dest'+n, sh_a, v1);
	AddWeek('d'+n, dow, v2);
	AlarmRadio(n, a_srs, v3);
	AddOption('sunh'+n, a_shs, v4);
	AddOption('spd'+n, a_spd, v5);
	AddMasterSlave("ms"+n, m_s, ms);
	SetChecked('en'+n, en);
	SetVal('time'+n, time);
}

function AddAlarms(ALARMS, en, time, height, action, speed, repeat, master)
{
	for (n=0; n<ALARMS; n++)
	{
		document.write('<tr><td colspan="3"><hr/></td></tr>\n<tr><td class="en"><label for="en' + n + '">\n');
		document.write('<input type="checkbox" id="en' + n + '" name="en' + n + '"/>' + en + '</label></td>\n');
		//out += ((ini.alarms[a].flags & ALARM_FLAG_ENABLED) ? " checked" : "");
		document.write('<td colspan="2" id="a_sel' + n + '" class="a_radio"></td></tr>\n<tr><td></td><td class="narrow">\n<span id="a_st' + n +'">');
		document.write('<label for="time' + n + '">' + time + '</label> <br/><input type="time" id="time' + n + '" name="time' + n + '" value="00:00" required></span>\n');
		document.write('<span id="a_ss' + n + '"><label for="sunh' + n + '">' + height + '</label> <br/><select id="sunh' + n + '" name="sunh' + n + '"></select></span>\n</td>\n');

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
	if (i==5 || i==7)
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
function SetIPSlaves(ip_slaves)
{
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
}

function ShowIPSlaves()
{
	var id = document.getElementById('ip_slaves');
	if (!id) return;
	while (id.firstChild) id.removeChild(id.lastChild);
	var ip = self_ip.match(/^(\d+)\.(\d+)\.(\d+)\.(\d+)$/);
    if(!ip) return;
	subnet = ip[1] + '.' + ip[2] + '.' + ip[3] + '.';
	for (var i = 0; i<IPSlaves.length; i++)
	{
		var sel = document.createElement('select');
		sel.id = "ip_sl" + i;
		sel.setAttribute("onChange", 'SetIPSlaveGr(' + i +');');
		id.appendChild(sel);
		AddOption(sel.id, [0,'1',1,'2',2,'3',3,'4',4,'5'], IPSlaves[i].num);
		var lbl = document.createElement('label');
		lbl.htmlFor = sel.id;
		lbl.innerHTML = ' ' + subnet + IPSlaves[i].ip + ' ';
		id.appendChild(lbl);

		var btn = document.createElement('button');
		btn.innerHTML = '−';
		btn.setAttribute("onClick", 'DelIPSlave(' + i + ');');
		id.appendChild(btn);
		var hn = document.createElement('span');
		hn.innerHTML = ' ' + IPSlaves[i].hostname;
		id.appendChild(hn);
		id.appendChild(document.createElement('br'));
	}
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

	for (var i = 0; i<IPSlaves.length; i++)
		if (IPSlaves[i].ip == ips[4]) return; // already exists

	IPSlaves.push({ ip: ips[4], num: 0, hostname: hostname});
	ShowIPSlaves();
}