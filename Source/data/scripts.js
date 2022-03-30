var timerId;
var timeout=1000;
var active;
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
						st(this, "switch", 'End1');
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
function Close() { Call("set?pos=100"); return false; }
function Steps(s) { Call("set?steps="+s); return false; }
function StepsOvr(s) { Call("set?stepsovr="+s); return false; }
function Stop() { Call("stop"); return false; }
function TestPreset(p) { var s=document.getElementById(p).value;var m=document.getElementById("length").value;if (Number(s)>Number(m)) { s=m; document.getElementById(p).value=s; } StepsOvr(s); return false; }
function SetPreset(p) { document.getElementById(p).value = document.getElementById("pos").innerHTML; }

function Test(dir)
{
	document.getElementById("btn_up").disabled=true;
	document.getElementById("btn_dn").disabled=true;
	pinout=document.getElementById("pinout").value;
	reversed=document.getElementById("reversed").value;
	delay=document.getElementById("delay").value;
	var xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function() 
	{
		if (this.readyState == 4 && this.status == 200) 
		{
			document.getElementById("btn_up").disabled=false;
			document.getElementById("btn_dn").disabled=false;
			document.getElementById("pos").innerHTML=this.responseText;
			document.getElementById("dest").innerHTML=this.responseText;
		}
	};
	url="test?pinout="+pinout+"&reversed="+reversed+"&delay="+delay;
	if (dir==1) url=url+"&up=1"; else url=url+"&down=1";
	xhttp.open("GET", url, true);
	xhttp.send();
}

function TestUp() { Test(1); }
function TestDown() { Test(0); }

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

function PinChange()
{
	var p = [0, 0, 0, 0, 0]; // all available pin options: none, 0, 2, 3, 15. 0 - free to use, 1 - busy
	const RX = 3; // index of RX pin in p array

	var slave = document.getElementById('slave');
	if (slave && slave.selectedIndex > 1) p[RX] = 1; // slave mode takes GPIO3 (RX)

	CheckPin(p, 'btn_pin');
	CheckPin(p, 'aux_pin');
	CheckPin(p, 'rf_pin');
	SetDisabled(p, 'btn_pin');
	SetDisabled(p, 'aux_pin');
	SetDisabled(p, 'rf_pin');
	var dis = p[RX];
	if (slave)
	{
		if (slave.selectedIndex > 1) dis = false;
		var op = document.getElementById('slave').getElementsByTagName("option");
		for (var i = 2; i < op.length; i++) op[i].disabled = dis;
	}
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